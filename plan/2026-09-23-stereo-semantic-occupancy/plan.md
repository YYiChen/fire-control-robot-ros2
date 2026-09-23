# 双目点云的实时三维占据地图与按钮关联

## Goal

用 ROS 2 Humble 的成熟 OctoMap 组件累积双目点云，在 `map` 坐标系形成三维占据地图；让现有具名按钮的坐标可与同一地图核对。维持纯仿真，不启动底盘或实机。

## Dirty-State Note

启动时根仓库 `main` 已同步 `origin/main` 到 `63fc7d6`。已有的 `gazebo_scene/lab_room.*` 与 `reference/.../rec.bak` 为其他人的未跟踪内容，不修改或暂存。

## Owned Files

- `plan/2026-09-23-stereo-semantic-occupancy/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/semantic_occupancy.launch.py`
- `stereo_sim/scripts/run_semantic_occupancy_test.sh`
- `stereo_sim/scripts/verify_semantic_occupancy.py`

如需扩大范围，先更新此清单。

## Read-Only Files

- `gazebo_scene/lab_room.*`、`reference/industrial_pc_2026-09-22/`
- 现有 `stereo_sim` 语义按钮定位实现；若发现必须修复的接口问题，先更新 owned files

## Shared Dependencies

- ROS 2 Humble、Gazebo Classic、双目点云 `/stereo/points2`、仿真 TF `map → stereo_left_camera_optical_frame`
- 系统当前未安装 `octomap_server`；仅通过 apt 安装所需包，不运行 `apt upgrade`，不改其他现有 ROS 包

## Expected Work

1. 安装并核验 OctoMap server 的 Humble 二进制包。
2. 增加隔离场景启动，订阅双目点云并输出 3D 占据地图。
3. 定量核验地图随相机移动累积、面板区域确有占据数据、按钮坐标与地图同框；检查坏数据不会创建可执行目标。
4. 文档写清数据来源、运行方法、性能和真实科研限制。

## Validation

- ROS topic/TF、OctoMap 与按钮输出的实测检查，不以节点存在替代地图内容。
- 与两个相机位置的 Gazebo 真值对照；Bash/Python 语法、`git diff --check`、退出后进程残留检查。

## Commit Intent

独立提交本 target 文件；仅暂存 owned files。

## Open Risks

- 点云由纹理/视差计算而来，墙面/均匀表面稀疏；地图不能等同精密 CAD。
- 此阶段相机姿态来自 Gazebo 真值；后续必须换成真实定位与外参链。
- OctoMap 提供空间占据，按钮 ID 是独立语义层；移动决策及按压控制尚需实现与验证。
