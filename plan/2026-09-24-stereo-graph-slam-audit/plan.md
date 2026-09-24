# 双目图优化建图与回环审计

## Goal

验证目前只有 VO 局部坐标的双目仿真能否接入成熟的 RTAB-Map 图优化 SLAM，记录地图/轨迹/回环证据以及与 VO-only OctoMap 的接口差异；仅隔离仿真，不改当前已通过的自动接近链或实机。

## Dirty-State Note

开始时 `main` 位于 `5465292`，比 `origin/main` 领先一提交（上轮 GitHub HTTPS 连接失败）；用户未跟踪的 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak` 不触碰、不暂存。本 target 目标文件无重叠。

## Owned Files

- `plan/2026-09-24-stereo-graph-slam-audit/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/semantic_graph_slam.launch.py`
- `stereo_sim/scripts/run_semantic_graph_slam_audit.sh`
- `stereo_sim/scripts/verify_semantic_graph_slam.py`
- `stereo_sim/scripts/vo_odom_tf.py`
- `stereo_sim/scripts/generate_graph_room.py`

如需扩展范围先更新清单。

## Read-Only Files

- 已验收的自动驾驶/VO/语义/OctoMap 源代码及用户场景
- 工控机与实机

## Shared Dependencies

- ROS2 Humble、Gazebo、已安装的 `rtabmap_odom`、`rtabmap_util`、双目仿真模型
- `rtabmap_slam` 尚未安装；先核查 apt 模拟结果与用户目录解包运行可行性，不执行系统升级，不要求实机

## Expected Work

1. 检查 RTAB-Map Humble 包和已有双目数据接口；若能在用户目录获得运行包，建立隔离图优化启动，不改变默认启动。
2. 用机器人闭合轨迹测试图节点、地图输出与回环/优化事件；明确检测到回环与真实轨迹误差改善是两种不同结论。
3. 核对语义地标如何在图优化后的 `map` 坐标中维护，避免把 VO 起点坐标直接当全局图坐标。
4. 首轮运行发现现有 VO 审计启动关闭了 TF 发布，图优化节点缺少 `vo_odom → stereo_base_link`；仅在隔离启动内增加与 VO 消息同时间戳的 TF 桥。
5. 加 TF 后往返 0.46m 只生成 3 个关键位姿（91 次图像处理），默认位移触发阈值对小场景过大；隔离实验将关键帧位移阈值设为 0.015m，再验证是否真正产生回环。
6. 第二轮发现错写为 `Rtabmap/LinearUpdate`，日志没有采用该参数；按 RTAB-Map 官方 `Parameters.h` 改为 `RGBD/LinearUpdate` 后重测。
7. 26 个关键位姿和 26 张占据图已建立；全局回环候选因局部闭环边误差比 3.02–3.68 超过默认 3 而被拒。隔离试验按 RTAB-Map 官方 ROS2 演示的 `OptimizeMaxError=4` 复测，同时比较图校正后的返航误差与仿真真值；若图误差变坏则不能接受该阈值。
8. 完整阈值 4 实验仍无被接受的全局回环，局部约束 18 条；返航 VO 闭合误差 0.0018m，图坐标闭合误差 0.0048m。恢复默认阈值 3，不继续放宽一致性门槛。
9. 默认阈值 3 的三轮完整小场景实验有 2/3 接受全局回环；图闭合误差分别约 0.0019、0.0047、0.0142m，并非稳定优于 VO。增加两个外观不同的静态视觉标志，维持同一轨迹/阈值，比较场景可辨识度对回环的影响。
10. 新视觉标志世界两轮均建立图与占据地图，但全局回环 0/2；分别有 16/18 条局部空间约束，说明标志未解决可靠回环。保留审计返回非零及数据库路径，记录为未通过全局回环门槛。语义节点现有 `map → stereo_base_link` 与新 `graph_map → vo_odom → stereo_base_link` 不能直接合并；重投影地标/点云须作为后续 target 验证。

## Validation

- 依赖/启动静态检查；隔离 ROS_DOMAIN_ID/Gazebo 测试；轨迹与图优化输出可量化；未实现的门槛保留证据；`git diff --check`、目标文件复审和状态审计。

## Commit Intent

只在运行和验证结果足以支持结论时提交本 target 文件；远端连接恢复后推送。

## Open Risks

- 需要的 `rtabmap_slam` ROS 包未安装，WSL 当前 `sudo -n` 不可用；若用户目录解包仍不能运行，记录明确依赖缺口，继续其他无需特权的工作。
- 小型无纹理场景可能无法形成可验证回环，不能把无回环误写为算法失败。
