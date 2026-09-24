# 图优化支路的三维占据图与按钮对齐

## Goal

在现有隔离双目 RTAB-Map 图建图上验证同一图位姿生成的三维占据图，并量化按钮位置与三维占据体素的关系；检查持续相机运动、遮挡和地图更新。不得把二维图或 VO-only OctoMap 误称为图校正三维地图。

## Dirty-State Note

开始时本地 `main` HEAD `bf8c53b`，因 GitHub HTTPS 连接失败比 `origin/main` 领先一提交。未跟踪用户文件 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world`、`reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak` 保留且不暂存。本 target owned 文件无重叠。

## Owned Files

- `plan/2026-09-24-graph-corrected-3d-map/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/semantic_graph_slam.launch.py`
- `stereo_sim/scripts/run_graph_semantic_test.sh`
- `stereo_sim/scripts/verify_graph_semantic.py`

## Read-Only Files

- 已验收的原 VO/OctoMap/自动接近节点、图语义投影实现、用户自建场景与实机

## Shared Dependencies

- RTAB-Map 官方 `Grid/3D`、`Grid/RayTracing` 与 ROS2 `octomap_occupied_space`/`octomap_binary` 发布接口；实际 WSL Humble 二进制须核验是否带 OctoMap 支持。
- Graph map `graph_map` 坐标、按钮图投影与现有三场景验证。

## Expected Work

1. 核对运行中的 RTAB-Map 三维地图能力与官方参数，选择隔离支路的最小参数，不改旧 VO 地图。
2. 订阅图支路三维占据点/树，量化地图更新与按钮三维表面距离；对缺失或错误坐标系失败，而非只检查 topic 存在。
3. 复测基础、偏航和遮挡负例；记录性能与回环限制，更新文档并只提交本 target 文件。
4. 首轮已确认 `/octomap_occupied_space` 有消息，但验证器将 Humble `read_points()` 的结构化 NumPy 元素误当元组切片而退出；改用字段名 `x/y/z` 后重跑。
5. 基础/偏航场景已见非空图坐标占据点与 OctoMap，按钮到最近三维占据点约 0.7–1.2 cm；完全遮挡时三维地图输出为 0。记录图处理频率与关键位姿节奏，明确尚未验证回环后的整图修正。

## Validation

- WSL ROS2/Gazebo 隔离无界面实验；三维体素有非空消息、正确 `graph_map` 坐标、按钮表面附近体素；遮挡时不产生伪造地图或按钮。
- Python 编译、Shell 语法、`git diff --check`、`git status --short --branch` 和 staged diff 复审。

## Commit Intent

验证通过后提交 `feat: align stereo landmarks with graph 3d occupancy`；GitHub 网络恢复时推送。

## Open Risks

- 本机 RTAB-Map 可能缺 OctoMap 编译支持；不能凭发布话题存在认定三维占据已建立。
- 小场景回环此前不稳定；即使三维体素与按钮同坐标，也不能证明回环后全图精度或长程动态环境可靠性。
