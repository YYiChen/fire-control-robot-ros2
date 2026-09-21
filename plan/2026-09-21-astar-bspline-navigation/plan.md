# A* 与 B 样条导航配置

## Goal

为已保存的 lab room 地图提供一套独立的 Nav2 学习配置：实际导航明确启用 A*；同时提供不接管底盘控制的 B 样条路径对比节点，为后续经过碰撞验证的平滑接入做准备。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

这两个未跟踪文件是用户已有场景，和本 target 无关，不编辑、不暂存。

## Owned Files

- `navigation_profiles/`
- `plan/2026-09-21-astar-bspline-navigation/plan.md`
- `plan/log.md`

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- `/opt/ros/humble/**`（WSL 系统安装）
- 用户保存的 world 与地图快照

## Shared Dependencies

- TurtleBot3 Navigation2 的已安装 `burger.yaml`，安装脚本在 WSL 端从该文件复制，避免假设版本一致。
- Nav2 `planner_server` 的 `NavfnPlanner` 与 `use_astar` 参数。
- `nav2_smoother`、`nav_msgs/Path`、`/plan` 话题。

## Expected Work

1. 提供 WSL 端安装脚本：复制本机 TurtleBot3 默认参数，在副本中仅启用 A*，不修改 `/opt/ros`。
2. 提供 A* 启动脚本与验证命令，要求没有遥控节点同时控制底盘。
3. 提供只读 B 样条路径诊断节点，将 `/plan` 采样为 `/plan_bspline`；不将该话题连接到控制器。
4. 记录 B 样条接入控制链的安全门：曲线必须在代价地图中检查碰撞后才可替换实际控制路径。

## Validation

- Python 语法编译检查。
- 安装脚本 `--help` 与参数文件路径检查。
- `git diff --check` 与 `git status --short --branch`。

这些检查验证文件结构和脚本可解析。实际 ROS 包发现、参数加载、A* 路径及 B 样条诊断话题需要在用户的 WSL 运行环境中验证；本会话不能直接执行 WSL 命令。

## Experience Signal (for human review)

WSLg 下 RViz 的 `LaserScan` 与 `Amcl Particle Swarm` 显示会冻结，实际试验保持关闭；该限制已单独写入用户记忆。

## Commit Intent

```text
feat: add lab room A-star navigation and B-spline diagnostic profile
```
