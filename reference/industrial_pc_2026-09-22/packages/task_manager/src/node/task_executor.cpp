#include "task_executor.hpp"
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <thread>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <map>

namespace task_manager {

TaskExecutor::TaskExecutor(
    std::shared_ptr<HardwareController> hardware_controller,
    const GlobalConfig& global_config,
    const std::vector<TaskPoint>& task_points,
    rclcpp::Node::SharedPtr node)
    : hardware_controller_(hardware_controller),
      global_config_(global_config),
      task_points_(task_points),
      node_(node)
{
}

bool TaskExecutor::executeTaskSequence(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle)
{
    if (!goal_handle) {
        RCLCPP_ERROR(node_->get_logger(), "无效的目标句柄");
        return false;
    }

    // 初始化反馈和结果
    auto feedback = std::make_shared<TaskJsAction::Feedback>();
    auto result = std::make_shared<TaskJsAction::Result>();
    
    try {
        // 获取任务步骤
        auto task_steps = createConfigurableTaskSteps(goal_handle);
        int total_steps = task_steps.size();

        if (total_steps == 0) {
            RCLCPP_WARN(node_->get_logger(), "没有任务步骤需要执行");
            result->result_code = 0;
            goal_handle->succeed(result);
            return true;
        }

        // 执行每个任务步骤
        for (int i = 0; i < total_steps; ++i) {
            // 检查取消
            if (checkCancel(goal_handle, result)) {
                return false;
            }
            
            // 执行步骤
            if (!task_steps[i]()) {
                RCLCPP_ERROR(node_->get_logger(), "任务步骤 %d 执行失败", i+1);
                result->result_code = -3 - i; // 不同的错误码
                // 任务失败处理
                handleTaskFailure(goal_handle, "任务步骤 " + std::to_string(i+1) + " 执行失败");
                goal_handle->abort(result);
                return false;
            }
            // 延时
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 更新进度
            updateProgress(goal_handle, i+1, total_steps, feedback);
        }
        
        // 任务完成
        result->result_code = 0;
        goal_handle->succeed(result);
        RCLCPP_INFO(node_->get_logger(), "任务完成");
        return true;
        
    } catch (const std::exception& e) {
        RCLCPP_ERROR(node_->get_logger(), "执行任务时发生异常: %s", e.what());
        result->result_code = -100; // 异常错误码
        goal_handle->abort(result);
        return false;
    }
}

// 创建基于配置的任务步骤
std::vector<std::function<bool()>> TaskExecutor::createConfigurableTaskSteps(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle) 
{
    std::vector<std::function<bool()>> task_steps;
    
    if (task_points_.empty()) {
        RCLCPP_WARN(node_->get_logger(), "任务点列表为空，无法创建任务步骤");
        return task_steps;
    }
    // TODO: 根据实际需要获取任务配置,需要解析task_cfg字符串
    // facp_model_ = goal_handle->get_goal()->task_cfg;
    
    const auto& task_point = task_points_[0];
    
    for (const auto& step_config : task_point.task_sequence) {
        if (step_config.task_type == "lift_control") {
            task_steps.push_back(
                [this, goal_handle, step_config]() {
                    return executeLiftControl(
                        goal_handle, 
                        step_config.getIntParam("lift_height")
                    );
                });
        }
        else if (step_config.task_type == "aruco_localization") {
            task_steps.push_back(
                [this, goal_handle, step_config]() {
                    return executeArucoLocalization(goal_handle, step_config);
                });
        }
        else if (step_config.task_type == "initialize_system") {
            task_steps.push_back([this, goal_handle]() {
                return executeInitializeSystem(goal_handle);
            });
        }
        else if (step_config.task_type == "handle_alarm") {
            task_steps.push_back([this, goal_handle, step_config]() {
                return executeHandleAlarm(goal_handle, step_config);
            });
        }
        else if (step_config.task_type == "routine_patrol") {
            task_steps.push_back([this, goal_handle, step_config]() {
                return executeRoutinePatrol(goal_handle, step_config);
            });
        }
        else if (step_config.task_type == "key_enable") {
            task_steps.push_back([this, goal_handle, step_config]() {
                return executeKeyEnableTask(goal_handle, step_config);
            });
        }
        else if (step_config.task_type == "adjust_button_position") {
            task_steps.push_back(
                [this, goal_handle, step_config]() {
                    return executeAdjustButtonPositionTask(goal_handle, step_config);
                });
        }
        else {
            RCLCPP_WARN(node_->get_logger(), "未知的任务类型: %s", step_config.task_type.c_str());
        }
    }
    
    return task_steps;
}

// 设备初始化
bool TaskExecutor::executeInitializeSystem(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle)
{
    RCLCPP_INFO(node_->get_logger(), "执行设备初始化任务");
    
    updateTaskStatus(TaskStatus::NAVIGATION, TaskStatus::NAVIGATION_TO_TASK, TaskStatus::ACTIVE);

    // if (!executeArmControl(goal_handle, global_config_.arm_start_pose)) {
    //     RCLCPP_ERROR(node_->get_logger(), "机械臂初始化失败");
    //     updateTaskStatus(TaskStatus::NAVIGATION, TaskStatus::NAVIGATION_TO_TASK, TaskStatus::FAILED, "机械臂初始化失败");
    //     return false;
    // }
    if(!executeJointAngleControl(goal_handle, {"all"}, {0.0})) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂初始化失败");
        updateTaskStatus(TaskStatus::NAVIGATION, TaskStatus::NAVIGATION_TO_TASK, TaskStatus::FAILED, "机械臂初始化失败");
        return false;
    }

    // if (!executeLiftControl(goal_handle, global_config_.default_lift_height)) {
    //     RCLCPP_ERROR(node_->get_logger(), "升降台初始化失败");
    //     updateTaskStatus(TaskStatus::NAVIGATION, TaskStatus::NAVIGATION_TO_TASK, TaskStatus::FAILED, "升降台初始化失败");
    //     return false;
    // }
    

    RCLCPP_INFO(node_->get_logger(), "设备初始化完成");
    updateTaskStatus(TaskStatus::NAVIGATION, TaskStatus::NAVIGATION_TO_TASK, TaskStatus::SUCCEEDED);
    return true;
}

