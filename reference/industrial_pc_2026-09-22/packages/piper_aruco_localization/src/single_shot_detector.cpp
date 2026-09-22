#include <iostream>

#include "aruco/aruco.h"
#include "aruco/cvdrawingutils.h"
#include "aruco_ros_utils.hpp"
#include "opencv2/opencv.hpp"
#include "panel_config_types.hpp"

#if __has_include("cv_bridge/cv_bridge.hpp")
#include "cv_bridge/cv_bridge.hpp"
#else
#include "cv_bridge/cv_bridge.h"
#endif
#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"

#include "rclcpp/rclcpp.hpp"
#include "rcpputils/asserts.hpp"
#include "sensor_msgs/image_encodings.hpp"
#include "tf2_ros/transform_broadcaster.h"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "common_interfaces/srv/trigger_image_capture.hpp"
#include "common_interfaces/srv/get_camera_info.hpp"
#include "common_interfaces/srv/get_panel_region_pose.hpp"

#include "pose_filter.hpp"

using TriggerImageCapture = common_interfaces::srv::TriggerImageCapture;
using GetCameraInfo = common_interfaces::srv::GetCameraInfo;

using GetPanelRegionPose = common_interfaces::srv::GetPanelRegionPose;

class SingleShotDetector : public rclcpp::Node
{

// 数据结构用于存储检测数据
struct DetectionData {
    std::vector<tf2::Transform> aruco_transforms;
    std::vector<geometry_msgs::msg::PoseStamped> aruco_poses;
    std::vector<tf2::Vector3> offset_errors;
    int successful_detections = 0;
};

struct FilteredData {
    std::vector<tf2::Transform> filtered_transforms;
    std::vector<tf2::Vector3> filtered_offset_errors;
};

// 处理标记检测结果的结构
struct MarkerProcessResult {
    bool main_found = false;
    bool aux_found = false;
    tf2::Transform main_marker_transform;
    tf2::Transform aux_marker_transform;
    geometry_msgs::msg::PoseStamped aruco_pose_in_base;
};

private:
    rclcpp::Node::SharedPtr client_node_;
    rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;
    cv::Mat inImage_;
    aruco::CameraParameters camParam_;
    tf2::Stamped<tf2::Transform> rightToLeft_;
    bool useRectifiedImages_;            // 是否使用矫正后的图像
    aruco::MarkerDetector mDetector_;    // 标记检测器
    std::vector<aruco::Marker> markers_; // 标记列表
    bool cam_info_received_;             // 相机信息是否已接收

    std::string arm_eih_path_; // 手眼标定文件路径
    std::string aruco_panel_path_;
    std::string arm_base_frame_;     // 机械臂基坐标系
    std::string arm_effector_frame_; // 末端坐标系
    std::string marker_frame_;       // 标记帧
    std::string camera_frame_;       // 相机帧

    double marker_size_; // 标记大小
    int marker_id_;      // 标记ID

    int sample_count_;       // 采样计数器
    double sample_delay_ms_; // 采样延时
    tf2::Vector3 offset_compensation_{0, 0, 0}; // 主辅标记偏移补偿

    // 存储区域位姿
    std::map<int, geometry_msgs::msg::PoseStamped> region_poses_map;

    // 存储区域和AR码之间的位置关系
    std::map<int, std::vector<double>> panel_regions_map;
    // 面板配置
    panel_config::PanelConfig panel_config_;                                              // 是否已完成初始标定

    geometry_msgs::msg::TransformStamped hand_eye_transform;                  // 手眼变换
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;                 // 变换监听器
    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;                              // 变换缓冲区
    std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;           // 变换广播器
    rclcpp::Client<TriggerImageCapture>::SharedPtr trigger_image_client_;     // 触发图像捕获客户端
    rclcpp::Client<GetCameraInfo>::SharedPtr get_camera_info_client_;         // 获取相机信息客户端
    rclcpp::Service<GetPanelRegionPose>::SharedPtr get_panel_region_service_; // 检测AR码服务端

public:
    SingleShotDetector()
        : Node("single_shot_detector"), cam_info_received_(false)
    {
        initParameters();
        initNode();
    }

    void initParameters()
    {
        // 检测是否有corner_refinement参数，如果有，输出警告（Aruco 3.0.0已废弃该参数）
        if (this->has_parameter("corner_refinement"))
        {
            RCLCPP_WARN(
                this->get_logger(),
                "Corner refinement options have been removed in ArUco 3.0.0, "
                "corner_refinement ROS parameter is deprecated");
        }

        // 声明节点参数
        this->declare_parameter<std::string>("aruco_panel_path", "");
        this->declare_parameter<double>("marker_size", 0.05);               // 标记大小
        this->declare_parameter<int>("marker_id", 300);                     // 标记ID
        this->declare_parameter<std::string>("arm_eih_path", "");           //  手眼标定参数文件路径
        this->declare_parameter<bool>("image_is_rectified", true);          // 是否使用矫正后的图像
        this->declare_parameter<float>("min_marker_size", 0.02);            // 最小标记大小
        this->declare_parameter<std::string>("detection_mode", "");         // 检测模式
        this->declare_parameter<std::string>("parameter_preset", "robust"); // 参数预设: robust, high_precision, custom
        this->declare_parameter<int>("sample_count", 5);
        this->declare_parameter<double>("sample_delay_ms", 100.0);

        // 获取最小标记大小
        float min_marker_size; // percentage of image area
        this->get_parameter_or<float>("min_marker_size", min_marker_size, 0.02);

        // 获取检测模式参数，默认"DM_FAST"，并设置Aruco检测器的检测模式
        std::string detection_mode;
        this->get_parameter_or<std::string>("detection_mode", detection_mode, "DM_FAST");
        if (detection_mode == "DM_FAST")
        {
            mDetector_.setDetectionMode(aruco::DM_FAST, min_marker_size);
        }
        else if (detection_mode == "DM_VIDEO_FAST")
        {
            mDetector_.setDetectionMode(aruco::DM_VIDEO_FAST, min_marker_size);
        }
        else
        {
            // Aruco version 2 mode
            mDetector_.setDetectionMode(aruco::DM_NORMAL, min_marker_size);
        }

        // 根据参数预设设置详细参数
        std::string parameter_preset;
        this->get_parameter_or<std::string>("parameter_preset", parameter_preset, "robust");

        if (parameter_preset == "high_precision")
        {
            setHighPrecisionParameters();
            RCLCPP_INFO(this->get_logger(), "使用高精度参数预设");
        }
        else if (parameter_preset == "robust")
        {
            setRobustParameters();
            RCLCPP_INFO(this->get_logger(), "使用鲁棒性参数预设");
        }
        else
        {
            // 自定义参数设置或使用默认参数
            setupCustomParameters();
            RCLCPP_INFO(this->get_logger(), "使用自定义参数配置");
        }

        markers_.reserve(2); // 预留空间

        // 获取其他参数
        this->get_parameter_or<double>("marker_size", marker_size_, 0.05);
        this->get_parameter_or<int>("marker_id", marker_id_, 300);
        this->get_parameter_or<std::string>("arm_eih_path", arm_eih_path_, "");
        this->get_parameter_or<std::string>("aruco_panel_path", aruco_panel_path_, "");
        this->get_parameter_or<bool>("image_is_rectified", useRectifiedImages_, true);
        this->get_parameter_or<int>("sample_count", sample_count_, 5);
        this->get_parameter_or<double>("sample_delay_ms", sample_delay_ms_, 50.0);
        loadPanelRegionsMap();
    }

