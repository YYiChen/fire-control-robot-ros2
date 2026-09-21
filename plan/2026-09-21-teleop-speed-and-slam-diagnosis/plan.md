+# 遥控速度翻倍与碰撞后建图诊断

## Goal

按用户要求，将 WSL 中 `slow_teleop` 的当前默认直行、转向速度各翻倍，并检查本次自建场景建图异常是否与机器人碰撞后位姿异常有关。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

这两个未跟踪场景文件来自用户当前自建场景，不属于本 target；本次不编辑、暂存或删除。

## Owned Files

- `plan/2026-09-21-teleop-speed-and-slam-diagnosis/plan.md`
- `plan/log.md`
- WSL：`~/ros2_ws/src/slow_teleop/slow_teleop/keyboard.py`
- WSL：`~/ros2_ws/src/slow_teleop/README.md`
- WSL：`~/ros2_ws/src/slow_teleop/config/slam_toolbox_fast.yaml`

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- 正在运行的 Gazebo 场景和 ROS 节点

## Shared Dependencies

- TurtleBot3 Burger 的 `/cmd_vel`、`/odom`、`/scan`
- 正在运行的 `slam_toolbox` 和 Gazebo 仿真
- `slow_teleop` 独立 Git 仓库

## Expected Work

1. 读取 SLAM、Gazebo 和遥控节点的运行日志，确认异常证据与边界。
2. 将线速度 `0.375 → 0.75 m/s`、角速度 `0.525 → 1.05 rad/s`，同步参数合法范围与使用说明。
3. 将 SLAM 的激光量程限制改为 Burger 实测发布值 `0.12–3.5 m`，消除不匹配警告。
4. 编译并在隔离 ROS 域验证节点的默认参数；不干扰用户当前仿真。
5. 记录诊断结果、验证和提交状态；分别提交 WSL 包和项目记录。

## Validation

- WSL：`python3 -m py_compile slow_teleop/keyboard.py`
- WSL：`colcon build --packages-select slow_teleop --symlink-install`
- WSL：隔离 ROS 域启动节点并查询 `linear_speed`、`angular_speed` 参数。
- 两个仓库均执行 `git diff --check`、`git status --short --branch`。
- 读取实时 `/odom` 与 SLAM 日志，作为碰撞诊断证据。

这些检查覆盖默认参数、ROS 包可构建性及本次碰撞诊断的直接数据面；不对用户正在运行的场景执行重置或自动驾驶。

## Experience Signal (for human review)

- 高速遥控与狭窄自建场景同时使用时，碰撞造成的里程计突变会干扰建图；本次先以实际 SLAM/Gazebo 日志确认。

## Commit Intent

```text
feat: double requested teleop speeds
```