// 报警处理任务函数
bool TaskExecutor::executeHandleAlarm(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行报警处理任务");

    // 1： 优先设置相机参数
    if (!hardware_controller_->setCameraParamsForScreen())
    {
        RCLCPP_ERROR(node_->get_logger(), "设置相机参数失败");
        return false;
    }

    // 更新任务状态
    updateTaskStatus(TaskStatus::ALARM_HANDLING, 0, TaskStatus::ACTIVE);

    // 延时1秒
    rclcpp::sleep_for(std::chrono::seconds(1));
    
    // 获取屏幕区域信息
    if (!executeScreenInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取屏幕区域信息失败");
        updateTaskStatus(
            TaskStatus::ALARM_HANDLING, 
            0, 
            TaskStatus::FAILED, 
            "获取屏幕区域信息失败");
        return false;
    }
    
    // 判断屏幕区域是否处于报警状态
    if (cur_screen_info_.current_page == ScreenInfo::ALERT_PAGE) 
    {
        RCLCPP_INFO(node_->get_logger(), "屏幕显示处于报警状态");
        
        // 执行消音操作
        updateTaskStatus(
            TaskStatus::ALARM_HANDLING, 
            TaskStatus::ALARM_HANDLING_SILENCING, 
            TaskStatus::ACTIVE);
        
        if (!executeMuteAlarmTask(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "消音操作失败");
            updateTaskStatus(
                TaskStatus::ALARM_HANDLING, 
                TaskStatus::ALARM_HANDLING_SILENCING, 
                TaskStatus::FAILED, "消音操作失败");
            return false;
        }

        // 执行重置操作
        updateTaskStatus(
            TaskStatus::ALARM_HANDLING, 
            TaskStatus::ALARM_HANDLING_RESETTING, 
            TaskStatus::ACTIVE);
        
        if (!executeResetAlarmTask(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "重置操作失败");
            updateTaskStatus(
                TaskStatus::ALARM_HANDLING, 
                TaskStatus::ALARM_HANDLING_RESETTING, 
                TaskStatus::FAILED, "重置操作失败");
            return false;
        }
    }

    // 任务成功完成
    updateTaskStatus(TaskStatus::ALARM_HANDLING, 0, TaskStatus::SUCCEEDED);
    return true;
}

// 日常巡检任务函数（已按要求完整修改）
bool TaskExecutor::executeRoutinePatrol(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                                       const TaskStep& step_config)
{
    // 1： 优先设置相机参数（与handle_alarm一致，确保指示灯识别清晰）
    // if (!hardware_controller_->setCameraParamsForScreen())
    // {
    //     RCLCPP_ERROR(node_->get_logger(), "设置相机参数失败");
    //     return false;
    // }
    // // 延时1秒，确保相机参数生效
    // rclcpp::sleep_for(std::chrono::seconds(1));



    RCLCPP_INFO(node_->get_logger(), "指示灯区域自检任务");
    
    if (!executeIndicatorInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取指示灯区域信息失败");
        return false;
    }

    // ========== 核心逻辑：仅聚焦火警灯，严格执行 火警灯亮→消音→复位→验证 流程 ==========
    const std::string FIRE_ALARM_LED_NAME = "火警";
    const uint8_t LED_STATE_OFF = 0;    // 熄灭（正常状态）
    const uint8_t LED_STATE_RED = 1;    // 红灯亮起（报警状态）
    const uint8_t LED_STATE_UNKNOWN = 4;// 状态未知

    bool fire_alarm_red_on = false;
    bool has_unknown_state = false;

    // 初始自检：仅判断火警灯状态，完全忽略消音灯及其他无关指示灯
    RCLCPP_INFO(node_->get_logger(), "========== 初始指示灯自检详情 ==========");
    for (const auto& led : cur_indicator_lights_) {
        RCLCPP_INFO(node_->get_logger(), "指示灯 - 名称：%s，状态：%d",
                    led.name.c_str(), (int)led.state);

        if (led.name == FIRE_ALARM_LED_NAME) {
            if (led.state == LED_STATE_RED) {
                fire_alarm_red_on = true;
                RCLCPP_WARN(node_->get_logger(), "自检发现：火警灯（%s）红灯亮起，立即执行消音→复位操作", led.name.c_str());
            } else if (led.state == LED_STATE_OFF) {
                RCLCPP_INFO(node_->get_logger(), "自检正常：火警灯（%s）已熄灭，无需执行任何操作", led.name.c_str());
            } else if (led.state == LED_STATE_UNKNOWN) {
                has_unknown_state = true;
                RCLCPP_WARN(node_->get_logger(), "自检发现：火警灯（%s）状态未知", led.name.c_str());
            }
        }

        // 其他指示灯未知状态仅日志提示，不影响核心流程
        if (led.state == LED_STATE_UNKNOWN && led.name != FIRE_ALARM_LED_NAME) {
            has_unknown_state = true;
            RCLCPP_WARN(node_->get_logger(), "自检发现：指示灯（%s）状态未知", led.name.c_str());
        }
    }
    RCLCPP_INFO(node_->get_logger(), "========== 初始自检完成 ==========");

    // 火警灯红灯亮起时，严格按 消音→复位→重新验证 顺序执行
    if (fire_alarm_red_on) {
        // 第一步：立即执行消音操作（解决火警报警声，无前置条件）
        RCLCPP_INFO(node_->get_logger(), "开始执行第一步：消音操作");
        if (!executeMuteAlarmTask(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "消音操作执行失败，任务终止");
            return false;
        }
        RCLCPP_INFO(node_->get_logger(), "消音操作执行成功");

        // 第二步：消音完成后，立即执行复位操作（无间隔衔接）
        RCLCPP_INFO(node_->get_logger(), "开始执行第二步：复位操作");
        if (!executeResetAlarmTask(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "复位操作执行失败，任务终止");
            return false;
        }
        RCLCPP_INFO(node_->get_logger(), "复位操作执行成功");

        // 第三步：复位完成后，重新获取指示灯状态，验证火警灯是否熄灭
        RCLCPP_INFO(node_->get_logger(), "复位完成，重新获取指示灯状态进行验证");
        if (!executeIndicatorInfo(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "重新获取指示灯区域信息失败，无法验证复位效果，任务终止");
            return false;
        }

        // 重新遍历指示灯，仅检查火警灯状态
        fire_alarm_red_on = false; // 重置标记，重新判断
        // RCLCPP_INFO(node_->get_logger(), "========== 复位后指示灯验证详情 ==========");
        // for (const auto& led : cur_indicator_lights_) {
        //     RCLCPP_INFO(node_->get_logger(), "指示灯 - 名称：%s，状态：%d",
        //                 led.name.c_str(), (int)led.state);

        //     if (led.name == FIRE_ALARM_LED_NAME) {
        //         if (led.state == LED_STATE_RED) {
        //             fire_alarm_red_on = true;
        //             RCLCPP_ERROR(node_->get_logger(), "验证失败：火警灯（%s）仍红灯亮起，报警未解除", led.name.c_str());
        //         } else if (led.state == LED_STATE_OFF) {
        //             RCLCPP_INFO(node_->get_logger(), "验证成功：火警灯（%s）已熄灭，报警解除", led.name.c_str());
        //         } else if (led.state == LED_STATE_UNKNOWN) {
        //             has_unknown_state = true;
        //             RCLCPP_WARN(node_->get_logger(), "验证发现：火警灯（%s）状态未知", led.name.c_str());
        //         }
        //     }
        // }
        RCLCPP_INFO(node_->get_logger(), "========== 复位后验证完成 ==========");

        // RCLCPP_INFO(node_->get_logger(), "验证成功：火警灯（%s）已熄灭，报警解除", led.name.c_str());
    }

    // 任务结果判定
    // 1. 火警灯仍红灯亮起，任务失败
    if (fire_alarm_red_on) {
        RCLCPP_ERROR(node_->get_logger(), "指示灯自检任务失败：火警灯仍红灯亮起，报警未解除");
        return false;
    }

    // // 2. 存在未知状态且不允许忽略，任务失败
    if (has_unknown_state && !step_config.getBoolParam("ignore_unknown_state", false)) {
        RCLCPP_WARN(node_->get_logger(), "指示灯自检存在未知状态，且配置为不允许忽略，任务失败");
        return false;
    }

    RCLCPP_INFO(node_->get_logger(), "指示灯区域自检任务成功完成");
    return true;
}

