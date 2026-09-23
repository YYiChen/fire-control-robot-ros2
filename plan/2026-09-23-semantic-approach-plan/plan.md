# 基于语义按钮与占据地图的接近路径建议

## Goal

在纯仿真中根据具名按钮位置、面板朝向与 OctoMap 投影地图，计算具有安全距离的观察/操作位，并用 A* 规划从当前相机位置到该位置的路径；只发布路径，不发送底盘速度命令。

## Dirty-State Note

开始时 `main` 已推送 `04328e9`。用户原有 `gazebo_scene/lab_room.*` 和参考源码下 `rec.bak` 未跟踪，不修改、不暂存。

## Owned Files

- `plan/2026-09-23-semantic-approach-plan/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/launch/semantic_occupancy.launch.py`
- `stereo_sim/scripts/semantic_approach_planner.py`
- `stereo_sim/scripts/verify_semantic_approach.py`
- `stereo_sim/scripts/verify_semantic_approach_synthetic.py`
- `stereo_sim/scripts/run_semantic_approach_test.sh`

## Read-Only Files

- 旧版双目/按钮/OctoMap 节点与用户未跟踪场景

## Shared Dependencies

- `/semantic_panel/button/{name}/pose`、`/semantic_panel/marker_pose`、`/projected_map`、`/model_states`
- 现有 Gazebo/OctoMap 实验，ROS 2 `nav_msgs`、`geometry_msgs`

## Expected Work

1. 由按钮和面板基准确定 0.25 m 前方候选位。
2. 将三维地图投影的已知空闲区域作为 A* 搜索空间；未知格、占据格和机器人半径缓冲区不可通行。
3. 发布规划路径和明确状态，任何输入缺失/路径阻塞都不发布可执行路径。
4. 在受控的已知空闲地图验证路径终点、障碍绕行和阻断保护；在当前单向双目场景检查视野不足时是否安全拒绝路径。

## Validation

- 独立 ROS 域无界面仿真及受控地图；定量验证投影地图/路径/状态与未知区域拒绝行为。
- 静态语法、`git diff --check`、残留进程检查。

## Commit Intent

只暂存本 target 文件，单独提交并推送。

## Open Risks

- 当前 `map` 中的相机/面板定位仍用 Gazebo 真值作仿真基准。
- 投影地图只有双目视野，未知区域需拒绝通过；仅有观察位与路径，尚无 Nav2 闭环运动、B 样条或机械臂按压。
- 首次实测 OctoMap 投影地图虽然包含 298 个空闲格，但可见扇区很窄；按 7 cm 足迹和未知区域缓冲后，起点或目标不满足已知空闲要求。不得为通过测试放松安全约束。
