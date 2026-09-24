# 双目仿真：第一阶段验证

这一目录分两阶段验证**双目测距**以及**画面按钮与三维坐标的关联**。它不会启动底盘、导航、机械臂、工控机驱动或任何真实设备。

第二阶段另有一个带 ArUco 和三个具名按钮的仿真场景，用于验证画面识别、双目测距和三维坐标关联。见下文“语义面板实验”。

## 本轮固定设计

| 项目 | 值 | 原因 |
|---|---:|---|
| 左右相机基线 | 0.06 m | 适合近距离面板观测的第一组参数 |
| 图像尺寸 | 640 × 480 | 降低 WSL 图形和点云负担 |
| 相机频率 | 30 Hz | 与当前激光仿真频率一致 |
| 目标距离 | 0.4、0.6、0.8 m | 覆盖机械臂靠近面板前的测试区间 |
| 同步模式 | 严格同步 | Gazebo 的双相机应使用同一个仿真时钟；不先掩盖时间戳问题 |

本阶段的通过标准：左右图像能发布、校正后同一目标位于相同行、`/stereo/disparity` 有稳定数据、`/stereo/points2` 可接收。它**不代表**真实双目精度已经通过。

## 1. 复制到 WSL 并检查依赖

在 WSL 终端运行：

```bash
mkdir -p ~/stereo_sim
cp -r "/mnt/c/Users/32126/Desktop/Leeds Homework/Semester 5/科研/ROS/stereo_sim/." ~/stereo_sim/
chmod +x ~/stereo_sim/scripts/check_stereo_sim_dependencies.sh
~/stereo_sim/scripts/check_stereo_sim_dependencies.sh
```

脚本只读取 ROS/Gazebo 的安装状态。成功时显示 `RESULT: READY`；缺少包时显示精确缺项和 `RESULT: NOT READY`，不会安装软件。

若仅缺图像处理包，确认结果后可以由你手动安装：

```bash
sudo apt install ros-humble-image-pipeline ros-humble-image-view
```

不要执行 `apt upgrade`。

## 2. 传感器模型完成后的处理管线

`stereo_test_world.launch.py` 会启动一个没有机器人、没有导航的测试世界：其中有一套固定双相机和一块位于相机前方 0.60 m 的高对比度面板。先启动它：

```bash
source /opt/ros/humble/setup.bash
ros2 launch ~/stereo_sim/launch/stereo_test_world.launch.py
```

该模型按 ROS 2 Humble 的 `libgazebo_ros_camera.so` 多相机接口设计。在本机 Humble 的实测输出中，发布下列四个话题：

```text
/stereo/stereo_rig/left/image_raw
/stereo/stereo_rig/left/camera_info
/stereo/stereo_rig/right/image_raw
/stereo/stereo_rig/right/camera_info
```

先用 `ros2 topic list | grep stereo` 实际确认话题名；若与你的 Humble 包输出不同，将看到的四个话题作为下列 launch 参数传入。启动处理链：

```bash
source /opt/ros/humble/setup.bash
ros2 launch ~/stereo_sim/launch/stereo_processing.launch.py use_sim_time:=true
```

它会启动两个图像校正节点、一个视差节点和一个点云节点，输出：

```text
/stereo/left/image_rect
/stereo/right/image_rect
/stereo/disparity
/stereo/points2
```

第一版 `/stereo/points2` 是 XYZ 深度点云，不附带颜色。这适合验证距离和三维定位；后续需要带颜色的点云时，再单独接入能发布彩色校正图的处理组件。

## 3. 第一项运行检查

```bash
ros2 topic hz /stereo/stereo_rig/left/image_raw
ros2 topic hz /stereo/stereo_rig/right/image_raw
ros2 topic echo --once /stereo/disparity
ros2 topic info /stereo/points2
```

左右原始图像应都接近 30 Hz。`disparity` 必须包含正的有效视差区域；如果全为无效值，优先检查相机基线、右相机 `CameraInfo.P[3]`、相机光轴方向和时间戳。

## 一键隔离测试

通过预检后，可以让脚本自动启动无界面 Gazebo、处理链和话题检查：

```bash
chmod +x ~/stereo_sim/scripts/run_stereo_sim_test.sh
~/stereo_sim/scripts/run_stereo_sim_test.sh
```

脚本使用 `ROS_DOMAIN_ID=77` 和 Gazebo 端口 `11377`，完成后会自动停止自己启动的进程，不影响日常的 TurtleBot、SLAM 或 Nav2。除检查话题外，它还会从画面中央区域计算深度，要求落在目标真实距离的 `0.58–1.50 倍` 接受区间内。