// 键允许任务函数
bool TaskExecutor::executeKeyEnableTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行键允许任务");
    // TODO: 添加键允许任务逻辑
    // 获取参数
    double key_pick_distance = step_config.getDoubleParam("key_pick_distance", 0.0); // 键悬停距离
    double open_position = step_config.getDoubleParam("open_position", 0.035); // 夹爪张开位置
    double close_position = step_config.getDoubleParam("close_position", 0.01); // 夹爪闭合位置
    
    // 获取键允许指示灯面板信息
    if(!executeKeyEnableInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取键允许指示灯面板信息失败");
        return false;
    }
    // 根据键允许指示灯状态决定是否需要执行键允许操作

    // TODO: 若指示灯亮起，则进行其他操作

    // 若指示灯熄灭，则执行键允许操作

    // 夹爪张开
    if (!hardware_controller_->controlGripper(true)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪张开失败");
        return false;
    }

    // 获取键允许区域位姿
    geometry_msgs::msg::PoseStamped key_pose;
    geometry_msgs::msg::PoseStamped target_pose;
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::KEY_ENABLE, key_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取键允许区域位姿失败");
        return false;
    }

    // 控制机械臂移动到钥匙位置
    target_pose = hardware_controller_->computePoseAboveAruco(key_pose, key_pick_distance, true);
    
    if (!executeArmControl(goal_handle, target_pose.pose)) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂停靠键允许区域任务失败");
        return false;
    }

    executeJointAngleControl(
        goal_handle, 
        {"joint6"}, 
        {-30.0}
    );

    // 夹爪闭合
    if (!hardware_controller_->controlGripper(false)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪闭合失败");
        return false;
    }

    // 延时1秒  
    rclcpp::sleep_for(std::chrono::seconds(1));

    // 控制机械臂夹爪旋转钥匙，打开键允许
    executeJointAngleControl(
        goal_handle, 
        {"joint6"}, 
        {60.0}
    );

    rclcpp::sleep_for(std::chrono::seconds(1));

    // 夹爪张开，释放钥匙
    if (!hardware_controller_->controlGripper(true)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪张开失败");
        return false;
    }

    rclcpp::sleep_for(std::chrono::seconds(1));

    // 机械臂抬起
    target_pose = hardware_controller_->computePoseAboveAruco(key_pose, key_pick_distance + 0.1, true);
    if (!executeArmControl(goal_handle, target_pose.pose)) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂抬起任务失败");
        return false;
    }

    rclcpp::sleep_for(std::chrono::seconds(1));

    // 控制机械臂夹爪旋转回初始位置

    // executeJointAngleControl(
    //     goal_handle, 
    //     {"joint6"}, 
    //     {90.0}
    // );

    // 夹爪闭合
    if (!hardware_controller_->controlGripper(false)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪闭合失败");
        return false;
    }

    // 回到初始位置
    executeJointAngleControl(
        goal_handle, 
        {"all"}, 
        {0.0}
    );


    // 如果有状态回调，调用它
    // if (screen_state_callback_) {
    //     screen_state_callback_(cur_screen_info_);
    // }

    return true;
        
}

