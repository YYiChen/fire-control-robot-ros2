#include "state_manager.hpp"
#include <rclcpp/rclcpp.hpp>

namespace task_manager {

StateManager::StateManager(rclcpp::Node::SharedPtr node)
    : node_(node)
{
    initPublishers();
}

void StateManager::initPublishers()
{
    if (node_) {
        task_status_pub_ = node_->create_publisher<TaskStatus>("/task/status", 10);
        screen_info_pub_ = node_->create_publisher<ScreenInfo>("/screen/info", 10);
    }
}

void StateManager::updateTaskStatus(const TaskStatus& status)
{
    current_task_status_ = status;
    publishStatus();
}

void StateManager::updateScreenInfo(const ScreenInfo::SharedPtr msg)
{
    if (msg) {
        current_screen_info_ = *msg;
        publishStatus();
    }
}

void StateManager::publishStatus()
{
    // 如果是第一次发布，或者状态发生了变化，则发布
    if (!first_status_published_ || hasTaskStatusChanged(last_published_status_)) {
        // 发布任务状态
        if (task_status_pub_) {
            task_status_pub_->publish(current_task_status_);
        }
        
        // 发布屏幕信息
        if (screen_info_pub_) {
            screen_info_pub_->publish(current_screen_info_);
        }
        
        // 更新最后发布状态
        last_published_status_ = current_task_status_;
        first_status_published_ = true;
    }
}

bool StateManager::hasTaskStatusChanged(const TaskStatus& new_status)
{
    return (current_task_status_.task_type != new_status.task_type) ||
           (current_task_status_.subtask_type != new_status.subtask_type) ||
           (current_task_status_.status != new_status.status) ||
           (current_task_status_.error_message != new_status.error_message);
}

} // namespace task_manager