    // 单独的参数变更处理函数
    rcl_interfaces::msg::SetParametersResult onParameterChange(
        const std::vector<rclcpp::Parameter> &parameters)
    {
        bool need_reload = false;
        std::string new_aruco_panel_path = aruco_panel_path_;
        std::string new_arm_eih_path = arm_eih_path_;
        std::string new_parameter_preset;

        for (const auto &param : parameters)
        {
            if (param.get_name() == "aruco_panel_path")
            {
                new_aruco_panel_path = param.as_string();
                need_reload = true;
            }
            else if (param.get_name() == "arm_eih_path")
            {
                new_arm_eih_path = param.as_string();
                need_reload = true;
            }
            else if (param.get_name() == "parameter_preset")
            {
                new_parameter_preset = param.as_string();
                need_reload = true;
            }
        }

        if (need_reload)
        {
            try
            {
                // 只有在路径真正改变时才重新加载
                if (new_aruco_panel_path != aruco_panel_path_)
                {
                    aruco_panel_path_ = new_aruco_panel_path;
                    loadPanelRegionsMap();
                }

                if (new_arm_eih_path != arm_eih_path_)
                {
                    arm_eih_path_ = new_arm_eih_path;
                    getHandEyeTransform(arm_eih_path_);
                }

                // 处理参数预设变更
                if (!new_parameter_preset.empty())
                {
                    if (new_parameter_preset == "high_precision")
                    {
                        setHighPrecisionParameters();
                        RCLCPP_INFO(this->get_logger(), "切换到高精度参数预设");
                    }
                    else if (new_parameter_preset == "robust")
                    {
                        setRobustParameters();
                        RCLCPP_INFO(this->get_logger(), "切换到鲁棒性参数预设");
                    }
                    else
                    {
                        setupCustomParameters();
                        RCLCPP_INFO(this->get_logger(), "切换到自定义参数配置");
                    }
                }

                RCLCPP_INFO(this->get_logger(), "配置参数更新完成");
            }
            catch (const std::exception &e)
            {
                RCLCPP_ERROR(this->get_logger(), "更新配置时出错: %s", e.what());
                rcl_interfaces::msg::SetParametersResult result;
                result.successful = false;
                result.reason = std::string("配置更新失败: ") + e.what();
                return result;
            }
        }

        rcl_interfaces::msg::SetParametersResult result;
        result.successful = true;
        return result;
    }
    void initNode()
    {
        client_node_ = std::make_shared<rclcpp::Node>("single_shot_detector_client");
        // 创建TF2的Buffer和Listener，用于后续TF变换的查找和监听。
        tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        // 创建TF2的Broadcaster，用于发布TF变换
        tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(this);
        trigger_image_client_ = client_node_->create_client<TriggerImageCapture>("/trigger_image");
        get_camera_info_client_ = client_node_->create_client<GetCameraInfo>("/get_camera_info");

        param_callback_handle_ = this->add_on_set_parameters_callback(
            std::bind(&SingleShotDetector::onParameterChange, this, std::placeholders::_1));

        // 创建检测AR码服务端，并绑定回调函数
        get_panel_region_service_ = this->create_service<GetPanelRegionPose>(
            "get_region_pose",
            std::bind(&SingleShotDetector::GetPanelRegionPoseCallback, this, std::placeholders::_1, std::placeholders::_2));

        getHandEyeTransform(arm_eih_path_);
    }

    // 修改自定义参数设置方法
    void setupCustomParameters()
    {
        aruco::MarkerDetector::Params params = mDetector_.getParameters();

        // 设置角点精化方法
        // params.setCornerRefinementMethod(aruco::CORNER_SUBPIX);

        // 设置阈值方法
        params.setThresholdMethod(aruco::MarkerDetector::THRES_ADAPTIVE, 7, 6);

        // 其他参数设置
        params.minSize = 0.02;           // 最小标记大小
        params.borderDistThres = 0.015f; // 边界距离阈值

        mDetector_.setParameters(params);
    }

    // 修改鲁棒性参数设置方法
    void setRobustParameters()
    {
        aruco::MarkerDetector::Params params = mDetector_.getParameters();

        // 设置角点精化方法
        params.setCornerRefinementMethod(aruco::CORNER_SUBPIX);

        // 设置检测模式和最小标记大小
        params.setDetectionMode(aruco::DM_FAST, 0.02);

        // 设置阈值方法
        params.setThresholdMethod(aruco::MarkerDetector::THRES_ADAPTIVE, 5, 7);

        // 设置其他参数
        params.NAttemptsAutoThresFix = 3;
        params.minSize = 0.01; // 最小标记大小
        params.borderDistThres = 0.01f;

        mDetector_.setParameters(params);
    }

