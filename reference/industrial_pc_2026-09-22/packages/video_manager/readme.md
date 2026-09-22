## video_manager 功能包说明

这是一个专门用于视频管理和相机控制的ROS 2功能包，主要支持海康威视工业相机，并提供图像采集、发布和相机控制功能。

### 功能概述

该功能包提供了完整的相机管理解决方案，包括实时图像采集、图像发布、相机参数控制和相机信息服务。它支持海康威视工业相机，通过MVS SDK与相机通信，并提供ROS 2接口供其他节点使用。

### 主要功能

1. **海康威视相机支持**
   - 支持USB和GigE接口的海康威视工业相机
   - 自动设备枚举和选择
   - 相机参数配置（曝光、增益等）

2. **图像采集与发布**
   - 实时图像采集
   - 图像畸变矫正
   - 连续图像发布模式
   - 按需图像抓取模式

3. **相机控制服务**
   - 相机参数设置服务
   - 图像触发抓取服务
   - 相机信息查询服务

4. **相机标定支持**
   - 支持从OST文件加载相机标定参数
   - 图像畸变矫正功能

#### ImagePub 节点
图像发布节点，持续采集并发布图像数据：

**主要功能：**
- 连续采集图像并发布到指定话题
- 发布相机信息(camera_info)
- 支持图像畸变矫正
- 从OST文件加载相机标定参数

**发布的消息：**
- `/hk_cam/image_raw`: 原始图像数据 sensor_msgs/msg/Image
- `/hk_cam/camera_info`: 相机标定信息 sensor_msgs/msg/CameraInfo

#### ImageServer 节点
图像服务节点，按需提供图像数据：

**主要功能：**
- 提供按需图像抓取服务
- 提供相机参数设置服务
- 提供相机信息查询服务
- 支持图像畸变矫正

**提供的服务：**
- `/trigger_image`: 触发图像抓取服务 common_interfaces/srv/TriggerImageCapture
- `/set_camera_params`: 设置相机参数服务 common_interfaces/srv/SetCameraParams
- `/get_camera_info`: 获取相机信息服务 common_interfaces/srv/GetCameraInfo

### ROS 2 接口

#### 服务 (Services)

1. **TriggerImageCapture** (`/trigger_image`)
   - 按需触发图像采集
   - 支持实时畸变矫正选项
   - 返回采集到的图像数据

2. **SetCameraParams** (`/set_camera_params`)
   - 设置相机曝光和增益参数
   - 支持自动/手动模式切换
   - 可设置参数上下限

3. **GetCameraInfo** (`/get_camera_info`)
   - 查询相机标定信息
   - 返回完整的相机内参和畸变参数

#### 消息 (Messages)

1. **Image** (`/hk_cam/image_raw`)
   - 发布实时图像数据
   - 包含时间戳和帧ID信息

2. **CameraInfo** (`/hk_cam/camera_info`)
   - 发布相机标定参数
   - 包含内参矩阵、畸变系数等

#### 参数 (Parameters)

### 使用说明

#### 启动图像发布节点
```bash
ros2 run video_manager image_pub --ros-args -p camera_device_id:="DEVICE_ID"
```

#### 启动图像服务节点
```bash
ros2 run video_manager image_server --ros-args -p camera_device_id:="DEVICE_ID"
```

#### 控制相机参数
```bash
# 设置手动曝光和增益
ros2 service call /set_camera_params common_interfaces/srv/SetCameraParams \
  "{auto_exposure_enabled: false, manual_exposure_time: 5000.0, \
    auto_gain_enabled: false, manual_gain_value: 8.0}"
```

#### 获取图像
```bash
# 触发单次图像采集
ros2 service call /trigger_image common_interfaces/srv/TriggerImageCapture \
  "{is_rectified: true}"
```

#### 查看相机信息
```bash
# 获取相机标定信息
ros2 service call /get_camera_info common_interfaces/srv/GetCameraInfo "{}"
```

### 注意事项

1. **硬件要求**
   - 需要安装海康威视MVS SDK
   - 相机需要正确连接并供电
   - 确保用户有访问相机设备的权限

2. **标定文件**
   - OST格式的相机标定文件需要正确配置
   - 文件路径需要在参数中正确指定

3. **性能考虑**
   - 图像采集线程独立运行，避免阻塞主线程
   - 支持多线程安全的图像缓冲区访问

4. **错误处理**
   - 包含完整的错误处理机制
   - 详细日志输出便于调试
