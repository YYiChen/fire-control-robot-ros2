## fire_panel_rec 功能包

这是一个专门用于消防面板识别的功能包，包含OCR文字识别和LED状态检测功能。

### 主要功能组件

#### 1. fire_panel_rec 节点
**功能：**
- 实时处理RTSP视频流
- 持续识别消防面板屏幕信息
- 发布面板状态信息到ROS2话题

**主要特性：**
- 多线程架构（捕获、处理、发布线程）
- 蓝色区域提取（识别消防面板显示区域）
- OCR文字识别与解析
- 持续状态信息发布

**依赖项：**
- PaddleOCR：文字识别引擎
- OpenCV：图像处理
- [video_manager](file:///home/nemo/ros2_projects/fia_bot_ws/install/video_manager/share/colcon-core/packages/video_manager)：视频管理

#### 2. rec_demo 节点
**功能：**
- 处理单张图片的OCR识别
- 用于测试和演示OCR功能

**使用方式：**
- 命令行参数指定输入图片和模型路径
- 适用于离线测试场景

#### 3. single_shot_rec 节点
**功能：**
- 提供服务接口的单次识别节点
- 可识别屏幕区域和LED区域
- 支持相机参数调整

**主要服务：**
- `/get_panel_info`：获取面板信息服务
- `/set_camera_params`：设置相机参数服务
- `/trigger_image`：触发图像捕获服务

**识别功能：**
- 屏幕区域OCR识别：提取面板数字信息（报警数量、时间等）
- LED区域状态识别：检测指示灯颜色状态（红、绿、黄、灭）

### 核心源文件说明

#### parse_screen.cpp
**功能：**
- 解析OCR识别结果
- 提取消防面板关键信息

**主要功能：**
- 文本清理和格式化
- 按行分组OCR结果
- 解析报警信息（首火、火警等）
- 提取状态标志（监管、故障、屏蔽等）
- 计算编辑距离进行文本匹配容错

#### parse_led_region.cpp
**功能：**
- 识别面板LED指示灯状态

**主要功能：**
- 图像预处理（CLAHE增强、反光处理）
- OCR文本过滤和验证
- LED区域定位
- 多颜色LED状态分析（HSV色彩空间）
- 多帧分析提高识别准确性
- 文本与LED位置关联

#### single_shot_rec_node.cpp
**功能：**
- 提供服务接口的识别节点主程序

**主要特性：**
- 服务端实现 `/get_panel_info` 接口
- 客户端调用图像捕获和相机控制服务
- 支持屏幕和LED区域识别
- 相机参数动态调整
- 多帧LED识别提高准确性


### 接口定义

**发布的话题：**
- `/fire_alarm/status`：面板状态信息（ScreenInfo消息）

**提供的服务：**
- `get_panel_info`：获取面板信息（GetPanelInfo服务）

**使用的服务：**
- `trigger_image`：触发图像捕获
- `set_camera_params`：设置相机参数
