# 双目相机随仿真底盘运动

## Goal

把已有双目传感器挂到 Gazebo TurtleBot3 Burger 底盘，在 `cmd_vel` 驱动的物理运动中验证视觉里程计、具名按钮和三维地图持续工作，为后续路径跟踪闭环建立可信仿真基线。只操作隔离仿真，不碰实机。

## Dirty-State Note

开始时 `main` 已推送 `ee1cff8`；用户已有未跟踪 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak` 不修改、不暂存。

## Owned Files

- `plan/2026-09-24-mobile-stereo-base/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/stereo_odometry_audit.launch.py`
- `stereo_sim/launch/semantic_visual_odometry.launch.py`
- `stereo_sim/launch/stereo_test_world.launch.py`
- `stereo_sim/launch/semantic_mobile_stereo.launch.py`
- `stereo_sim/scripts/generate_stereo_mobile_model.py`
- `stereo_sim/scripts/run_semantic_mobile_test.sh`
- `stereo_sim/scripts/verify_semantic_mobile.py`
- `stereo_sim/worlds/semantic_panel_mobile.world`

如需扩展，先更新此清单。

## Read-Only Files

- TurtleBot3 系统安装模型、已有静态双目模型和世界、用户场景与真实工控机

## Shared Dependencies

- ROS Humble 的 `turtlebot3_gazebo`、Gazebo 差速驱动插件
- 已验收的视觉里程计、语义、OctoMap 和路径建议节点

## Expected Work

1. 从系统 TurtleBot3 Burger 模型生成临时仿真模型，在 base_link 添加双目传感器，保留原有底盘物理/差速插件。
2. 在独立世界启动带双目底盘；短距离、低速 `cmd_vel` 前进与转向，用 Gazebo 真值仅做验证。
3. 证明机器人真实运动时 VO、按钮位置、地图和路径状态持续更新；若不能，记录具体原因，不把服务瞬移测试冒充底盘运动。

## Validation

- 生成 SDF 结构、隔离 Gazebo/ROS 实测、失效保护、旧启动回归、Python/Shell 静态检查、Git diff/status。

## Commit Intent

只提交本 target 文件；运行通过后同步私有 GitHub。

## Open Risks

- TurtleBot3 原模型相机安装高度与物理稳定性；轮速加速度限制可能使短程运动缓慢。
- 点云计算与 VO 在物理运动时可能进一步降低吞吐；不能用 Gazebo 真值补偿误差。

## Review Result

两次隔离仿真已验证低速车轮运动中 VO、具名按钮、OctoMap 和路径状态；具体数值见 `plan/log.md`。车辆尚未按规划路径闭环运行，这仍是下一 target，不将本次固定速度动作当作自主导航。
