## lift_control 功能包说明

这是一个用于控制升降设备的ROS 2功能包，通过串口通信与升降控制器硬件进行交互。

### 功能概述

该功能包提供了一个名为 lift_controller_node 的节点，用于控制和监控升降设备。它通过Modbus RTU协议与升降控制器通信，支持多种控制命令和状态查询功能。

### 主要功能

1. **串口通信管理**
   - 自动检测可用的串口设备
   - 支持配置串口参数（默认波特率9600）
   - 多线程串口数据读取和处理

2. **升降设备控制**
   - 停止(STOP)
   - 上升(UP)
   - 下降(DOWN)
   - 复位(RESET)
   - 设置目标高度(SET_HEIGHT)
   - 步进上升(STEP_UP)
   - 步进下降(STEP_DOWN)
   - 设置设备ID(SET_DEVICE_ID)

3. **状态监控**
   - 定时查询升降设备当前位置
   - 发布设备状态信息
   - 支持手动查询设备信息

### 通信协议

使用Modbus RTU协议与设备通信，支持以下功能码：
- 功能码03：读取保持寄存器（用于查询状态）
- 功能码06：写单个保持寄存器（用于发送控制命令）

### ROS 2 接口

#### 服务 (Services)
- `lift/command` common_interfaces/srv/LiftCommand
  - 用于发送控制命令到升降设备
  - 支持多种命令类型（停止、上升、下降、设置高度等）

#### 消息 (Messages)
- `lift/status` common_interfaces/msg/LiftStatus
  - 发布升降设备的当前状态，包括各电机位置和设备ID
- `lift/query` common_interfaces/msg/LiftQuery
  - 用于查询升降设备信息（位置、设备ID等）

#### 参数 (Parameters)
- `port` (string, 默认: "/dev/ttyUSB0")
  - 串口设备路径
- `timeout_ms` (int, 默认: 500)
  - 串口通信超时时间（毫秒）
- `default_device_id` (int, 默认: 1)
  - 设备默认ID
- `step_size` (int, 默认: 1)
  - 步进控制的步长（毫米）


### 使用说明

1. **硬件连接**
   - 将升降控制器通过USB转串口设备连接到计算机
   - 确保串口设备权限正确（通常需要添加用户到`dialout`组）

2. **运行节点**
   ```bash
   ros2 run lift_control lift_controller_node
   ```

3. **发送控制命令**
   ```bash
   # 停止升降设备
   ros2 service call /lift/command common_interfaces/srv/LiftCommand "{command_type: 1}"
   
   # 设置目标高度为100mm
   ros2 service call /lift/command common_interfaces/srv/LiftCommand "{command_type: 5, target_height: 100}"
   ```

4. **查看状态**
   ```bash
   # 订阅状态信息
   ros2 topic echo /lift/status
   ```

### 注意事项

1. **安全考虑**
   - 在发送控制命令前，建议先确认当前设备状态
   - 避免频繁发送命令，给设备留出响应时间

2. **硬件兼容性**
   - 当前实现针对特定的升降控制器协议
   - 如需适配其他设备，可能需要调整通信协议部分

3. **错误处理**
   - 节点包含基本的错误处理机制，如串口打开失败会终止节点
   - 对于通信错误，节点会记录日志并继续运行