依次验证 0.40 m、0.60 m、0.80 m：

```bash
~/stereo_sim/scripts/run_stereo_range_test.sh
```

本机 WSL 的已验证结果如下；深度值由中央 40 × 40 像素的正视差中位数换算得出：

| 面板距离 | 中央深度结果 | 视差消息数 | 点云消息数 | 结果 |
|---:|---:|---:|---:|---|
| 0.40 m | 0.371 m | 234 | 225 | 通过 |
| 0.60 m | 0.573 m | 234 | 209 | 通过 |
| 0.80 m | 0.769 m | 231 | 197 | 通过 |

三次测试均使用无界面 Gazebo，结束后已确认隔离 ROS 域中没有遗留节点或 `gzserver` 进程。

## 目前边界

本仓库已加入独立 Gazebo 双相机模型与测试世界，使用 Humble 的 `libgazebo_ros_camera.so`。第一轮运行仍需要通过第 1 步确认本机包和插件存在；若模型输出话题与默认值不同，用 launch 参数覆盖即可，例如：

```bash
ros2 launch ~/stereo_sim/launch/stereo_processing.launch.py \
  left_image_topic:=/实际左图话题 \
  left_camera_info_topic:=/实际左相机信息话题 \
  right_image_topic:=/实际右图话题 \
  right_camera_info_topic:=/实际右相机信息话题
```

## 语义面板实验：画面按钮与三维地图坐标关联

将此目录复制到 WSL 的 `~/stereo_sim`，然后运行：

```bash
source /opt/ros/humble/setup.bash
chmod +x ~/stereo_sim/scripts/*.sh
~/stereo_sim/scripts/run_semantic_panel_test.sh
python3 ~/stereo_sim/scripts/verify_semantic_panel_faults.py
```

第一条测试在独立的 `ROS_DOMAIN_ID=78` 和 Gazebo 端口 `11378` 中启动无界面场景，自动移动双目相机，核验坐标后关闭本次启动的进程。第二条用合成 ROS 消息验证失效保护，不启动 Gazebo。两条均不启动底盘、导航、机械臂或实机设备。

若想在 Gazebo 图形界面观察同一场景，另开 WSL 终端运行：

```bash
source /opt/ros/humble/setup.bash
ros2 launch ~/stereo_sim/launch/semantic_panel.launch.py gui:=true
```

主要输出：

| 话题 / TF | 含义 |
|---|---|
| `/semantic_panel/annotated_image` | 左目画面中的按钮框、名称及测距 |
| `/semantic_panel/button/{mute,reset,confirm}/pose` | 具名按钮在 `map` 中的三维位置 |
| `/semantic_panel/marker_pose`、`map → panel_marker_582` | ArUco 582 的三维基准 |
| `/semantic_panel/markers` | RViz 中的三个按钮球形标记 |
| `/semantic_panel/status` | 每次处理的检测状态、像素、深度、世界坐标、一致性误差 |
| `/stereo/points2` | 场景的双目点云；当前仅有 XYZ，无语义标签 |

节点按时间戳配对左图与视差，从仿真按钮颜色提取三个目标，求出按钮中心深度，并用 ArUco 位置/方向检查按钮是否与同一面板相符。只有有效深度且一致性误差不超过 2.5 cm 才发布按钮位置。`map → stereo_left_camera_optical_frame` 由 Gazebo 模型真值产生，**这是仿真测试的捷径**，不能当成实机定位实现。

2026-09-23 本机 WSL2/ROS Humble 无界面测试结果（每个场景都含相机移动前、后两段）：

| 场景 | 面板位置与偏航 | 按钮三维误差中位数（mute/reset/confirm） | 图像位移 | 结果 |
|---|---|---|---|---|
| A | x=0.60 m, y=0, yaw=0 | 3/1/2 mm | 16.0/4.5/8.9 px | 通过 |
| B | x=0.75 m, y=0.02 m, yaw=-0.06 rad | 2/1/2 mm | 16.7/9.0/4.2 px | 通过 |

失效测试也通过：遮去 ArUco 后三个按钮均不输出位置；视差全无效时也均不输出位置。这些数值属于**已知尺寸、理想配色、Gazebo 真值相机位姿**的封闭场景，尚未测量标定误差、反光/遮挡、任意真实按钮、机器人位姿漂移，也尚未形成 RTAB-Map 三维占据地图、路径规划或机械臂按压闭环。当前左目用原图，而视差由校正图计算；仿真相机无畸变所以可对齐，实机必须使用同一校正坐标系。毫米级表中结果不可直接外推到真实消防控制室。

