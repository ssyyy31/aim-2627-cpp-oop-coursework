# Design and debugging record

对应实现：`include/` + `src/` 下的 `aim_oop_core` 核心库、`src/debug/main.cpp` 独立旧控制器、
`ros2_ws/` 中的 ROS 2 集成。本文档描述的行为与代码一一对应。

## Class responsibilities

四个命名空间各自只承担一类职责，依赖方向严格单向：
`app → dispatch → warehouse`，`dispatch → audit`，`warehouse` 与 `audit` 互不依赖、也不依赖上层。

| 命名空间 | 核心类型 | 职责 | 明确不做 |
| --- | --- | --- | --- |
| `warehouse` | `Order`、`PickStation`、`RobotState`、`Inventory` | 领域状态与不变量：站点注册、按 ID 查询、列出站点、按订单筛选合格候选站、原子扣减并占用机器人、校验并完成订单 | 不知道策略名称，不包含 `dispatch/` 或 ROS 头文件，不做审计记录 |
| `dispatch` | `DispatchPolicy`、`PriorityPolicy`、`StockPolicy`、`make_policy`、`Scheduler`、`DispatchDecision` | 策略选择与派单编排：候选筛选 → 策略选择 → 原子扣减 → 写审计；以及站点与订单双重匹配的任务完成 | 策略不持库存、不改状态；`Scheduler` 不实现库存扣减细节 |
| `audit` | `TraceEvent`、`Trace` | 按插入顺序保存成功事件的容器 | 不校验订单合法性，不调度器 |
| `app` | `normalize_sku`、`valid_station_id`、`valid_order_id`、`run`、`print_trace` | 输入规范化、命令行入口与格式化输出 | 不实现库存扣减或状态转换规则 |

`Inventory` 内部使用 `std::map<std::string, PickStation>`，以站点 ID 为键，因此
`available_for()` 的遍历天然满足"候选站按站点 ID 字典序升序"的要求，无需额外排序。
公开方法签名与模板头文件保持一致，未新增或删除任何公开接口，也未使用全局变量保存状态。

所有修改类操作遵循"先完成全部合法性校验，再写入状态"的顺序，任何一条校验不通过即返回失败，
容器内不留部分修改。

## Strategy choice

两种策略都继承 `dispatch::DispatchPolicy`，输入是已按站点 ID 字典序排列的合格候选站列表，
只返回选中的站点 ID，不修改任何状态。

| 策略 | `name()` | 选择规则 |
| --- | --- | --- |
| `PriorityPolicy` | `priority` | 取候选列表首项，即字典序最小的合格站点 |
| `StockPolicy` | `stock` | 取当前库存最多者；并列最多时取字典序较小的站点 ID |

`StockPolicy::select` 用一次线性扫描维护 `best`，仅在 `current.stock > best->stock`（严格大于）时替换，
相等时保留先出现的站点；因为候选列表已按站点 ID 升序，先出现者即字典序较小者，平局规则由此自然成立。
策略可在不改动 `Scheduler` 任何一行的前提下通过 `dispatch::make_policy("priority" / "stock")` 切换。

两种策略会选出不同站点的具体输入示例（同一初始状态、同一订单）：

初始 `add_station("S1", 2)`、`add_station("S2", 4)`，两台机器人均为 `idle`；
提交订单 `{"O1", "SKU-A", 1}`。

| 策略 | 候选站（字典序） | 选中 | 派单后状态 |
| --- | --- | --- | --- |
| `priority` | S1(2, idle)、S2(4, idle) | **S1** | S1 库存 2→1、busy、持有 O1；S2 保持 4、idle |
| `stock` | S1(2, idle)、S2(4, idle) | **S2** | S2 库存 4→3、busy、持有 O1；S1 保持 2、idle |

平局示例（对应公开测试 `test_stock_policy_and_failed_submission`）：初始
S1=1、S2=4、S3=4，提交 `{"O1", "SKU-A", 1}` 时三站都满足候选条件（S1 库存恰好等于数量，
**并未**因库存不足被排除）；S2 与 S3 库存并列最多，`stock` 选字典序较小的 **S2**。

## State transitions

`Scheduler::submit` 的完整顺序：校验订单 ID/SKU/数量 → `Inventory::available_for` 取候选 →
候选为空即拒绝 → `policy_->select` 选站 → `Inventory::reserve` 原子扣减并置 busy →
成功才追加审计。策略选择之前不修改任何状态。

`Scheduler::complete(station_id, order_id, failure_reason)` 中两个参数都必须精确匹配：
`station_id` 是目标站点，`order_id` 必须等于该站当前持有的订单 ID，二者不存在提示或回退关系。
调用方负责记录成功派单返回的真实站点，`DispatcherNode` 因此维护
`accepted_stations_`（订单 ID → 站点 ID）映射，收到两字段的 `COMPLETE O1` 时按该映射查回真实站点。
失败时写入 `failure_reason`，取值依次为 `invalid station id`、`invalid order id`、
`unknown station`、`robot not busy`、`order not held by station`。

| 操作情形 | 库存 | 机器人状态 | 当前订单 ID | `audit::Trace` |
| --- | --- | --- | --- | --- |
| 派单成功 | 选中站点减去订单数量（结果 ≥ 0） | 选中站点 idle → busy | 选中站点记录该订单 ID | 追加 `DISPATCH <order_id> <station_id>` |
| 派单失败（非法数量 / 非法 ID / 无候选站） | 所有站点不变 | 所有站点不变 | 所有站点不变 | 不追加任何记录 |
| 完成成功 | 目标站点不变（已拣出货不恢复） | 目标站点 busy → idle | 目标站点清空 | 追加 `COMPLETE <order_id> <station_id>` |
| 重复完成 | 不变 | 不变 | 不变 | 不追加记录 |
| 错误完成（站点不持有该订单 / 站点不存在） | 不变 | 不变 | 不变 | 不追加记录 |

