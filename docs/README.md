# Operation and verification

本文档面向接手维护者，所有命令均从仓库根目录出发，可直接复制执行。
环境要求：支持 C++17 的编译器、CMake 3.16 及以上；核心库仅依赖 C++ 标准库。

## Build

从干净仓库开始，本地配置、构建、公开测试、演示程序与结构校验：

```bash
cmake -S . -B build
cmake --build build --parallel 4
./build/public_tests
./build/core_demo
python3 tools/validate_structure.py
```

一条命令完成配置、构建与全部注册测试（等价于上面的构建加 ctest）：

```bash
bash tools/check.sh
```

预期结果：

| 命令 | 预期输出 / 退出码 |
| --- | --- |
| `cmake --build build --parallel 4` | 编译无错误、无警告，生成 `aim_oop_core`、`core_demo`、`public_tests`、`dispatch_cli`、`debug_task` |
| `./build/public_tests` | 输出 `PUBLIC 2/2`，退出码 `0` |
| `./build/core_demo` | 仅输出版本号 `AIM OOP CW2 1.0`，退出码 `0`（`./build/dispatch_cli` 无参数时才输出 `Use the ROS 2 demo package for the end-to-end runtime.`） |
| `python3 tools/validate_structure.py` | 输出 `Structure OK`，退出码 `0` |
| `ctest --test-dir build --output-on-failure` | `public_tests` 与 `debug_task` 两项均为 `Passed` |

## Debug output

Q4 是独立于核心库的旧控制器程序，构建后可直接运行：

```bash
./build/debug_task
```

修复前的初始错误输出（模板原样，先扣库存再判失败，导致 `S2` 被扣成负数）：

```text
S1 no -1 yes 0
```

修复后的目标输出（失败派单严格无副作用）：

```text
S1 no 5 yes 0
```

五个字段含义：`S1` 为订单 O1 分配到的站点；`no` 为 O2（数量 6）被拒绝；`5` 为被拒绝后 `S2` 的剩余库存（未被污染）；`yes` 为完成 O1 后 O3（数量 1）被接受；`0` 为接受 O3 后 `S1` 的最终库存（3 − 2 − 1）。

缺陷复现、根因分析与边界验证记录见 [DESIGN.md](DESIGN.md) 的 Debugging evidence 小节。

## ROS 2 demo

ROS 2 部分需要 Ubuntu 24.04 + ROS 2 Jazzy（Base 即可），与本地 CMake 构建相互独立，复用同一份
`include/` 与 `src/` 核心源码：

```bash
source /opt/ros/jazzy/setup.bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch warehouse_dispatch warehouse_demo.launch.py
```

预期输出对照（终端日志与 `/dispatch_results` 话题的 `std_msgs/String` 消息，按下列顺序出现且仅出现六条核心结果）：

| # | 输入事件（`/warehouse_events`） | 调度判定 | 预期输出（`/dispatch_results`） | 状态变化 |
| --- | --- | --- | --- | --- |
| 1 | `ORDER O1 SKU-A 1` | S1(3, idle)、S2(2, idle) 均合格，priority 取字典序最小 | `ACCEPT O1 S1` | S1 库存 3→2，置 busy，持有 O1 |
| 2 | `ORDER O2 SKU-A 1` | S1 已 busy 被排除，仅 S2 合格 | `ACCEPT O2 S2` | S2 库存 2→1，置 busy，持有 O2 |
| 3 | `ORDER O3 SKU-A 2` | 两站均 busy，候选为空 | `REJECT O3` | 无变化，不写审计轨迹 |
| 4 | `COMPLETE O1` | 匹配到持有 O1 且 busy 的 S1 | `COMPLETE O1` | S1 busy→idle，清空订单，库存保持 2 |
| 5 | `ORDER O4 SKU-A 3` | S1 idle 但库存 2 < 3，S2 busy，候选为空 | `REJECT O4` | 无变化，不写审计轨迹 |
| 6 | `WAIT 1` | 等待控制指令，不改状态 | `WAITED 1` | 无变化，随后两节点自动退出 |

拒绝行不带尾随空格；评分按每行去除首尾空白后比较。全部六条输出完成后，`ros2 launch`
进程自动返回命令行提示符，不留常驻节点进程。

## Failure notes

1. **`ros2: command not found` 或 `colcon: command not found`**
   原因是未加载 ROS 环境。执行 `source /opt/ros/jazzy/setup.bash` 后重试；若
   `colcon` 仍缺失，安装 `ros-jazzy-dev-tools`。可用 `printenv ROS_DISTRO` 确认应输出
   `jazzy`。

2. **launch 结束后节点未自动退出、进程残留**
   原因是自动退出链路被破坏。定位步骤：
   ```bash
   ros2 node list                       # 确认 warehouse_simulator / warehouse_dispatcher 是否仍在
   ros2 topic echo /warehouse_events    # 确认第六条 WAIT 1 是否已发出
   ```
   若事件序列发完但节点仍在，检查 `simulator_node.cpp` 中发完事件后是否调用了
   `rclcpp::shutdown()`，以及 `dispatcher_node.cpp` 的 `WAIT` 分支是否保留了
   `rclcpp::shutdown()`；两者缺一都会导致进程常驻。

3. **输出条数不是六条，或出现 `BAD`**
   通常是话题类型或事件字段数不匹配。用 `ros2 topic info /warehouse_events --verbose`
   确认消息类型为 `std_msgs/msg/String`；`ORDER` 事件必须是四个字段
   （`ORDER <订单ID> <SKU> <数量>`），字段数不为 4 会走 `BAD` 分支。

4. **`./build/public_tests` 报 `ERROR: TODO: implement ...`**
   说明核心库仍有占位实现未替换。执行 `grep -R "TODO: implement" -n include src`
   定位残留文件，补齐后重新 `cmake --build build`。

5. **`Structure check failed`**
   按脚本给出的条目逐条修：include guard 必须是文件第一行 `#ifndef`、第二行 `#define`；
   任何头文件中都不得出现 `using namespace`；`include/warehouse/domain.hpp` 不得包含
   `dispatch/` 或 `rclcpp` 相关头文件。禁止通过修改 `tools/validate_structure.py`
   或构建文件绕过检查。

6. **本地构建目录污染仓库**
   `build/`、`install/`、`log/` 不得提交。提交前执行 `git status` 确认只有
   `include/`、`src/`、`docs/`、`README.md` 与 `.agent-sessions/bundles/` 的改动。
