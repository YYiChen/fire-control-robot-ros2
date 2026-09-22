// task_manager.cpp
#include "task_manager.hpp"
#include "task_executor.hpp"
#include "hardware_controller.hpp"
#include "state_manager.hpp"
#include "configuration_manager.hpp"

namespace task_manager {

// 实现静态创建函数
std::shared_ptr<TaskManager> TaskManager::create()
{
    // 首先创建对象
    auto task_manager = std::shared_ptr<TaskManager>(new TaskManager());
    
    // 初始化状态管理器
    task_manager->state_manager_ = std::make_shared<StateManager>(task_manager);
    
    // 初始化硬件控制器
    task_manager->hardware_controller_ = std::make_shared<HardwareController>(
        task_manager,
        task_manager->client_node_);
        
    // 初始化任务执行器
    task_manager->task_executor_ = std::make_shared<TaskExecutor>(
        task_manager->hardware_controller_,
        task_manager->config_manager_->getGlobalConfig(),
        task_manager->config_manager_->getTaskPoints(),
        task_manager);

    // 设置回调函数
    task_manager->task_executor_->setTaskStatusCallback(
        [state_manager = task_manager->state_manager_](const TaskStatus& status) {
            state_manager->updateTaskStatus(status);
        }
    );

    task_manager->task_executor_->setScreenStateCallback(
        [state_manager = task_manager->state_manager_](const ScreenInfo& info) {
            state_manager->updateScreenInfo(std::make_shared<ScreenInfo>(info));
        }
    );
    
    return task_manager;
}

TaskManager::TaskManager()
    : Node("task_manager")
{
    initParam();
    initNode();
}

void TaskManager::initParam()
{
    this->declare_parameter<std::string>("task_config_path", "");
    this->get_parameter_or<std::string>("task_config_path", task_config_path_, "");
    
    // 初始化配置管理器
    config_manager_ = std::make_shared<ConfigurationManager>();
    config_manager_->loadTaskConfig(task_config_path_);
}

void TaskManager::initNode()
{
    // 创建客户端节点
    client_node_ = std::make_shared<rclcpp::Node>("task_manager_client");
    
    // 创建Action服务端
    taskJs_server_ = rclcpp_action::create_server<TaskJsAction>(
        this,
        "/execute_js_task",
        std::bind(&TaskManager::taskJsHandleGoal, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&TaskManager::taskJsHandleCancel, this, std::placeholders::_1),
        std::bind(&TaskManager::taskJsHandleAccepted, this, std::placeholders::_1)
    );
}

rclcpp::Node::SharedPtr TaskManager::getClientNode()
{
    return client_node_;
}

// Action服务回调实现
rclcpp_action::GoalResponse TaskManager::taskJsHandleGoal(
    const rclcpp_action::GoalUUID &uuid,
    std::shared_ptr<const TaskJsAction::Goal> goal) 
{
    RCLCPP_INFO(this->get_logger(), "收到新任务请求: %s", goal->task_cfg.c_str());
    (void)uuid;
    
    if (is_running_task_.load()) {
        RCLCPP_WARN(this->get_logger(), "有任务正在运行，拒绝新任务");
        return rclcpp_action::GoalResponse::REJECT;
    }

    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse TaskManager::taskJsHandleCancel(
    const std::shared_ptr<GoalHandleTaskJs> goal_handle) 
{
    RCLCPP_INFO(this->get_logger(), "任务被取消");
    (void)goal_handle;
    return rclcpp_action::CancelResponse::ACCEPT;
}

void TaskManager::taskJsHandleAccepted(const std::shared_ptr<GoalHandleTaskJs> goal_handle) 
{
    is_running_task_.store(true);
    // 将任务执行委托给TaskExecutor
    std::thread([this, goal_handle]() {
        task_executor_->executeTaskSequence(goal_handle);
        is_running_task_.store(false);
    }).detach();
}

} // namespace task_manager