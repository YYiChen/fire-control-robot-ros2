## piper_aruco_localization 功能包说明

这是一个用于ArUco标记检测和机器人定位的ROS 2功能包，主要用于通过视觉识别ArUco标记来确定机器人末端执行器或其他物体的精确位置。

### 功能概述

该功能包提供了一个名为 single_shot_detector 的节点，用于检测ArUco标记并计算机器人末端执行器的精确位置。它通过相机捕获图像，检测ArUco标记，并结合手眼标定参数计算物体在机器人坐标系中的位置。

### 主要功能

1. **ArUco标记检测**
   - 支持多种检测模式和参数预设（鲁棒性、高精度等）
   - 支持主标记和辅助标记的检测
   - 多次采样以提高检测精度

2. **坐标变换计算**
   - 结合手眼标定参数进行坐标变换
   - 支持从相机坐标系到机器人基坐标系的转换
   - 计算面板上各个区域的精确位置

3. **面板区域定位**
   - 支持定义面板上多个区域的位置
   - 基于主标记位置计算各区域在机器人坐标系中的位置
   - 支持主辅标记配合使用以提高定位精度

4. **异常值过滤**
   - 使用中位数绝对偏差(MAD)算法过滤异常检测结果
   - 提高定位结果的稳定性和准确性


### ROS 2 接口

#### 服务 (Services)
- `get_region_pose` common_interfaces/srv/GetPanelRegionPose
  - 用于获取面板特定区域的位姿
  - 支持获取主辅标记中心位置等特殊区域

#### 客户端服务 (Client Services)
- `/trigger_image` common_interfaces/srv/TriggerImageCapture
  - 触发图像捕获服务
- `/get_camera_info` common_interfaces/srv/GetCameraInfo
  - 获取相机信息的服务

#### 参数 (Parameters)
- `aruco_panel_path` (string)
  - ArUco面板配置文件路径
- `marker_size` (double, 默认: 0.05)
  - ArUco标记的物理尺寸（米）
- `marker_id` (int, 默认: 300)
  - 主ArUco标记ID
- `arm_eih_path` (string)
  - 手眼标定参数文件路径
- `image_is_rectified` (bool, 默认: true)
  - 图像是否已经过矫正
- `parameter_preset` (string, 默认: "robust")
  - 参数预设: "robust", "high_precision", "custom"
- `sample_count` (int, 默认: 5)
  - 采样次数
- `sample_delay_ms` (double, 默认: 100.0)
  - 采样间隔（毫秒）

### 配置文件

#### 手眼标定文件 (YAML格式)
包含相机与机器人末端执行器之间的标定参数，定义坐标系关系和变换矩阵。

#### 面板配置文件 (YAML格式)
定义面板上各个区域的相对位置关系，包括：
- 主标记和辅助标记ID
- 各区域相对于标记的偏移量
- 区域类型定义

### 工作流程

1. **初始化阶段**
   - 加载手眼标定参数
   - 加载面板区域配置
   - 初始化ArUco检测器和参数

2. **检测阶段**
   - 通过服务调用获取图像和相机信息
   - 多次采样检测ArUco标记
   - 计算标记在机器人坐标系中的位置

3. **处理阶段**
   - 异常值过滤
   - 计算平均位姿
   - 基于主标记位置计算各区域位姿

4. **输出阶段**
   - 通过服务返回请求区域的位姿
   - 发布TF变换


### 使用说明

1. **启动节点**
   ```bash
   ros2 run piper_aruco_localization single_shot_detector
   ```

2. **调用服务获取区域位姿**
   ```bash
   # 获取特定区域的位姿
   ros2 service call /get_region_pose common_interfaces/srv/GetPanelRegionPose "{panel_region: {region_id: 1}}"
   ```

3. **动态参数调整**
   ```bash
   # 调整参数预设
   ros2 param set /single_shot_detector parameter_preset "high_precision"
   ```

### 注意事项

1. **性能考虑**
   - 多次采样会增加检测时间，可根据实际需求调整采样次数
   - 不同参数预设会影响检测精度和速度

2. **标定要求**
   - 需要准确的手眼标定参数以确保定位精度
   - 相机标定质量直接影响检测效果

3. **环境要求**
   - 需要良好的光照条件以确保标记检测准确性
   - 标记表面应保持清洁，避免反光或遮挡