// 升降台控制任务函数
bool TaskExecutor::executeLiftControl(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const int& height)
{
    (void)goal_handle;
    RCLCPP_INFO(node_->get_logger(), "执行升降台高度控制任务: %d", height);

    bool success = hardware_controller_->setLiftHeight(height);
    if (!success) {
        RCLCPP_ERROR(node_->get_logger(), "升降台高度控制任务失败");
        return false;
    }

    // 等待升降台到达目标高度或超时
    auto wait_start = node_->now();
    int timeout_ms = 20000;
    int retry_interval_ms = 3000; // 3秒内高度无变化则重试
    
    int last_height = hardware_controller_->getCurrentLiftHeight();
    auto last_change_time = node_->now();

    while (rclcpp::ok()) {
        // 检查是否到达目标高度（允许5mm误差）
        if (hardware_controller_->getCurrentLiftHeight() == height) {
            RCLCPP_INFO(node_->get_logger(), "升降台已到达目标高度: %d (当前: %d)", 
                       height, hardware_controller_->getCurrentLiftHeight());
            break;
        }

        if ((node_->now() - wait_start).seconds() * 1000 > timeout_ms) {
            RCLCPP_ERROR(node_->get_logger(), "升降台高度控制任务超时");
            return false;
        }

        // 检查高度是否在一段时间内没有变化
        if (last_height != hardware_controller_->getCurrentLiftHeight()) {
            // 高度发生变化，更新最后变化时间和高度值
            last_height = hardware_controller_->getCurrentLiftHeight();
            last_change_time = node_->now();
        } else if ((node_->now() - last_change_time).seconds() * 1000 > retry_interval_ms) {
            // 在retry_interval_ms毫秒内高度没有变化，重新发送命令
            RCLCPP_WARN(node_->get_logger(), "升降台在%dms内未移动，重新发送命令，当前高度: %d", 
                        retry_interval_ms, hardware_controller_->getCurrentLiftHeight());
            hardware_controller_->setLiftHeight(height);
            last_change_time = node_->now(); // 重置计时器
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    RCLCPP_INFO(node_->get_logger(), "升降台高度控制任务完成");
    return true;
}

// 机械臂控制任务函数
bool TaskExecutor::executeArmControl(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const geometry_msgs::msg::Pose& target_pose)
{
    (void)goal_handle;
    RCLCPP_INFO(node_->get_logger(), "执行机械臂动作任务");
    ArmControlAction::Goal arm_goal;

    geometry_msgs::msg::PoseStamped target_pose_stamped;
    target_pose_stamped.header.frame_id = "arm_base_link"; // 设置参考坐标系
    target_pose_stamped.header.stamp = node_->now();
    target_pose_stamped.pose = target_pose;
    arm_goal.waypoints.push_back(target_pose_stamped);
    
    return hardware_controller_->sendArmControlRequest(arm_goal);
}

// 关节角度控制任务函数
bool TaskExecutor::executeJointAngleControl(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    std::vector<std::string> joint_names,
    std::vector<double> target_angles)
{
    RCLCPP_INFO(node_->get_logger(), "执行关节角度控制任务");
    
    // 验证参数
    if (joint_names.empty() || target_angles.empty()) {
        RCLCPP_ERROR(node_->get_logger(), "关节名称或目标角度列表为空");
        return false;
    }
    
    if (joint_names.size() != target_angles.size()) {
        RCLCPP_ERROR(node_->get_logger(), "关节名称数量(%zu)与目标角度数量(%zu)不匹配", 
                    joint_names.size(), target_angles.size());
        return false;
    }
    
    RCLCPP_INFO(node_->get_logger(), "控制%zu个关节到指定角度", joint_names.size());
    
    // 构造关节控制目标
    ArmControlAction::Goal arm_goal;
    
    // 将关节名称和角度添加到目标中
    for (size_t i = 0; i < joint_names.size(); ++i) {
        arm_goal.joint_name.push_back(joint_names[i]);
        arm_goal.target_angle.push_back(target_angles[i]);
        RCLCPP_INFO(node_->get_logger(), "关节 %s 目标角度: %f", 
                   joint_names[i].c_str(), target_angles[i]);
    }
    
    // 发送关节控制请求到硬件控制器
    bool success = hardware_controller_->sendArmControlRequest(arm_goal);
    
    if (!success) {
        RCLCPP_ERROR(node_->get_logger(), "关节角度控制任务执行失败");
        return false;
    }
    
    RCLCPP_INFO(node_->get_logger(), "关节角度控制任务执行完成");
    return true;
}


// 夹爪抓取任务函数
bool TaskExecutor::executeGripperPick(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行夹爪控制任务");
    
    // 获取参数
    double open_position = step_config.getDoubleParam("open_position", 0.035); // 夹爪张开位置
    double close_position = step_config.getDoubleParam("close_position", 0.01); // 夹爪闭合位置
    
    // 步骤1: 控制夹爪张开到指定位置
    RCLCPP_INFO(node_->get_logger(), "步骤1: 张开夹爪到位置 %f", open_position);
    std::vector<double> open_positions = {open_position};
    if (!hardware_controller_->controlGripper(open_positions)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪张开失败");
        return false;
    }
    
    // 短暂等待
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 步骤2: 控制夹爪闭合夹取钥匙
    RCLCPP_INFO(node_->get_logger(), "步骤2: 闭合夹爪到位置 %f", close_position);
    std::vector<double> close_positions = {close_position};
    if (!hardware_controller_->controlGripper(close_positions)) {
        RCLCPP_ERROR(node_->get_logger(), "夹爪闭合失败");
        return false;
    }
    
    // 等待夹爪稳定夹取
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    RCLCPP_INFO(node_->get_logger(), "机械臂抓取操作完成");
    return true;
}

// 获取AR码-面板区域位姿任务函数
bool TaskExecutor::executeArucoLocalization(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "获取AR码-面板区域位姿任务");
    // 1: 优先设置相机增益
    if (!hardware_controller_->setCameraParamsForNormal())
    {
        RCLCPP_ERROR(node_->get_logger(), "设置相机参数失败");
        return false;
    }
    
    // 2: 移动机械臂到拍摄位置
    geometry_msgs::msg::Pose capture_pose = step_config.getPoseParam("capture_pose");
    int marker_id = step_config.getIntParam("marker_id");
    
    RCLCPP_INFO(node_->get_logger(), "执行Aruco定位任务，标记ID: %d", marker_id);
    if (!executeArmControl(goal_handle, capture_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "执行Aruco定位任务失败");
        return false;
    }
    // 延时等待机械臂稳定
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 3: 获取AR码主辅码中间位位姿，初定位
    bool found_main_aux = false;
    int max_retries = 10;
    int retry_count = 0;
    int loops_after_found = 1;  // 找到主辅码后还需要循环的次数

    while (!found_main_aux || loops_after_found < 1)
    {
        geometry_msgs::msg::PoseStamped center_pose;
        std::string message;
        if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::MAIN_AUX_CENTER, center_pose, message)) {
            RCLCPP_ERROR(node_->get_logger(), "获取AR码位姿失败");
            return false;
        }
        RCLCPP_INFO(node_->get_logger(), "Aruco定位信息: %s", message.c_str());

        // 4: 计算机械臂目标位姿，移动机械臂到Aruco码上方指定位置
        double aruco_hover_distance = step_config.getDoubleParam("aruco_hover_distance");
        double capture_offset = step_config.getDoubleParam("capture_offset", 0.0);
        
        // 获取 Aruco 平面上方吸附点在 base 下的 Pose
        geometry_msgs::msg::PoseStamped target_pose = hardware_controller_->computePoseAboveAruco(
            center_pose, aruco_hover_distance, true); // 翻转 Z 轴方向
        target_pose.pose.position.z = target_pose.pose.position.z - capture_offset;
        
        if (!executeArmControl(goal_handle, target_pose.pose)) {
            RCLCPP_ERROR(node_->get_logger(), "执行Aruco定位任务失败");
            return false;
        }
        
        if(message.find("检测到主辅码") != std::string::npos) {
            found_main_aux = true;
            RCLCPP_INFO(node_->get_logger(), "检测到主辅码");
        } else if (!found_main_aux) {
            RCLCPP_WARN(node_->get_logger(), "未检测到主辅码，重试中... (%d/%d)", retry_count + 1, max_retries);
            retry_count++;
            if (retry_count >= max_retries) {
                RCLCPP_ERROR(node_->get_logger(), "多次未检测到主辅码，任务失败");
                return false;
            }
        }
        
        // 如果已经找到主辅码，增加计数器
        if (found_main_aux) {
            loops_after_found++;
            if (loops_after_found >= 1) {
                RCLCPP_INFO(node_->get_logger(), "已完成主辅码检测后的额外循环");
            }
        }
        
        // 延时等待机械臂稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    

    // 5: 再次获取AR码位姿，精定位
    geometry_msgs::msg::PoseStamped ar_pose_refined;
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::ORIGIN, ar_pose_refined)) {
        RCLCPP_ERROR(node_->get_logger(), "获取AR码位姿失败");
        return false;
    }

    recordArPoseData(ar_pose_refined, "refined");

    return true;
}

