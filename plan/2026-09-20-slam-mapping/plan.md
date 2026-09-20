# 完成 SLAM 建图（TurtleBot3 + Gazebo）

## Goal

在 WSL2(ROS2 Humble) + Gazebo 里，用 TurtleBot3 跑通 **SLAM 建图**：小车成功加载 → 建图 → 保存地图文件。（导航 Nav2 本 target 不做。）

## Dirty-State Note

`N/A (no git)` —— 本目录尚未纳入 git。若启用后需补 `git status --short --branch`。

## Owned Files

- `~/my_map.yaml`、`~/my_map.pgm`（建图产物）
- `~/.bashrc` 中新增的环境变量行（`export TURTLEBOT3_MODEL=burger`）
- 本 plan 目录、`plan/log.md`（记录）

## Read-Only Files

- 论文原件与 `论文_md版/`、`论文解读与复现路线.md`、`ROS2技术栈_学习路线.md`
- `AGENT.md`、`AGENTS.md`、`我要做的操作.md`

## Shared Dependencies

- ROS2 包：`turtlebot3_gazebo`、`turtlebot3_cartographer`、`turtlebot3_teleop`、`nav2_map_server`
- 环境变量契约：`TURTLEBOT3_MODEL=burger`（**缺则机器人不 spawn**）

## Expected Work

1. 确认/设置 `TURTLEBOT3_MODEL=burger`（写入 `~/.bashrc` 并 `source`）。
2. 启动世界：`ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py`，确认小车出现。
3. 启动建图：`ros2 launch turtlebot3_cartographer cartographer.launch.py`。
4. 遥控扫图：`ros2 run turtlebot3_teleop teleop_keyboard`，走遍环境。
5. 保存地图：`ros2 run nav2_map_server map_saver_cli -f ~/my_map`。

## Validation

- `echo $TURTLEBOT3_MODEL` → `burger`
- `ros2 topic list | grep -E 'scan|odom'` → 有 `/scan`、`/odom`
- `ls -l ~/my_map.yaml ~/my_map.pgm` → 两文件存在
- `bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/diag_gazebo.sh` → 无 spawn 异常

**为何够用**：本 target 是"仿真里跑通建图"，产物的存在性（地图文件）与话题的存在性（scan/odom）就是最直接的确定性证据；不需要更重的测试。（不新增测试代码、不跑全量。）

## Experience Signal (for human review)

- **候选信号**：`TURTLEBOT3_MODEL` 为空时，`turtlebot3_gazebo` 会**加载世界但不 spawn 机器人**（进程里只有 `gzserver`，无 `/scan`/`/odom`），表象是"Gazebo 里没有小车"。排查工具 `diag_gazebo.sh` 一下就定位。
- 是否提取为 `docs/experience/` 经验：**由人决定**。

## Commit Intent

```text
plan: add SLAM mapping target (TurtleBot3 + Gazebo)
```
（当前无 git → `N/A (no git)`）
