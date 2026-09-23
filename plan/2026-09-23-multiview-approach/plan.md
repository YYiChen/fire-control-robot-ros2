# 多视角双目扫掠与安全接近路径验收

## Goal

在纯 Gazebo 仿真中，让相机从多个位置扫视面板及其前方，通过累积 OctoMap 扩大已知空闲区域，并验证语义按钮目标的 A* 接近路径何时可安全输出。

## Dirty-State Note

开始时 `main` 已推送到 `c67a6b4`；已有用户未跟踪的 `gazebo_scene/lab_room.*` 和 `reference/.../rec.bak` 不修改、不暂存。

## Owned Files

- `plan/2026-09-23-multiview-approach/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/run_multiview_approach_test.sh`
- `stereo_sim/scripts/verify_multiview_approach.py`

若实验发现现有地图/规划接口需要修复，先更新 owned files。

## Read-Only Files

- 现有双目、语义、OctoMap、规划实现与用户场景

## Shared Dependencies

- Gazebo `/set_entity_state`，多视角图像与点云，`/projected_map`，`/semantic_panel/approach_status` 和路径
- 隔离 ROS domain / Gazebo master，避免影响用户当前实例

## Expected Work

1. 逐步移动仿真相机并等待每个视角的双目数据进入地图。
2. 记录各视角的占据/空闲地图增长以及是否能够形成已知空闲通道。
3. 如果路径出现，验证目标距离、地图安全性与按键身份；若未出现，定量描述覆盖不足的原因，不绕过未知格约束。

## Validation

- 隔离 Gazebo 实测、路径/地图数值、静态检查、Git diff 检查及实验进程退出检查。

## Commit Intent

仅提交本 target 文件，保留研究中未通过的可见证据。

## Open Risks

- 相机扫掠借助 Gazebo 真值位姿，尚未形成车辆/机械臂真实运动链。
- 当前面板纹理较少、视差点云稀疏，可能不足以证明 7 cm 足迹走廊空闲。
