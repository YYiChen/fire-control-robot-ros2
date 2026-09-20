# 维护日志（plan/log.md）

> 项目级、被接受的维护历史。事实记录，不替代 git。
> 字段：date / target / changed areas / validation performed / commit status。
> 本项目当前**未启用 git** → commit status 记 `N/A (no git)`。

---

## 2026-09-18 ~ 09-19 —— 搭建 ROS2 开发环境

- **target**：在 Windows 上搭建 WSL2 + Ubuntu 22.04 + ROS2 Humble 环境（直连安装）。
- **changed areas**：
  - 安装 WSL2 + Ubuntu-22.04 发行版；OOBE 创建用户 `yyyyyc001`。
  - 安装 ROS2 Humble（`/opt/ros/humble`）、colcon、rosdep。
  - 配置 VS Code Remote-WSL、环境自动 source、`.vscode` 配置。
  - 生成脚本：`install_wsl2_direct.ps1`、`install_ros2_humble.sh`、`check_ros2_env.sh`、`setup_ros2_dev_env.sh`、`Open-ROS2-VSCode.bat` 等。
  - 产出文档：`ROS2开发环境_完整记录与现状.md`（+ `scripts/` 归档）。
- **validation performed**：`check_ros2_env.sh` 体检 24/24 通过；`ros2 doctor` 正常；`echo $DISPLAY`=`:0`（WSLg）；`xeyes` 出窗；talker/listener。。。以 `ros2` CLI 与 `ros2 run` 验证。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— 论文获取与整理

- **target**：获取并整理学长论文，作为复现/学习依据。
- **changed areas**：
  - 通读论文 docx（139,843 字符，6 章）。
  - 生成 `论文_md版/`（`00_总览与索引.md` + `01`~`06` 分章 md）。
  - 生成 `论文解读与复现路线.md`（技术栈 + 复现路线）。
- **validation performed**：md 文件生成并抽查内容；确认公式(OMML)/图片未提取（已知局限）。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— 学习路线、依赖安装、文档体系

- **target**：制定"学会整套技术栈"的路线并安装依赖；建立项目文档体系。
- **changed areas**：
  - 生成 `ROS2技术栈_学习路线.md`（A~H 分阶段）、`install_learning_stack.sh`（依赖安装）。
  - 安装 Gazebo、turtlebot3、nav2、slam-toolbox、moveit 等（部分包可能未装全）。
  - 生成 `AGENT.md`（项目上下文）、`我要做的操作.md`（操作清单）、本 kernel 结构（`AGENTS.md` / `plan/` / `docs/experience/`）。
- **validation performed**：`diag_gazebo.sh` 自检；`ros2 topic list`；Gazebo 进程检查。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— M1 完成（小车正常显示）+ 启用 git

- **target**：完成 SLAM 建图（`plan/2026-09-20-slam-mapping/`）。
- **changed areas**：
  - 修复 `TURTLEBOT3_MODEL`（写入 `~/.bashrc`）→ TurtleBot3 在 Gazebo **正常显示**。
  - **启用 git**：`git init` + 首次提交（纳入文档 / 论文 / 计划 / 经验模板）。
- **validation performed**：
  - `diag_gazebo.sh` 初检 → `TURTLEBOT3_MODEL=''`；无 `/scan` `/odom`；仅 `gzserver`、无 spawn 进程。
  - 修复后：**用户确认小车正常显示**（M1 ✅）。
  - **M2（建图）命令尚未运行**。
- **commit status**：首次提交（`chore: init repo ...`）
- **待办**：跑建图 4 条命令（`turtlebot3_world` → `cartographer` → `teleop` → `map_saver`），产出并验证 `~/my_map.yaml` + `~/my_map.pgm`。

## 2026-09-20 —— 多 Agent 协同发现 + git 启用（复核）

- **target**：启用 git；复核已有的多 Agent 产物。
- **changed areas**：
  - **启用 git**：`git init`（`main`），首次提交 `1a7b505`（32 文件，含文档 / 论文 / plan / experience）。
  - 发现本目录**已有另一 Agent 的产物**：`docs/project-data-index.md`、`plan/2026-09-20-project-data-index/`（**非本 Agent 创建**；随之入库，但其 dirty 变化归该 Agent，本 Agent 不编辑）。
- **validation / 核查差异**（来源：`docs/project-data-index.md`）：
  - `~/ros2_ws` **只有 `.vscode`、无 `src`** → 旧记录"已创建第一个包"**不成立**（`create_first_ros2_pkg.sh` 未真正跑过）。
  - **另一 Agent 可直连运行 WSL 命令**（`wsl --list --verbose`、`ros2 pkg list`、`ls -la ~/ros2_ws`），而**本 Agent 沙箱拦截 `wsl.exe`** → 能力因会话而异（已在 `AGENT.md` / `AGENTS.md` 加限定）。
- **commit status**：`1a7b505`（首次）；另有 `plan/2026-09-20-project-data-index/` 的 dirty 归另一 Agent。
- **备注**：另一 Agent 索引称"本目录不是 Git 仓库"——**已被本次 `git init` 取代（过时）**。
- **分工确认（用户澄清）**：这是**用户有意为之**的多 Agent 协同——**该 Agent 能直连 WSL（负责执行 / 核查）**，**本 Agent 被沙箱拦（负责文档 / 设计）**。已写入 `AGENT.md` / `AGENTS.md` 作为项目约定。
