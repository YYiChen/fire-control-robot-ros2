## task_manager 功能包说明

这是一个用于管理机器人任务执行的核心功能包，负责协调机械臂、升降台、视觉系统等硬件设备，执行复杂的自动化任务序列。

### 功能概述

该功能包提供了一个名为 task_manager 的节点，用于解析任务配置文件并执行预定义的任务序列。它采用模块化设计，将任务管理、硬件控制、状态管理等功能分离到不同的组件中。

### 核心功能

1. **任务配置管理**
   - 从YAML文件加载任务配置
   - 解析全局配置和任务点位信息
   - 支持多种任务类型配置

2. **任务执行管理**
   - 基于Action的服务接口
   - 支持任务取消和进度反馈
   - 统一的错误处理机制

3. **硬件设备控制**
   - 机械臂控制（位置和关节角度）
   - 升降台高度控制
   - 夹爪控制
   - 相机参数设置
   - 面板信息获取

4. **状态监控**
   - 实时发布任务状态
   - 屏幕信息监控
   - 任务进度反馈

### 系统架构

#### 主要组件

1. **TaskManager** - 主节点类
   - 负责初始化整个系统
   - 管理Action服务接口
   - 协调各子模块工作

2. **ConfigurationManager** - 配置管理器
   - 加载和解析YAML配置文件
   - 提供配置数据访问接口

3. **TaskExecutor** - 任务执行器
   - 解析任务配置并创建执行步骤
   - 执行具体的任务逻辑
   - 提供进度反馈和错误处理

4. **HardwareController** - 硬件控制器
   - 与各种硬件设备通信
   - 提供统一的硬件控制接口

5. **StateManager** - 状态管理器
   - 管理和发布系统状态
   - 处理状态更新和变化检测

### 支持的任务类型

1. **设备初始化** (`initialize_system`)
   - 机械臂回到起始位置
   - 升降台设置默认高度

2. **报警处理** (`handle_alarm`)
   - 检测屏幕报警状态
   - 执行消音和重置操作
   - 支持密码输入

3. **日常巡检** (`routine_patrol`)
   - 指示灯区域自检
   - 设备状态检查

4. **键允许操作** (`key_enable`)
   - 检测键允许状态
   - 执行钥匙旋转操作

5. **ArUco定位** (`aruco_localization`)
   - 机械臂移动到拍摄位置
   - 获取面板区域位姿
   - 精确定位

6. **升降台控制** lift_control
   - 设置升降台目标高度
   - 等待到达目标位置

7. **微调按键位置** (`adjust_button_position`)
   - 获取指定按钮位姿
   - 精确点击按钮

### ROS 2 接口

#### Action服务
- `/execute_js_task` TaskJsAction
  - 用于执行复杂的任务序列
  - 支持进度反馈和取消操作

#### 话题发布
- `/task/status` TaskStatus
  - 发布当前任务状态信息
- `/screen/info` ScreenInfo
  - 发布屏幕信息

#### 服务客户端
- `set_camera_params` SetCameraParamsSrv
  - 设置相机参数
- `lift/command` LiftCommandSrv
  - 控制升降台
- `get_region_pose` GetPanelRegionPoseSrv
  - 获取面板区域位姿
- `get_panel_info` GetPanelInfoSrv
  - 获取面板信息
- `control_gripper` ControlGripperSrv
  - 控制夹爪
- /execute_arm_task ArmControlAction
  - 控制机械臂

#### 话题订阅
- `lift/status` LiftStatus
  - 订阅升降台状态信息


### 核心类详解

#### ConfigurationManager
负责加载和管理任务配置文件：
- 支持YAML格式配置文件解析
- 提供配置数据访问接口
- 包含全局配置和任务点位信息

#### TaskExecutor
任务执行的核心类：
- 根据配置创建任务执行步骤
- 实现各种具体任务逻辑
- 提供统一的进度反馈和错误处理
- 支持任务取消检查

#### HardwareController
硬件设备控制接口：
- 封装各种硬件服务调用
- 提供同步的硬件控制方法
- 处理硬件状态订阅和更新

#### StateManager
状态管理器：
- 管理任务状态和屏幕信息
- 发布状态更新到相关话题
- 检测状态变化并触发更新

### 使用说明

#### 启动节点
```bash
ros2 run task_manager task_manager --ros-args -p task_config_path:=/path/to/config.yaml
```

#### 执行任务
```bash
# 发送任务请求
ros2 action send_goal /execute_js_task common_interfaces/action/TaskJsAction "{task_cfg: 'task_config'}"
```

#### 监控状态
```bash
# 查看任务状态
ros2 topic echo /task/status

# 查看屏幕信息
ros2 topic echo /screen/info
```


### 注意事项

1. **配置文件**
   - 确保配置文件路径正确且可访问
   - 配置文件格式必须符合YAML规范

2. **硬件依赖**
   - 需要相关硬件服务节点正常运行
   - 确保网络通信正常

3. **错误处理**
   - 系统具有完善的错误处理机制
   - 任务失败时会尝试恢复到安全状态

4. **性能考虑**
   - 使用多线程执行器提高响应性
   - 合理设置超时和重试机制
