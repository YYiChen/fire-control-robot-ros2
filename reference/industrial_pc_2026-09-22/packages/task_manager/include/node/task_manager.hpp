// task_manager.hpp
#ifndef TASK_MANAGER_HPP_
#define TASK_MANAGER_HPP_

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <common_interfaces/action/task_js.hpp>
#include <memory>
#include <atomic>

// 前向声明
namespace task_manager {
    class TaskExecutor;
    class HardwareController;
    class StateManager;
    class ConfigurationManager;
}

namespace task_manager
{
    using TaskJsAction = common_interfaces::action::TaskJs;
    using GoalHandleTaskJs = rclcpp_action::ServerGoalHandle<TaskJsAction>;

    class TaskManager : public rclcpp::Node
    {
    public:
        // 添加静态创建函数
        static std::shared_ptr<TaskManager> create();
        ~TaskManager() = default;

        rclcpp::Node::SharedPtr getClientNode();

    private:
        // 将构造函数设为私有
        TaskManager();
        
        // Action 服务回调
        rclcpp_action::GoalResponse taskJsHandleGoal(
            const rclcpp_action::GoalUUID &uuid,
            std::shared_ptr<const TaskJsAction::Goal> goal);
        
        rclcpp_action::CancelResponse taskJsHandleCancel(
            const std::shared_ptr<GoalHandleTaskJs> goal_handle);
        
        void taskJsHandleAccepted(const std::shared_ptr<GoalHandleTaskJs> goal_handle);

        // 初始化函数
        void initParam();
        void initNode();

        // 成员变量
        std::string task_config_path_;
        std::shared_ptr<rclcpp::Node> client_node_;
        std::shared_ptr<ConfigurationManager> config_manager_;
        std::shared_ptr<StateManager> state_manager_;
        std::shared_ptr<HardwareController> hardware_controller_;
        std::shared_ptr<TaskExecutor> task_executor_;
        
        rclcpp_action::Server<TaskJsAction>::SharedPtr taskJs_server_;
        std::atomic<bool> is_running_task_{false};
    };

} // namespace task_manager

#endif // TASK_MANAGER_HPP_