若调整面板位置，可在 WSL 内生成场景并复测，例如：

```bash
python3 ~/stereo_sim/scripts/generate_semantic_panel_world.py \
  --template ~/stereo_sim/worlds/semantic_panel.world \
  --output ~/stereo_sim/worlds/semantic_panel_shifted.world \
  --panel-x 0.75 --panel-y 0.02 --panel-yaw -0.06
SEMANTIC_WORLD=~/stereo_sim/worlds/semantic_panel_shifted.world \
SEMANTIC_PANEL_X=0.75 SEMANTIC_PANEL_Y=0.02 SEMANTIC_PANEL_YAW=-0.06 \
  ~/stereo_sim/scripts/run_semantic_panel_test.sh
```

## 实时三维占据地图：OctoMap

在 WSL 安装一次 ROS 2 Humble 的 OctoMap server（只安装，不升级现有 ROS）：

```bash
sudo apt install ros-humble-octomap-server
```

运行同一面板场景的三维建图与语义关联检查：

```bash
~/stereo_sim/scripts/run_semantic_occupancy_test.sh
```

该脚本使用独立 `ROS_DOMAIN_ID=79` / Gazebo 端口 `11379`。`semantic_occupancy.launch.py` 将 `/stereo/points2` 接入 OctoMap，使用 `map` 作为固定坐标系、2.5 cm 体素和 1.2 m 最大测距；`/octomap_point_cloud_centers` 是已占据体素中心的三维点云，`/octomap_binary` 为可保存的占据树消息。按钮名称和位置仍由 `/semantic_panel/button/*/pose` 提供，OctoMap 不会自动替这些体素命名。

2026-09-23 WSL 无界面测试：默认面板、相机移动前后分别观察到 114/168 个已占据体素，三个按钮到最近已占据体素约 0.010/0.015/0.009 m；偏移并旋转的面板分别观察到 184/192 个体素，相应距离约 0.019/0.012/0.011 m。两个场景都持续收到点云和地图更新，三个按钮位置仍通过 Gazebo 真值比较。实验依赖真值相机 TF，并未解决实机 SLAM、遮挡、不规则障碍、导航控制和机械臂按压。2.5 cm 地图体素和厘米级表面匹配也不应被解释成按压精度。

## 按钮接近位与 A* 路径建议

`semantic_occupancy.launch.py` 还会启动一个**只规划、不驱动车体**的节点。默认目标为 `reset` 按钮，取面板法线朝相机的一侧，在按钮前 0.25 m 处选候选观察位；用 `/projected_map` 的已知空闲格做 A* 搜索，先对占据格和未知格施加 7 cm 足迹缓冲。只有起终点和整条路径都有已知空闲空间时，才在 `/semantic_panel/approach_path` 发布非空 `nav_msgs/Path`。`/semantic_panel/approach_status` 说明 `ready`、输入过期、起终点不安全或无路可达的原因。此路径是**建议**，并未接入 Nav2 或机械臂。

```bash
source /opt/ros/humble/setup.bash
python3 ~/stereo_sim/scripts/verify_semantic_approach_synthetic.py
~/stereo_sim/scripts/run_semantic_approach_test.sh
```

受控地图测试得到 16 个路点，绕过中间障碍的最大横向位移约 0.138 m；将通道堵死后状态为 `no_known_free_path`，输出空路径。当前实际的单视角双目场景中，OctoMap 投影虽然有约 300 个空闲格，但空闲扇区不足以容纳 7 cm 足迹，规划器输出 `start_or_goal_not_free`，不输出路径。要让它在真实场景给出安全路线，需增加视角覆盖或使用已有 2D 激光地图/导航图，解决定位与地图一致性，并在有车体模型的仿真里验证运动闭环。

### 多视角扫掠后实际给出路径

```bash
~/stereo_sim/scripts/run_multiview_approach_test.sh
```

该脚本在独立的 `ROS_DOMAIN_ID=81` / Gazebo 端口 `11381` 中，利用 Gazebo 服务逐步移动**仿真相机**到八个视角，逐步累积 OctoMap，然后检查建议路径。2026-09-23 实测：初始投影有 214 个已知空闲格，前七个视角仍拒绝规划；最后相机到 `x=0.12, y=0` 后有 513 个已知空闲格，状态变为 `ready`，输出 8 个路点，终点距 `reset` 按钮 0.249 m，路径中 0 个路点落入占据/未知格。该脚本不操作 TurtleBot 或真实硬件；相机移动是受控实验动作，不能替代底盘导航闭环。

