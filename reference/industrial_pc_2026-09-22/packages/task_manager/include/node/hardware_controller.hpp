// hardware_controller.hpp
#ifndef HARDWARE_CONTROLLER_HPP_
#define HARDWARE_CONTROLLER_HPP_

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <common_interfaces/action/execute_arm_task.hpp>
#include <common_interfaces/srv/lift_command.hpp>
#include <common_interfaces/srv/get_panel_info.hpp>
#include <common_interfaces/srv/get_panel_region_pose.hpp>
#include <common_interfaces/msg/lift_status.hpp>
#include <common_interfaces/msg/screen_info.hpp>
#include <common_interfaces/msg/led_status.hpp>
#include <common_interfaces/msg/panel_region.hpp>
#include <common_interfaces/srv/control_gripper.hpp>
#include <common_interfaces/srv/set_camera_params.hpp>

#include "task_config_types.hpp"

namespace task_manager
{
  using ArmControlAction = common_interfaces::action::ExecuteArmTask;
  using GoalHandleArmControl = rclcpp_action::ClientGoalHandle<ArmControlAction>;
  using LiftCommandSrv = common_interfaces::srv::LiftCommand;
  using GetPanelInfoSrv = common_interfaces::srv::GetPanelInfo;
  using GetPanelRegionPoseSrv = common_interfaces::srv::GetPanelRegionPose;
  using LiftStatus = common_interfaces::msg::LiftStatus;
  using ScreenInfo = common_interfaces::msg::ScreenInfo;
  using LEDStatus = common_interfaces::msg::LEDStatus;
  using PanelRegionMsg = common_interfaces::msg::PanelRegion;
  using ControlGripperSrv = common_interfaces::srv::ControlGripper;
  using SetCameraParamsSrv = common_interfaces::srv::SetCameraParams;

  class HardwareController
  {
  public:
    HardwareController(rclcpp::Node::SharedPtr node, rclcpp::Node::SharedPtr client_node);
    ~HardwareController() = default;
    // 相机参数设置
    bool setCameraParams(bool auto_gain_enabled, 
                        float manual_gain_value,
                        float auto_gain_lower_limit = 0.1, 
                        float auto_gain_upper_limit = 16.0);
    // 预设参数方法
    bool setCameraParamsForLED();      // 针对LED指示灯的参数设置
    bool setCameraParamsForScreen();   // 针对屏幕的参数设置
    bool setCameraParamsForNormal();   // 正常场景的参数设置
    
    // 升降台控制
    bool setLiftHeight(int height);
    int getCurrentLiftHeight() const { return cur_lift_height_; }
    void liftStatusCallback(const std::shared_ptr<LiftStatus> msg);

    // 机械臂控制
    bool sendArmControlRequest(const ArmControlAction::Goal& goal_msg);

    // 夹爪控制
    bool controlGripper(bool open);
    bool controlGripper(const std::string& named_position);
    bool controlGripper(const std::vector<double>& joint_positions);
    
    bool getScreenInfo(const std::string& panel_model,
                      ScreenInfo& screen_info);
    bool getIndicatorLights(const std::string& panel_model,
                            std::vector<LEDStatus>& indicator_lights);  
    // bool getDeviceChannelInfo(const std::string& panel_model,
    //                           DeviceChannelInfo& device_channel_info);

    bool getPanelRegionPose(
      const int region_id, 
      geometry_msgs::msg::PoseStamped& region_pose);

    bool getPanelRegionPose(const int region_id, 
      geometry_msgs::msg::PoseStamped& region_pose,
      std::string& message);
    
    // 位姿计算
    geometry_msgs::msg::PoseStamped computePoseAboveAruco(
        const geometry_msgs::msg::PoseStamped& aruco_pose_in_base,
        double height_above,
        bool flip_z_axis = false);

  private:
    rclcpp::Node::SharedPtr node_;
    rclcpp::Node::SharedPtr client_node_;
    
    // 客户端和服务
    rclcpp_action::Client<ArmControlAction>::SharedPtr arm_client_;
    rclcpp::Client<LiftCommandSrv>::SharedPtr lift_command_client_;
    rclcpp::Client<GetPanelInfoSrv>::SharedPtr get_panel_info_client_;
    rclcpp::Client<GetPanelRegionPoseSrv>::SharedPtr get_panel_region_pose_client_;
    rclcpp::Client<ControlGripperSrv>::SharedPtr gripper_control_client_;
    rclcpp::Subscription<LiftStatus>::SharedPtr lift_status_sub_;
    rclcpp::Client<SetCameraParamsSrv>::SharedPtr set_camera_params_client_;

    // 状态数据
    int cur_lift_height_ = 0;
  };
} // namespace task_manager

#endif // HARDWARE_CONTROLLER_HPP_