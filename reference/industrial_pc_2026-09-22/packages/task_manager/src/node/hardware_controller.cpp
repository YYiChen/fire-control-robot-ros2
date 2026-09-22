// hardware_controller.cpp
#include "hardware_controller.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

namespace task_manager {

HardwareController::HardwareController(
    rclcpp::Node::SharedPtr node, 
    rclcpp::Node::SharedPtr client_node)
    : node_(node), client_node_(client_node)
{
    // 
    set_camera_params_client_ = client_node_->create_client<SetCameraParamsSrv>("set_camera_params");
    // 创建机械臂控制 Action 客户端
    arm_client_ = rclcpp_action::create_client<ArmControlAction>(client_node_, "/execute_arm_task");
    
    // 初始化升降指令客户端
    lift_command_client_ = client_node_->create_client<LiftCommandSrv>("lift/command");
    
    // 获取面板区域位姿服务
    get_panel_region_pose_client_ = client_node_->create_client<GetPanelRegionPoseSrv>("get_region_pose");
    
    // 获取面板信息服务
    get_panel_info_client_ = client_node_->create_client<GetPanelInfoSrv>("get_panel_info");
    
    // 添加夹爪控制服务客户端
    gripper_control_client_ = client_node_->create_client<ControlGripperSrv>("control_gripper");
    
    // 初始化升降状态订阅者
    lift_status_sub_ = node_->create_subscription<LiftStatus>(
        "lift/status", 10, 
        std::bind(&HardwareController::liftStatusCallback, this, std::placeholders::_1));
}

bool HardwareController::setCameraParams(bool auto_gain_enabled, 
                                       float manual_gain_value,
                                       float auto_gain_lower_limit, 
                                       float auto_gain_upper_limit)
{
    auto request = std::make_shared<SetCameraParamsSrv::Request>();
    request->auto_gain_enabled = auto_gain_enabled;
    request->manual_gain_value = manual_gain_value;
    request->auto_gain_lower_limit = auto_gain_lower_limit;
    request->auto_gain_upper_limit = auto_gain_upper_limit;
    
    // 等待服务可用
    while (!set_camera_params_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 set_camera_params 服务...");
    }
    
    // 发送请求并等待结果
    auto result = set_camera_params_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "相机参数设置结果: %s", response->message.c_str());
        return response->success;
    } else {
        RCLCPP_ERROR(node_->get_logger(), "相机参数设置服务调用失败");
        return false;
    }
}

bool HardwareController::setCameraParamsForLED()
{
    // 针对LED指示灯或屏幕的低增益设置
    return setCameraParams(false, 6.0f);
}

bool HardwareController::setCameraParamsForScreen()
{
    // 针对屏幕的参数设置（与LED相同）
    return setCameraParams(false, 6.0f);
}

bool HardwareController::setCameraParamsForNormal()
{
    // 正常场景的参数设置
    return setCameraParams(false, 16.0f);
}

bool HardwareController::setLiftHeight(int height)
{
    auto request = std::make_shared<LiftCommandSrv::Request>();
    request->command_type = 5;
    request->device_id = 0;
    request->target_height = height;
    
    while (!lift_command_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 lift/command 服务...");
    }
    
    auto result = lift_command_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "%s", response->message.c_str());
        if (response->success) { 
            return true;
        } else {
            return false;
        }
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取升降台控制服务失败");
        return false;
    }
}

void HardwareController::liftStatusCallback(
    const std::shared_ptr<LiftStatus> msg)
{
    cur_lift_height_ = msg->positions[0];
}

bool HardwareController::sendArmControlRequest(
    const ArmControlAction::Goal& goal_msg)
{
    if (!arm_client_->wait_for_action_server(std::chrono::seconds(20))) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂服务不可用");
        return false;
    }

    // 发送目标并等待结果
    auto future = arm_client_->async_send_goal(goal_msg);
    auto send_goal_timeout = std::chrono::seconds(5);
    if (rclcpp::spin_until_future_complete(
        client_node_->get_node_base_interface(), 
        future,
        send_goal_timeout) != rclcpp::FutureReturnCode::SUCCESS)
    {
        RCLCPP_ERROR(node_->get_logger(), "发送机械臂目标失败");
        return false;
    }

    auto goal_handle = future.get();
    if (!goal_handle) {
        RCLCPP_ERROR(node_->get_logger(), "机械臂目标被拒绝");
        return false;
    }

    // 等待动作完成
    auto result_future = arm_client_->async_get_result(goal_handle);
    auto result_timeout = std::chrono::seconds(15);
    if (rclcpp::spin_until_future_complete(
        client_node_->get_node_base_interface(), 
        result_future,
        result_timeout) != rclcpp::FutureReturnCode::SUCCESS) 
    {
        RCLCPP_ERROR(node_->get_logger(), "获取机械臂结果失败");
        return false;
    }

    auto result = result_future.get();
    return result.code == rclcpp_action::ResultCode::SUCCEEDED;
}

