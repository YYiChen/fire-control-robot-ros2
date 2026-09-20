# 低速键盘遥控包

## Goal

为 TurtleBot3 Burger 仿真提供低速、按键超时自动停车的 ROS2 键盘遥控包，便于初学者建图。

## Dirty-State Note

开始时 `git status --short --branch` 为 `## main`，工作区干净。

## Owned Files

- WSL `~/ros2_ws/src/slow_teleop/`（新建包，包括独立 Git 记录）
- `plan/2026-09-20-slow-teleop/plan.md`
- `plan/log.md`

## Read-Only Files

- `/opt/ros/humble/` 已安装 ROS 软件包
- 现有 Gazebo、SLAM、RViz 进程和仿真世界

## Shared Dependencies

- ROS2 Humble；`rclpy`、`geometry_msgs`；Gazebo TurtleBot3 订阅 `/cmd_vel`（`Twist`）。
- 遥控节点只在用户手动启动并聚焦其终端时接收按键。

## Expected Work

1. 在 WSL 工作区新建 Python ROS2 包与简短使用说明。
2. 实现低速 WASD 控制、按键超时停车、空格急停、退出时零速度。
3. 编译并检查 ROS2 可发现该包；不自动遥控机器人。
4. 在 WSL 包目录单独提交源码，并在本项目记录安装与验证结果。

## Validation

- `python3 -m py_compile` 检查节点语法。
- `colcon build --packages-select slow_teleop --symlink-install` 和 `ros2 pkg executables slow_teleop`。
- `git diff --check`、`git status --short --branch`。
- 不通过真实按键驱动车辆；按键控制留给用户亲自试用。

## Experience Signal (for human review)

通用 `teleop_twist_keyboard` 按一次移动键后持续保持速度，对初学者的仿真建图操作不方便。

## Commit Intent

WSL 包：`feat: add timed low-speed keyboard teleop`

本项目文档：`docs: record slow teleop package setup`
