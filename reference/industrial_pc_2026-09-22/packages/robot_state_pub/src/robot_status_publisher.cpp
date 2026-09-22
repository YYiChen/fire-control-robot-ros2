#include "robot_status_publisher.hpp"

RobotStatusPublisher::RobotStatusPublisher(const rclcpp::NodeOptions &options)
    : Node("robot_status_publisher", options)
{
    initPublisher();
}

void RobotStatusPublisher::initPublisher()
{
    robot_status_pub_ = this->create_publisher<common_interfaces::msg::RobotStatus>("/fia_robot/status", 10);
    task_status_sub_ = this->create_subscription<common_interfaces::msg::TaskStatus>(
        "/task/status",
        10,
        [this](const common_interfaces::msg::TaskStatus::SharedPtr msg) {
            this->taskStatusCallback(msg);
        });
    
    timer_ = this->create_wall_timer(
        std::chrono::seconds(1),
        [this]() { this->timerCallback(); });
}

void RobotStatusPublisher::taskStatusCallback(const common_interfaces::msg::TaskStatus::SharedPtr msg)
{
    // RCLCPP_INFO(this->get_logger(),
    //             "Received Task: %s, Type: %u, SubType: %u, Status: %u",
    //             msg->task_id.c_str(),
    //             msg->task_type,
    //             msg->subtask_type,
    //             msg->status);
    robot_status_msg_.task_status = *msg;
}

void RobotStatusPublisher::timerCallback()
{

    robot_status_msg_.battery_percent = 85.5f;  


    // RCLCPP_INFO(this->get_logger(), "Publishing Robot Status");
    robot_status_pub_->publish(robot_status_msg_);
}