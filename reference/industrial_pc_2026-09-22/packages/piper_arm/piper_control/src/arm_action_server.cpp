// arm_action_server.cpp
#include "arm_action_server.hpp"

#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <cmath>

namespace piper_control
{

    inline double deg2rad(double d) { return d * M_PI / 180.0; }

    ArmActionServer::ArmActionServer(const rclcpp::NodeOptions &options)
        : rclcpp::Node("arm_action_server", options)
    {
    }

    void ArmActionServer::initialize()
    {
        // 在这里进行需要 shared_from_this 的初始化
        RCLCPP_INFO(this->get_logger(), "Initializing ARM MoveGroupInterface...");
        arm_group_ptr_ = std::make_shared<moveit::planning_interface::MoveGroupInterface>(
            shared_from_this(), "arm");

        // 添加 gripper 组初始化
        RCLCPP_INFO(this->get_logger(), "Initializing Gripper MoveGroupInterface...");
        gripper_group_ptr_ = std::make_shared<moveit::planning_interface::MoveGroupInterface>(
            shared_from_this(), "gripper");

        // 设置最大速度缩放因子 (1.0 为最大速度)
        arm_group_ptr_->setMaxVelocityScalingFactor(1.0);
        
        // 设置最大加速度缩放因子 (1.0 为最大加速度)
        arm_group_ptr_->setMaxAccelerationScalingFactor(1.0);

        // 打印可用的 gripper 信息
        if (gripper_group_ptr_->getJointModelGroupNames().size() > 0)
        {
            RCLCPP_INFO(this->get_logger(), "Gripper MoveGroupInterface Initialized successfully.");
            RCLCPP_INFO(this->get_logger(), "Gripper planning frame: %s", gripper_group_ptr_->getPlanningFrame().c_str());
            for (const auto &joint : gripper_group_ptr_->getJointNames())
            {
                RCLCPP_INFO(this->get_logger(), "Gripper joint: %s", joint.c_str());
            }
        }
        else
        {
            RCLCPP_WARN(this->get_logger(), "Gripper group not found or not configured properly.");
        }

        RCLCPP_INFO(this->get_logger(), "MoveGroupInterface Initialized successfully.");
        std::vector<std::string> groups = arm_group_ptr_->getJointModelGroupNames();
        for (const auto &group : groups)
        {
            RCLCPP_INFO(this->get_logger(), "Available Planning Group: %s", group.c_str());
        }
        // 打印当前规划所使用的参考坐标系
        RCLCPP_INFO(this->get_logger(), "Current planning frame: %s", arm_group_ptr_->getPlanningFrame().c_str());

        action_server_ = rclcpp_action::create_server<ExecuteArmTask>(
            this,
            "/execute_arm_task",
            [this](const rclcpp_action::GoalUUID &uuid, std::shared_ptr<const ExecuteArmTask::Goal> goal)
            {
                return this->handle_goal(uuid, goal);
            },
            [this](std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
            {
                return this->handle_cancel(goal_handle);
            },
            [this](std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
            {
                this->handle_accepted(goal_handle);
            });

        // 添加夹爪控制服务
        gripper_service_ = create_service<ControlGripper>(
            "control_gripper",
            [this](const std::shared_ptr<ControlGripper::Request> request,
                   std::shared_ptr<ControlGripper::Response> response)
            {
                this->handleGripperControl(request, response);
            });
    }

    //------------机械臂控制相关---------------//
    bool ArmActionServer::moveToTarget(
        const geometry_msgs::msg::Pose &target_pose,
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
    {
        // --- 1. 规划 ---
        arm_group_ptr_->setPoseTarget(target_pose);

        moveit::planning_interface::MoveGroupInterface::Plan plan;
        bool plan_success = false;
        for (int i = 0; i < 3; ++i)
        { // 尝试规划3次
            plan_success = (arm_group_ptr_->plan(plan) == moveit::core::MoveItErrorCode::SUCCESS);
            if (plan_success)
                break;
            RCLCPP_WARN(this->get_logger(), "Planning failed, retrying... (%d/3)", i + 1);
            rclcpp::sleep_for(std::chrono::seconds(1));
        }

        if (!plan_success)
        {
            RCLCPP_ERROR(this->get_logger(), "Motion planning failed after multiple attempts.");
            return false;
        }

        // --- 3. 异步执行 ---
        auto exec_result = arm_group_ptr_->asyncExecute(plan);

        if (exec_result != moveit::core::MoveItErrorCode::SUCCESS)
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to start motion execution.");
            return false;
        }

        // rclcpp is not ok
        return false;
    }

    // 同步阻塞
    bool ArmActionServer::moveToTargetAndWait(const geometry_msgs::msg::Pose &target_pose)
    {
        arm_group_ptr_->setPoseTarget(target_pose);

        // move() 是一个集规划和执行于一体的阻塞式函数
        // 尝试执行移动，可以加入重试逻辑
        moveit::core::MoveItErrorCode result;
        const int max_attempts = 3; // 最多尝试3次
        for (int attempt = 1; attempt <= max_attempts; ++attempt)
        {
            RCLCPP_INFO(this->get_logger(), "Attempting to move (attempt %d/%d)...", attempt, max_attempts);
            result = arm_group_ptr_->move(); // move() 会自己完成规划和执行

            if (result == moveit::core::MoveItErrorCode::SUCCESS)
            {
                RCLCPP_INFO(this->get_logger(), "Motion completed successfully.");
                return true;
            }

            // 如果失败了，判断是否是可恢复的错误
            // 例如，如果只是规划失败，可以重试
            if (result == moveit::core::MoveItErrorCode::PLANNING_FAILED)
            {
                RCLCPP_WARN(this->get_logger(), "Planning failed on attempt %d. Retrying...", attempt);
                if (attempt < max_attempts)
                {
                    rclcpp::sleep_for(std::chrono::seconds(1)); // 等待1秒再重试
                }
            }
            else
            {
                // 对于其他更严重的错误（如控制器失败、无效目标），直接退出
                RCLCPP_ERROR(this->get_logger(), "Motion failed with a non-recoverable error: %d. Aborting.", result.val);
                break;
            }
        }

        RCLCPP_ERROR(this->get_logger(), "Motion failed after %d attempts with final error code: %d", max_attempts, result.val);
        return false;
    }

    bool ArmActionServer::moveToTargetAndWait(const geometry_msgs::msg::PoseStamped &target_pose,
                                              const std::string &end_effector_link = "")
    {
        if (end_effector_link.empty())
        {
            arm_group_ptr_->setPoseTarget(target_pose);
            RCLCPP_INFO(this->get_logger(), "Attempting to move to PoseStamped target");
        }
        else
        {
            arm_group_ptr_->setPoseTarget(target_pose, end_effector_link);
            RCLCPP_INFO(this->get_logger(), "Attempting to move to PoseStamped target with end effector '%s'",
                        end_effector_link.c_str());
        }

        // move() 是一个集规划和执行于一体的阻塞式函数
        // 尝试执行移动，可以加入重试逻辑
        moveit::core::MoveItErrorCode result;
        const int max_attempts = 3; // 最多尝试3次
        for (int attempt = 1; attempt <= max_attempts; ++attempt)
        {
            RCLCPP_INFO(this->get_logger(), "Attempting to move to PoseStamped target (attempt %d/%d)...", attempt, max_attempts);
            result = arm_group_ptr_->move(); // move() 会自己完成规划和执行

            if (result == moveit::core::MoveItErrorCode::SUCCESS)
            {
                RCLCPP_INFO(this->get_logger(), "Motion completed successfully.");
                return true;
            }

            // 如果失败了，判断是否是可恢复的错误
            // 例如，如果只是规划失败，可以重试
            if (result == moveit::core::MoveItErrorCode::PLANNING_FAILED)
            {
                RCLCPP_WARN(this->get_logger(), "Planning failed on attempt %d. Retrying...", attempt);
                if (attempt < max_attempts)
                {
                    rclcpp::sleep_for(std::chrono::seconds(1)); // 等待1秒再重试
                }
            }
            else
            {
                // 对于其他更严重的错误（如控制器失败、无效目标），直接退出
                RCLCPP_ERROR(this->get_logger(), "Motion failed with a non-recoverable error: %d. Aborting.", result.val);
                break;
            }
        }

        RCLCPP_ERROR(this->get_logger(), "Motion failed after %d attempts with final error code: %d", max_attempts, result.val);
        return false;
    }

    // 示例改进代码
    bool ArmActionServer::moveToTargetImproved(const geometry_msgs::msg::PoseStamped &target_pose)
    {
        // 设置从当前状态开始规划
        arm_group_ptr_->setStartStateToCurrentState();
        
        // 规范化四元数
        geometry_msgs::msg::PoseStamped normalized_pose = target_pose;
        tf2::Quaternion q(target_pose.pose.orientation.x, target_pose.pose.orientation.y,
                        target_pose.pose.orientation.z, target_pose.pose.orientation.w);
        q.normalize();
        normalized_pose.pose.orientation.x = q.x();
        normalized_pose.pose.orientation.y = q.y();
        normalized_pose.pose.orientation.z = q.z();
        normalized_pose.pose.orientation.w = q.w();
        
        arm_group_ptr_->setPoseTarget(normalized_pose);
        
        moveit::core::MoveItErrorCode result;
        const int max_attempts = 3; // 最多尝试3次
        for (int attempt = 1; attempt <= max_attempts; ++attempt)
        {
            RCLCPP_INFO(this->get_logger(), "Attempting to move to PoseStamped target (attempt %d/%d)...", attempt, max_attempts);
            result = arm_group_ptr_->move(); // move() 会自己完成规划和执行

            if (result == moveit::core::MoveItErrorCode::SUCCESS)
            {
                RCLCPP_INFO(this->get_logger(), "Motion completed successfully.");
                return true;
            }

            // 如果失败了，判断是否是可恢复的错误
            // 例如，如果只是规划失败，可以重试
            if (result == moveit::core::MoveItErrorCode::PLANNING_FAILED)
            {
                RCLCPP_WARN(this->get_logger(), "Planning failed on attempt %d. Retrying...", attempt);
                if (attempt < max_attempts)
                {
                    rclcpp::sleep_for(std::chrono::seconds(1)); // 等待1秒再重试
                }
            }
            else
            {
                // 对于其他更严重的错误（如控制器失败、无效目标），直接退出
                RCLCPP_ERROR(this->get_logger(), "Motion failed with a non-recoverable error: %d. Aborting.", result.val);
                break;
            }
        }

        RCLCPP_ERROR(this->get_logger(), "Motion failed after %d attempts with final error code: %d", max_attempts, result.val);
        return false;
    }
    
    
    // 控制指定关节名称旋转
    bool ArmActionServer::rotateSpecificJoint(const std::string &joint_name, double target_angle)
    {
        if (!arm_group_ptr_)
        {
            RCLCPP_ERROR(this->get_logger(), "Arm MoveGroupInterface not initialized.");
            return false;
        }

        // 获取当前所有关节值
        std::vector<double> current_joint_values = arm_group_ptr_->getCurrentJointValues();
        std::vector<std::string> joint_names = arm_group_ptr_->getJointNames();
        
        // 打印当前关节状态用于调试
        RCLCPP_INFO(this->get_logger(), "Current joint values before control:");
        for (size_t i = 0; i < joint_names.size() && i < current_joint_values.size(); ++i) {
            RCLCPP_INFO(this->get_logger(), "  %s: %f", joint_names[i].c_str(), current_joint_values[i]);
        }

        // 创建目标关节值映射
        std::map<std::string, double> target_joint_values;
        for (size_t i = 0; i < joint_names.size() && i < current_joint_values.size(); ++i) {
            if (joint_names[i] == joint_name) {
                // 设置目标关节的目标值
                target_joint_values[joint_names[i]] = current_joint_values[i] + target_angle;
                RCLCPP_INFO(this->get_logger(), "Setting target for %s: %f (was %f)", 
                        joint_names[i].c_str(), target_angle, current_joint_values[i]);
            } else {
                // 保持其他关节在当前位置
                target_joint_values[joint_names[i]] = current_joint_values[i];
            }
        }

        // 设置所有关节的目标值，以保持其他关节不动
        arm_group_ptr_->setJointValueTarget(target_joint_values);

        // 执行运动
        moveit::core::MoveItErrorCode result = arm_group_ptr_->move();
        if (result == moveit::core::MoveItErrorCode::SUCCESS)
        {
            RCLCPP_INFO(this->get_logger(), "Joint %s rotated to %f successfully.",
                        joint_name.c_str(), target_angle);
            return true;
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to rotate joint %s. Error code: %d",
                        joint_name.c_str(), result.val);
            return false;
        }
    }

    bool ArmActionServer::rotateSpecificJointDegrees(const std::string &joint_name, double target_angle_degrees)
    {

        // 将角度转换为弧度
        double target_angle_radians = deg2rad(target_angle_degrees);

        return rotateSpecificJoint(joint_name, target_angle_radians);
    }

    // 设置所有关节为零度
    bool ArmActionServer::setAllJointsToZero()
    {
        if (!arm_group_ptr_)
        {
            RCLCPP_ERROR(this->get_logger(), "Arm MoveGroupInterface not initialized.");
            return false;
        }

        // 直接设置所有关节为0
        std::vector<double> zero_positions(arm_group_ptr_->getJointNames().size(), 0.0);
        arm_group_ptr_->setJointValueTarget(zero_positions);

        arm_group_ptr_->move();
        // 执行运动
        moveit::core::MoveItErrorCode result = arm_group_ptr_->move();
        if (result == moveit::core::MoveItErrorCode::SUCCESS)
        {
            RCLCPP_INFO(this->get_logger(), "All joints set to zero successfully.");
            return true;
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to set all joints to zero. Error code: %d", result.val);
            return false;
        }
    }

    rclcpp_action::GoalResponse ArmActionServer::handle_goal(
        const rclcpp_action::GoalUUID &uuid,
        std::shared_ptr<const ExecuteArmTask::Goal> goal)
    {
        RCLCPP_INFO(this->get_logger(), "Received goal request with %ld waypoints", goal->waypoints.size());
        (void)uuid;
        // 拒绝新任务（如果当前有任务在运行）
        std::lock_guard<std::mutex> lock(task_mutex_);
        if (is_running_task_)
        {
            RCLCPP_WARN(this->get_logger(), "An arm task is already in progress. Rejecting new goal.");
            return rclcpp_action::GoalResponse::REJECT;
        }

        is_running_task_ = true; // 标记任务开始
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    }

    rclcpp_action::CancelResponse ArmActionServer::handle_cancel(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
    {
        RCLCPP_INFO(this->get_logger(), "Received request to cancel goal");
        return rclcpp_action::CancelResponse::ACCEPT;
    }

    void ArmActionServer::handle_accepted(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
    {
        std::thread([=]()
                    { this->execute(goal_handle); })
            .detach();
    }

    void ArmActionServer::execute(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle)
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        const auto goal = goal_handle->get_goal();
        auto feedback = std::make_shared<ExecuteArmTask::Feedback>();
        auto result = std::make_shared<ExecuteArmTask::Result>();

        try {
            // 检查是否有关节角度控制任务
            if (!goal->joint_name.empty() && !goal->target_angle.empty()) {
                executeJointControl(goal_handle, goal, feedback, result);
            } else {
                // 执行路径点控制任务
                executeWaypointControl(goal_handle, goal, feedback, result);
            }
        } catch (...) {
            // 确保在任何异常情况下都重置运行标志
            is_running_task_ = false;
            throw;
        }
        
        // 确保任务完成后重置运行标志
        is_running_task_ = false;
    }

    void ArmActionServer::executeJointControl(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle,
        const std::shared_ptr<const ExecuteArmTask::Goal> goal,
        const std::shared_ptr<ExecuteArmTask::Feedback> feedback,
        const std::shared_ptr<ExecuteArmTask::Result> result)
    {
        RCLCPP_INFO(this->get_logger(), "执行关节角度控制任务，关节数量: %zu", goal->joint_name.size());

        // 检查是否是关节归零命令: {"all"} {0}
        if (goal->joint_name.size() == 1 && 
            goal->target_angle.size() == 1 && 
            goal->joint_name[0] == "all" && 
            goal->target_angle[0] == 0.0)
        {
            RCLCPP_INFO(this->get_logger(), "收到关节归零命令，执行所有关节归零操作");
            
            // 执行所有关节归零
            if (setAllJointsToZero())
            {
                result->success = true;
                result->message = "所有关节归零完成";
                goal_handle->succeed(result);
            }
            else
            {
                result->success = false;
                result->message = "关节归零失败";
                goal_handle->abort(result);
            }
            return;
        }

        // 检查关节名称和目标角度数量是否匹配
        if (goal->joint_name.size() != goal->target_angle.size())
        {
            RCLCPP_ERROR(this->get_logger(), "关节名称数量(%zu)与目标角度数量(%zu)不匹配",
                         goal->joint_name.size(), goal->target_angle.size());
            result->success = false;
            result->message = "关节名称数量与目标角度数量不匹配";
            goal_handle->abort(result);
            return;
        }

        bool joint_success = true;
        // 依次控制每个关节
        for (size_t i = 0; i < goal->joint_name.size(); ++i)
        {
            // 在每次移动前，检查是否收到了取消请求
            if (goal_handle->is_canceling())
            {
                result->success = false;
                result->message = "Task canceled during joint control.";
                goal_handle->canceled(result);
                return;
            }

            RCLCPP_INFO(this->get_logger(), "控制关节 %s 到角度 %f",
                        goal->joint_name[i].c_str(), goal->target_angle[i]);

            if (!rotateSpecificJointDegrees(goal->joint_name[i], goal->target_angle[i]))
            {
                RCLCPP_ERROR(this->get_logger(), "控制关节 %s 失败", goal->joint_name[i].c_str());
                joint_success = false;
                break;
            }

            // 发布反馈
            feedback->current_waypoint_index = static_cast<int32_t>(i);
            feedback->percent_complete = 100.0 * (i + 1) / goal->joint_name.size();
            goal_handle->publish_feedback(feedback);
        }

        if (joint_success)
        {
            result->success = true;
            result->message = "所有关节角度控制完成";
            goal_handle->succeed(result);
        }
        else
        {
            result->success = false;
            result->message = "关节角度控制失败";
            goal_handle->abort(result);
        }
    }

    void ArmActionServer::executeWaypointControl(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle,
        const std::shared_ptr<const ExecuteArmTask::Goal> goal,
        const std::shared_ptr<ExecuteArmTask::Feedback> feedback,
        const std::shared_ptr<ExecuteArmTask::Result> result)
    {
        int total_points = goal->waypoints.size();
        bool success = true;

        // 如果没有路径点，直接返回成功
        if (total_points == 0)
        {
            RCLCPP_INFO(this->get_logger(), "没有路径点需要执行");
            result->success = true;
            result->message = "任务完成";
            goal_handle->succeed(result);
            return;
        }

        for (int i = 0; i < total_points; ++i)
        {
            // 在每次移动前，检查是否收到了取消请求
            if (goal_handle->is_canceling())
            {
                result->success = false;
                result->message = "Task canceled before moving to the next waypoint.";
                goal_handle->canceled(result);
                return;
            }

            RCLCPP_INFO(this->get_logger(), "Moving to waypoint %d...", i);

            // bool ok = moveToTarget(goal->waypoints[i], goal_handle);     // 异步执行
            // bool ok = moveToTargetAndWait(goal->waypoints[i]); // 同步阻塞
            bool ok = moveToTargetImproved(goal->waypoints[i]); // 改进后的同步阻塞
            if (!ok)
            {
                RCLCPP_ERROR(this->get_logger(), "Failed to move to waypoint %d", i);
                success = false;
                break; // 或 continue，取决于是否希望继续执行后续点
            }

            RCLCPP_INFO(this->get_logger(), "Executing waypoint %d", i);
            // 等待机械臂稳定
            if (i < total_points - 1) { // 不是最后一个点时才等待稳定
                // 选择一种稳定检测方法：
                
                // 方法1：简单时间等待
                // waitForStability(0.5);
                
                // 方法2：基于末端执行器稳定检测（推荐）
                waitForEndEffectorStability(2.0, 0.001, 0.01);
                
                // 方法3：基于关节稳定检测
                // waitForJointStability(2.0, 0.01);
            }

            feedback->current_waypoint_index = i;
            feedback->percent_complete = 100.0 * (i + 1) / total_points;
            goal_handle->publish_feedback(feedback);
        }

        if (success)
        {
            result->success = true;
            result->message = "All waypoints completed successfully";
            goal_handle->succeed(result);
        }
        else
        {
            result->success = false;
            result->message = "Failed during execution";
            goal_handle->abort(result);
        }
    }

    void ArmActionServer::waitForStability(double wait_time_seconds)
    {
        RCLCPP_INFO(this->get_logger(), "Waiting for %f seconds for stabilization...", wait_time_seconds);
        rclcpp::sleep_for(std::chrono::milliseconds(static_cast<int>(wait_time_seconds * 1000)));
    }

    // 基于关节速度的稳定检测
    bool ArmActionServer::waitForEndEffectorStability(double timeout_seconds,
                                                    double position_threshold, // 1mm
                                                    double orientation_threshold) // 约0.57度
    {
        RCLCPP_INFO(this->get_logger(), "Waiting for end-effector stability...");
        
        auto start_time = std::chrono::steady_clock::now();
        const double check_interval = 0.1; // 100ms检查间隔
        
        // 获取初始位置
        geometry_msgs::msg::PoseStamped initial_pose = arm_group_ptr_->getCurrentPose();
        
        while (rclcpp::ok()) {
            // 检查是否超时
            auto current_time = std::chrono::steady_clock::now();
            double elapsed_time = std::chrono::duration<double>(current_time - start_time).count();
            if (elapsed_time > timeout_seconds) {
                RCLCPP_WARN(this->get_logger(), "Timeout waiting for end-effector stability");
                return false;
            }
            
            // 获取当前末端执行器位置
            geometry_msgs::msg::PoseStamped current_pose = arm_group_ptr_->getCurrentPose();
            
            // 计算位置变化
            double position_diff = sqrt(
                pow(current_pose.pose.position.x - initial_pose.pose.position.x, 2) +
                pow(current_pose.pose.position.y - initial_pose.pose.position.y, 2) +
                pow(current_pose.pose.position.z - initial_pose.pose.position.z, 2)
            );
            
            // 计算姿态变化（简化计算）
            double orientation_diff = sqrt(
                pow(current_pose.pose.orientation.x - initial_pose.pose.orientation.x, 2) +
                pow(current_pose.pose.orientation.y - initial_pose.pose.orientation.y, 2) +
                pow(current_pose.pose.orientation.z - initial_pose.pose.orientation.z, 2) +
                pow(current_pose.pose.orientation.w - initial_pose.pose.orientation.w, 2)
            );
            
            RCLCPP_DEBUG(this->get_logger(), "Position diff: %f, Orientation diff: %f", 
                        position_diff, orientation_diff);
            
            // 检查是否稳定
            if (position_diff < position_threshold && orientation_diff < orientation_threshold) {
                RCLCPP_INFO(this->get_logger(), "End-effector stabilized");
                return true;
            }
            
            // 更新参考位置
            initial_pose = current_pose;
            
            // 等待下一个检查周期
            rclcpp::sleep_for(std::chrono::milliseconds(static_cast<int>(check_interval * 1000)));
        }
        
        return false;
    }

    //-------------夹爪控制相关--------------------//
    bool ArmActionServer::controlGripper(bool open)
    {
        if (!gripper_group_ptr_)
        {
            RCLCPP_ERROR(this->get_logger(), "Gripper MoveGroupInterface not initialized.");
            return false;
        }

        try
        {
            // 使用命名目标控制夹爪
            if (open)
            {
                if (!gripper_group_ptr_->setNamedTarget("open"))
                {
                    RCLCPP_WARN(this->get_logger(), "Failed to set 'open' named target, using joint values instead.");
                    std::vector<double> open_position = {0.0}; // 根据实际夹爪调整
                    gripper_group_ptr_->setJointValueTarget(open_position);
                }
            }
            else
            {
                if (!gripper_group_ptr_->setNamedTarget("close"))
                {
                    RCLCPP_WARN(this->get_logger(), "Failed to set 'close' named target, using joint values instead.");
                    std::vector<double> close_position = {0.035}; // 根据实际夹爪调整
                    gripper_group_ptr_->setJointValueTarget(close_position);
                }
            }

            // 执行运动
            moveit::core::MoveItErrorCode result = gripper_group_ptr_->move();
            if (result == moveit::core::MoveItErrorCode::SUCCESS)
            {
                RCLCPP_INFO(this->get_logger(), "Gripper %s successfully.", open ? "opened" : "closed");
                return true;
            }
            else
            {
                RCLCPP_ERROR(this->get_logger(), "Failed to move gripper. Error code: %d", result.val);
                return false;
            }
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "Exception occurred while controlling gripper: %s", e.what());
            return false;
        }
    }

    bool ArmActionServer::controlGripper(const std::string &named_position)
    {
        if (!gripper_group_ptr_)
        {
            RCLCPP_ERROR(this->get_logger(), "Gripper MoveGroupInterface not initialized.");
            return false;
        }

        if (!gripper_group_ptr_->setNamedTarget(named_position))
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to set named target: %s", named_position.c_str());
            return false;
        }

        moveit::core::MoveItErrorCode result = gripper_group_ptr_->move();
        if (result == moveit::core::MoveItErrorCode::SUCCESS)
        {
            RCLCPP_INFO(this->get_logger(), "Gripper moved to position: %s", named_position.c_str());
            return true;
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to move gripper to %s. Error code: %d",
                         named_position.c_str(), result.val);
            return false;
        }
    }

    bool ArmActionServer::controlGripper(const std::vector<double> &joint_positions)
    {
        if (!gripper_group_ptr_)
        {
            RCLCPP_ERROR(this->get_logger(), "Gripper MoveGroupInterface not initialized.");
            return false;
        }

        if (!gripper_group_ptr_->setJointValueTarget(joint_positions))
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to set joint value target for gripper.");
            return false;
        }

        // 执行前记录目标位置
        RCLCPP_INFO(this->get_logger(), "Attempting to move gripper to position: %f", joint_positions[0]);

        moveit::core::MoveItErrorCode result = gripper_group_ptr_->move();
        if (result == moveit::core::MoveItErrorCode::SUCCESS)
        {
            // 执行后检查当前位置
            std::vector<double> current_positions = gripper_group_ptr_->getCurrentJointValues();
            RCLCPP_INFO(this->get_logger(), "Gripper moved to specified joint positions. Target: %f, Current: %f, Error: %f",
                        joint_positions[0], current_positions[0], 
                        std::abs(joint_positions[0] - current_positions[0]));

            // RCLCPP_INFO(this->get_logger(), "Gripper moved to specified joint positions.");
            return true;
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to move gripper to joint positions. Error code: %d", result.val);
            return false;
        }
    }

    void ArmActionServer::handleGripperControl(
        const std::shared_ptr<ControlGripper::Request> request,
        std::shared_ptr<ControlGripper::Response> response)
    {
        RCLCPP_INFO(this->get_logger(), "Received gripper control request");

        bool success = false;

        // 优先级：joint_positions > named_position > open
        if (!request->joint_positions.empty())
        {
            RCLCPP_INFO(this->get_logger(), "Controlling gripper with joint positions");
            success = controlGripper(request->joint_positions);
        }
        else if (!request->named_position.empty())
        {
            RCLCPP_INFO(this->get_logger(), "Controlling gripper with named position: %s",
                        request->named_position.c_str());
            success = controlGripper(request->named_position);
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "Controlling gripper with open flag: %s",
                        request->open ? "open" : "close");
            success = controlGripper(request->open);
        }

        response->success = success;
        response->message = success ? "Gripper control completed successfully" : "Failed to control gripper";

        RCLCPP_INFO(this->get_logger(), "Gripper control %s", success ? "succeeded" : "failed");
    }

} // namespace piper_control