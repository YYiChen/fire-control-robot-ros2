// #include <task_manager.hpp>

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   auto node = std::make_shared<task_manager::TaskManager>();
//   rclcpp::executors::MultiThreadedExecutor exec;
//   exec.add_node(node);
//   exec.spin();
//   rclcpp::shutdown();
//   return 0;
// }


// main.cpp
#include "task_manager.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    
    // 使用新的创建方法
    auto task_manager = task_manager::TaskManager::create();
    
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(task_manager);
    executor.spin();
    
    rclcpp::shutdown();
    return 0;
}