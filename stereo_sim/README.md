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

当 Gazebo 已经发布下列四个话题时，再运行：

```text
/stereo/left/image_raw
/stereo/left/camera_info
/stereo/right/image_raw
/stereo/right/camera_info
```

启动处理链：

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

## 3. 第一项运行检查

```bash
ros2 topic hz /stereo/left/image_raw
ros2 topic hz /stereo/right/image_raw
ros2 topic echo --once /stereo/disparity
ros2 topic info /stereo/points2
```

左右原始图像应都接近 30 Hz。`disparity` 必须包含正的有效视差区域；如果全为无效值，优先检查相机基线、右相机 `CameraInfo.P[3]`、相机光轴方向和时间戳。

## 目前边界

本仓库还没有加入 Gazebo 双相机模型。原因是 ROS 2 Humble 的 Gazebo Classic 安装中是否含 `libgazebo_ros_multicamera.so` 必须由第 1 步实际探测决定。探测结果出来后，再添加匹配你本机插件的 SDF 模型与启动文件，避免把不适配的 ROS 1 插件配置写进你的工作区。
