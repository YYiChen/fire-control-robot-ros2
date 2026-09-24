# 双目视觉按钮与图地图坐标对齐

## Goal

将当前由双目图像识别出的面板/按钮三维位置接入隔离的 RTAB-Map 图坐标系；验证相机运动及图地图更新时，同一按钮的地图位置、仿真真值与占据图能对齐，并确保缺失输入不发布伪造的新位置。此 target 不声称完成真实面板状态识别或可靠全局回环。

## Dirty-State Note

开始时 `main` 与 `origin/main` 一致，HEAD `c9dad91`。未跟踪的 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world` 和 `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak` 属于用户/其他工作，不触碰、不暂存；本 target owned 文件无重叠。

## Owned Files

- `plan/2026-09-24-graph-semantic-landmarks/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/semantic_panel_node.py`
- `stereo_sim/scripts/vo_odom_tf.py`
- `stereo_sim/scripts/graph_semantic_projector.py`
- `stereo_sim/launch/semantic_graph_slam.launch.py`
- `stereo_sim/scripts/run_graph_semantic_test.sh`
- `stereo_sim/scripts/verify_graph_semantic.py`

## Read-Only Files

- 已验收的 VO/OctoMap/自动接近脚本与用户自建场景
- 工控机与实机

## Shared Dependencies

- RTAB-Map `/mapGraph` 中的 `graph_map → vo_odom` 修正、现有 `/vo/odom` 和 `vo_odom → stereo_base_link` TF
- 现有语义节点输出三个按钮与 ArUco 位姿；Gazebo 真值只用于验证
- WSL 用户目录的 ROS2 Humble 包覆盖层和隔离 `ROS_DOMAIN_ID`

## Expected Work

1. 给语义节点增加可配置的输出坐标系和 TF 发布开关，默认保持旧行为；图实验改为输出 `vo_odom` 中的检测，避免 TF 双父节点。
2. 新增投影节点，将具有同一 ID 的按钮观测保存在 VO 坐标中，用图最新修正投影到 `graph_map`；拒绝错误坐标系和过期输入，图修正时重投影已确认地标。
3. 在隔离图启动和真值审计中验证按钮坐标随相机运动稳定、图/占据地图与语义位置一致；记录尚未覆盖的全局回环与真实图优化限制。
4. 首轮启动发现新节点成员名 `publishers` 与 rclpy Node 只读属性冲突，导致节点退出且缺失话题；改为专用名称后重跑完整隔离实验。
5. 第二轮在启动后被投影数学断言拦截，发现四元数乘法的 y 分量索引错误；修正后先跑纯旋转/时间戳检查，再跑端到端。
6. 基础场景通过后，将同一图语义实验扩展到平移并偏航的面板及完全遮挡的负例；在运动末段用新拍摄时间戳的数据比对真值，避免重复发布的旧地标掩盖失败。
7. 加严末段时间戳后发现静止时图不产生关键帧，按 `/mapGraph` 时间戳判活会误停投影。接入 `/info` 处理心跳判活，图修正仍取最近有效 `/mapGraph`，重跑矩阵。
8. 遮挡负例中 RTAB VO `publish_null_when_lost=true` 产生 NaN 姿态，TF 桥与图节点都收到无效里程计。扩展 owned 范围，让隔离 TF 桥只转发有效 odom 到图节点并过滤 TF；语义节点也拒收无效 odom，再复测遮挡。
9. 前两种场景及遮挡负例通过后，增加“图坐标按钮到 RTAB `/map` 最近占据格”的空间对照，避免仅验证 topic 的 `frame_id` 字符串。该距离只用于确认同一地图尺度内对齐，不代表按钮操作精度。

## Validation

- Python 编译、旧语义节点默认行为回归、投影数学与失效条件检查；隔离 Gazebo/ROS2 端到端运行。
- `git diff --check`、目标 diff 复审、`git status --short --branch`；只提交 owned 文件。

## Commit Intent

验证后提交 `feat: project stereo button landmarks into graph map`，再同步共享仓库。

## Open Risks

- 稀疏场景全局回环此前不稳定；本 target 必须区分“图坐标正确投影”和“回环提高精度”。
- 若 RTAB-Map 图更新和相机检测存在明显时序不一致，应先缩小结论，不得把瞬时重投影误称为长期语义地图。
- 已验证的 2D 图地图与按钮同尺度，但图全局回环、局部非刚性变形及图校正的三维 OctoMap 未覆盖；按钮没有作为 RTAB 图优化约束。
