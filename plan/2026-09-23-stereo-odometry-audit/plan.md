# 双目视觉里程计与 Gazebo 真值对照

## Goal

评估成熟的 ROS2 RTAB-Map 双目视觉里程计是否能从现有 Gazebo 双目画面恢复相机运动，定量与 Gazebo 真值对照。此阶段不替换正在工作的语义/OctoMap 链路，先得出可复验的可用性结论。

## Dirty-State Note

开始时 `main` 已推送 `efa7720`；用户已有未跟踪场景 `gazebo_scene/lab_room.*` 与参考源码的 `rec.bak` 不修改、不暂存。

## Owned Files

- `plan/2026-09-23-stereo-odometry-audit/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/stereo_odometry_audit.launch.py`
- `stereo_sim/scripts/run_stereo_odometry_audit.sh`
- `stereo_sim/scripts/verify_stereo_odometry_audit.py`
- `stereo_sim/scripts/normalize_stereo_camera_info.py`

若需其他文件，先更新此清单。

## Read-Only Files

- 当前工作的语义/OctoMap/路径节点、用户场景、实机和工控机

## Shared Dependencies

- 官方 `rtabmap_ros` 双目视觉里程计（本 target 已在 WSL Humble 安装 `ros-humble-rtabmap-odom`），已存在的双目校正图和相机参数
- Gazebo `/model_states` 只作测试真值，不给视觉里程计提供位姿
- 安装只允许 apt install，不运行 apt upgrade，不升级现有 ROS 包

## Expected Work

1. 预检/安装所需 RTAB-Map ROS2 组件，不安装整个不必要的 metapackage。
2. 在隔离仿真里以双目校正图运行视觉里程计，记录 `/odom` 更新率、轨迹和失效日志。
   Gazebo 实测左右原始 `CameraInfo.P[3]` 均约为 -30，RTAB-Map 判断基线 0；仅在审计支路将左目 `P[3]` 归零，右目保留 `-fx*0.06`，不改原始话题。
3. 连续沿前方、横向移动并小角度转向，比较视觉里程计的位置向量及偏航角与 Gazebo 真值，说明成功或失败边界；不以位移长度相近掩盖方向误差。
4. 把结果写入文档，决定是否具备替换真值 TF 的条件。

## Validation

- ROS2 话题消息、时间戳、位姿增量数值和日志；静态检查、Git diff、退出后隔离进程检查。

## Commit Intent

单独提交审计脚本和事实记录；若失败也保留失败条件，不把未验证方案宣布为已实现。

## Open Risks

- 当前仿真面板可跟踪纹理少；离散相机位姿跳变不等于真实平滑运动。
- 未有真实相机内外参和硬件同步，本测试只评价模拟条件。
- 两次短程受控运动通过，但中途单帧横向位姿仍有约厘米级波动；结果不足以证明长程 SLAM、回环和机械臂精度。
