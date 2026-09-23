# 双目语义建图与自动接近的多场景稳健性审计

## Goal

在隔离 Gazebo 中改变面板位置与偏航，并加入视觉遮挡/干扰，测量双目按钮定位、VO、地图和自动接近的成功条件与失效保护；不把单一理想正视实验当作科研结论。仅仿真，不连接实机。

## Dirty-State Note

开始时 `main` 与 `origin/main` 同步在 `c7e4cad`；用户已有未跟踪 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak`，不修改、不暂存。

## Owned Files

- `plan/2026-09-24-stereo-robustness-matrix/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/semantic_mobile_stereo.launch.py`
- `stereo_sim/launch/semantic_autonomous_mobile.launch.py`
- `stereo_sim/scripts/run_semantic_robustness_matrix.sh`
- `stereo_sim/scripts/verify_semantic_robustness.py`
- `stereo_sim/scripts/semantic_panel_node.py`
- `stereo_sim/scripts/semantic_path_follower.py`
- `stereo_sim/scripts/generate_semantic_panel_world.py`
- `stereo_sim/scripts/verify_semantic_occlusion.py`

如需扩展范围，先更新此清单。

## Read-Only Files

- 已验收的 VO/地图源代码、系统 TurtleBot3 模型、用户场景与真实设备

## Shared Dependencies

- `generate_semantic_panel_world.py` 生成面板偏移/偏航场景
- 既有 `semantic_autonomous_mobile.launch.py`、具名按钮和路径/控制状态话题

## Expected Work

1. 给移动/自动启动提供可覆盖的 world 参数，默认场景保持不变。
2. 建立小规模场景矩阵，先测面板偏移和偏航；对每次运行记录真值、视觉估计误差、地图更新时间、规划及停车结果。
3. 对遮挡或彩色干扰确认系统是否停止并说明原因；若发现明确实现缺陷，先更新 owned files 再修。
4. 首轮偏移场景在确认安全目标 0.031m 处停车，偏航场景小标记 PnP 朝向漂移至 0.092rad；评估适合 0.025m 栅格的终点误差界，并对静态标记的地图朝向做时间一致性约束，再重跑隔离矩阵。
5. 次轮偏航场景的法向误差降至 0.035rad，但静态按钮末段坐标误差升至 0.024–0.031m，重规划终点产生跳动；对同一面板的按钮坐标做有界多帧融合，并核验最新目标与真实车位。

## Validation

- 场景生成/解析、隔离 ROS/Gazebo 运行、可复现指标、旧默认场景回归、Python/Shell 静态和 Git diff/status。

## Commit Intent

只提交本 target 文件，测试结果入 `plan/log.md`；通过审核后同步 GitHub。

## Open Risks

- 控制器可能因未知地图或 VO 漂移安全停车；此类失败保留证据，不放松安全阈值换通过。
- 理想配色与 ArUco 仍远离真实工业面板；本矩阵只扩展仿真覆盖范围。
- 一次偏航试跑在停车后验证器未收到 `/projected_map`；后续单组及完整矩阵未复现，仍需长时间录包检查地图/VO 桥接与 ROS QoS。

## Outcome

- 完整矩阵 3/3 通过；默认自动场景与独立失效注入回归通过。
- 面板朝向和具名按钮位置的融合仅适用于单个静态 ArUco 面板；非静态目标、标签切换和长期回环仍未解决。