void TaskExecutor::recordArPoseData(const geometry_msgs::msg::PoseStamped& ar_pose, const std::string& pose_type)
{
    try {
        // ====================== 核心修复：提前计算欧拉角，扩大作用域 ======================
        double roll, pitch, yaw;
        tf2::Quaternion q(
            ar_pose.pose.orientation.x,
            ar_pose.pose.orientation.y,
            ar_pose.pose.orientation.z,
            ar_pose.pose.orientation.w
        );
        tf2::Matrix3x3 m(q);
        m.getRPY(roll, pitch, yaw);

        // ====================== 1. 原有默认路径（完全不变，兼容旧逻辑） ======================
        std::string default_record_dir = "/tmp/aruco_pose_records";
        std::filesystem::create_directories(default_record_dir);
        
        // 生成基于日期的文件名
        auto now = std::chrono::system_clock::now();
        auto time_t_default = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream date_ss_default;
        date_ss_default << std::put_time(std::localtime(&time_t_default), "%Y%m%d");
        
        std::string default_filename = default_record_dir + "/aruco_pose_" + date_ss_default.str() + ".txt";
        
        // 生成详细时间戳
        auto ms_default = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream timestamp_ss_default;
        timestamp_ss_default << std::put_time(std::localtime(&time_t_default), "%Y-%m-%d %H:%M:%S");
        timestamp_ss_default << "." << std::setfill('0') << std::setw(3) << ms_default.count();
        
        // 追加位姿数据到默认文件
        std::ofstream default_file(default_filename, std::ios::app);
        if (default_file.is_open()) {
            // 如果文件是新创建的，添加文件头
            default_file.seekp(0, std::ios::end);
            if (default_file.tellp() == 0) {
                default_file << "AR Code Pose Records" << std::endl;
                default_file << "===================" << std::endl << std::endl;
            }
            
            // 写入记录分隔符
            default_file << "----------------------------------------" << std::endl;
            default_file << "Timestamp: " << timestamp_ss_default.str() << std::endl;
            default_file << "Pose Type: " << pose_type << std::endl;
            default_file << "Frame ID: " << ar_pose.header.frame_id << std::endl;
            default_file << "Position (x, y, z): " 
                         << ar_pose.pose.position.x << ", " 
                         << ar_pose.pose.position.y << ", " 
                         << ar_pose.pose.position.z << std::endl;
            default_file << "Orientation (x, y, z, w): " 
                         << ar_pose.pose.orientation.x << ", " 
                         << ar_pose.pose.orientation.y << ", " 
                         << ar_pose.pose.orientation.z << ", " 
                         << ar_pose.pose.orientation.w << std::endl;
            
            // 直接使用提前计算的欧拉角
            default_file << "Euler Angles (roll, pitch, yaw): " 
                         << roll << ", " 
                         << pitch << ", " 
                         << yaw << std::endl;
            default_file << "----------------------------------------" << std::endl << std::endl;
            
            default_file.close();
            RCLCPP_INFO(node_->get_logger(), "AR码位姿数据已记录到默认文件: %s", default_filename.c_str());
        } else {
            RCLCPP_WARN(node_->get_logger(), "无法打开默认路径文件进行记录: %s", default_filename.c_str());
        }

        // ====================== 2. 新增指定路径（/home/ubuntu/arm_ws/src/record/aruco_position） ======================
        std::string new_record_dir = "/home/ubuntu/arm_ws/src/record/aruco_position";
        std::filesystem::create_directories(new_record_dir); // 自动创建多级目录
        
        // 生成基于日期的文件名（和默认路径格式一致，便于对应检索）
        auto time_t_new = std::chrono::system_clock::to_time_t(now);
        std::stringstream date_ss_new;
        date_ss_new << std::put_time(std::localtime(&time_t_new), "%Y%m%d");
        std::string new_filename = new_record_dir + "/aruco_pose_" + date_ss_new.str() + ".txt";
        
        // 生成详细时间戳
        auto ms_new = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::stringstream timestamp_ss_new;
        timestamp_ss_new << std::put_time(std::localtime(&time_t_new), "%Y-%m-%d %H:%M:%S");
        timestamp_ss_new << "." << std::setfill('0') << std::setw(3) << ms_new.count();
        
        // 追加位姿数据到新增文件（保存默认完整内容 + 欧拉角，和默认路径一致）
        std::ofstream new_file(new_filename, std::ios::app);
        if (new_file.is_open()) {
            // 如果文件是新创建的，添加文件头
            new_file.seekp(0, std::ios::end);
            if (new_file.tellp() == 0) {
                new_file << "AR Code Pose Records (Default Content + Euler Angles)" << std::endl;
                new_file << "=====================================================" << std::endl << std::endl;
            }
            
            // 写入记录分隔符
            new_file << "----------------------------------------" << std::endl;
            new_file << "Timestamp: " << timestamp_ss_new.str() << std::endl;
            new_file << "Pose Type: " << pose_type << std::endl;
            new_file << "Frame ID: " << ar_pose.header.frame_id << std::endl;
            new_file << "Position (x, y, z): " 
                     << ar_pose.pose.position.x << ", " 
                     << ar_pose.pose.position.y << ", " 
                     << ar_pose.pose.position.z << std::endl;
            new_file << "Orientation (x, y, z, w): " 
                     << ar_pose.pose.orientation.x << ", " 
                     << ar_pose.pose.orientation.y << ", " 
                     << ar_pose.pose.orientation.z << ", " 
                     << ar_pose.pose.orientation.w << std::endl;
            
            // 直接使用提前计算的欧拉角（无作用域问题）
            new_file << "Euler Angles (roll, pitch, yaw): " 
                     << roll << ", " 
                     << pitch << ", " 
                     << yaw << std::endl;
            new_file << "----------------------------------------" << std::endl << std::endl;
            
            new_file.close();
            RCLCPP_INFO(node_->get_logger(), "AR码位姿数据已记录到新增文件: %s", new_filename.c_str());
        } else {
            RCLCPP_WARN(node_->get_logger(), "无法打开新增路径文件进行记录: %s", new_filename.c_str());
        }
    }
    catch (const std::exception& e) {
        RCLCPP_WARN(node_->get_logger(), "记录AR码位姿数据时发生错误: %s", e.what());
    }
}


