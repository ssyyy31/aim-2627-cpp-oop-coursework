# AIM 2627 C++ OOP Coursework 02

仓库拣货机器人任务分配系统。全部任务规格见 [Problem.pdf](Problem.pdf)，本 README 只负责本地启动。

```bash
cmake -S . -B build
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/debug_task
```

## 修改范围

- 可以修改：`include/`、`src/`、`README.md`、`docs/`。
- 不要修改：`tests/`、`tools/`、`.github/`、`CMakeLists.txt`、`Problem.pdf`。
- `src/debug/main.cpp` 是 Q4 的独立调试任务；不要通过绕开缺陷的输出方式得分。

## ROS 2 演示

ROS 2 部分在 `ros2_ws/` 中独立构建，复用 `include/` 和 `src/` 的核心实现。课程使用 ROS 2 Jazzy，演示不依赖硬件。命令见题面 Q2。

## 文档交付

在 `docs/README.md` 和 `docs/DESIGN.md` 中完成 Q5。初始文件只是提示骨架，正式要求以题面为准。

## Agent 会话归档

允许并鼓励合理使用 AI 辅助开发，但必须保留可核对的过程记录。参照 Python CW 与 C++ Assessment 01，本作业使用 Agent Session Commit（AgentLedger）v0.1.5。

在个人学生仓库根目录配置，不要在教师完整目录或其他项目中安装：

```bash
python -m venv .venv-agent-session-commit
source .venv-agent-session-commit/bin/activate
python -m pip install --index-url https://pypi.org/simple 'agent-session-commit[pre-commit]==0.1.5'
agent-session-commit install --pre-commit
agent-session-commit status
```

每次正常 `git commit` 后，post-commit hook 会把新增 AI 会话打包到 `.agent-sessions/bundles/` 并 amend 到同一提交。`.agent-sessions/bundles/` 必须提交；`.agent-sessions/source/` 与 `.venv-agent-session-commit/` 已忽略。没有使用 AI 时，在 README 末尾明确声明并提供开发记录。

提交前检查：

```bash
git rev-parse HEAD
git show --stat HEAD -- .agent-sessions/
```

## AI 使用与开发过程

**本作业绝大部分由本人自主完成。** 题面解读、模块与类的划分、`warehouse::Inventory` 状态转换与失败无副作用设计、`dispatch` 两种策略与 `Scheduler` 的算法逻辑、Q4 缺陷的根因判断与修复思路、以及 `docs/README.md`、`docs/DESIGN.md` 两篇文档的内容，均由本人独立推导并编写，公开方法签名与模板头文件未作改动。开发过程中使用了 AI 辅助工具，仅用于下文的走查与校对环节。

**AI 仅用于以下辅助环节**

- 查阅 C++ 语法细节（引用传递、`const` 正确性、标准库容器用法），确认写法与既有接口一致。
- 对本人已写好的函数做一遍走查，提示可能遗漏的边界条件与异常输入分支。
- 构建与运行命令的语法提示（`cmake` / `ctest` / 脚本调用方式）。
- 文档措辞的润色与错别字校对，不涉及技术判断。

**未采用的建议**：曾考虑把候选过滤完全下放到策略层，因与题面"策略仅接收已筛选候选站"的职责边界冲突而弃用；曾考虑在 ROS 节点复制一份库存校验，因 Q2 禁止事项而弃用。两处取舍均由本人依据题面判定。

**验证方式**：以 `tools/check.sh`（`cmake` 构建 + `ctest` 公开测试）、`./build/debug_task` 输出 `S1 no 5 yes 0`、`python3 tools/validate_structure.py` 输出 `Structure OK` 为准。上述命令均已在本机实际跑通，公开测试全部通过，详见 `docs/DESIGN.md` 末尾的验证状态说明。

**本次复核（2026-11-26 晚间）**：AI 对全部实现与文档做了一轮走查，并直接修正了 `docs/DESIGN.md` 中两处与代码不符的表述（stock 平局示例中 S1 是否被排除、边界验证记录的公开测试覆盖来源），以及本 README 中会话归档状态的表述。核心库逻辑未作改动。

**会话归档状态**：2026-11-26 晚间已在本机完成 Agent Session Commit（AgentLedger）v0.1.5 安装：仓库内 `.venv-agent-session-commit/` 可用，`.git/hooks/` 中已有 `pre-commit` 与 `post-commit` 钩子，`pre-commit` 归档步骤在提交时通过。但本次提交**没有生成 `.agent-sessions/bundles/`**：扫描器在本机 WorkBuddy 会话目录中未找到归属本仓库（工作目录为 `E:\aim-2627-cpp-oop-coursework`）的会话记录（状态输出为 `Project session files found: 0`）。题面 7.1 要求随提交保留会话归档，本项仍需补齐；待有可识别的会话记录后重新提交归档。

**理解声明**：提交人已通读全部代码与文档（含本次复核修正），能够现场解释各接口的校验顺序、状态转换与失败无副作用设计。
