# 视觉里程计驱动的按钮与三维地图链路

## Goal

在纯 Gazebo 仿真中让语义按钮定位、OctoMap 累积与接近路径共用 RTAB-Map 双目视觉里程计位姿；Gazebo 真值只供验证器比较，运行节点不读取。验证运动后按钮 map 坐标及地图/路径一致性。

## Dirty-State Note

开始时 `main` 已推送 `8914fde`；用户已有未跟踪 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak` 不修改、不暂存。

## Owned Files

- `plan/2026-09-24-visual-odom-semantic-map/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/semantic_panel_node.py`
- `stereo_sim/scripts/semantic_approach_planner.py`
- `stereo_sim/scripts/vo_synchronized_cloud.py`
- `stereo_sim/launch/semantic_visual_odometry.launch.py`
- `stereo_sim/scripts/run_semantic_visual_odometry_test.sh`
- `stereo_sim/scripts/verify_semantic_visual_odometry.py`

如需扩展，先更新此清单。

## Read-Only Files

- 已验收的 Gazebo 真值语义场景、用户场景；不改真实机器人/工控机

## Shared Dependencies

- 已安装的 `ros-humble-rtabmap-odom`、`ros-humble-octomap-server`
- 相机参数修正器、基座到光学相机 TF、双目视差与点云链
- `nav_msgs/Odometry` 与图像/视差共同时间戳

## Expected Work

1. 让语义节点和路径规划器可选视觉里程计位姿源，并严格按时间戳匹配图像与位姿；无有效位姿不得输出目标。
2. 新增隔离启动，RTAB-Map VO → 语义按钮 map 坐标、相机 TF → OctoMap → 路径建议。
3. 平滑移动相机，多视角观察，核验按钮身份/位置、地图积累、视觉里程计误差与路径状态。
   若 30 Hz 点云先于较慢 VO/TF 到达造成 OctoMap 队列丢帧，加入按同帧 VO 时间戳选点云的限速桥接；不在缺少匹配位姿时推测或伪造 TF。
4. 文档和记录标明结果、适用范围及剩余真实科研风险。

## Validation

- 隔离 ROS/Gazebo 实测、静态检查、针对旧真值模式的回归验证、失效保护、Git diff 和进程清理。

## Commit Intent

只提交本 target 文件，实际验证通过再同步私有 GitHub。

## Open Risks

- VO 短程审计通过但中途位姿有厘米级波动，误差可能累积并影响按钮融合阈值。
- 相机与面板仅理想彩色/二维码，未覆盖真实按钮识别和机械臂动作。
- 现有 CameraInfo 修正仅在 VO 支路；双目点云链仍使用原始 Gazebo CameraInfo，其距离性能须在集成后核对。

## Review Result

三次隔离仿真复测通过语义位置、VO 误差、占据图增量与安全接近路径检查；详见 `plan/log.md`。原始点云 30 Hz 快于可用 VO/TF 时的间歇停更已通过同帧限速桥接处理。该结论只覆盖短程理想面板场景，不代表科研总目标完成。