// 获取指示灯区域信息任务函数
// bool TaskExecutor::executeIndicatorInfo(
//     const std::shared_ptr<GoalHandleTaskJs> goal_handle,
//     const TaskStep& step_config)
// {
//     RCLCPP_INFO(node_->get_logger(), "获取指示灯区域信息");
    
//     geometry_msgs::msg::PoseStamped pose;
//     // 获取区域位姿
//     if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::LED, pose)) {
//         RCLCPP_ERROR(node_->get_logger(), "获取指示灯区域位姿失败");
//         return false;
//     }
    
//     double capture_offset = step_config.getDoubleParam("capture_offset", 0.0);
//     geometry_msgs::msg::PoseStamped target_pose;
//     target_pose = hardware_controller_->computePoseAboveAruco(pose, step_config.getDoubleParam("led_hover_distance"));
//     target_pose.pose.position.z = target_pose.pose.position.z - capture_offset;
    
    
//     // ===================== 新增：打印指示灯区域目标位姿 =====================
//     RCLCPP_INFO(node_->get_logger(), "指示灯区域目标位姿：x=%.4f, y=%.4f, z=%.4f", 
//                 target_pose.pose.position.x,
//                 target_pose.pose.position.y,
//                 target_pose.pose.position.z);
//     // ======================================================================
    
//     // 控制机械臂停靠指示灯区域
//     if (!executeArmControl(goal_handle, target_pose.pose)) {
//         RCLCPP_ERROR(node_->get_logger(), "机械臂停靠指示灯区域任务失败");
//         return false;
//     }
    
//     // 请求获取面板信息
//     cur_indicator_lights_.clear(); // 清空之前的指示灯信息
//     if (!hardware_controller_->getIndicatorLights(task_points_[0].facp_model, 
//                                                 cur_indicator_lights_)) {
//         RCLCPP_ERROR(node_->get_logger(), "获取指示灯区域信息失败");
//         return false;
//     }
    
//     return true;
// }

// 获取指示灯区域信息任务函数（已修改，对齐handle_alarm逻辑）
bool TaskExecutor::executeIndicatorInfo(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "获取指示灯区域信息");
    
    geometry_msgs::msg::PoseStamped pose;
    // 获取区域位姿
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::LED, pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取指示灯区域位姿失败");
        return false;
    }
    
    double capture_offset = step_config.getDoubleParam("capture_offset", 0.0);
    geometry_msgs::msg::PoseStamped target_pose;
    
    // 【修改1：开启Z轴翻转，第三个参数传递true，与handle_alarm一致】
    target_pose = hardware_controller_->computePoseAboveAruco(
        pose, 
        step_config.getDoubleParam("led_hover_distance"),
        true  // 关键：显式开启Z轴翻转，生成可行姿态
    );
    target_pose.pose.position.z = target_pose.pose.position.z - capture_offset;
    
    // 【修改2：新增姿态倾斜调整，复用handle_alarm的姿态优化逻辑】
    tf2::Quaternion current_orientation;
    tf2::fromMsg(target_pose.pose.orientation, current_orientation);

    // 创建绕X轴的小角度旋转（使相机向下倾斜，与handle_alarm保持一致）
    double tilt_angle = 0.1; // 与executeScreenInfo中的参数保持一致，可根据实际微调
    tf2::Quaternion tilt_rotation;
    tilt_rotation.setRPY(0, tilt_angle, 0); // 仅绕X轴旋转，优化姿态合理性

    // 应用旋转并更新目标位姿
    tf2::Quaternion new_orientation = current_orientation * tilt_rotation;
    new_orientation.normalize();
    target_pose.pose.orientation = tf2::toMsg(new_orientation);
    
    // ===================== 保留原有打印：验证目标位姿 =====================
    RCLCPP_INFO(node_->get_logger(), "指示灯区域目标位姿：x=%.4f, y=%.4f, z=%.4f", 
                target_pose.pose.position.x,
                target_pose.pose.position.y,
                target_pose.pose.position.z);
    // ======================================================================
    
    // 控制机械臂停靠指示灯区域
    if (!executeArmControl(goal_handle, target_pose.pose)) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂停靠指示灯区域任务失败");
        return false;
    }
    
    // 请求获取面板信息
    cur_indicator_lights_.clear(); // 清空之前的指示灯信息
    if (!hardware_controller_->getIndicatorLights(task_points_[0].facp_model, 
                                                cur_indicator_lights_)) {
        RCLCPP_ERROR(node_->get_logger(), "获取指示灯区域信息失败");
        return false;
    }
    
    return true;
}

