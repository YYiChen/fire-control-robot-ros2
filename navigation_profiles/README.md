# Lab room：A* 与 B 样条路径对比

这是给 ROS2 Humble + TurtleBot3 Burger 的**独立学习配置**。它不改 `/opt/ros`，也不修改已保存的 Gazebo 世界或地图。

## 这套文件实际做什么

| 部分 | 状态 | 作用 |
| --- | --- | --- |
| A* | 实际生效 | 从本机安装的 TurtleBot3 `burger.yaml` 复制一份，将 `planner_server → GridBased → use_astar` 改为 `true`。|
| DWB | 保持原配置 | 继续负责局部避障和底盘速度控制。|
| B 样条 | 只读诊断 | 将 Nav2 `/plan` 发布成 `/plan_bspline`，只用于在 RViz 比较曲线。它绝不控制 `/cmd_vel`。|

不能直接把未验证的 B 样条曲线交给控制器：平滑曲线可能切入墙或柱子。等 A* 基线稳定、并完成碰撞检查后，再把平滑器接进行为树。

## 第一次安装到 WSL

在 WSL 新终端执行。代码会复制到 `~/ros2_ws`，不会从 `/mnt/c` 直接运行。

```bash
source /opt/ros/humble/setup.bash

mkdir -p ~/ros2_ws/src/lab_room_navigation_profiles
cp -r /mnt/c/Users/32126/Desktop/Leeds\ Homework/Semester\ 5/科研/ROS/navigation_profiles/. \
  ~/ros2_ws/src/lab_room_navigation_profiles/

python3 ~/ros2_ws/src/lab_room_navigation_profiles/install_lab_room_nav2_profile.py \
  --workspace ~/ros2_ws
```

如果提示缺少 `yaml`，运行：

```bash
sudo apt install python3-yaml
```

这是安装一个 Python YAML 库，不会升级系统或 ROS 包。

## 用 A* 启动导航

先启动已保存的 Gazebo 场景。随后在第二个终端启动导航：

```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
export TURTLEBOT3_MODEL=burger

ros2 launch turtlebot3_navigation2 navigation2.launch.py \
  use_sim_time:=True \
  map:=$HOME/maps/lab_room_obstacles_v1_2026-09-21_16-56-48.yaml \
  params_file:=$HOME/ros2_ws/src/lab_room_navigation_profiles/generated/lab_room_nav2_astar.yaml
```

在 RViz 保持 `LaserScan` 和 `Amcl Particle Swarm` 关闭。设置一次 **2D Pose Estimate** 后，使用：

```bash
ros2 param get /planner_server GridBased.use_astar
```

输出必须是：

```text
Boolean value is: True
```

先仅用 Nav2 Goal 做三次目标测试。遥控只可用于 AMCL 初始化，设置完初始位置后要结束遥控节点，避免多个节点同时发布 `/cmd_vel`。

## 观察 B 样条候选路径

导航启动后，在第三个终端运行：

```bash
source /opt/ros/humble/setup.bash
python3 ~/ros2_ws/src/lab_room_navigation_profiles/bspline_path_diagnostic.py
```

发送 Nav2 Goal 后验证：

```bash
ros2 topic echo --once /plan_bspline
```

RViz 中可通过 **Add → By topic → /plan_bspline → Path** 加入曲线显示。比较 `/plan` 与 `/plan_bspline` 是否在同一条走廊内；若曲线靠近或穿过墙体/柱子，不得将它接入控制器。

## 下一阶段的接入门槛

只有同时满足以下条件，才把平滑器接入 `A* → 平滑 → DWB` 控制链：

1. AMCL 位姿与 Gazebo 中真实小车一致；
2. A* 基线连续三次到达目标，期间没有人工遥控；
3. `/plan_bspline` 在有墙、柱子、门口的测试路径上都没有切入占用区；
4. Nav2 日志没有持续出现 `No valid trajectories` 或 `Failed to make progress`。

当前诊断节点使用的是实际三次均匀 B 样条采样，不是 Nav2 内置通用平滑器。这样能先学习并观察论文中的曲线拟合思想，同时保留 DWB 的原始、已验证路径作为安全边界。
