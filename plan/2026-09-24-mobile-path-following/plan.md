# 仿真底盘跟踪按钮接近路径

## Goal

在独立 Gazebo 场景中让装载双目相机的 TurtleBot3 根据视觉里程计与已知空闲地图中的 A* 接近路径，自动移动到 `reset` 按钮前约 0.25 m，停稳；任何路径、地图、位姿、规划状态过期或失效时必须发送零速度。不操作实机。

## Dirty-State Note

开始时 `main` 本地提交 `662f31e`；GitHub 443 暂不可达，分支领先远端 1 个提交。用户已有未跟踪 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak` 不修改、不暂存。

## Owned Files

- `plan/2026-09-24-mobile-path-following/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/semantic_path_follower.py`
- `stereo_sim/scripts/semantic_panel_node.py`
- `stereo_sim/launch/semantic_visual_odometry.launch.py`
- `stereo_sim/launch/semantic_mobile_stereo.launch.py`
- `stereo_sim/launch/semantic_autonomous_mobile.launch.py`
- `stereo_sim/scripts/run_semantic_autonomous_test.sh`
- `stereo_sim/scripts/verify_semantic_autonomous.py`
- `stereo_sim/scripts/verify_semantic_follower_faults.py`

如需扩展，先更新此清单。

## Read-Only Files

- 已验收的底盘/传感器模型、VO、地图和 A* 规划；用户场景与真实设备

## Shared Dependencies

- `/semantic_panel/approach_path`、`/semantic_panel/approach_status`、`/projected_map`、`/vo/odom`
- Gazebo 差速车的 `/cmd_vel`（独立 `ROS_DOMAIN_ID`）

## Expected Work

1. 新增带状态门控的低速路径跟踪器，只在安全输入新鲜且路径已知空闲时输出速度，抵达接近位后停住。
   起始相机盲区若使规划器报 `start_or_goal_not_free`，只在激光前方净空和全部感知输入有效时进行最多 0.12 m 的受限低速主动扫图，其他情况保持零速。
2. 隔离启动底盘、视觉/地图/规划链和跟踪器；验证器仅观察真值和命令，不手动驾驶。
3. 校验抵达误差、停车、机器人姿态、地图/路径连续性以及输入失效时的停车。
   移动车体后发现现有 ArUco 朝向近似会随相机转向而漂移；先使用标记 PnP 姿态并与仿真面板法向对照，避免错误接近位。

## Validation

- 隔离 ROS/Gazebo 自动移动实测、失效保护、Python/Shell 检查、Git diff/status；不默认跑无关全量测试。

## Commit Intent

只提交本 target 文件；GitHub 可用时再同步未推送提交。

## Open Risks

- 当前路径在局部 VO 坐标系，未建立回环校正；定位跳变或地图过期必须停车。
- 原规划终点由 2D 栅格确定，不包含机械臂可达性；到位不等于可按压。

## Review Result

两次隔离实验自动到位并停稳；最新重规划目标仍有数厘米漂移，具体证据和边界记在 `plan/log.md`。这个短程仿真 target 已验证，不代表整体科研目标完成。
