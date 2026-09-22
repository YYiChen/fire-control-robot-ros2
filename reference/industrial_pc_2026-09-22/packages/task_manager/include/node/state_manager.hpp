// state_manager.hpp
#ifndef STATE_MANAGER_HPP_
#define STATE_MANAGER_HPP_

#include "rclcpp/rclcpp.hpp"
#include <common_interfaces/msg/task_status.hpp>
#include <common_interfaces/msg/screen_info.hpp>
#include <atomic>

namespace task_manager
{
  using TaskStatus = common_interfaces::msg::TaskStatus;
  using ScreenInfo = common_interfaces::msg::ScreenInfo;

  class StateManager
  {
  public:
    StateManager(rclcpp::Node::SharedPtr node);
    ~StateManager() = default;

    void initPublishers();
    
    // 状态更新函数
    void updateTaskStatus(const TaskStatus& status);
    void updateScreenInfo(const ScreenInfo::SharedPtr msg);
    void publishStatus();
    
    // 获取当前状态
    const TaskStatus& getCurrentTaskStatus() const { return current_task_status_; }
    const ScreenInfo& getCurrentScreenInfo() const { return current_screen_info_; }
    
    // 设置发布频率
    void setStatusPublishRate(int rate_hz) { status_publish_rate_ = rate_hz; }
    
  private:
    bool hasTaskStatusChanged(const TaskStatus& new_status);
    
  private:
    rclcpp::Node::SharedPtr node_;
    
    // 发布者
    rclcpp::Publisher<TaskStatus>::SharedPtr task_status_pub_;
    rclcpp::Publisher<ScreenInfo>::SharedPtr screen_info_pub_;
    
    // 当前状态
    TaskStatus current_task_status_;
    ScreenInfo current_screen_info_;
    
    // 上次发布状态
    TaskStatus last_published_status_;
    bool first_status_published_ = false;
    
    // 配置
    int status_publish_rate_ = 2; // 默认2Hz
  };
} // namespace task_manager

#endif // STATE_MANAGER_HPP_