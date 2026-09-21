# 按倍率调整键盘遥控默认速度

## Goal

按用户的明确要求，将 `slow_teleop` 默认直线速度乘以 2.5、默认转向速度乘以 1.5，并保留自动停车行为。

## Dirty-State Note

开始时项目根目录存在另一 Agent 的未提交 `plan/log.md` 修改和两个 `gazebo_scene/lab_room.*` 未跟踪文件；它们不属于本 target，且不与本 target 的新 plan 文件或 WSL 遥控包源文件重叠。`plan/log.md` 中记录的转向 ×2 与用户本轮要求 ×1.5 不一致，保持只读以避免覆盖另一 Agent 的在途工作。

## Owned Files

- WSL `~/ros2_ws/src/slow_teleop/slow_teleop/keyboard.py`
- WSL `~/ros2_ws/src/slow_teleop/README.md`
- `plan/2026-09-21-teleop-speed-multipliers/plan.md`

## Read-Only Files

- 项目根目录的未提交 `plan/log.md`
- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- 当前运行的 Gazebo、SLAM、RViz 和遥控进程

## Shared Dependencies

- TurtleBot3 Burger 订阅 `/cmd_vel`；`slow_teleop` 以 `geometry_msgs/msg/Twist` 发布。
- 已运行的遥控节点不会读取源码更新；用户需要退出并重新启动该节点。

## Expected Work

1. 将默认直线速度 `0.15 → 0.375 m/s`，角速度 `0.35 → 0.525 rad/s`。
2. 将可接受的直线速度上限扩展到覆盖新的默认值，并同步 README。
3. 编译并隔离验证新默认参数；不向当前机器人的 `/cmd_vel` 发布移动命令。

## Validation

- `python3 -m py_compile` 和 `colcon build --packages-select slow_teleop --symlink-install`。
- 在隔离 ROS 域启动节点，检查显示与参数服务中的速度数值。
- WSL 包 `git diff --check` 与状态；项目根目录仅检查本 target 文件，不暂存他人的变更。

## Experience Signal (for human review)

同一速度倍率需求在并行未提交记录与最新用户消息中不一致；实现以最新用户明确指令为准。

## Commit Intent

WSL 包：`feat: apply requested teleop speed multipliers`

项目计划：待 `plan/log.md` 的现有修改归属清晰后再记录和提交。
