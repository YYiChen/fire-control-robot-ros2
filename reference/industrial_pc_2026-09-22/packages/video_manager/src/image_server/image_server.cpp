#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "opencv2/opencv.hpp"
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include "sensor_msgs/msg/image.hpp"
#include "cv_bridge/cv_bridge.h"
#include <common_interfaces/srv/set_camera_params.hpp>
#include <common_interfaces/srv/trigger_image_capture.hpp>
#include <common_interfaces/srv/get_camera_info.hpp>

#include "video_manager/hikvision/hk_camera_reader.h"

// 添加新的类型别名
using SetCameraParams = common_interfaces::srv::SetCameraParams;
using SetCameraParamsRequest = std::shared_ptr<SetCameraParams::Request>;
using SetCameraParamsResponse = std::shared_ptr<SetCameraParams::Response>;

using TriggerImageCapture = common_interfaces::srv::TriggerImageCapture;
using TriggerImageCaptureRequest = std::shared_ptr<TriggerImageCapture::Request>;
using TriggerImageCaptureResponse = std::shared_ptr<TriggerImageCapture::Response>;

using GetCameraInfo = common_interfaces::srv::GetCameraInfo;
using GetCameraInfoRequest = std::shared_ptr<GetCameraInfo::Request>;
using GetCameraInfoResponse = std::shared_ptr<GetCameraInfo::Response>;

class ImageServer : public rclcpp::Node
{
public:
    ImageServer() : Node("image_server")
    {
        loadParam();
        initNode();
        openVideo();

        grab_thread_ = std::thread(&ImageServer::grabFrames, this);
    }

    void loadParam()
    {
        // 将视频URL参数改为设备ID参数
        this->declare_parameter<std::string>("camera_device_id", ""); // 空字符串表示自动选择
        this->declare_parameter<std::string>("ost_path", "");
        this->declare_parameter<bool>("is_rectified", false);

        this->get_parameter("camera_device_id", camera_device_id_); // 获取设备ID
        this->get_parameter("ost_path", ost_path_);
        this->get_parameter("is_rectified", is_rectified_);
         
        if (!ost_path_.empty()) 
        {
            std::ifstream infile(ost_path_);
            
            std::string line;
            // 跳到 width
            while (std::getline(infile, line)) {
                if (line.find("width") != std::string::npos) {
                    std::getline(infile, line);
                    image_width_ = std::stoi(line);
                }
                if (line.find("height") != std::string::npos) {
                    std::getline(infile, line);
                    image_height_ = std::stoi(line);
                }
                if (line.find("camera matrix") != std::string::npos) {
                    double vals[9];
                    for (int i = 0; i < 3; ++i) {
                        std::getline(infile, line);
                        std::istringstream iss(line);
                        for (int j = 0; j < 3; ++j) iss >> vals[i*3 + j];
                    }
                    camera_matrix = cv::Mat(3, 3, CV_64F, vals).clone();
                }
                if (line.find("distortion") != std::string::npos) {
                    std::getline(infile, line);
                    std::istringstream iss(line);
                    double vals[5];
                    for (int i = 0; i < 5; ++i) iss >> vals[i];
                    dist_coeffs = cv::Mat(1, 5, CV_64F, vals).clone();
                }
                if (line.find("rectification") != std::string::npos) {
                    double vals[9];
                    for (int i = 0; i < 3; ++i) {
                        std::getline(infile, line);
                        std::istringstream iss(line);
                        for (int j = 0; j < 3; ++j) iss >> vals[i*3 + j];
                    }
                    rectification_matrix = cv::Mat(3, 3, CV_64F, vals).clone();
                }
                if (line.find("projection") != std::string::npos) {
                    double vals[12];
                    for (int i = 0; i < 3; ++i) {
                        std::getline(infile, line);
                        std::istringstream iss(line);
                        for (int j = 0; j < 4; ++j) iss >> vals[i*4 + j];
                    }
                    projection_matrix = cv::Mat(3, 4, CV_64F, vals).clone();
                }
            }
            infile.close();
            
            RCLCPP_INFO(this->get_logger(), "Loaded camera parameters from TXT.");
        }
    }

    void initNode()
    {
        // 服务初始化
        trigger_image_srv_ = create_service<TriggerImageCapture>(
            "/trigger_image",
            [this](const TriggerImageCaptureRequest& request,
                   const TriggerImageCaptureResponse& response)
            {
                handleImage(request, response);
            });
        
        get_camera_info_srv_ = create_service<GetCameraInfo>(
            "/get_camera_info",
            [this](const GetCameraInfoRequest& request,
                   const GetCameraInfoResponse& response)
            {
                handleGetCameraInfo(request, response);
            });
        
        set_camera_params_srv_ = create_service<SetCameraParams>(
            "/set_camera_params",
            [this](const SetCameraParamsRequest& request,
                const SetCameraParamsResponse& response)
            {
                handleSetCameraParams(request, response);
            });
    }

