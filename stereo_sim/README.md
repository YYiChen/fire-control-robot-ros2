# 双目仿真：第一阶段验证

这一目录只验证**仿真相机的左右图像是否能形成视差和点云**。它不会启动底盘、导航、机械臂、工控机驱动或任何真实设备。

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
