# 启动文件说明

## 1. piper_perception_launch.py

这是一个用于ArUco标记检测和定位的启动文件。

**主要功能：**
- 启动 piper_aruco_localization 包中的 single_shot_detector 节点
- 配置ArUco标记检测参数，包括：
  - 标记ID（默认582）
  - 标记大小（默认0.04米）
  - 角点优化方法（默认LINES，可选NONE, HARRIS, LINES, SUBPIX）
  - 参数预设（默认robust，可选high_precision, custom）
- 加载手眼标定参数文件和ArUco面板配置文件

**依赖包：**
- piper_aruco_localization

## 2. hk_camera_test.launch.py

这是一个用于测试海康威视摄像头的启动文件。

**主要功能：**
- 启动 video_manager 节点
- 配置摄像头参数：
  - 加载摄像头标定文件（hk_ost.txt）
  - 设置图像是否已去畸变（默认false）
  - 发布话题名：`/hk_cam/image_raw`

**节点说明：**
- image_server：图像服务器节点，负责从摄像头获取图像数据
- image_pub：图像发布节点（当前被注释掉）

**依赖包：**
- video_manager

## 3. fia_robot_test.launch.py

这是整个机器人系统的主启动文件，集成了多个子系统。

**主要功能：**
- 启动 Piper 机械臂及其MoveIt控制模块
- 启动视觉系统（包括摄像头和ArUco标记检测）
- 启动任务管理器和执行器
- 启动消防面板识别系统
- 启动升降控制器
- 启动ROS桥接WebSocket服务器

**包含的主要节点和子系统：**
- start_single_piper.launch.py：启动Piper机械臂基础功能
- production.launch.py：启动Piper MoveIt生产环境
- execute_arm_task：机械臂任务执行节点
- image_server：图像服务器节点
- piper_perception_launch.py：ArUco标记感知系统
- lift_controller_node：升降控制器节点
- rosbridge_websocket：WebSocket通信桥接
- task_manager：任务管理器节点（延迟1秒启动）
- single_shot_rec：消防面板单次识别程序

**依赖包：**
- piper
- piper_with_gripper_moveit
- piper_control
- video_manager
- fia_launch
- fire_panel_rec
- lift_control
- rosbridge_server
- task_manager
## 4. ocr_rec_test.launch.py

这是一个专门用于测试OCR（光学字符识别）功能的启动文件。

**主要功能：**
- 运行 fire_panel_rec 包中的 rec_demo 程序
- 配置OCR识别参数：
  - 输入图像文件路径
  - 检测模型路径（ch_PP-OCRv3_det_infer）
  - 分类模型路径（ch_ppocr_mobile_v2.0_cls_infer）
  - 识别模型路径（ch_PP-OCRv3_rec_infer）
  - 标签文件路径（ppocr_keys_v1.txt）

**应用场景：**
- 消防面板文字识别测试
- OCR识别功能验证

**依赖包：**
- fire_panel_rec