// 夹爪控制函数实现
bool HardwareController::controlGripper(bool open)
{
    auto request = std::make_shared<ControlGripperSrv::Request>();
    request->open = open;
    
    while (!gripper_control_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 control_gripper 服务...");
    }
    
    auto result = gripper_control_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "夹爪控制结果: %s", response->message.c_str());
        return response->success;
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取夹爪控制服务失败");
        return false;
    }
}

bool HardwareController::controlGripper(const std::string& named_position)
{
    auto request = std::make_shared<ControlGripperSrv::Request>();
    request->named_position = named_position;
    
    while (!gripper_control_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 control_gripper 服务...");
    }
    
    auto result = gripper_control_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "夹爪控制结果: %s", response->message.c_str());
        return response->success;
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取夹爪控制服务失败");
        return false;
    }
}

bool HardwareController::controlGripper(const std::vector<double>& joint_positions)
{
    auto request = std::make_shared<ControlGripperSrv::Request>();
    request->joint_positions = joint_positions;
    
    while (!gripper_control_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 control_gripper 服务...");
    }
    
    auto result = gripper_control_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "夹爪控制结果: %s", response->message.c_str());
        return response->success;
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取夹爪控制服务失败");
        return false;
    }
}




bool HardwareController::getScreenInfo(
    const std::string& panel_model,
    ScreenInfo& screen_info)
{
    auto request = std::make_shared<GetPanelInfoSrv::Request>();
    request->panel_model = panel_model;
    request->panel_region.region_id = PanelRegionMsg::SCREEN;

    while (!get_panel_info_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 get_panel_info 服务...");
    }
    
    auto result = get_panel_info_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "%s", response->message.c_str());
        if (response->success) { 
            screen_info = response->screen_info;
            return true;
        } else {
            return false;
        }
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取屏幕信息服务失败");
        return false;
    }
}

bool HardwareController::getIndicatorLights(
    const std::string& panel_model,
    std::vector<LEDStatus>& indicator_lights)
{
    auto request = std::make_shared<GetPanelInfoSrv::Request>();
    request->panel_model = panel_model;
    request->panel_region.region_id = PanelRegionMsg::LED;

    while (!get_panel_info_client_->wait_for_service(std::chrono::seconds(1))) {
        RCLCPP_INFO(node_->get_logger(), "等待 get_panel_info 服务...");
    }
    
    auto result = get_panel_info_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        RCLCPP_INFO(node_->get_logger(), "%s", response->message.c_str());
        if (response->success) { 
            indicator_lights = response->indicator_lights;
            return true;
        } else {
            return false;
        }
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取指示灯信息服务失败");
        return false;
    }
}

// bool HardwareController::getDeviceChannelInfo(
//     const std::string& panel_model,
//     DeviceChannelInfo& device_channel_info)
// {
//     auto request = std::make_shared<GetPanelInfoSrv::Request>();
//     request->panel_model = panel_model;
//     request->panel_region.region_id = PanelRegionMsg::DEVICE_CHANNEL;

//     while (!get_panel_info_client_->wait_for_service(std::chrono::seconds(1))) {
//         RCLCPP_INFO(node_->get_logger(), "等待 get_panel_info 服务...");
//     }
    
//     auto result = get_panel_info_client_->async_send_request(request);
//     if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
//         rclcpp::FutureReturnCode::SUCCESS) 
//     {
//         auto response = result.get();
//         RCLCPP_INFO(node_->get_logger(), "%s", response->message.c_str());
//         if (response->success) { 
//             // 注意：需要在GetPanelInfo.srv中添加DeviceChannelInfo字段
//             // device_channel_info = response->device_channel_info;
//             return true;
//         } else {
//             return false;
//         }
//     } else {
//         RCLCPP_ERROR(node_->get_logger(), "获取设备通道信息服务失败");
//         return false;
//     }
// }

