# 消防控制室近似 Gazebo 场景

参照论文中的室内栅格图建立的练习场景，不是论文房间的精确复刻。房间约 8 m × 5 m，包含前侧 1 m 入口、左上房间 0.8 m 门洞、右侧分隔、两根圆柱和四个设备箱。先用 TurtleBot3 Burger 验证场景和建图，再逐步调整尺寸与外观。

## 文件

| 文件 | 用途 |
|---|---|
| `fire_control_room.world` | Gazebo Classic 的 SDF 场景 |
| `fire_control_room.launch.py` | 加载同目录的 world 并生成 TurtleBot3；生成服务最多等待 120 秒；`gui:=false` 可无界面运行 |

## 第一次试跑（在 WSL Ubuntu 终端）

先在旧的遥控终端按 `Q`，再分别在旧 SLAM、RViz、Gazebo 的终端按 `Ctrl+C`。地图已保存到 `~/maps/`；不要同时启动两个 Gazebo 世界或两个 SLAM 节点。

```bash
mkdir -p ~/my_worlds
cp "/mnt/c/Users/32126/Desktop/Leeds Homework/Semester 5/科研/ROS/gazebo_scene/fire_control_room.world" ~/my_worlds/
cp "/mnt/c/Users/32126/Desktop/Leeds Homework/Semester 5/科研/ROS/gazebo_scene/fire_control_room.launch.py" ~/my_worlds/

source /opt/ros/humble/setup.bash
export TURTLEBOT3_MODEL=burger
ros2 launch ~/my_worlds/fire_control_room.launch.py
```

在 Gazebo 左侧 **Models** 下应出现房间墙体、圆柱、设备和 `burger`。在新终端验证：

```bash
source /opt/ros/humble/setup.bash
gz model -m burger -p
ros2 topic info /scan
```

`gz model` 应显示接近 `(0.5, 0.5)` 的小车位置，`/scan` 应有 1 个发布者。如果场景出现但没有小车，先查看 Gazebo 启动终端是否出现 `SpawnEntity` 超时错误。

## 在新场景建图

下面每条长时间运行的命令使用**不同终端**。保持 Gazebo 终端运行；先启动 SLAM，再启动 RViz 与遥控。

```bash
# 终端 2：SLAM Toolbox（调快地图刷新）
source /opt/ros/humble/setup.bash
ros2 launch slam_toolbox online_async_launch.py use_sim_time:=true slam_params_file:=$HOME/ros2_ws/src/slow_teleop/config/slam_toolbox_fast.yaml
```

```bash
# 终端 3：RViz；Fixed Frame 设为 map，添加 Map 并选择 /map
source /opt/ros/humble/setup.bash
rviz2 --ros-args -p use_sim_time:=true
```

```bash
# 终端 4：自动停车键盘遥控；W/A/S/D 移动，空格停车，Q 退出
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
ros2 run slow_teleop slow_teleop
```

地图扫完后，在另一个终端保存为新的文件名：

```bash
source /opt/ros/humble/setup.bash
mkdir -p ~/maps
ros2 run nav2_map_server map_saver_cli -f ~/maps/fire_control_room_01
```

应生成 `~/maps/fire_control_room_01.yaml` 和 `.pgm`。靠近门、柱子、设备时可以把遥控速度降到 `0.08` m/s：退出旧遥控后运行 `ros2 run slow_teleop slow_teleop --ros-args -p linear_speed:=0.08`。

## 调整场景

编辑 `.world` 中各模型的 `<pose>x y z roll pitch yaw</pose>` 与几何 `<size>`、圆柱 `<radius>`。保持门洞和设备之间有足够净宽；SDF 能通过语法检查，并不代表机器人一定能通过。代码与场景不要写进 `/opt/ros/` 系统目录。