// 获取屏幕区域信息任务函数
bool TaskExecutor::executeScreenInfo(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "获取屏幕区域信息");
    
    double capture_offset = step_config.getDoubleParam("capture_offset", 0.0);
    geometry_msgs::msg::PoseStamped screen_pose;
    geometry_msgs::msg::PoseStamped target_pose;

    // 获取屏幕区域位姿
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::SCREEN, screen_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取屏幕区域位姿失败");
        return false;
    }

    // 控制机械臂停靠屏幕区域
    target_pose = hardware_controller_->computePoseAboveAruco(screen_pose, step_config.getDoubleParam("screen_hover_distance"), true);
    target_pose.pose.position.z = target_pose.pose.position.z - capture_offset;
    
    // 添加向下倾斜的角度调整
    tf2::Quaternion current_orientation;
    tf2::fromMsg(target_pose.pose.orientation, current_orientation);

    // 创建一个绕X轴的小角度旋转（使相机向下倾斜）
    double tilt_angle = 0.1; // 向下倾斜，可根据需要调整
    tf2::Quaternion tilt_rotation;
    tilt_rotation.setRPY(0, tilt_angle, 0); // 绕X轴旋转

    // 应用旋转
    tf2::Quaternion new_orientation = current_orientation * tilt_rotation;
    new_orientation.normalize();

    // 更新目标姿态
    target_pose.pose.orientation = tf2::toMsg(new_orientation);
    
    if (!executeArmControl(goal_handle, target_pose.pose)) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂停靠屏幕区域任务失败");
        return false;
    }

    // 获取屏幕区域信息
    if (!hardware_controller_->getScreenInfo(task_points_[0].facp_model, 
                                           cur_screen_info_)) {
        RCLCPP_ERROR(node_->get_logger(), "获取屏幕区域信息失败");
        return false;
    }
    
    // 如果有屏幕状态回调，调用它
    if (screen_state_callback_) {
        screen_state_callback_(cur_screen_info_);
    }
    
    return true;
}

// 获取键允许区域信息任务函数
bool TaskExecutor::executeKeyEnableInfo(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    double capture_offset = step_config.getDoubleParam("capture_offset", 0.0);
    double key_hover_distance = step_config.getDoubleParam("key_hover_distance");
    geometry_msgs::msg::PoseStamped key_enable_pose;
    geometry_msgs::msg::PoseStamped target_pose;

    // 获取键允许区域位姿
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::KEY_ENABLE, key_enable_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取键允许区域位姿失败");
        return false;
    }

    // 控制机械臂停靠键允许区域上方
    target_pose = hardware_controller_->computePoseAboveAruco(key_enable_pose, key_hover_distance, true);
    target_pose.pose.position.z = target_pose.pose.position.z - capture_offset;   // TODO: 确保高度合适
    
    if (!executeArmControl(goal_handle, target_pose.pose)) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂停靠键允许区域任务失败");
        return false;
    }

    // TODO: 获取键允许区域信息
    // if (!hardware_controller_->getPanelInfo(task_points_[0].facp_model, PanelRegionMsg::KEY_ENABLE,
    //                                        cur_screen_info_, cur_indicator_lights_)) {
    //     RCLCPP_ERROR(node_->get_logger(), "获取键允许区域信息失败");
    //     return false;
    // }

    
    
    return true;
}

// 获取设备通道控制区信息任务函数
bool TaskExecutor::executeDeviceChannelInfo(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{ 
    RCLCPP_INFO(node_->get_logger(), "获取设备通道控制区信息");
    // TODO: 获取设备通道控制区信息
    return true;
}

// 点击按键任务函数
bool TaskExecutor::executeClickButtonTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const geometry_msgs::msg::PoseStamped& target_pose)
{
    (void)goal_handle;
    RCLCPP_INFO(node_->get_logger(), "点击按键任务");

    ArmControlAction::Goal arm_goal;

    // 第一步：移动到按键上方的停靠点（比目标点高一些）
    geometry_msgs::msg::PoseStamped hover_pose;
    hover_pose = hardware_controller_->computePoseAboveAruco(target_pose, 
        global_config_.default_button_hover_distance, true); 
    hover_pose.header.frame_id = "arm_base_link";
    hover_pose.header.stamp = node_->now();
    arm_goal.waypoints.push_back(hover_pose);
    
    // 第二步：向下移动到按键按下点
    geometry_msgs::msg::PoseStamped press_pose;
    press_pose = hardware_controller_->computePoseAboveAruco(target_pose, 
        global_config_.default_button_hover_distance - 
        global_config_.default_button_press_distance, true);
    press_pose.header.frame_id = "arm_base_link";
    press_pose.header.stamp = node_->now();
    arm_goal.waypoints.push_back(press_pose);
    
    // 第三步：回到停靠点
    arm_goal.waypoints.push_back(hover_pose);

    return hardware_controller_->sendArmControlRequest(arm_goal);
}

// 微调按键位置任务函数
bool TaskExecutor::executeAdjustButtonPositionTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行微调按键位置任务");

    // 从 step_config 中获取按键对应的区域 ID
    int region_id = step_config.getIntParam("button_region_id", -1);
    RCLCPP_INFO(node_->get_logger(), "请求获取区域 ID: %d 的位姿", region_id);
    
    if (region_id == -1) {
        RCLCPP_ERROR(node_->get_logger(), "未指定有效的按键区域 ID");
        return false;
    }

    // 获取指定区域的位姿
    geometry_msgs::msg::PoseStamped button_pose;
    if (!hardware_controller_->getPanelRegionPose(region_id, button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取区域 ID %d 的位姿失败", region_id);
        return false;
    }

    if (!executeClickButtonTask(goal_handle, button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "点击按钮失败");
        return false;
    }

    RCLCPP_INFO(node_->get_logger(), "微调按键位置任务完成");
    return true;
}

// 消音任务函数
bool TaskExecutor::executeMuteAlarmTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    (void)goal_handle;
    (void)step_config;
    RCLCPP_INFO(node_->get_logger(), "执行消音操作");
    
    geometry_msgs::msg::PoseStamped mute_button_pose;
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::BUTTON_MUTE, mute_button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取消音按钮位姿失败");
        return false;
    }
    
    // 点击消音按键
    if (!executeClickButtonTask(goal_handle, mute_button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "点击消音按钮失败");
        return false;
    }

    return true;
}