## 双目视觉里程计审计：逐步移除仿真真值捷径

使用 [RTAB-Map 官方 ROS2 双目里程计](https://github.com/introlab/rtabmap_ros) 做**独立审计**；下文另有接入语义/OctoMap 的隔离实验。WSL 中仅补装所需包（本机 apt 报告 0 升级、14 新装）：

```bash
sudo apt install ros-humble-rtabmap-odom
~/stereo_sim/scripts/run_stereo_odometry_audit.sh
```

审计在 `ROS_DOMAIN_ID=82` / Gazebo 端口 `11382` 运行。发现 Gazebo 多相机插件将左右 `CameraInfo.P[3]` 都发布为约 `-30`；RTAB-Map 因两个投影平移相同而判断基线为 0。审计支路的 `normalize_stereo_camera_info.py` 只将左目 `P[3]` 归零，保留右目的 `-fx×0.06 m`，原话题和当前工作链不变。节点使用 `stereo_base_link → stereo_left_camera_optical_frame` 静态 TF 和 RTAB-Map `Reg/Force3DoF=true`，其 2D 运动约束因此作用在竖直朝上的基座坐标系，而非光学坐标系。

2026-09-24 两次独立无界面重复：相机平滑前进 0.10 m、横移 0.05 m、转向 0.08 rad；真值只交给审计验证器，RTAB-Map 仅收双目校正图、相机参数和固定的基座到相机外参。结果如下：

| 运行 | 最终平移向量误差 | 最终偏航误差 | /vo/odom 消息数 | 结果 |
|---|---:|---:|---:|---|
| 1 | 0.0051 m | 0.0096 rad | 643 | 通过审计阈值 |
| 2 | 0.0021 m | 0.0047 rad | 617 | 通过审计阈值 |

测试中单个中途样本仍可能出现约 1 cm 的横向波动，因此最终误差不足以证明全过程毫米级稳定；尚需多场景、往返/回环、遮挡、低纹理和机器人运动的基准测试。

## 视觉里程计接管语义按钮、占据地图与接近路径

```bash
source /opt/ros/humble/setup.bash
~/stereo_sim/scripts/run_semantic_visual_odometry_test.sh
```

隔离脚本使用 `ROS_DOMAIN_ID=83` / Gazebo 端口 `11383`；运行后会停止自己启动的进程。`semantic_visual_odometry.launch.py` 复用上面的 RTAB-Map 双目配置，让语义节点与路径规划器订阅 `/vo/odom`，它们均**不订阅** `/model_states`。Gazebo 真值仅供验证器比较。`map` 是视觉里程计起点建立的局部坐标系，不具有回环校正，也不应视为实机全局地图。`map → stereo_base_link` 由 VO 位姿产生，基座到左相机光学坐标系使用固定外参。

实测发现 30 Hz 原始点云有时先于较慢的视觉里程计/TF 到达 OctoMap，导致 TF 消息队列丢帧、地图中断。`vo_synchronized_cloud.py` 只转发与有效 `/vo/odom` 同时间戳的点云到 `/vo/points2`，最多约 5 Hz；没有对应视觉位姿的点云不参与建图。原始双目画面与 `/stereo/points2` 保持原频率，限速只作用于本实验的三维建图输入。规划器会明确报告过期输入、不可通行或 `ready`，不会在未知区生成路线。

2026-09-24 三次独立无界面复测均通过：相机平滑前进 0.10 m、横移 0.05 m 后，VO 位移向量误差约 0.0048/0.0015/0.0017 m；三个按钮在移动阶段的三维位置误差中位数约 1.4–4.5 mm；OctoMap 投影已知空闲格从 216 增至 331/343/336；`reset` 的建议路径均为 7 个路点，终点距按钮 0.249 m，检查时所有路点落在已知空闲格。建图发生过间歇性停更，桥接修正后这三轮未复现，但三轮不能证明长期稳定。

这一步打通了**理想面板场景中的视觉位姿 → 按钮三维坐标 → 累积占据图 → 接近路径建议**；测试器用 Gazebo 服务移动相机，系统尚未自动驾驶真实或仿真车体。现有按钮依赖理想颜色和 ArUco，仍需真实面板识别、长程视觉 SLAM/回环、相机与机械臂外参、运动中失效恢复、多场景统计和按压闭环，才能达到完整科研验证。

## 双目装上可运动的仿真底盘

```bash
source /opt/ros/humble/setup.bash
~/stereo_sim/scripts/run_semantic_mobile_test.sh
```

脚本在 `ROS_DOMAIN_ID=84` / Gazebo 端口 `11384` 启动隔离实验，结束时停止本次进程。启动阶段从系统安装的 TurtleBot3 Burger SDF 生成 `~/stereo_sim_generated/stereo_mobile_bot.sdf`，在车体 `base_link` 上增加现有 6 cm 基线双目相机，保留原有轮子、碰撞体和差速驱动；系统模型本身不修改。新场景只有面板，机器人由 Gazebo `spawn_entity.py` 放入。测试器通过 `/cmd_vel` 低速前进、转向并发送零速停止，使用 `/model_states` 只核验真值；运行中的 VO、语义和路径节点仍使用图像/VO，不读取真值。

2026-09-24 两次独立实测：车体分别前进约 0.14939 m，转向约 0.122 rad；VO 位移向量误差 0.0025/0.0049 m；运动后 `reset` 按钮三维位置误差中位数 0.0061/0.0065 m；OctoMap 投影已知空闲格从 351→539 和 361→509，地图在结束时分别约 0.14/0.20 秒前更新，规划器均报告 `ready` 和 7 个路点。这证明**相机随车轮驱动移动时**整条感知与建图链能继续运行。测试器仍按固定短动作给出速度，机器人尚未自动跟随 A* 路径，也未接近并按下真实按钮。

若只想看画面，可在 WSL 终端运行 `ros2 launch ~/stereo_sim/launch/semantic_mobile_stereo.launch.py gui:=true`，应看到面板前的一台 TurtleBot3 Burger。这个独立启动不会自动驾驶车体；执行自动测试时使用上面的隔离脚本。

## 仿真车体自动驶向具名按钮前的观察位

```bash
source /opt/ros/humble/setup.bash
~/stereo_sim/scripts/run_semantic_autonomous_test.sh
python3 ~/stereo_sim/scripts/verify_semantic_follower_faults.py
```

该实验使用独立 `ROS_DOMAIN_ID=85` / Gazebo 端口 `11385`，同时启动双目车、VO、具名按钮、OctoMap、A* 路径建议和低速跟踪器。车体先等待安全路径；如果起点的近距离区域尚未知，只在 `/scan` 前方安全、地图/里程计/激光新鲜时，以 0.025 m/s 最多前进 0.12 m 扩展视角。路径出现后才跟踪；路径、地图、VO、激光过期，前方有障碍，或路线穿过非空闲格时均发零速。自动实验使用距按钮 **0.28 m** 的观察位；通用规划器默认仍是 0.25 m。测试器只观察，不发布驾驶命令。

最初直接等待路线时，因相机起点附近未知，车体保持原地，这是安全行为。加入激光守护的有限扫图后两次独立测试均自动到位：车体共行进约 0.272/0.252 m，最后 VO 与停车时采用的观察位相距 0.023/0.024 m，停车后 2 秒在 Gazebo 真值中的平面漂移为 0；地图持续更新。最后 VO 与**最新重规划目标**相距 0.024/0.045 m，显示目标估计仍有波动，不能据此声称厘米级实机导航精度。独立失效注入检查确认路径不可达、激光过期、前方近障碍和起步扫图被堵时速度为零。

移动中发现旧 ArUco 朝向近似把“相机正前方”当作“面板法向”，使接近位随车转向横向漂移；现在标记朝向使用 `solvePnP` 的三维旋转。理想正向面板的实验里，运动前后法向的横向分量保持约在 ±0.02 内。按钮深度与标记一致性检查仍沿用受控场景的局部几何。此控制器是便于验证整条链的仿真低速跟踪器，尚未代替 Nav2 的动态避障、长程定位与恢复行为，也没有机械臂或真实面板识别。

## 面板姿态变化与遮挡稳健性矩阵

```bash
source /opt/ros/humble/setup.bash
~/stereo_sim/scripts/run_semantic_robustness_matrix.sh all
# 可单独运行 offset、rotated 或 occluded
```

脚本分别在隔离的 `ROS_DOMAIN_ID=86/87/88` 运行，生成的 world 和日志仅放在 WSL 用户目录下。`offset` 把面板移到 `(0.68, 0.05)`；`rotated` 再使其绕竖直轴偏航 `0.12 rad`；`occluded` 在相机与面板之间加一块不透明挡板。前两组比较 Gazebo 真值与视觉估计，检查三个按钮、面板朝向、持续建图、路径与停车；第三组要求没有按钮坐标/标记观测、没有导航运动且车停止。单组运行适合排障，例如在末尾加 `rotated`。

2026-09-24 一轮完整矩阵 **3/3 通过**：偏移与偏航场景的三个按钮最终误差中位数均约 6–9 mm，面板朝向最终偏航误差分别约 0.008/0.015 rad；停车时 VO 距记录目标约 0.032/0.031 m，停车后的 Gazebo 真值位移为 0。遮挡场景没有发布按钮坐标或标记观测，车保持原地。小尺寸 ArUco 的单帧 PnP 姿态和单帧视差会抖动；当前对**同一静态面板**的多帧观测做有界中位数融合，并在标记位置跳变超过 0.10 m 时清空历史。失效注入检查仍确认路径不可达、激光过期、前方障碍和受阻扫图时速度为零。

该矩阵的遮挡试验中控制状态为 `stopped_stale_input`，说明它验证了最终安全停车，没有单独证明每个感知故障分支。一次偏航试跑曾在停车后未被验证器收到投影地图而失败，单组复测和完整矩阵未复现；这仍是需长期重复统计的间歇风险。这里的面板颜色、ArUco、短路径和静态几何均为受控仿真；不能据此称真实设备状态识别、长程 SLAM、动态避障或机械臂按压已完成。

## 双目图优化 SLAM 的隔离审计

```bash
~/stereo_sim/scripts/run_semantic_graph_slam_audit.sh
```

该脚本使用独立的 `ROS_DOMAIN_ID=89` 和 Gazebo 端口 `11389`，在用户目录生成世界、RTAB-Map 数据库和日志。WSL 系统尚未安装 `rtabmap_slam` 时，脚本将所需 Humble Debian 包解压到 `~/stereo_sim_deps/overlay`，不修改系统 ROS 安装。运行时由现有双目 VO 提供 `/vo/odom`，隔离 TF 桥发布同时间戳的 `vo_odom → stereo_base_link`，RTAB-Map 图节点输出 `graph_map → vo_odom`、`/mapGraph` 和二维占据图 `/map`。验证器用 Gazebo 服务令**仿真相机**沿 0.15 m × 0.08 m 闭合路线移动；真值只供比较，不输入图优化。新增两个黑白视觉标志的世界仅供此次实验。

2026-09-24 实测：原始稀疏面板世界在默认 `RGBD/OptimizeMaxError=3` 下三次完整运行产生 25–30 个图节点、相同数量的占据图，其中 2/3 次接受全局回环。三轮 VO 首尾闭合距离约 0.0060/0.0045/0.0061 m，图校正后约 0.0019/0.0047/0.0142 m；图优化并未稳定降低误差。把一致性阈值改成 4 的一次探索仍未接受全局回环，图闭合距离也较 VO 大，因此恢复默认 3。加两个视觉标志后的两次独立运行分别建立 23/22 个图节点、38/39 条约束及 23/22 张占据图，但全局回环均未被接受；16/18 条非相邻约束属于局部空间约束。审计脚本对此返回 `INCOMPLETE`，仍打印数据库和日志路径供分析。图首尾距离是采样的近似指标，不代表地图每一点误差，也不能证明优化带来真实精度提升。

目前工作链中的按钮位姿、OctoMap、接近规划都处于 VO 起点定义的 `map` 坐标系。语义节点直接广播 `map → stereo_base_link`，图节点另行广播 `graph_map → vo_odom`；若简单把两套启动并在一起，基座会有两个 TF 父节点，按钮与地图也不会随回环同步校正。下一阶段须统一 TF 树，并在图位姿修正后按观测时间戳重投影按钮地标及点云，或重新建图，再验证旧按钮坐标、障碍和路径是否保持对齐。当前图实验与已通过的自动接近链保持隔离，不能宣称完成科研级实时三维语义建图。

### 按钮观测投影到图地图

```bash
~/stereo_sim/scripts/run_graph_semantic_test.sh base
~/stereo_sim/scripts/run_graph_semantic_test.sh rotated
~/stereo_sim/scripts/run_graph_semantic_test.sh occluded
```

这组隔离实验使用 `ROS_DOMAIN_ID=90/91/92`。语义节点在这里把双目检测结果明确标成 `vo_odom`，并关闭自身的相机/面板 TF 广播；图支路仅转发有效 VO 里程计，建立 `graph_map → vo_odom → stereo_base_link`。新投影节点订阅 RTAB-Map `/mapGraph` 修正和 `/info` 处理心跳，将按钮及标记转成 `/graph_semantic_panel/.../pose`，坐标系为 `graph_map`。图修正后可重投影已保存的观测，但消息仍保留**原图像采集时间戳**；状态话题将久未看到的按钮标为 `stored`，不能把缓存位置当作新检测。旧 VO/OctoMap/接近启动沿用原 `map` 默认值，已做回归。

2026-09-24 在 Gazebo 无界面试验：基础面板和偏航 `0.12 rad` 的面板均检测到三个按钮、8 个以上图节点与持续更新的二维地图；相机前进 0.10 m、横移 0.05 m 后，新采集的图坐标按钮位置到模型真值的误差在这几轮为约 1–33 mm，末次基础场景为 2–3 mm，按钮平面位置到 RTAB 地图最近占据格约 1–3.4 cm。完全遮挡时没有发布按钮位置；视觉里程计丢失时的 NaN 姿态被隔离图支路拒绝，未再进入 TF。投影输出从每个原始检测触发三按钮重复发布改为只发布更新的按钮，基础场景仍通过。

这些数据证明在**短距离理想面板仿真**中，图像检测的按钮与图地图使用同一坐标尺度；它们不证明真实按钮识别、长期地标管理或回环校正后的三维地图一致性。此阶段只验收了二维占据图；原 VO 驱动 OctoMap 尚未接入图优化。图投影目前对缓存的 VO 地标应用最新全局 `map_to_odom` 变换，尚未把按钮作为图约束，也无法表示长轨迹图优化的局部非刚性修正。进行按压或自动导航前，还需真实面板图像/OCR/灯态数据、可靠回环、回环后的三维占据图一致性、失效恢复与车臂协同验证。

### 同一图支路的三维占据图

随后按 [RTAB-Map 官方 `Grid/3D` 与 `Grid/RayTracing` 参数](https://github.com/introlab/rtabmap/blob/master/corelib/include/rtabmap/core/Parameters.h)在上述隔离图启动中启用 2.5 cm 三维栅格、1.2 m 最大深度。沿用三个 `run_graph_semantic_test.sh` 场景；验证器现在除 `/map` 外，还要求 `/octomap_occupied_space` 有非空三维点、`/octomap_binary` 有非空树数据，且两者坐标系均为 `graph_map`。按钮位置与最近体素用完整 XYZ 距离比较。此 OctoMap 由 RTAB-Map 的图节点及其局部栅格生成，不是旧的 VO-only `octomap_server`。

2026-09-24 WSL 无界面实测：基础场景有 8 次三维点云更新、末次 173 个占据点、8 条 OctoMap 消息；偏航面板有 9 次三维点云更新、末次 267 个占据点、9 条 OctoMap 消息。移动结束时三个按钮距最近三维占据点，基础场景约 0.010/0.012/0.011 m，偏航场景约 0.007/0.010/0.010 m；相应按钮到仿真真值的误差在这些运行中约 0.013–0.029 m。完全遮挡时三个按钮、图节点、三维占据点和 OctoMap 输出均为零。启动日志确认四个 `Grid/*` 参数被图节点采纳。当前处理频率设为 2 Hz，三维图仅在关键位姿更新时发布，不能声称 30 Hz 全图重建或长时间实时性能已达标。仍需真回环后重建一致性、长轨迹/多面板、光照遮挡、语义持久化与真实图像数据验证。

## 消防面板文字与指示灯离线基线（合成域）

这个基线为后续 PaddleOCR 与双目语义地图集成提供**可复跑的输入、真值和评分格式**。它在 WSL 用户目录生成 11 张中文面板图：10 张可定位面板覆盖红/黄/绿/熄灭、弱光、倾斜、反光、灯遮挡和字遮挡，另 1 张遮挡 ArUco。先检测 ArUco 582 并将画面校正到面板坐标，再在固定文字行中运行 Tesseract 中文 OCR；LED 位置由**OCR 实际输出的文字框**向左搜索得到，灯色/亮灭从校正图像像素判定。输出每个灯相对标记的平面三维坐标（z=0），供以后接已有的 `graph_map` 标记位姿。评分时才读取真值，推理代码不使用样本真值。

只需下载并解压 Ubuntu 包到用户目录，不安装系统包、不使用 sudo：

```bash
mkdir -p ~/stereo_sim_deps/tesseract_pkgs ~/stereo_sim_deps/tesseract_overlay
cd ~/stereo_sim_deps/tesseract_pkgs
apt download tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng
for package in ./*.deb; do
  dpkg-deb -x "$package" ~/stereo_sim_deps/tesseract_overlay
done
bash ~/stereo_sim/scripts/run_panel_perception_baseline.sh
```

结果在 `~/stereo_sim_generated/panel_perception_baseline/`：`truth.json` 是生成器真值，`report.json` 包含逐样本 OCR 原文、预测灯态、`marker_local_xyz_m`、失败原因和汇总；图片同目录。报告中预测像素坐标属于**校正后的面板图**，真值像素坐标属于**原生成图**，空间误差统一在标记坐标系中比较。负例应满足：`led_occluded` 对“火警”输出 `unknown`，`text_occluded` 不凭空补“火警”，`marker_occluded` 不输出坐标或确定灯态。`position_samples` 是**成功输出坐标且目标可见的条件样本数**，不能只看其中位误差而忽略漏检。

2026-09-24 首次 WSL Tesseract 4.1.1 / OpenCV 4.5.4 运行曾得到 27/29 个可见文字、26/28 个灯态正确和 0.0067 m 位置误差中位数。后续 Gazebo 实拍发现小标记的整数角点误差被透视校正放大；加入亚像素角点细化后，重跑 10 张可定位合成图得到 **29/29 个文字、28/28 个灯态正确、28 个位置样本的误差中位数 0.001 m**，遮挡文字无“火警”假阳性、遮挡灯为 `unknown`、标记遮挡图拒绝输出，失败列表为空。这仍是固定字体/布局的人造图，**不能外推真实面板或论文 PP-OCRv5 的性能**，也不代表灯态已经实时写入图地图。

## Gazebo 双目相机实拍中文面板

```bash
source /opt/ros/humble/setup.bash
bash ~/stereo_sim/scripts/run_gazebo_perception_capture.sh
bash ~/stereo_sim/scripts/run_gazebo_perception_capture.sh \
  ~/stereo_sim_generated/perception_camera_all_on all_on
bash ~/stereo_sim/scripts/run_gazebo_perception_capture.sh \
  ~/stereo_sim_generated/perception_camera_all_off all_off
```

脚本在隔离的 `ROS_DOMAIN_ID=94` / Gazebo 端口 `11394` 启动测试世界，默认生成 `fire_on` 面板，也可以在输出目录后给出 `fire_on`、`all_on` 或 `all_off` 案例名。它从 `/stereo/stereo_rig/left/image_raw` 取一张带非零 ROS 时间戳的真实 Gazebo 渲染帧。生成真值与图像推理分开：验收器只在推理完成后对照 `generation.json`，检查文字召回、LED 状态和标记局部坐标。结果保存在指定的 `~/stereo_sim_generated/` 目录，包括 `gazebo_left_raw.png`、`gazebo_capture_report.json`、启动日志和生成模型。

2026-09-24 五次分别重启 Gazebo 的采集均通过：`fire_on` 三次、`all_on` 一次、`all_off` 一次。每次 640×480 图像都检测到 ArUco 582，文字 3/3、LED 状态 3/3 正确；红/黄/绿各一盏亮时颜色正确，三个灯全灭时均判为 `off`。按钮相对标记的坐标误差分别约 0.00036、0.00076、0.00149 m（各次相同），低于此合成验收的 0.02 m 门槛。实拍诊断帧中标记只有约 47×47 像素，整数角点的约 1 px 误差使外推到整块面板的单应变换偏移；加入 `cornerSubPix` 后行文字回到正确 ROI。停机检查未发现 ROS 域 94 的残留节点或该世界的 Gazebo 服务进程。

这证明的是一张固定、无遮挡的**合成面板纹理**可由 Gazebo 左目话题采集并在受控坐标系中完成 OCR/灯态/二维面板内坐标关联。还没有验证真实面板图像、PP-OCRv5、双目视差生成的按钮深度、按钮投影到 `graph_map`、相机/机器人移动、视角和遮挡变化或统计泛化性能。下一步应把同帧文字框/LED 中心与校正双目视差关联，得到相机坐标 XYZ，再经采集时刻 TF/图优化投影至地图，并与 Gazebo 真值做多距离、多姿态测试。
