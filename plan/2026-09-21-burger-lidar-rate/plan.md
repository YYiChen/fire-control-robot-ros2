+# 提高 Burger 仿真激光扫描频率

## Goal

在不降低用户当前遥控速度（0.75 m/s、1.05 rad/s）的前提下，为 lab_room 场景提供 15 Hz 激光扫描的 Burger 模型与启动入口。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

这两个未跟踪文件是用户现有场景资产，不编辑、暂存或删除。本 target 只在 WSL 用户目录创建独立模型和启动文件，并记录操作。

## Owned Files

- plan/2026-09-21-burger-lidar-rate/plan.md
- plan/log.md
- WSL: ~/my_models/turtlebot3_burger_15hz/model.sdf
- WSL: ~/my_worlds/lab_room_15hz.launch.py

## Read-Only Files

- gazebo_scene/lab_room.launch.py
- gazebo_scene/lab_room.world
- WSL: ~/my_worlds/lab_room.launch.py
- WSL: /opt/ros/humble/share/turtlebot3_gazebo/models/turtlebot3_burger/model.sdf
- 正在运行的 Gazebo、SLAM 和 RViz

## Shared Dependencies

- TurtleBot3 Gazebo Burger SDF and robot_state_publisher
- User lab_room.world and building_editor_models
- /scan, /odom, /cmd_vel topics and slam_toolbox

## Expected Work

1. Copy the system Burger SDF into a user-owned model file and set only its lidar sensor update_rate from 5 Hz to 15 Hz.
2. Create a separate launch file that spawns this SDF in the existing lab_room world while keeping the normal Burger state publisher.
3. Validate SDF syntax and static launch imports without stopping the active simulation.
4. Record the new startup and verification boundary in plan/log.md and commit project records.

## Validation

- gz sdf -k ~/my_models/turtlebot3_burger_15hz/model.sdf
- assert one lidar update_rate is 15 and the source system file is unchanged
- python3 -m py_compile ~/my_worlds/lab_room_15hz.launch.py
- git diff --check and git status --short --branch in this project

These checks prove model syntax and launch-file syntax. The 15 Hz runtime topic rate requires a fresh Gazebo launch and will be verified after the user switches to the new launcher.

## Experience Signal (for human review)

- The packaged Burger SDF uses a 5 Hz simulated lidar. Raising speed without changing this rate increases robot movement between scan frames.

## Commit Intent

```text
docs: add high-rate lidar launch record
```

