// task_executor.hpp
#ifndef TASK_EXECUTOR_HPP_
#define TASK_EXECUTOR_HPP_

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <common_interfaces/action/task_js.hpp>
#include <common_interfaces/msg/screen_info.hpp>
#include <common_interfaces/msg/led_status.hpp>
#include <common_interfaces/msg/task_status.hpp>
#include <common_interfaces/msg/panel_region.hpp>
#include "hardware_controller.hpp"
#include "task_config_types.hpp"
#include <map>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace task_manager
{
  using TaskJsAction = common_interfaces::action::TaskJs;
  using GoalHandleTaskJs = rclcpp_action::ServerGoalHandle<TaskJsAction>;
  using TaskStatus = common_interfaces::msg::TaskStatus;
  using PanelRegionMsg = common_interfaces::msg::PanelRegion;
  using ScreenInfo = common_interfaces::msg::ScreenInfo;
  using LEDStatus = common_interfaces::msg::LEDStatus;

  class TaskExecutor
  {
  public:
    TaskExecutor(std::shared_ptr<HardwareController> hardware_controller,
                 const GlobalConfig& global_config,
                 const std::vector<TaskPoint>& task_points,
                 rclcpp::Node::SharedPtr node);
    ~TaskExecutor() = default;

    // 主要任务执行函数
    bool executeTaskSequence(const std::shared_ptr<GoalHandleTaskJs> goal_handle);
    
    // 具体任务函数
    bool executeInitializeSystem(const std::shared_ptr<GoalHandleTaskJs> goal_handle);
    bool executeHandleAlarm(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                           const TaskStep& step_config);
    bool executeRoutinePatrol(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                             const TaskStep& step_config);
    bool executeKeyEnableTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                              const TaskStep& step_config);
    void setTaskStatusCallback(std::function<void(const TaskStatus&)> callback) {
        task_status_callback_ = callback;
    }
    
    void setScreenStateCallback(std::function<void(const ScreenInfo&)> callback) {
        screen_state_callback_ = callback;
    }

  private:
    void recordArPoseData(const geometry_msgs::msg::PoseStamped& ar_pose, const std::string& pose_type);
    std::vector<std::function<bool()>> createConfigurableTaskSteps(
        const std::shared_ptr<GoalHandleTaskJs> goal_handle);
    
    // 辅助任务函数
    bool executeLiftControl(const std::shared_ptr<GoalHandleTaskJs> goal_handle, const int& height);
    bool executeArmControl(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                          const geometry_msgs::msg::Pose& target_pose);
    bool executeJointAngleControl(
          const std::shared_ptr<GoalHandleTaskJs> goal_handle,
          std::vector<std::string> joint_names,
          std::vector<double> target_angles);
    bool executeGripperPick(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                              const TaskStep& step_config);
    bool executeArucoLocalization(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                                     const TaskStep& step_config);
    bool executeIndicatorInfo(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                                          const TaskStep& step_config);
    bool executeScreenInfo(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                                       const TaskStep& step_config);
    bool executeKeyEnableInfo(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                                          const TaskStep& step_config);
    bool executeDeviceChannelInfo(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                                  const TaskStep& step_config);
    bool executeClickButtonTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                               const geometry_msgs::msg::PoseStamped& target_pose);
    bool executeAdjustButtonPositionTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                                        const TaskStep& step_config);
    bool executeMuteAlarmTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                             const TaskStep& step_config);
    bool executeResetAlarmTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                              const TaskStep& step_config);
    bool executeInputPasswordTask(const std::shared_ptr<GoalHandleTaskJs> goal_handle, 
                                 const TaskStep& step_config);

    // 管理函数
    bool handleTaskFailure(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                          const std::string& error_message);
    void updateProgress(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                       int current_step, int total_steps,
                       const std::shared_ptr<TaskJsAction::Feedback>& feedback);
    bool checkCancel(const std::shared_ptr<GoalHandleTaskJs> goal_handle,
                    const std::shared_ptr<TaskJsAction::Result>& result);
    
    void updateTaskStatus(uint8_t task_type, uint8_t subtask_type, uint8_t status, 
                         const std::string& error_message = "");

  private:
    std::shared_ptr<HardwareController> hardware_controller_;
    GlobalConfig global_config_;
    std::vector<TaskPoint> task_points_;
    rclcpp::Node::SharedPtr node_;
    std::string facp_model_;
    
    // 回调函数
    std::function<void(const TaskStatus&)> task_status_callback_;
    std::function<void(const ScreenInfo&)> screen_state_callback_;
    
    // 当前状态
    ScreenInfo cur_screen_info_;
    std::vector<LEDStatus> cur_indicator_lights_;
  };
} // namespace task_manager

#endif // TASK_EXECUTOR_HPP_