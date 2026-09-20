# 建图速度与刷新调优

## Goal

保存用户当前完成的地图，将练习遥控的直线速度提高约 3 倍，并提供下一次建图更快刷新的 SLAM 配置。

## Dirty-State Note

开始时 Windows 项目与 WSL `slow_teleop` 包均为干净的 `main`。

## Owned Files

- WSL `~/ros2_ws/src/slow_teleop/slow_teleop/keyboard.py`
- WSL `~/ros2_ws/src/slow_teleop/README.md`
- WSL `~/ros2_ws/src/slow_teleop/config/slam_toolbox_fast.yaml`（新建）
- `plan/2026-09-20-mapping-speed-refresh/plan.md`
- `plan/log.md`

## Read-Only Files

- `/opt/ros/humble/share/slam_toolbox/config/mapper_params_online_async.yaml`
- 本 target 新保存的地图 `~/maps/turtlebot3_world_2026-09-20_01.{yaml,pgm,posegraph,data}`（保存后只读）
- 当前 Gazebo、SLAM、RViz 与遥控进程

## Shared Dependencies

- ROS2 Humble、TurtleBot3 Burger 与 SLAM Toolbox `online_async_launch.py`。
- 运行中节点不会因源码默认值修改自动提速；用户须退出遥控后重新启动。

## Expected Work

1. 保存并核验当前地图的导航用图像和 SLAM 位姿图。
2. 将默认直线速度从 0.05 调到 0.15 m/s，转向维持温和速度；保留自动停车，并让节点处理参数查询。
3. 基于系统自带在线异步 YAML 创建较快刷新的配置并在 README 给出下次启动命令。
4. 验证和记录；不擅自重启用户当前仿真或 SLAM。

## Validation

- 地图保存输出、PGM/YAML 实际存在及元数据一致；位姿图服务返回成功且文件存在。
- `python3 -m py_compile` 与 `colcon build --packages-select slow_teleop --symlink-install`。
- YAML 可被 ROS2 参数解析；检查四个关键参数。
- 两个仓库 `git diff --check` 与状态。

## Experience Signal (for human review)

本次实测默认 `map_update_interval=5.0` 秒、`minimum_travel_distance=0.5` 米；用户感知的延迟主要对应默认设置。

## Commit Intent

WSL 包：`feat: tune teleop speed and mapping refresh`。

本项目文档：`docs: record mapping speed tuning and saved map`。
