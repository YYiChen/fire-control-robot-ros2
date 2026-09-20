# 消防控制室场景修复

## Goal

修复现有 Gazebo 场景中被设备挤窄的门洞、设备与墙重叠和缺失的外墙入口，并让启动与建图说明沿用当前已跑通的 ROS2 工作流。

## Dirty-State Note

开始时 `git status --short --branch` 为 `## main` 与 `?? gazebo_scene/`。三份未跟踪场景文件由另一 Agent 创建；用户本轮明确授权我修复它们。文件时间戳与上轮检查一致，未发现并发更新。

## Owned Files

- `gazebo_scene/fire_control_room.world`
- `gazebo_scene/fire_control_room.launch.py`
- `gazebo_scene/README.md`
- `plan/2026-09-20-fire-control-room-repair/plan.md`
- `plan/log.md`
- WSL `~/my_worlds/fire_control_room.world` 与 `~/my_worlds/fire_control_room.launch.py`（部署副本）

## Read-Only Files

- 当前运行的 Gazebo/SLAM/RViz/遥控进程及已保存地图
- `/opt/ros/humble/` 系统安装文件
- `~/ros2_ws/src/slow_teleop/` 练习包

## Shared Dependencies

- TurtleBot3 Burger、gazebo_ros、slam_toolbox 与先前调优的 `slow_teleop`。
- 新场景必须在停止旧 Gazebo 之后启动；当前运行会话不在本 target 中断。

## Expected Work

1. 改场景几何：保留近似平面布局，放宽通路、移开重叠设备、增加前侧入口。
2. 改 launch：从 launch 所在目录读取 world，并增加生成机器人时的等待时间。
3. 改 README：提供与现有 SLAM Toolbox、RViz、低速遥控和地图保存一致的操作顺序。
4. 部署到 WSL 并做 SDF、launch、几何与必要的仿真验证。

## Validation

- `gz sdf -k` 检查 SDF；Python/ROS2 launch 静态检查。
- 解析几何并检查门洞净宽、设备与墙非重叠，以及各房间对 Burger 的可通达性。
- 在不影响当前 ROS2 会话的隔离环境中验证场景可载入与机器人可生成（若资源允许）。
- `git diff --check`、`git status --short --branch`，只提交本 target 文件。

## Experience Signal (for human review)

初版 scene 通过语法检查，但 `equip_a` 实际挤占通往左上房间的门前空间；SDF 合法并不等于机器人能通行。

## Commit Intent

`fix: make fire control room scene navigable`