输入清洗与校验规则：`app::normalize_sku` 去除首尾空白并把剩余字符全部转为大写
（`" sku-a "` → `"SKU-A"`）；`app::valid_station_id` 与 `app::valid_order_id`
拒绝空字符串以及首尾含空白字符的输入（`"S1 "` → `false`）。`warehouse::Inventory` 内部
对同一套规则做了等价自检（不反向依赖 `app` 层），因此：`add_station` 遇到非法站点 ID 或
负库存抛出 `std::invalid_argument`；重复注册同一站点 ID 抛出 `std::invalid_argument`
且不覆盖原有库存与机器人状态；订单数量 ≤ 0 时候选集为空，派单被拒绝。
全局不变量成立：任意站点库存恒 ≥ 0，同一台机器人同一时刻最多持有一张订单。

## Debugging evidence

### 复现输入

`src/debug/main.cpp` 的固定场景，独立于 `aim_oop_core`：初始 `S1=3`、`S2=5`，两台机器人均空闲。

1. 提交 O1（数量 2）→ S1 接受，库存 3→1，置 busy
2. 提交 O2（数量 6）→ S1 已 busy 跳过，S2 库存 5 < 6，必须拒绝
3. 完成 O1 → S1 回到 idle，库存保持 1（不恢复）
4. 提交 O3（数量 1）→ S1 空闲且库存恰好 1，接受，最终 S1 库存 0

### 错误现象

```text
S1 no -1 yes 0
```

第 3 个字段应为 `5`，实际为 `-1`：被拒绝的 O2 仍把 S2 库存扣成了负数，违反"失败无副作用"。

### 根因分析

原 `submit` 在找到第一个空闲站点后**先扣减再判断**：

```cpp
item.second -= quantity;          // 先写状态
if (item.second < 0) {
    return {false, "", "insufficient stock"};   // 后判失败
}
```

扣减已经落到 `stock_` 容器里，此时返回失败并不会撤销写入，库存被永久污染成 `-1`。
原代码还缺少订单数量合法性校验，`quantity <= 0` 会被当作"库存充足"而成功派单。

### 修复前后的逻辑差异

修复只调整校验与修改的先后顺序，保留原有类结构、公开方法签名、控制流
（仍按站点 ID 顺序取第一个空闲站点，该站库存不足即拒绝，不继续扫描后续站点）
以及五个输出字段的打印格式：

```cpp
if (item.second < quantity) {           // 先只读校验，不写容器
    return {false, "", "insufficient stock"};
}
item.second -= quantity;                // 全部条件满足后才扣减
busy_[station_id] = true;
orders_[station_id] = order_id;
return {true, station_id, "accepted"};
```

修复后输出：

```text
S1 no 5 yes 0
```

> 注：本仓库交付时未在 Git 历史中保留修复前的独立提交（会话中断导致），
> 因此按题面允许的方式，在此手工记录初始错误输出、修复后输出与上述缺陷分析。

### 边界验证记录

| # | 场景 | 输入 | 预期结果 | 实际运行结果 |
| --- | --- | --- | --- | --- |
| 1 | 库存不足 | `S1=3, S2=5`；O1 数量 2 成功后提交 O2 数量 6 | O2 被拒绝；S2 库存保持 5、仍空闲、无订单 | `S1 no 5 yes 0` — 第 2 字段 `no`、第 3 字段 `5`，与预期一致 |
| 2 | 库存恰好等于订单数量 | S1 完成 O1 后库存 1；提交 O3 数量 1 | O3 被接受，S1 库存 1→0 | 第 4 字段 `yes`、第 5 字段 `0`（3 − 2 − 1），与预期一致 |
| 3 | 忙碌站点被跳过 | O1 持有 S1 期间提交 O2 | 候选扫描越过 busy 的 S1，只在 S2 上判定 | 修复后 S1 保持库存 1、busy 状态与订单归属不变 |
| 4 | 非法数量（`quantity <= 0`） | 提交数量 0 或 −3 | 拒绝，不修改任何站点库存与忙碌状态 | 代码走查确认：`LegacyDispatcher::submit` 在任何写入前由 `quantity <= 0` 分支返回；公开测试未直接覆盖该输入，以隐藏测试为准 |
| 5 | 失败后继续派单 | 拒绝 O2 之后提交 O3 数量 1 | 后续派单不受失败请求影响 | O3 正常被 S1 接受，输出第 4 字段 `yes` |
| 6 | 重复完成同一订单 | 核心库：对已 idle 的站点再次 `complete` | 返回 `false`，`failure_reason = "robot not busy"`，状态与 Trace 均不变 | 代码走查确认：`Scheduler::complete` 先检查 `robot_state` 再调用 `finish`，失败路径提前返回且无写入；公开测试未直接覆盖该输入，以隐藏测试为准 |

上述 1–3 与 5 由 `./build/debug_task` 单次运行的五个输出字段直接体现（该程序只打印这五个字段）；
4 与 6 的结论来自对上述校验分支的逐行走查，公开测试未直接覆盖。可执行验证命令见
[README.md](README.md)。

**验证状态说明**：本机 `E:\aim-2627-cpp-oop-coursework` 已完成编译与测试。`tools/check.sh`
（`cmake` 构建 + `ctest` 公开测试）全部用例通过，`./build/debug_task` 输出
`S1 no 5 yes 0`，`python3 tools/validate_structure.py` 输出 `Structure OK`。上表"实际运行结果"
一列为上述命令的真实输出。