    // 修改高精度参数设置方法
    void setHighPrecisionParameters()
    {
        aruco::MarkerDetector::Params params = mDetector_.getParameters();

        // 设置角点精化方法为亚像素级
        params.setCornerRefinementMethod(aruco::CORNER_SUBPIX);

        // 设置检测模式
        params.setDetectionMode(aruco::DM_NORMAL, 0.01);

        // 设置自适应阈值方法
        params.setThresholdMethod(aruco::MarkerDetector::THRES_ADAPTIVE, 7, 5);

        // 设置更多尝试次数以提高精度
        params.NAttemptsAutoThresFix = 5;
        params.minSize = 0.005; // 更小的最小标记大小要求
        params.borderDistThres = 0.02f;

        mDetector_.setParameters(params);
    }
    void getHandEyeTransform(const std::string &file_path)
    {
        try
        {
            // 1. 读取标定文件
            YAML::Node config = YAML::LoadFile(file_path);

            // 获取坐标系
            const auto &params = config["parameters"];
            std::string calibration_type = params["calibration_type"].as<std::string>();
            arm_base_frame_ = params["robot_base_frame"].as<std::string>();
            camera_frame_ = params["tracking_base_frame"].as<std::string>();
            marker_frame_ = params["tracking_marker_frame"].as<std::string>();
            arm_effector_frame_ = params["robot_effector_frame"].as<std::string>();

            // 断言相机帧和标记帧不为空
            rcpputils::assert_true(
                camera_frame_ != "" && marker_frame_ != "",
                "Found the camera frame or the marker_frame to be empty!. camera_frame : " +
                    camera_frame_ + " and marker_frame : " + marker_frame_);

            // 2. 解析标定参数
            const auto &transform = config["transform"];

            // 设置父子坐标系（根据 eye_in_hand 或 eye_to_hand 模式）
            if (calibration_type == "eye_in_hand")
            {
                hand_eye_transform.header.frame_id = arm_effector_frame_; // gripper_base
                hand_eye_transform.child_frame_id = camera_frame_;        // camera_optical_frame
            }
            else
            {
                // 如果是 eye_to_hand 模式，需调整坐标系关系
                throw std::runtime_error("仅支持 eye_in_hand 模式！");
            }

            // 设置平移和旋转
            hand_eye_transform.transform.translation.x = transform["translation"]["x"].as<double>();
            hand_eye_transform.transform.translation.y = transform["translation"]["y"].as<double>();
            hand_eye_transform.transform.translation.z = transform["translation"]["z"].as<double>();
            hand_eye_transform.transform.rotation.x = transform["rotation"]["x"].as<double>();
            hand_eye_transform.transform.rotation.y = transform["rotation"]["y"].as<double>();
            hand_eye_transform.transform.rotation.z = transform["rotation"]["z"].as<double>();
            hand_eye_transform.transform.rotation.w = transform["rotation"]["w"].as<double>();
        }
        catch (const YAML::Exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "手眼标定文件解析失败: %s", e.what());
            throw std::runtime_error("手眼标定文件解析失败");
        }
    }

    void loadPanelRegionsMap()
    {
        if (!aruco_panel_path_.empty())
        {
            try
            {
                YAML::Node config = YAML::LoadFile(aruco_panel_path_);
                panel_config_ = panel_config::PanelConfig::fromYaml(config);

                for (const auto &group_pair : panel_config_.region_groups)
                {
                    for (const auto &region_pair : group_pair.second.regions)
                    {
                        int region_type_id = region_pair.second.type_id;
                        panel_regions_map[region_type_id] = region_pair.second.rel_position;
                    }
                }
                RCLCPP_INFO(this->get_logger(), "成功加载面板区域数据，共 %zu 个区域", panel_regions_map.size());
            }
            catch (const YAML::Exception &e)
            {
                RCLCPP_ERROR(this->get_logger(), "面板区域文件解析失败: %s", e.what());
                throw std::runtime_error("面板区域文件解析失败");
            }
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "未提供面板区域文件路径");
            throw std::runtime_error("未提供面板区域文件路径");
        }
    }

    // 获取区域类型信息
    int getRegionType(const std::string &region_name) const
    {
        for (const auto &group_pair : panel_config_.region_groups)
        {
            auto it = group_pair.second.regions.find(region_name);
            if (it != group_pair.second.regions.end())
            {
                return it->second.type_id;
            }
        }
        return common_interfaces::msg::PanelRegion::UNKNOWN;
    }

    std::string getRegionTypeNameById(int region_id) const
    {
        for (const auto &group_pair : panel_config_.region_groups)
        {
            for (const auto &region_pair : group_pair.second.regions)
            {
                if (region_pair.second.type_id == region_id)
                {
                    return region_pair.first; // 返回区域名称
                }
            }
        }
        return "UNKNOWN";
    }

    bool getTransform(
        const std::string &refFrame, const std::string &childFrame,
        geometry_msgs::msg::TransformStamped &transform)
    {
        std::string errMsg;

        if (!tf_buffer_->canTransform(
                refFrame, childFrame, tf2::TimePointZero,
                tf2::durationFromSec(0.5), &errMsg))
        {
            RCLCPP_ERROR_STREAM(this->get_logger(), "Unable to get pose from TF: " << errMsg);
            return false;
        }
        else
        {
            try
            {
                transform = tf_buffer_->lookupTransform(
                    refFrame, childFrame, tf2::TimePointZero, tf2::durationFromSec(0.5));
            }
            catch (const tf2::TransformException &e)
            {
                RCLCPP_ERROR_STREAM(
                    this->get_logger(),
                    "Error in lookupTransform of " << childFrame << " in " << refFrame << " : " << e.what());
                return false;
            }
        }
        return true;
    }

    // 计算并存储所有区域位姿
    bool calculateAndStoreAllRegionPoses(
        const geometry_msgs::msg::PoseStamped &aruco_pose_in_base)
    {
        region_poses_map.clear();

        // 为每个区域计算位姿
        for (const auto &pair : panel_regions_map)
        {
            int region_type_id = pair.first;
            auto &pixel_offset = pair.second;
            
            geometry_msgs::msg::Pose region_pose_in_aruco;
            region_pose_in_aruco.position.x = pixel_offset[0];
            region_pose_in_aruco.position.y = pixel_offset[1];
            region_pose_in_aruco.position.z = pixel_offset[2];
            if(region_type_id != common_interfaces::msg::PanelRegion::ORIGIN)  // 原点区域不补偿
            {
                region_pose_in_aruco.position.x += offset_compensation_.x();
                region_pose_in_aruco.position.y += offset_compensation_.y();
                // region_pose_in_aruco.position.z += offset_compensation_.z(); 
            }
            // 打印region_pose_in_aruco
            // RCLCPP_INFO(this->get_logger(), "区域 %s 在Aruco坐标系下的位置: [%.4f, %.4f, %.4f]",
            //             getRegionTypeNameById(region_type_id).c_str(),
            //             region_pose_in_aruco.position.x,
            //             region_pose_in_aruco.position.y,
            //             region_pose_in_aruco.position.z);

            // 将区域姿态设置为无旋转，即和 Aruco 的坐标轴完全一致（X 朝右、Y 朝上、Z 朝外）
            region_pose_in_aruco.orientation = tf2::toMsg(tf2::Quaternion(0, 0, 0, 1));

            // 设置旋转（Aruco自身姿态）
            // tf2::Quaternion aruco_ori;
            // tf2::fromMsg(aruco_pose_in_base.pose.orientation, aruco_ori);
            // region_pose_in_aruco.orientation =  tf2::toMsg(aruco_ori);

            // 将 Pose 转为 tf2::Transform
            tf2::Transform tf_base_aruco, tf_aruco_region;
            tf2::fromMsg(aruco_pose_in_base.pose, tf_base_aruco);
            tf2::fromMsg(region_pose_in_aruco, tf_aruco_region);

            // tf2::Quaternion aruco_ori;
            // tf2::fromMsg(aruco_pose_in_base.pose.orientation, aruco_ori);

            // 做变换：T_base_region = T_base_aruco * T_aruco_region
            tf2::Transform tf_base_region = tf_base_aruco * tf_aruco_region;

            // tf2::Quaternion flip_quat;
            // flip_quat.setRPY(M_PI, 0, 0);  // 180度绕X轴
            // tf2::Quaternion adjusted_ori = aruco_ori * flip_quat;
            // adjusted_ori.normalize();
            // tf_base_region.setRotation(adjusted_ori);

            // 转回 geometry_msgs::msg::Pose
            geometry_msgs::msg::PoseStamped region_pose_in_base;
            region_pose_in_base.header = aruco_pose_in_base.header;
            geometry_msgs::msg::Pose pose_msg;
            tf2::toMsg(tf_base_region, pose_msg);
            region_pose_in_base.pose = pose_msg;
            // 打印计算得到的区域位姿
            // RCLCPP_INFO(this->get_logger(), "区域 %s 在机械臂基坐标系下的位置: [%.4f, %.4f, %.4f]",
            //             getRegionTypeNameById(region_type_id).c_str(),
            //             region_pose_in_base.pose.position.x,
            //             region_pose_in_base.pose.position.y,
            //             region_pose_in_base.pose.position.z);

            region_poses_map[region_type_id] = region_pose_in_base;
        }
        return true;
    }


    /**
 * 基于理论位置关系计算两个码中间的位置
 * @param center_pose 输出：基于理论位置关系计算的中间位置（在机械臂基坐标系下）
 * @return 是否成功计算
 */
    bool calculateCenterPoseFromTheory(
        geometry_msgs::msg::PoseStamped& center_pose,
        bool& is_main_aux)
    {
        sensor_msgs::msg::Image img_msg;
        sensor_msgs::msg::CameraInfo cam_info_msg;
        
        try
        {
            // 获取图像和相机信息
            img_msg = getImageFromService();
            cam_info_msg = getCameraInfoFromService();

            // 转换图像
            cv_bridge::CvImagePtr cv_ptr;
            cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
            inImage_ = cv_ptr->image;
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "服务调用失败: %s", e.what());
            return false;
        }

        // 设置相机参数
        camParam_ = aruco_ros::rosCameraInfo2ArucoCamParams(cam_info_msg, useRectifiedImages_);
        
        // ArUco 标记检测
        markers_.clear();
        mDetector_.detect(inImage_, markers_, camParam_, marker_size_, false);
        
        tf2::Transform main_marker_transform, aux_marker_transform;
        bool main_marker_found = false, aux_marker_found = false;
        
        // 查找主码和辅码
        for (const auto& marker : markers_) {
            if (marker.id == panel_config_.main_aruco_id) {
                main_marker_found = true;
                main_marker_transform = aruco_ros::arucoMarker2Tf2(marker);
            }
            else if (marker.id == panel_config_.aux_aruco_id) {
                aux_marker_found = true;
                aux_marker_transform = aruco_ros::arucoMarker2Tf2(marker);
            }
        }
        
        // 如果没有检测到任何码，返回失败
        if (!main_marker_found && !aux_marker_found) {
            RCLCPP_WARN(this->get_logger(), "未检测到主码或辅码");
            return false;
        }
        
        tf2::Transform center_transform;
        
        // 如果只检测到一个码，基于理论位置关系计算另一个码的位置，然后计算中间位置
        if (main_marker_found && !aux_marker_found) {
            RCLCPP_INFO(this->get_logger(), "只检测到主码，基于理论位置关系计算辅码位置");
            is_main_aux = false; 
            // 获取理论偏移量（考虑坐标系变换）
            tf2::Vector3 expected_offset(
                panel_config_.main_aux_offset[0],
                panel_config_.main_aux_offset[1],
                panel_config_.main_aux_offset[2]
            );
            
            // 实际值的(x,y,z)对应理论值的(-y,x,z)
            tf2::Vector3 transformed_expected_offset(
                -expected_offset.y(),
                expected_offset.x(),
                expected_offset.z()
            );
            
            // 计算辅码的理论位置
            tf2::Transform aux_marker_theory_transform = main_marker_transform;
            tf2::Vector3 main_pos = main_marker_transform.getOrigin();
            // tf2::Vector3 aux_pos_theory = main_pos + transformed_expected_offset;
            tf2::Vector3 aux_pos_theory = main_pos + expected_offset;

            aux_marker_theory_transform.setOrigin(aux_pos_theory);
            
            // 计算中间位置
            tf2::Vector3 center_pos = (main_pos + aux_pos_theory) * 0.5;
            
            center_transform.setOrigin(center_pos);
            center_transform.setRotation(main_marker_transform.getRotation());
        }
        else if (!main_marker_found && aux_marker_found) {
            RCLCPP_INFO(this->get_logger(), "只检测到辅码，基于理论位置关系计算主码位置");
            is_main_aux = false; 
            // 获取理论偏移量（考虑坐标系变换）
            tf2::Vector3 expected_offset(
                panel_config_.main_aux_offset[0],
                panel_config_.main_aux_offset[1],
                panel_config_.main_aux_offset[2]
            );
            
            // 实际值的(x,y,z)对应理论值的(-y,x,z)
            tf2::Vector3 transformed_expected_offset(
                expected_offset.y(),
                -expected_offset.x(),
                expected_offset.z()
            );
            
            // 计算主码的理论位置（辅码位置 - 偏移量）
            tf2::Transform main_marker_theory_transform = aux_marker_transform;
            tf2::Vector3 aux_pos = aux_marker_transform.getOrigin();
            // tf2::Vector3 main_pos_theory = aux_pos + transformed_expected_offset;
            tf2::Vector3 main_pos_theory = aux_pos - expected_offset;
            main_marker_theory_transform.setOrigin(main_pos_theory);
            
            // 计算中间位置
            tf2::Vector3 center_pos = (main_pos_theory + aux_pos) * 0.5;
            
            center_transform.setOrigin(center_pos);
            center_transform.setRotation(aux_marker_transform.getRotation());
        }
        else // 两个码都检测到了，直接计算中间位置
        {
            RCLCPP_INFO(this->get_logger(), "检测到主码和辅码，直接计算中间位置");
            is_main_aux = true;
            
            tf2::Vector3 main_pos = main_marker_transform.getOrigin();
            tf2::Vector3 aux_pos = aux_marker_transform.getOrigin();
            
            // 计算中间位置
            tf2::Vector3 center_pos = (main_pos + aux_pos) * 0.5;
            
            center_transform.setOrigin(center_pos);
            center_transform.setRotation(main_marker_transform.getRotation());
        }
        
        // 创建相机坐标系下的中间位置Pose
        geometry_msgs::msg::PoseStamped center_pose_in_camera;
        center_pose_in_camera.header.stamp = img_msg.header.stamp;
        center_pose_in_camera.header.frame_id = camera_frame_;
        tf2::toMsg(center_transform, center_pose_in_camera.pose);
        
        // 发布手眼标定的静态 TF
        hand_eye_transform.header.stamp = img_msg.header.stamp;
        tf_broadcaster_->sendTransform(hand_eye_transform);
        
        try {
            // 获取相机到基座的变换
            geometry_msgs::msg::TransformStamped transform_to_base = tf_buffer_->lookupTransform(
                arm_base_frame_,
                camera_frame_,
                tf2::TimePointZero,
                tf2::durationFromSec(0.5));
            
            // 将中间位置从相机坐标系转换到机械臂基坐标系
            tf2::doTransform(center_pose_in_camera, center_pose, transform_to_base);
            
            RCLCPP_INFO(this->get_logger(), 
                "中间位置(基坐标系): x=%.4f, y=%.4f, z=%.4f", 
                center_pose.pose.position.x, 
                center_pose.pose.position.y, 
                center_pose.pose.position.z);
            
            return true;
        }
        catch (const tf2::TransformException &ex) {
            RCLCPP_ERROR(this->get_logger(), "坐标变换失败: %s", ex.what());
            return false;
        }
    }

    // 主方法：检测Aruco并计算末端位姿
    bool detectArucoAndCalculateEffectorPose(std::string &information)
    {
        auto detection_data = collectDetectionData();
        
        if (detection_data.successful_detections == 0)
        {
            information = "所有采样都未能检测到AR码";
            return false;
        }

        // 异常值过滤
        auto filtered_data = filterDetectionData(detection_data);

        // 计算平均误差补偿
        calculateAndApplyOffsetCompensation(filtered_data.filtered_offset_errors);

        // 计算平均位姿
        auto averaged_pose = calculateAveragePose(filtered_data.filtered_transforms, detection_data.aruco_poses);

        // 使用平均后的位姿计算所有区域位姿
        bool success = calculateAndStoreAllRegionPoses(averaged_pose);
        if (success)
        {
            RCLCPP_INFO(this->get_logger(), "基于 %d 次采样成功计算区域位姿，有效采样: %d",
                        sample_count_, detection_data.successful_detections);
            return true;
        }
        else
        {
            information = "计算区域位姿失败";
            return false;
        }
    }

    // 收集检测数据
    DetectionData collectDetectionData()
    {
        DetectionData data;
        
        // 多次采样
        for (int i = 0; i < sample_count_; ++i)
        {
            sensor_msgs::msg::Image img_msg;
            sensor_msgs::msg::CameraInfo cam_info_msg;
            
            try
            {
                // 1. 获取图像和相机信息
                img_msg = getImageFromService();
                cam_info_msg = getCameraInfoFromService();

                // 转换图像
                cv_bridge::CvImagePtr cv_ptr;
                cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
                inImage_ = cv_ptr->image;
            }
            catch (const std::exception &e)
            {
                RCLCPP_WARN(this->get_logger(), "服务调用失败 (%d/%d): %s", i + 1, sample_count_, e.what());
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sample_delay_ms_));
                continue;
            }

            // 2. 设置相机参数
            camParam_ = aruco_ros::rosCameraInfo2ArucoCamParams(cam_info_msg, useRectifiedImages_);
            rightToLeft_.setIdentity();
            rightToLeft_.setOrigin(tf2::Vector3(-cam_info_msg.p[3] / cam_info_msg.p[0],
                                                -cam_info_msg.p[7] / cam_info_msg.p[5], 0.0));

            // 3. ArUco 标记检测
            markers_.clear();
            mDetector_.detect(inImage_, markers_, camParam_, marker_size_, false);

            auto sample_result = processMarkers(markers_, img_msg, cam_info_msg);
            
            if (sample_result.main_found) {
                data.aruco_poses.push_back(sample_result.aruco_pose_in_base);
                tf2::Transform tf_base_aruco;
                tf2::fromMsg(sample_result.aruco_pose_in_base.pose, tf_base_aruco);
                data.aruco_transforms.push_back(tf_base_aruco);
                data.successful_detections++;
            }
            
            if (sample_result.main_found && sample_result.aux_found) {
                // 计算辅助标记相对于主标记的变换
                tf2::Transform main_to_aux = sample_result.main_marker_transform.inverse() * sample_result.aux_marker_transform;
                
                // 获取实际的偏移量
                tf2::Vector3 actual_offset = main_to_aux.getOrigin();
                // 打印实际偏移量
                RCLCPP_INFO(this->get_logger(), 
                    "实际偏移量: x=%.4f, y=%.4f, z=%.4f (米)", 
                    actual_offset.x(), actual_offset.y(), actual_offset.z());
                
                // 从配置中获取理论偏移量
                tf2::Vector3 expected_offset(
                    panel_config_.main_aux_offset[0],
                    panel_config_.main_aux_offset[1],
                    panel_config_.main_aux_offset[2]
                );
                // 实际值的(x,y,z)对应理论值的(-y,x,z)
                tf2::Vector3 transformed_expected_offset(
                    -expected_offset.y(),
                    expected_offset.x(),
                    expected_offset.z()
                );
                
                // 计算误差
                // tf2::Vector3 offset = actual_offset - transformed_expected_offset;
                tf2::Vector3 offset = actual_offset - expected_offset;
                data.offset_errors.push_back(offset);
                
                RCLCPP_INFO(this->get_logger(), 
                    "主辅标记偏移误差: x=%.4f, y=%.4f, z=%.4f (米)", 
                    offset.x(), offset.y(), offset.z());
            }
            else if (sample_result.main_found || sample_result.aux_found)
            {
                RCLCPP_WARN(this->get_logger(), "只检测到一个标记，无法计算相对位置关系");
            }
            else
            {
                RCLCPP_WARN(this->get_logger(), "未检测到主标记或辅助标记");
            }

            // 采样间隔
            if (i < sample_count_ - 1)
            {
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sample_delay_ms_));
            }
        }
        
        return data;
    }

    // 处理标记
    MarkerProcessResult processMarkers(
        const std::vector<aruco::Marker>& markers, 
        const sensor_msgs::msg::Image& img_msg,
        const sensor_msgs::msg::CameraInfo& cam_info_msg)
    {
        MarkerProcessResult result;
        
        for (const auto &marker : markers)
        {
            if (marker.id == panel_config_.main_aruco_id)
            {
                result.main_found = true;
                // 计算相机坐标系下的姿态
                result.main_marker_transform = aruco_ros::arucoMarker2Tf2(marker);

                geometry_msgs::msg::PoseStamped aruco_pose_in_camera;
                aruco_pose_in_camera.header.stamp = img_msg.header.stamp;
                aruco_pose_in_camera.header.frame_id = camera_frame_;
                tf2::toMsg(result.main_marker_transform, aruco_pose_in_camera.pose);

                // 发布手眼标定的静态 TF
                hand_eye_transform.header.stamp = img_msg.header.stamp;
                tf_broadcaster_->sendTransform(hand_eye_transform);

                // 发布 marker_frame_ 的 TF（可选，供其他节点使用）
                geometry_msgs::msg::TransformStamped marker_transform;
                marker_transform.header.stamp = img_msg.header.stamp;
                marker_transform.header.frame_id = camera_frame_;
                marker_transform.child_frame_id = marker_frame_;
                tf2::toMsg(result.main_marker_transform, marker_transform.transform);
                tf_broadcaster_->sendTransform(marker_transform);

                // 获取相机到基座的变换
                geometry_msgs::msg::TransformStamped transform_to_base;
                try
                {
                    transform_to_base = tf_buffer_->lookupTransform(
                        arm_base_frame_,
                        aruco_pose_in_camera.header.frame_id,
                        tf2::TimePointZero,
                        tf2::durationFromSec(0.5));
                }
                catch (tf2::TransformException &ex)
                {
                    RCLCPP_WARN(this->get_logger(), "坐标变换失败: %s", ex.what());
                    result.main_found = false;
                    return result;
                }

                tf2::doTransform(aruco_pose_in_camera, result.aruco_pose_in_base, transform_to_base);
                break;
            }
            else if(marker.id == panel_config_.aux_aruco_id)
            {
                result.aux_found = true;
                result.aux_marker_transform = aruco_ros::arucoMarker2Tf2(marker);
            }
        }
        
        return result;
    }

    // 过滤检测数据
    FilteredData filterDetectionData(const DetectionData& detection_data)
    {
        FilteredData filtered_data;
        double outlier_threshold = 0.03; // 3cm阈值，可根据需要调整
        this->get_parameter_or<double>("outlier_threshold", outlier_threshold, 0.03);

        // 过滤变换数据
        if (!filterOutliers(detection_data.aruco_transforms, filtered_data.filtered_transforms, outlier_threshold))
        {
            RCLCPP_WARN(this->get_logger(), "异常值过滤失败，使用所有采样数据");
            filtered_data.filtered_transforms = detection_data.aruco_transforms;
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "异常值过滤: 原始 %zu 个样本，过滤后 %zu 个样本",
                        detection_data.aruco_transforms.size(), filtered_data.filtered_transforms.size());
        }

        // 过滤偏移误差数据
        if (!filterOffsetErrors(detection_data.offset_errors, filtered_data.filtered_offset_errors, outlier_threshold))
        {
            RCLCPP_WARN(this->get_logger(), "偏移误差异常值过滤失败，使用所有采样数据");
            filtered_data.filtered_offset_errors = detection_data.offset_errors;
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "偏移误差异常值过滤: 原始 %zu 个样本，过滤后 %zu 个样本",
                        detection_data.offset_errors.size(), filtered_data.filtered_offset_errors.size());
        }
        
        return filtered_data;
    }

    // 计算并应用偏移补偿
    void calculateAndApplyOffsetCompensation(
        const std::vector<tf2::Vector3>& filtered_offset_errors)
    {
        tf2::Vector3 avg_offset_compensation(0, 0, 0);
        if (!filtered_offset_errors.empty()) {
            for (const auto& offset : filtered_offset_errors) {
                avg_offset_compensation += offset;
            }
            avg_offset_compensation /= filtered_offset_errors.size();
            
            // 更新成员变量用于后续的位姿计算
            offset_compensation_ = avg_offset_compensation;
            
            RCLCPP_INFO(this->get_logger(), 
                "平均偏移补偿: x=%.4f, y=%.4f, z=%.4f (米)", 
                avg_offset_compensation.x(), avg_offset_compensation.y(), avg_offset_compensation.z());
        }
    }

    // 计算平均位姿
    geometry_msgs::msg::PoseStamped calculateAveragePose(
        const std::vector<tf2::Transform>& filtered_transforms,
        const std::vector<geometry_msgs::msg::PoseStamped>& aruco_poses)
    {
        // 计算平均位姿
        tf2::Vector3 avg_position(0, 0, 0);
        std::vector<tf2::Quaternion> quaternions;

        for (const auto &tf : filtered_transforms)
        {
            avg_position += tf.getOrigin();
            quaternions.push_back(tf.getRotation());
        }

        // 平均位置
        avg_position /= filtered_transforms.size();

        // 平均四元数（简化实现）
        tf2::Quaternion avg_quaternion = quaternions[0];
        for (size_t i = 1; i < quaternions.size(); ++i)
        {
            double weight = 1.0 / filtered_transforms.size();
            avg_quaternion = avg_quaternion.slerp(quaternions[i], weight * i);
        }
        avg_quaternion.normalize();

        // 构造平均后的位姿
        tf2::Transform avg_transform(avg_quaternion, avg_position);
        geometry_msgs::msg::PoseStamped averaged_pose;
        if (!aruco_poses.empty()) {
            averaged_pose = aruco_poses[0];
        }
        tf2::toMsg(avg_transform, averaged_pose.pose);
        
        return averaged_pose;
    }

    
    // 使用中位数绝对偏差(MAD)方法过滤异常值
    bool filterOutliers(const std::vector<tf2::Transform> &transforms,
                        std::vector<tf2::Transform> &filtered_transforms,
                        double position_threshold = 0.05)
    {
        if (transforms.size() <= 2)
        {
            filtered_transforms = transforms;
            return true;
        }

        // 计算位置平均值
        tf2::Vector3 avg_position(0, 0, 0);
        for (const auto &tf : transforms)
        {
            avg_position += tf.getOrigin();
        }
        avg_position /= transforms.size();

        // 计算每个点到平均位置的距离
        std::vector<double> distances;
        distances.reserve(transforms.size());
        for (const auto &tf : transforms)
        {
            double distance = (tf.getOrigin() - avg_position).length();
            distances.push_back(distance);
        }

        // 计算距离的中位数作为更鲁棒的统计量
        std::vector<double> sorted_distances = distances;
        std::sort(sorted_distances.begin(), sorted_distances.end());
        double median_distance = sorted_distances[sorted_distances.size() / 2];

        // 使用中位数绝对偏差(MAD)来确定更合理的阈值
        std::vector<double> abs_deviations;
        for (const auto &dist : distances)
        {
            abs_deviations.push_back(std::abs(dist - median_distance));
        }
        std::sort(abs_deviations.begin(), abs_deviations.end());
        double mad = abs_deviations[abs_deviations.size() / 2];

        // 使用MAD设置阈值（更鲁棒的方法）
        double robust_threshold = median_distance + 2.0 * 1.4826 * mad;          // 1.4826是正态分布的缩放因子
        double final_threshold = std::max(robust_threshold, position_threshold); // 至少使用设定的最小阈值

        // 过滤远离平均值的点
        int filtered_count = 0;
        for (size_t i = 0; i < transforms.size(); ++i)
        {
            if (distances[i] <= final_threshold)
            {
                filtered_transforms.push_back(transforms[i]);
            }
            else
            {
                RCLCPP_WARN(this->get_logger(), "过滤异常值: 距离平均位置 %.3fm (阈值: %.3fm)",
                            distances[i], final_threshold);
                filtered_count++;
            }
        }

        RCLCPP_INFO(this->get_logger(), "异常值过滤统计: 总计 %zu 个样本，过滤 %d 个异常值",
                    transforms.size(), filtered_count);

        return filtered_transforms.size() > transforms.size() / 2; // 至少保留一半的数据
    }

    // 使用中位数绝对偏差(MAD)方法过滤偏移误差异常值
    bool filterOffsetErrors(const std::vector<tf2::Vector3> &offsets,
                        std::vector<tf2::Vector3> &filtered_offsets,
                        double position_threshold = 0.05)
    {
        if (offsets.size() <= 2)
        {
            filtered_offsets = offsets;
            return true;
        }

        // 计算位置平均值
        tf2::Vector3 avg_offset(0, 0, 0);
        for (const auto &offset : offsets)
        {
            avg_offset += offset;
        }
        avg_offset /= offsets.size();

        // 计算每个点到平均位置的距离
        std::vector<double> distances;
        distances.reserve(offsets.size());
        for (const auto &offset : offsets)
        {
            double distance = (offset - avg_offset).length();
            distances.push_back(distance);
        }

        // 计算距离的中位数作为更鲁棒的统计量
        std::vector<double> sorted_distances = distances;
        std::sort(sorted_distances.begin(), sorted_distances.end());
        double median_distance = sorted_distances[sorted_distances.size() / 2];

        // 使用中位数绝对偏差(MAD)来确定更合理的阈值
        std::vector<double> abs_deviations;
        for (const auto &dist : distances)
        {
            abs_deviations.push_back(std::abs(dist - median_distance));
        }
        std::sort(abs_deviations.begin(), abs_deviations.end());
        double mad = abs_deviations[abs_deviations.size() / 2];

        // 使用MAD设置阈值（更鲁棒的方法）
        double robust_threshold = median_distance + 2.0 * 1.4826 * mad;          // 1.4826是正态分布的缩放因子
        double final_threshold = std::max(robust_threshold, position_threshold); // 至少使用设定的最小阈值

        // 过滤远离平均值的点
        int filtered_count = 0;
        for (size_t i = 0; i < offsets.size(); ++i)
        {
            if (distances[i] <= final_threshold)
            {
                filtered_offsets.push_back(offsets[i]);
            }
            else
            {
                RCLCPP_WARN(this->get_logger(), "过滤偏移误差异常值: 距离平均位置 %.3fm (阈值: %.3fm)",
                            distances[i], final_threshold);
                filtered_count++;
            }
        }

        RCLCPP_INFO(this->get_logger(), "偏移误差异常值过滤统计: 总计 %zu 个样本，过滤 %d 个异常值",
                    offsets.size(), filtered_count);

        return filtered_offsets.size() > offsets.size() / 2; // 至少保留一半的数据
    }

    void GetPanelRegionPoseCallback(
        const std::shared_ptr<GetPanelRegionPose::Request> request,
        std::shared_ptr<GetPanelRegionPose::Response> response)
    {
        std::string information;
        std::string region_name = getRegionTypeNameById(request->panel_region.region_id);

        if (request->panel_region.region_id == PanelRegionMsg::MAIN_AUX_CENTER)
        {
            geometry_msgs::msg::PoseStamped center_pose;
            bool is_main_aux = false;
            if (!calculateCenterPoseFromTheory(center_pose, is_main_aux))
            {
                response->success = false;
                response->message = "计算主辅码中间位置失败:" + information;
                return;
            }
            response->pose = center_pose;
            response->success = true;
            // 根据is_main_aux的值来定制返回消息
            if (is_main_aux) {
                response->message = "计算成功（区域: " + region_name + ", 检测到主辅码）";
            } else {
                response->message = "计算成功（区域: " + region_name + ", 基于理论位置计算）";
            }
            
            return;
        }

        if (request->panel_region.region_id == PanelRegionMsg::ORIGIN || region_poses_map.empty())
        {
            if (!detectArucoAndCalculateEffectorPose(information))
            {
                response->success = false;
                response->message = "检测区域姿态失败:" + information;
                return;
            }
        }

        // RCLCPP_INFO(this->get_logger(), "region_id: %d", request->panel_region.region_id);
        // for (auto &map : region_poses_map)
        // {
        //   RCLCPP_INFO(this->get_logger(), "region_poses_map_id: %d", map.first);
        // }

        // 检查请求的区域是否存在
        if (region_poses_map.find(request->panel_region.region_id) == region_poses_map.end())
        {
            response->success = false;
            response->message = "区域 '" + region_name + "' 未找到";
            return;
        }

        response->pose = region_poses_map[request->panel_region.region_id];
        response->success = true;
        response->message = "检测成功（区域: " + region_name + ")";
    }

    sensor_msgs::msg::Image getImageFromService()
    {

        auto request = std::make_shared<TriggerImageCapture::Request>();
        request->is_rectified = false;

        while (!trigger_image_client_->wait_for_service(std::chrono::seconds(1)))
        {
            RCLCPP_INFO(this->get_logger(), "等待/trigger_image服务...");
        }
        auto result = trigger_image_client_->async_send_request(request);
        if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == rclcpp::FutureReturnCode::SUCCESS)
        {
            auto response = result.get();
            if (response->success)
            {
                return response->image;
            }
            else
            {
                throw std::runtime_error("服务端返回失败，未获取图像");
            }
        }
        else
        {
            throw std::runtime_error("获取图像服务失败");
        }
    }

    sensor_msgs::msg::CameraInfo getCameraInfoFromService()
    {
        auto request = std::make_shared<GetCameraInfo::Request>();
        while (!get_camera_info_client_->wait_for_service(std::chrono::seconds(1)))
        {
            RCLCPP_INFO(this->get_logger(), "等待/get_camera_info服务...");
        }
        auto result = get_camera_info_client_->async_send_request(request);
        if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == rclcpp::FutureReturnCode::SUCCESS)
        {
            auto response = result.get();
            return response->camera_info;
        }
        else
        {
            throw std::runtime_error("获取相机信息服务失败");
        }
    }

    cv::Mat advancedPreprocessImage(const cv::Mat &input_image)
    {
        cv::Mat processed_image;

        // 转换为灰度图
        if (input_image.channels() == 3)
        {
            cv::cvtColor(input_image, processed_image, cv::COLOR_RGB2GRAY);
        }
        else
        {
            processed_image = input_image.clone();
        }

        // 评估图像质量以决定预处理策略
        cv::Scalar mean, stddev;
        cv::meanStdDev(processed_image, mean, stddev);
        double brightness = mean[0] / 255.0;
        double contrast = stddev[0] / 255.0;

        // 根据图像质量选择不同的预处理方法
        if (brightness < 0.3 || contrast < 0.1)
        {
            // 图像过暗或对比度过低，使用CLAHE
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(4.0);
            clahe->setTilesGridSize(cv::Size(8, 8));
            clahe->apply(processed_image, processed_image);
        }
        else if (contrast < 0.2)
        {
            // 对比度一般，使用普通直方图均衡化
            cv::equalizeHist(processed_image, processed_image);
        }
        else
        {
            // 对比度足够，可能不需要预处理
            // 可以应用轻微的锐化来增强边缘
            cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
            cv::filter2D(processed_image, processed_image, -1, kernel);
        }
        // 确保输出图像是三通道彩色图
        // if (processed_image.channels() == 1)
        // {
        //   cv::cvtColor(processed_image, processed_image, cv::COLOR_GRAY2RGB);
        // }

        return processed_image;
    }

    


};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SingleShotDetector>();
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
    rclcpp::shutdown();
}
