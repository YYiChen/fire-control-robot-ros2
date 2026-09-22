#include "rclcpp/rclcpp.hpp"
#include "arm_action_server.hpp"
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto action_server_node  = std::make_shared<piper_control::ArmActionServer>(rclcpp::NodeOptions());
    // auto action_server_node = rclcpp::create_node<piper_control::ArmActionServer>("arm_action_server", rclcpp::NodeOptions());
    
    action_server_node->initialize();
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(action_server_node);
    executor.spin();
    rclcpp::shutdown();
    return 0;
}