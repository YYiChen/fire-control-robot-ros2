# 仿真面板的双目语义定位与三维场景关联

## Goal

沿既有双目仿真继续构建可重复的科研实验：从左目画面实时识别具名按钮，用同一时刻的视差与相机模型恢复三维位置，将目标变换到可复用的面板/世界坐标系，并与 Gazebo 真值比较。此 target 不连接实机。

## Dirty-State Note

开始时 `main...origin/main`，以下三项为已有未跟踪内容，归属不在本 target，不修改、不暂存：

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak`

## Owned Files

- `plan/2026-09-23-semantic-panel-simulation/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `.gitattributes`（仅固定本目录 Bash 脚本的 LF 换行）
- `stereo_sim/launch/semantic_panel.launch.py`
- `stereo_sim/launch/stereo_test_world.launch.py`（仅在本 target 需要场景参数时）
- `stereo_sim/scripts/semantic_panel_node.py`
- `stereo_sim/scripts/verify_semantic_panel.py`
- `stereo_sim/scripts/verify_semantic_panel_faults.py`
- `stereo_sim/scripts/run_semantic_panel_test.sh`
- `stereo_sim/scripts/generate_semantic_panel_world.py`
- `stereo_sim/scripts/generate_panel_marker.py`
- `stereo_sim/worlds/semantic_panel.world`
- `stereo_sim/models/semantic_panel/model.config`
- `stereo_sim/models/semantic_panel/model.sdf`

若实现中需要额外文件，先更新此清单。

## Read-Only Files

- 用户未跟踪的 `gazebo_scene/lab_room.*`
- `reference/industrial_pc_2026-09-22/` 源码与配置
- 系统 `/opt/ros/humble`、实机和工控机

## Shared Dependencies

- ROS 2 Humble、Gazebo Classic、`stereo_sim` 现有双目图像/视差链路
- `sensor_msgs`、`geometry_msgs`、`tf2_ros`、OpenCV
- 后续可复用 RTAB-Map、MoveIt2，但不以是否安装它们作为本 target 成功的替代证据

## Expected Work

1. 在 Gazebo 中建立带 ArUco 基准和多个有明确 ID 的按钮面板，记录各按钮真值坐标。
2. 增加逐帧识别与同步视差融合节点，输出图像标注、各按钮三维目标及 RViz 标记；无效深度或错误坐标变换必须可观察。
3. 建立相机、面板、世界坐标的 TF 关系，并测试不同观察位置或面板位置时的持续关联。
4. 验证时间戳一致性、目标身份、坐标系、定位误差及失效行为，不以存在话题代替验证。
5. 把复现实验方法、结果与未解决的科研风险写入 README / log。

## Validation

- Python、Bash、SDF/XML 静态检查与 `git diff --check`。
- 在隔离 ROS domain 和 Gazebo master 中运行仿真，核对按钮的图像坐标、三维坐标及场景真值；至少两个目标位置和两个观察配置。
- 检查无效视差、遮挡或失去基准时不会输出虚假的可执行按钮位姿。
- 检查进程退出后隔离域没有残留节点；只暂存 owned files。

## Commit Intent

按可验证阶段提交 `feat: add stereo semantic panel localization simulation`，并在 log 中记录实际提交及远端状态。

## Open Risks

- 当前点云测距偏差为厘米量级，尚未达到毫米级按钮操作要求。
- 现有场景的相机是静态的，尚无与机械臂基座相连的 TF 链。
- 仿真按钮的色彩/纹理识别仅是融合链路验证，真实面板识别仍需专门数据与评估。