// 重载
bool HardwareController::getPanelRegionPose(
    const int region_id, 
    geometry_msgs::msg::PoseStamped& region_pose)
{ 
    std::string message;
    return getPanelRegionPose(region_id, region_pose, message);
}
bool HardwareController::getPanelRegionPose(
    const int region_id, 
    geometry_msgs::msg::PoseStamped& region_pose,
    std::string& message)
{
    auto request = std::make_shared<GetPanelRegionPoseSrv::Request>();
    request->panel_region.region_id = region_id;
    
    while (!get_panel_region_pose_client_->wait_for_service(std::chrono::seconds(5))) {
        RCLCPP_INFO(node_->get_logger(), "等待 get_region_pose 服务...");
    }
    
    auto result = get_panel_region_pose_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
        rclcpp::FutureReturnCode::SUCCESS) 
    {
        auto response = result.get();
        message = response->message;
        RCLCPP_INFO(node_->get_logger(), "%s", response->message.c_str());
        if (response->success) { 
            region_pose = response->pose;
            return true;
        } else {
            return false;
        }
    } else {
        RCLCPP_ERROR(node_->get_logger(), "获取面板区域姿态服务失败");
        return false;
    }
}

geometry_msgs::msg::PoseStamped HardwareController::computePoseAboveAruco(
    const geometry_msgs::msg::PoseStamped& aruco_pose_in_base,
    double height_above,
    bool flip_z_axis)
{
    // 1. 获取 Aruco 的变换 tf
    tf2::Transform tf_base_aruco;
    tf2::fromMsg(aruco_pose_in_base.pose, tf_base_aruco);

    // 2. 构造 Aruco 上方目标点的姿态（位置 + 姿态）
    geometry_msgs::msg::Pose region_pose_in_aruco;
    region_pose_in_aruco.position.x = 0.0; // 相对于Aruco原点的坐标
    region_pose_in_aruco.position.y = 0.0;
    region_pose_in_aruco.position.z = height_above;
    // 将区域姿态设置为无旋转，即和 Aruco 的坐标轴完全一致（X 朝右、Y 朝上、Z 朝外）
    region_pose_in_aruco.orientation = tf2::toMsg(tf2::Quaternion(0, 0, 0, 1));
    tf2::Transform tf_aruco_target;
    tf2::fromMsg(region_pose_in_aruco, tf_aruco_target);

    tf2::Quaternion aruco_ori;
    tf2::fromMsg(aruco_pose_in_base.pose.orientation, aruco_ori);

    // 3. 变换到 base 坐标系：T_base_target = T_base_aruco * T_aruco_target
    tf2::Transform tf_base_target = tf_base_aruco * tf_aruco_target;

    // 4. 旋转 Aruco 的姿态
    if (flip_z_axis) {
        // 翻转 Z 轴方向（180°绕 X 轴）
        tf2::Quaternion flip_quat;
        // flip_quat.setRPY(M_PI, 0, 0);  // 180度绕X轴
        flip_quat.setRPY(M_PI, 0, -M_PI/2);  //180度绕X轴, -90度绕Z轴
        tf2::Quaternion adjusted_ori = aruco_ori * flip_quat;
        adjusted_ori.normalize();
        tf_base_target.setRotation(adjusted_ori);
    }
    else {
        // 设置固定旋转
        tf2::Quaternion fixed_rotation;
        fixed_rotation.setValue(0.49365, 0.5048, -0.468, 0.53148);
        fixed_rotation.normalize(); // 确保四元数标准化
        tf_base_target.setRotation(fixed_rotation);
    }

    // 5. 输出结果
    geometry_msgs::msg::PoseStamped target_pose_in_base;
    target_pose_in_base.header = aruco_pose_in_base.header; // 保持 frame_id 和时间戳
    geometry_msgs::msg::Pose pose_msg;
    tf2::toMsg(tf_base_target, pose_msg);
    target_pose_in_base.pose = pose_msg;

    return target_pose_in_base;
}

} // namespace task_manager