    void openVideo()
    {
        // 使用海康相机设备ID打开相机
        if (!reader_.open(camera_device_id_))
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to open HK camera");
            return;
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "Open HK Camera successfully");
        }
    }

    void grabFrames() {
        cv::Mat frame;
        while (rclcpp::ok()) {
            if (reader_.read(frame)) {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                latest_frame_ = std::make_shared<cv::Mat>(frame.clone());
                cv::waitKey(1);
            }
        }
    }

    // 实现参数设置处理函数
    void handleSetCameraParams(const SetCameraParamsRequest& req, const SetCameraParamsResponse& res)
    {
        bool success = true;
        std::string message = "";
        
        try {
            // 使用默认值处理未设置的参数
            const float DEFAULT_GAIN_LOWER_LIMIT = 0.0f;
            const float DEFAULT_GAIN_UPPER_LIMIT = 16.0f;
            const float DEFAULT_MANUAL_GAIN = 8.0f;
            const float DEFAULT_EXPOSURE_LOWER_LIMIT = 9.0f;
            const float DEFAULT_EXPOSURE_UPPER_LIMIT = 8764.0f;
            const float DEFAULT_MANUAL_EXPOSURE = 8764.0f;

            if (req->auto_gain_enabled) {
                // 启用自动增益并设置上下限
                if (!reader_.setAutoGainEnabled(true)) {
                    success = false;
                    message += "Failed to enable auto gain. ";
                } else {
                    float lower_limit = (req->auto_gain_lower_limit == 0.0f) ? 
                                        DEFAULT_GAIN_LOWER_LIMIT : req->auto_gain_lower_limit;
                    float upper_limit = (req->auto_gain_upper_limit == 0.0f) ? 
                                        DEFAULT_GAIN_UPPER_LIMIT : req->auto_gain_upper_limit;
                                        
                    if (!reader_.setAutoGainLimits(lower_limit, upper_limit)) {
                        success = false;
                        message += "Failed to set auto gain limits. ";
                    } else {
                        message += "Auto gain enabled with limits [" + 
                                std::to_string(lower_limit) + ", " + 
                                std::to_string(upper_limit) + "]. ";
                    }
                }
            } else {
                // 禁用自动增益并设置手动增益值
                if (!reader_.setAutoGainEnabled(false)) {
                    success = false;
                    message += "Failed to disable auto gain. ";
                } else {
                    float gain_value = (req->manual_gain_value == 0.0f) ? 
                                    DEFAULT_MANUAL_GAIN : req->manual_gain_value;
                                    
                    if (!reader_.setManualGain(gain_value)) {
                        success = false;
                        message += "Failed to set manual gain value. ";
                    } else {
                        message += "Manual gain set to " + std::to_string(gain_value) + ". ";
                    }
                }
            }

            // 处理曝光控制
            if (req->auto_exposure_enabled) {
                // 启用自动曝光并设置上下限
                if (!reader_.setAutoExposureEnabled(true)) {
                    success = false;
                    message += "Failed to enable auto exposure. ";
                } else {
                    // 使用默认值处理未设置的参数
                    float lower_limit = (req->auto_exposure_lower_limit == 0.0f) ? 
                                        DEFAULT_EXPOSURE_LOWER_LIMIT : req->auto_exposure_lower_limit;
                    float upper_limit = (req->auto_exposure_upper_limit == 0.0f) ? 
                                        DEFAULT_EXPOSURE_UPPER_LIMIT : req->auto_exposure_upper_limit;
                                        
                    if (!reader_.setAutoExposureLimits(lower_limit, upper_limit)) {
                        success = false;
                        message += "Failed to set auto exposure limits. ";
                    } else {
                        message += "Auto exposure enabled with limits [" + 
                                std::to_string(lower_limit) + ", " + 
                                std::to_string(upper_limit) + "]. ";
                    }
                }
            } else {
                // 禁用自动曝光并设置手动曝光时间
                if (!reader_.setAutoExposureEnabled(false)) {
                    success = false;
                    message += "Failed to disable auto exposure. ";
                } else {
                    float exposure_time = (req->manual_exposure_time == 0.0f) ? 
                                        DEFAULT_MANUAL_EXPOSURE : req->manual_exposure_time;
                                        
                    if (!reader_.setExposureTime(exposure_time)) {
                        success = false;
                        message += "Failed to set manual exposure time. ";
                    } else {
                        message += "Manual exposure time set to " + std::to_string(exposure_time) + " us. ";
                    }
                }
            }
            
            if (success) {
                // 标记参数已更新并记录时间
                gain_params_updated_ = true;
                last_gain_update_time_ = std::chrono::steady_clock::now();
            }
        } catch (const std::exception& e) {
            success = false;
            message = "Exception occurred: " + std::string(e.what());
        }
        
        res->success = success;
        res->message = message;
        
        if (success) {
            RCLCPP_INFO(this->get_logger(), "Camera parameters updated: %s", message.c_str());
        } else {
            RCLCPP_ERROR(this->get_logger(), "Failed to update camera parameters: %s", message.c_str());
        }
    }

    void handleImage(const TriggerImageCaptureRequest& req, const TriggerImageCaptureResponse& res)
    {
        // if (gain_settings_changed) {
        //     // 丢弃接下来的几帧图像（等待设置生效）
        //     int frames_to_skip = 3; // 跳过3帧
        //     cv::Mat temp_frame;
        //     for (int i = 0; i < frames_to_skip; ++i) {
        //         std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 大约一帧的时间
        //         {
        //             std::lock_guard<std::mutex> lock(frame_mutex_);
        //             // 清空缓冲区中的旧帧
        //         }
        //     }
            
        //     // 等待新的一帧
        //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // }

        std::shared_ptr<cv::Mat> frame;
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            frame = latest_frame_;
        }
        
        if (frame && !frame->empty()) 
        {
            sensor_msgs::msg::Image img_msg;
            if(req->is_rectified)
            {
                cv::Mat un_img;
                cv::undistort(*frame, un_img, camera_matrix, dist_coeffs); // 做畸变矫正
                cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", un_img).toImageMsg(img_msg);
            }
            else
            {
                cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", *frame).toImageMsg(img_msg);
            }
            // 存储图像数据
            cv::imwrite("/tmp/captured_image.jpg", *frame);

            img_msg.header.stamp = this->now();
            img_msg.header.frame_id = "camera";
            res->image = img_msg;
            res->success = true;
            RCLCPP_INFO(this->get_logger(), "已抓取图像并发送。");
        }
        else
        {
            res->success = false;
            RCLCPP_WARN(this->get_logger(), "尚未接收到图像。");
        }
    }

    void handleGetCameraInfo(const GetCameraInfoRequest& req, const GetCameraInfoResponse& res)
    {
        (void)req; // 无输入参数
        sensor_msgs::msg::CameraInfo camera_info_msg;
        camera_info_msg.header.stamp = this->now();
        camera_info_msg.header.frame_id = "camera";
        camera_info_msg.height = image_height_;
        camera_info_msg.width = image_width_;

        // 填充相机内参矩阵
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                camera_info_msg.k[i * 3 + j] = camera_matrix.at<double>(i, j);
            }
        }

        // 填充畸变系数
        for (int i = 0; i < dist_coeffs.cols-1; ++i) {
            // camera_info_msg.d[i] = dist_coeffs.at<double>(0, i);
            camera_info_msg.d.push_back(dist_coeffs.at<double>(0, i));
        }

        // 填充校正矩阵
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                camera_info_msg.r[i * 3 + j] = rectification_matrix.at<double>(i, j);
            }
        }

        // 填充投影矩阵
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                camera_info_msg.p[i * 4 + j] = projection_matrix.at<double>(i, j);
            }
        }

        // 设置其他参数（根据需要调整）
        camera_info_msg.distortion_model = "plumb_bob"; // 畸变模型
        camera_info_msg.roi.x_offset = 0;
        camera_info_msg.roi.y_offset = 0;
        camera_info_msg.roi.height = image_height_;
        camera_info_msg.roi.width = image_width_;
        camera_info_msg.roi.do_rectify = false;

        res->camera_info = camera_info_msg;
        res->success = true;
    }

private:
    rclcpp::Service<SetCameraParams>::SharedPtr set_camera_params_srv_;
    rclcpp::Service<TriggerImageCapture>::SharedPtr trigger_image_srv_;
    rclcpp::Service<GetCameraInfo>::SharedPtr get_camera_info_srv_;
    
    HKCameraReader reader_; // 使用海康相机读取器
    cv::Mat camera_matrix;        // 内参矩阵，需初始化
    cv::Mat dist_coeffs;          // 畸变矩阵，需初始化
    cv::Mat rectification_matrix; // 校正矩阵，需初始化
    cv::Mat projection_matrix;    // 投影矩阵，需初始化

    std::mutex frame_mutex_;
    std::shared_ptr<cv::Mat> latest_frame_;
    std::thread grab_thread_;

    std::string ost_path_;
    std::string camera_device_id_; // 海康相机设备ID
    bool is_rectified_;
    int image_width_;
    int image_height_;

    bool gain_params_updated_; // 标记增益参数是否已更新
    std::chrono::time_point<std::chrono::steady_clock> last_gain_update_time_; // 上次增益更新时间
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImageServer>());
    rclcpp::shutdown();
    return 0;
}