// 重置任务函数
bool TaskExecutor::executeResetAlarmTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行重置报警操作");

    geometry_msgs::msg::PoseStamped reset_button_pose;
    geometry_msgs::msg::PoseStamped confirm_button_pose;

    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::BUTTON_RESET, reset_button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取重置按钮位姿失败");
        return false;
    }
    
    if (!hardware_controller_->getPanelRegionPose(PanelRegionMsg::BUTTON_CONFIRM, confirm_button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "获取确认按钮位姿失败");
        return false;
    }

    // 点击重置按键
    if (!executeClickButtonTask(goal_handle, reset_button_pose)) {
        RCLCPP_ERROR(node_->get_logger(), "点击重置按钮失败");
        return false;
    }
    
    // 获取屏幕区域信息
    if (!executeScreenInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取区域信息失败");
        return false;
    }

    // 判断是否进入输入密码界面
    if (cur_screen_info_.current_page == ScreenInfo::PASSWORD_INPUT_PAGE) {
        if (!executeInputPasswordTask(goal_handle, step_config)) {
            RCLCPP_ERROR(node_->get_logger(), "输入密码失败");
            return false;
        }
    }

    // 再次获取屏幕区域信息
    if (!executeScreenInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取区域信息失败");
        return false;
    }

    if (cur_screen_info_.current_page == ScreenInfo::CONFIRM_CLICK_PAGE) {
        // 点击确认按键
        if (!executeClickButtonTask(goal_handle, confirm_button_pose)) {
            RCLCPP_ERROR(node_->get_logger(), "点击确认按钮失败");
            return false;
        }
    }

    // 再次获取屏幕区域信息
    if (!executeScreenInfo(goal_handle, step_config)) {
        RCLCPP_ERROR(node_->get_logger(), "获取区域信息失败");
        return false;
    }

    if (cur_screen_info_.current_page == ScreenInfo::NORMAL_PAGE) {
        RCLCPP_INFO(node_->get_logger(), "报警已成功重置，屏幕回到正常页面");
        return true;
    } else {
        RCLCPP_ERROR(node_->get_logger(), "报警重置失败，屏幕仍处于报警状态");
        return false;
    }
}

// 输入密码任务函数
bool TaskExecutor::executeInputPasswordTask(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const TaskStep& step_config)
{
    RCLCPP_INFO(node_->get_logger(), "执行输入密码任务");
    
    // 从配置参数中解析密码
    std::string password = step_config.getStringParam("password");
    
    // 验证密码格式（应为数字字符串）
    if (password.empty()) {
        RCLCPP_ERROR(node_->get_logger(), "密码为空");
        return false;
    }
    
    for (char c : password) {
        if (!std::isdigit(c)) {
            RCLCPP_ERROR(node_->get_logger(), "密码包含非数字字符: %c", c);
            return false;
        }
    }
    
    RCLCPP_INFO(node_->get_logger(), "解析密码: %s", password.c_str());
    
    // 为每个数字按钮定义区域ID映射（假设按钮0-9对应的区域ID为BUTTON_0-BUTTON_9）
    std::map<char, int> digit_to_region_id = {
        {'0', PanelRegionMsg::BUTTON_0},
        {'1', PanelRegionMsg::BUTTON_1},
        {'2', PanelRegionMsg::BUTTON_2},
        {'3', PanelRegionMsg::BUTTON_3},
        {'4', PanelRegionMsg::BUTTON_4},
        {'5', PanelRegionMsg::BUTTON_5},
        {'6', PanelRegionMsg::BUTTON_6},
        {'7', PanelRegionMsg::BUTTON_7},
        {'8', PanelRegionMsg::BUTTON_8},
        {'9', PanelRegionMsg::BUTTON_9}
    };

    // 按顺序输入每个数字
    for (size_t i = 0; i < password.length(); ++i) {
        char digit = password[i];
        RCLCPP_INFO(node_->get_logger(), "输入第 %zu 位数字: %c", i+1, digit);
        
        // 检查是否为有效的数字
        if (digit_to_region_id.find(digit) == digit_to_region_id.end()) {
            RCLCPP_ERROR(node_->get_logger(), "未定义数字按钮 '%c' 的区域ID", digit);
            return false;
        }
        
        // 获取对应数字按钮的位姿
        geometry_msgs::msg::PoseStamped digit_button_pose;
        if (!hardware_controller_->getPanelRegionPose(digit_to_region_id[digit], digit_button_pose)) {
            RCLCPP_ERROR(node_->get_logger(), "获取数字按钮 '%c' 位姿失败", digit);
            return false;
        }
        
        // 点击数字按钮
        if (!executeClickButtonTask(goal_handle, digit_button_pose)) {
            RCLCPP_ERROR(node_->get_logger(), "点击数字按钮 '%c' 失败", digit);
            return false;
        }
        
        // 添加短暂延迟，确保按钮响应完成
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    RCLCPP_INFO(node_->get_logger(), "密码输入完成");
    return true;
}

// 处理任务失败的统一函数
bool TaskExecutor::handleTaskFailure(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const std::string& error_message)
{
    RCLCPP_ERROR(node_->get_logger(), "任务执行失败: %s", error_message.c_str());
    
    // 记录当前错误信息
    // last_error_message_ = error_message;
    
    // 尝试让机械臂回到起始位置
    // if (!executeArmControl(goal_handle, global_config_.arm_start_pose)) {
    //     RCLCPP_ERROR(node_->get_logger(), "无法将机械臂返回起始位置");
    //     return false;
    // }

    if(!executeJointAngleControl(
        goal_handle, 
        {"all"}, 
        {0.0}
    )) {
        RCLCPP_ERROR(node_->get_logger(), "无法将机械臂返回起始位置");
        return false;
    }

    RCLCPP_INFO(node_->get_logger(), "已将机械臂返回起始位置");
    return true;
}

// 创建统一的进度更新函数
void TaskExecutor::updateProgress(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    int current_step, int total_steps,
    const std::shared_ptr<TaskJsAction::Feedback>& feedback)
{
    feedback->progress_percent = current_step * 100 / total_steps;
    goal_handle->publish_feedback(feedback);
    RCLCPP_INFO(node_->get_logger(), "当前任务进度: %d%%", feedback->progress_percent);
}

// 创建统一的任务取消检查函数
bool TaskExecutor::checkCancel(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle,
    const std::shared_ptr<TaskJsAction::Result>& result)
{
    if (goal_handle->is_canceling()) {
        result->result_code = -1;
        goal_handle->canceled(result);
        RCLCPP_INFO(node_->get_logger(), "任务已取消");
        return true;
    }
    return false;
}

// 更新任务状态函数
void TaskExecutor::updateTaskStatus(
    uint8_t task_type, 
    uint8_t subtask_type, 
    uint8_t status,
    const std::string& error_message)
{
    TaskStatus task_status_msg;
    task_status_msg.stamp = node_->now();
    task_status_msg.task_type = task_type;
    task_status_msg.subtask_type = subtask_type;
    task_status_msg.status = status;
    task_status_msg.error_message = error_message;
    
    if (task_status_callback_) {
        task_status_callback_(task_status_msg);
    }
}

} // namespace task_manager
