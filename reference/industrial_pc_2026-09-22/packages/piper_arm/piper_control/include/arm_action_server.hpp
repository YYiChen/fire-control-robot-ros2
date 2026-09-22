#ifndef PIPER_CONTROL_ARM_ACTION_SERVER_HPP_
#define PIPER_CONTROL_ARM_ACTION_SERVER_HPP_

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <common_interfaces/action/execute_arm_task.hpp>
#include <common_interfaces/srv/control_gripper.hpp>
#include "geometry_msgs/msg/pose.hpp"

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>


#include <memory>
#include <thread>

namespace piper_control
{

class ArmActionServer : public rclcpp::Node
{
public:
    using ExecuteArmTask = common_interfaces::action::ExecuteArmTask;
    using GoalHandleExecuteArmTask = rclcpp_action::ServerGoalHandle<ExecuteArmTask>;
    using ControlGripper = common_interfaces::srv::ControlGripper;

    explicit ArmActionServer(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
    void initialize();

    
    
private:
    rclcpp_action::Server<ExecuteArmTask>::SharedPtr action_server_;
    rclcpp::Service<ControlGripper>::SharedPtr gripper_service_;

    std::shared_ptr<moveit::planning_interface::MoveGroupInterface> arm_group_ptr_; 
    std::shared_ptr<moveit::planning_interface::MoveGroupInterface> gripper_group_ptr_;
    moveit::planning_interface::PlanningSceneInterface psi;
    // std::atomic<bool> is_running_task_{false};
    bool is_running_task_ = false;
    std::mutex task_mutex_;

    bool moveToTarget(const geometry_msgs::msg::Pose &target_pose,
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle);
    bool moveToTargetAndWait(const geometry_msgs::msg::Pose &target_pose);
    bool moveToTargetAndWait(const geometry_msgs::msg::PoseStamped &target_pose,
                             const std::string& end_effector_link);
    bool moveToTargetImproved(const geometry_msgs::msg::PoseStamped &target_pose);

    bool rotateSpecificJoint(const std::string& joint_name, double target_angle);
    bool rotateSpecificJointDegrees(const std::string& joint_name, double target_angle_degrees);
    bool setAllJointsToZero();

    rclcpp_action::GoalResponse handle_goal(
        const rclcpp_action::GoalUUID &uuid,
        std::shared_ptr<const ExecuteArmTask::Goal> goal);

    rclcpp_action::CancelResponse handle_cancel(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle);

    void handle_accepted(const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle);

    void execute(const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle);
    void executeJointControl(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle,
        const std::shared_ptr<const ExecuteArmTask::Goal> goal,
        const std::shared_ptr<ExecuteArmTask::Feedback> feedback,
        const std::shared_ptr<ExecuteArmTask::Result> result);
        
    void executeWaypointControl(
        const std::shared_ptr<GoalHandleExecuteArmTask> goal_handle,
        const std::shared_ptr<const ExecuteArmTask::Goal> goal,
        const std::shared_ptr<ExecuteArmTask::Feedback> feedback,
        const std::shared_ptr<ExecuteArmTask::Result> result);
    
    void waitForStability(double wait_time_seconds = 0.5);
    
    bool waitForEndEffectorStability(double timeout_seconds = 2.0,
                                    double position_threshold = 0.001, // 1mm
                                    double orientation_threshold = 0.01);

    // 夹爪控制相关函数
    bool controlGripper(bool open);
    bool controlGripper(const std::string& named_position);
    bool controlGripper(const std::vector<double>& joint_positions);
    
    // 添加服务回调函数
    void handleGripperControl(
        const std::shared_ptr<ControlGripper::Request> request,
        std::shared_ptr<ControlGripper::Response> response);

};

} // namespace piper_control

#endif // PIPER_CONTROL_ARM_ACTION_SERVER_HPP_