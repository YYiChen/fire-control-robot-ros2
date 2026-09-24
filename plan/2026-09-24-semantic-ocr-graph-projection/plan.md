# 双目 OCR 地标投影到图地图

## Target Owner

`/root`（当前主 Agent）

## Goal

将新双目 OCR/LED 节点输出的相机光学坐标，按观测时间经 TF 转到 `vo_odom`，再按 RTAB-Map 的 `map_to_odom` 校正投影到 `graph_map`。由于 SLAM 地图原点可任意选取，验证器只用独立的 Gazebo 相机初始位姿估算 `graph_map` 与 SDF 世界的刚体对齐，然后在固定面板、相机仿真移动时比较三种指示灯位置，并检查遮挡/无 TF 等失效状态不产生错误地图点。

## Dirty-State Note

起始状态（`git status --short --branch`）：

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

工作区有 3 个无关未跟踪文件；本 target 不读写、不暂存这些文件。当前分支与 `origin/main` 同步。

## Owned Files

- `plan/2026-09-24-semantic-ocr-graph-projection/plan.md`
- `stereo_sim/scripts/semantic_ocr_graph_projector.py`
- `stereo_sim/launch/semantic_ocr_graph_slam.launch.py`
- `stereo_sim/worlds/semantic_ocr_panel.world`
- `stereo_sim/scripts/run_semantic_ocr_graph_test.sh`
- `stereo_sim/scripts/verify_semantic_ocr_graph.py`
- `stereo_sim/README.md`
- `plan/log.md`

## Read-Only Files

- `stereo_sim/scripts/stereo_panel_depth_node.py`
- `stereo_sim/scripts/graph_semantic_projector.py`
- `stereo_sim/launch/semantic_graph_slam.launch.py`
- `stereo_sim/launch/stereo_odometry_audit.launch.py`
- `stereo_sim/launch/stereo_test_world.launch.py`
- `stereo_sim/scripts/generate_gazebo_perception_panel.py`
- `stereo_sim/scripts/verify_stereo_panel_depth.py`
- `stereo_sim/worlds/perception_panel.world`
- ROS 2 Humble / Gazebo Classic runtime packages and user-local RTAB-Map overlay

## Shared Dependencies

- `/stereo_panel/{fire,fault,main_power}/pose` in the organized cloud's camera frame
- time-stamped TF chain `vo_odom -> stereo_base_link -> stereo_left_camera_optical_frame`
- RTAB-Map `/mapGraph` correction `T_graph_map_odom`
- existing OCR detector's generated Gazebo panel model and truth metadata
- existing graph-SLAM launch and graph-frame button projection, which must keep working unchanged

## Expected Work

1. Add an isolated projector that buffers OCR camera poses until the exact-time camera-to-VO transform exists, publishes VO poses, then applies the latest fresh graph correction and publishes graph-frame semantic poses.
2. Add a dedicated graph-SLAM launch, world, runner, and verifier for synthetic OCR/LED observations while leaving the established launch and projector behavior unchanged.
3. Align `graph_map` with the SDF world using only the initial Gazebo base truth and time-matched TF, then verify camera-frame, VO-frame, and graph-frame consistency against generated panel geometry before and after controlled camera movement; record missing-target behavior and runtime limitations.
4. Update the stereo simulation README and `plan/log.md`, inspect the diff, and commit only owned files.

## Validation

- Python compilation for the new projector and verifier; launch-file compilation/import; shell syntax; SDF XML parse.
- Pure transform checks with nontrivial translation and rotation, including observation timestamp and frame preservation.
- Isolated ROS 2 Humble/Gazebo run in a dedicated domain: initial detection, camera movement, graph-frame reprojection, world-truth error, and no-target/TF-unavailable failure handling.
- Existing graph semantic test and existing OCR-to-point-cloud test as focused regressions if the isolated runtime permits.
- `git diff --check` and `git status --short --branch`; stage only owned files.

## Experience Signal (for human review)

The new OCR/LED path and the older colored-button path use different source topics and coordinate frames. This target keeps them in a parallel projector to avoid changing established graph semantics; a human may later decide whether to unify their message contract. A nontrivial quaternion unit check caught a scalar-term indexing error before the final simulator run; the mathematical guard stays in the verifier.

## Commit Intent

提交信息：

```text
feat: project OCR stereo landmarks into graph map
```
