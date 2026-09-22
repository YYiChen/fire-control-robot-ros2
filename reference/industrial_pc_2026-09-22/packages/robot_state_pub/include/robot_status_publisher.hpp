#ifndef ROBOT_STATUS_PUBLISHER_HPP_
#define ROBOT_STATUS_PUBLISHER_HPP_

#include "rclcpp/rclcpp.hpp"
#include "common_interfaces/msg/task_status.hpp"
#include "common_interfaces/msg/robot_status.hpp"

class RobotStatusPublisher : public rclcpp::Node
{
public:
    explicit RobotStatusPublisher(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
    
private:
    void timerCallback();
    void initPublisher();
    void taskStatusCallback(const common_interfaces::msg::TaskStatus::SharedPtr msg);

    rclcpp::Publisher<common_interfaces::msg::RobotStatus>::SharedPtr robot_status_pub_;
    rclcpp::Subscription<common_interfaces::msg::TaskStatus>::SharedPtr task_status_sub_;

    rclcpp::TimerBase::SharedPtr timer_;

    common_interfaces::msg::RobotStatus robot_status_msg_;
};

#endif // ROBOT_STATUS_PUBLISHER_HPP_