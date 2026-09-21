+# 30 Hz 雷达、平面约束与后期建图异常诊断

## Goal

保存用户当前 lab_room 地图；提供 30 Hz 仿真激光模型和抑制机器人翻倒的平面约束；基于实际运行参数排查建图后半段出现地图乱码的原因，并给出可验证的高速度建图配置。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

这两个未跟踪文件属于用户场景，不编辑、暂存或删除。

## Owned Files

- plan/2026-09-21-high-rate-planar-slam/plan.md
- plan/log.md
- WSL: ~/ros2_ws/src/planar_lock_plugin/*
- WSL: ~/my_models/turtlebot3_burger_30hz/model.sdf
- WSL: ~/my_worlds/lab_room_30hz.launch.py
- WSL: ~/ros2_ws/src/slow_teleop/config/slam_toolbox_high_rate.yaml

## Read-Only Files

- gazebo_scene/lab_room.launch.py
- gazebo_scene/lab_room.world
- WSL: ~/my_worlds/lab_room.world
- WSL: ~/my_worlds/lab_room_15hz.launch.py
- WSL: /opt/ros/humble/share/turtlebot3_gazebo/models/turtlebot3_burger/model.sdf
- WSL: ~/ros2_ws/src/slow_teleop/config/slam_toolbox_fast.yaml
- Current running Gazebo, RViz, SLAM, and teleop processes

## Shared Dependencies

- TurtleBot3 Burger Gazebo SDF, Gazebo Classic C++ plugin API, and robot_state_publisher
- /scan, /odom, /clock, /map, /tf and slam_toolbox
- User lab_room world and Building Editor model path

## Expected Work

1. Save current /map to an independent map YAML/PGM pair and verify both files.
2. Measure active scan, odometry, simulation, and SLAM parameters; inspect the model sensor and world physics configuration.
3. Create a Gazebo ModelPlugin that constrains the robot to its spawn height and yaw while retaining horizontal movement and collision response.
4. Create a 30 Hz Burger model, disable only lidar-ray visualization, and load the planar-lock plugin through a separate launch file.
5. Create a high-rate SLAM profile that consumes 30 Hz scans and disables loop closure for the diagnostic run.
6. Build and validate static syntax. Record results and commit only target project records and the WSL plugin/config repository.

## Validation

- ls -l ~/maps/lab_room_obstacles_2026-09-21.yaml ~/maps/lab_room_obstacles_2026-09-21.pgm
- colcon build --packages-select planar_lock_plugin
- gz sdf -k ~/my_models/turtlebot3_burger_30hz/model.sdf
- python3 -m py_compile ~/my_worlds/lab_room_30hz.launch.py
- assert exactly one lidar rate is 30 and high-rate SLAM profile uses a compatible scan interval
- git diff --check and git status --short --branch in each Git repository

Static checks prove artifacts can load and compile. The final 30 Hz /scan rate, real-time factor, collision behavior, and map stability require the user to switch the running Gazebo session to the new launcher.

## Experience Signal (for human review)

- A high sensor publish rate is ineffective if SLAM minimum_time_interval still throttles processing. Late map distortion may originate from loop closure/global optimization instead of an immediate sensor failure.

## Commit Intent

```text
feat: add high-rate planar mapping support
```

