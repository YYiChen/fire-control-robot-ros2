## piper_control 功能包说明

这是一个用于控制 Piper 机械臂的 ROS 2 功能包，基于 MoveIt 进行运动规划和控制，提供机械臂和夹爪的完整控制功能。

### 功能概述

该功能包提供了一个名为 execute_arm_task 的节点，实现了机械臂的路径规划、运动控制和夹爪控制功能。通过 Action 和 Service 接口，可以控制机械臂执行复杂的运动任务和夹爪操作。

### 主要功能

1. **机械臂运动控制**
   - 通过路径点控制机械臂运动
   - 关节角度控制
   - 机械臂归零功能
   - 运动稳定性检测

2. **夹爪控制**
   - 开合控制
   - 命名位置控制
   - 关节角度精确控制

3. **任务管理**
   - Action 服务器实现异步任务处理
   - 任务取消支持
   - 实时反馈机制

### ROS 2 接口

#### Action 接口
- /execute_arm_task common_interfaces/action/ExecuteArmTask
  - 用于执行机械臂任务的 Action 服务器
  - 支持路径点控制和关节角度控制两种模式
  - 提供任务执行反馈和结果

#### Service 接口
- `control_gripper` common_interfaces/srv/ControlGripper
  - 用于控制夹爪的服务接口
  - 支持多种控制方式（开合、命名位置、关节角度）

#### 消息类型
- common_interfaces/action/ExecuteArmTask
  - Goal: 包含路径点列表或关节控制信息
  - Feedback: 包含当前执行进度信息
  - Result: 包含任务执行结果

- common_interfaces/srv/ControlGripper
  - Request: 包含夹爪控制参数
  - Response: 包含控制结果

### 运动控制策略

1. **路径规划重试机制**
   - 规划失败时自动重试最多3次
   - 包含适当的延迟避免连续失败

2. **运动执行模式**
   - 提供异步和同步两种执行方式
   - 推荐使用改进的同步执行方式（包含四元数规范化）

3. **稳定性检测**
   - 多种稳定性检测方法可选
   - 基于末端执行器位置和姿态变化的检测（推荐）

### 夹爪控制方式

1. **命名位置控制**
   - 支持 "open" 和 "close" 命名位置
   - 自动回退到关节角度控制

2. **关节角度控制**
   - 精确控制夹爪关节角度
   - 执行后验证当前位置

3. **开合控制**
   - 简单的布尔值控制开合状态


### 使用说明

#### 启动节点
```bash
ros2 run piper_control execute_arm_task
```

#### 机械臂控制示例

1. **路径点控制**
```bash
# 发送路径点控制任务
ros2 action send_goal /execute_arm_task common_interfaces/action/ExecuteArmTask \
  "{waypoints: [{header: {frame_id: 'base_link'}, pose: {position: {x: 0.3, y: 0.0, z: 0.2}, orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}}}]}"

```

2. **关节角度控制**
```bash
# 控制特定关节旋转
ros2 action send_goal /execute_arm_task common_interfaces/action/ExecuteArmTask \
  "{joint_name: ['joint1'], target_angle: [45.0]}"

# 所有关节归零
ros2 action send_goal /execute_arm_task common_interfaces/action/ExecuteArmTask \
  "{joint_name: ['all'], target_angle: [0.0]}"
```

#### 夹爪控制示例

1. **开合控制**
```bash
# 打开夹爪
ros2 service call /control_gripper common_interfaces/srv/ControlGripper "{open: true}"

# 关闭夹爪
ros2 service call /control_gripper common_interfaces/srv/ControlGripper "{open: false}"
```

2. **命名位置控制**
```bash
# 使用命名位置
ros2 service call /control_gripper common_interfaces/srv/ControlGripper "{named_position: 'open'}"
```

3. **关节角度控制**
```bash
# 精确控制关节角度
ros2 service call /control_gripper common_interfaces/srv/ControlGripper "{joint_positions: [0.0]}"
```

### 注意事项

1. **MoveIt 配置**
   - 需要正确配置 MoveIt 规划组（"arm" 和 "gripper"）
   - 确保 SRDF 文件中定义了正确的关节和链接

2. **坐标系**
   - 路径点控制使用标准的 ROS 坐标系
   - 默认规划框架在初始化时会打印到日志

3. **错误处理**
   - 包含完整的错误处理和重试机制
   - 对于不可恢复的错误会记录详细日志

4. **并发控制**
   - 使用互斥锁防止多个任务同时执行
   - 支持任务取消功能
