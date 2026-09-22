#include <iostream>
#include <args.h>
#include <paddleocr.h>
#include <paddlestructure.h>
#include <gflags/gflags.h>
#include "opencv2/opencv.hpp"
#include "parse_screen.h"
#include "parse_led_region.h"

#if __has_include("cv_bridge/cv_bridge.hpp")
#include "cv_bridge/cv_bridge.hpp"
#else
#include "cv_bridge/cv_bridge.h"
#endif

#include "rclcpp/rclcpp.hpp"
#include "rcpputils/asserts.hpp"
#include "sensor_msgs/image_encodings.hpp"

#include "common_interfaces/msg/screen_info.hpp"
#include "common_interfaces/msg/led_status.hpp"
#include "common_interfaces/msg/panel_region.hpp"
#include "common_interfaces/srv/trigger_image_capture.hpp"
#include "common_interfaces/srv/set_camera_params.hpp"
#include "common_interfaces/srv/get_panel_info.hpp"

using namespace PaddleOCR;

using SetCameraParamsSrv = common_interfaces::srv::SetCameraParams;
using TriggerImageCapture = common_interfaces::srv::TriggerImageCapture;
using GetPanelInfo = common_interfaces::srv::GetPanelInfo;
using ScreenInfoMsg = common_interfaces::msg::ScreenInfo;
using LEDStatusMsg = common_interfaces::msg::LEDStatus;
using PanelRgionMsg = common_interfaces::msg::PanelRegion;

struct CameraParams {
    bool auto_gain_enabled = true;
    float manual_gain_value = 16.0f;
    float auto_gain_lower_limit = 0.1f;
    float auto_gain_upper_limit = 16.0f;
    bool auto_exposure_enabled = true;
    float manual_exposure_time = 8764.0f;
    float auto_exposure_lower_limit = 9.0f;
    float auto_exposure_upper_limit = 8764.0f;
};


class SingleShotRec : public rclcpp::Node
{
public:
    SingleShotRec(int argc, char *argv[]) : Node("single_shot_rec")
    {
        checkParams();
        initNode();
    }

    void checkParams()
    {
        if (FLAGS_type == "ocr")
        {
            if (FLAGS_det_model_dir.empty() || FLAGS_rec_model_dir.empty())
            {
                std::cout << "Need a path to detection and recogition model"
                             "[Usage] --det_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ --rec_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ "
                          << std::endl;
                exit(1);
            }
        }
        else if (FLAGS_type == "structure")
        {
            if (FLAGS_det_model_dir.empty() || FLAGS_rec_model_dir.empty() || FLAGS_lay_model_dir.empty() || FLAGS_tab_model_dir.empty())
            {
                std::cout << "Need a path to detection, recogition, layout and table model"
                             "[Usage] --det_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ --rec_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ --lay_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ --tab_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ "
                          << std::endl;
                exit(1);
            }
        }
    }

    void initNode()
    {
        client_node_ = std::make_shared<rclcpp::Node>("single_shot_rec_client");
        trigger_image_client_ = client_node_->create_client<TriggerImageCapture>("/trigger_image");
        set_camera_params_client_ = client_node_->create_client<SetCameraParamsSrv>("set_camera_params");
        
        // 创建服务
        service_ = this->create_service<GetPanelInfo>(
            "get_panel_info",
            [this](const std::shared_ptr<GetPanelInfo::Request> request,
                   std::shared_ptr<GetPanelInfo::Response> response)
            {
                getPanelInfoCallback(request, response);
            });
        RCLCPP_INFO(this->get_logger(), "SingleShotRec节点初始化完成");
    }

    bool setCameraParams(const CameraParams& params)
    {
        auto request = std::make_shared<SetCameraParamsSrv::Request>();
        request->auto_gain_enabled = params.auto_gain_enabled;
        request->manual_gain_value = params.manual_gain_value;
        request->auto_gain_lower_limit = params.auto_gain_lower_limit;
        request->auto_gain_upper_limit = params.auto_gain_upper_limit;
        request->auto_exposure_enabled = params.auto_exposure_enabled;
        request->manual_exposure_time = params.manual_exposure_time;
        request->auto_exposure_lower_limit = params.auto_exposure_lower_limit;
        request->auto_exposure_upper_limit = params.auto_exposure_upper_limit;
        
        // 等待服务可用
        while (!set_camera_params_client_->wait_for_service(std::chrono::seconds(1))) {
            RCLCPP_INFO(this->get_logger(), "等待 set_camera_params 服务...");
        }
        
        // 发送请求并等待结果
        auto result = set_camera_params_client_->async_send_request(request);
        if (rclcpp::spin_until_future_complete(client_node_->get_node_base_interface(), result) == 
            rclcpp::FutureReturnCode::SUCCESS) 
        {
            auto response = result.get();
            RCLCPP_INFO(this->get_logger(), "相机参数设置结果: %s", response->message.c_str());
            return response->success;
        } else {
            RCLCPP_ERROR(this->get_logger(), "相机参数设置服务调用失败");
            return false;
        }
    }

    bool setCameraParams(bool auto_gain_enabled=true, 
                        float manual_gain_value=16.0f,
                        float auto_gain_lower_limit=0.1f, 
                        float auto_gain_upper_limit=16.0f,
                        bool auto_exposure_enabled=true,
                        float manual_exposure_time=8764.0f,
                        float auto_exposure_lower_limit=9.0f,
                        float auto_exposure_upper_limit=8764.0f)
    {
        CameraParams params;
        params.auto_gain_enabled = auto_gain_enabled;
        params.manual_gain_value = manual_gain_value;
        params.auto_gain_lower_limit = auto_gain_lower_limit;
        params.auto_gain_upper_limit = auto_gain_upper_limit;
        params.auto_exposure_enabled = auto_exposure_enabled;
        params.manual_exposure_time = manual_exposure_time;
        params.auto_exposure_lower_limit = auto_exposure_lower_limit;
        params.auto_exposure_upper_limit = auto_exposure_upper_limit;
        
        return setCameraParams(params);
    }

    void getPanelInfoCallback(
        const std::shared_ptr<GetPanelInfo::Request> request,
        std::shared_ptr<GetPanelInfo::Response> response)
    {
        switch(request->panel_region.region_id) {
            case PanelRgionMsg::SCREEN:
                handleScreenRegionRequest(request, response);
                break;
            case PanelRgionMsg::LED:
                handleLEDRegionRequest(request, response);
                break;
            case PanelRgionMsg::UNKNOWN:
            default:
                response->success = false;
                response->message = "未知或未指定区域";
                break;
        }
    }

    void handleScreenRegionRequest(
        const std::shared_ptr<GetPanelInfo::Request> request,
        std::shared_ptr<GetPanelInfo::Response> response)
    {
        try
        {
            auto img_msg = getImageFromService();
            cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
            cv::Mat frame = cv_ptr->image;
            cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
            cv::imwrite("/home/ubuntu/arm_ws/src/fire_panel_rec/res/output/panel.jpg", frame);

            ScreenInfoMsg screen_info_msg;
            std::string rec_info;

            if (recScreenRegion(frame, screen_info_msg, rec_info))
            {
                response->success = true;
                response->message = "识别屏幕区域成功";
                screen_info_msg.model = request->panel_model;
                response->screen_info = screen_info_msg;
            }
            else
            {
                response->success = false;
                response->message = "识别屏幕区域失败: " + rec_info;
            }
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "处理屏幕区域请求失败: %s", e.what());
            response->success = false;
            response->message = "处理屏幕区域请求失败: " + std::string(e.what());
        }
    }

    void handleLEDRegionRequest(
        const std::shared_ptr<GetPanelInfo::Request> request,
        std::shared_ptr<GetPanelInfo::Response> response)
    {
        try
        {
            // 根据需要选择单帧或多帧处理
            bool use_multi_frame = true; // 可以根据需要设置为false使用单帧处理
            bool brightness_only = true; // 工业相机模式下使用亮度识别
            
            parse_led_region.setBrightnessOnlyMode(brightness_only);

            std::vector<LEDStatusMsg> led_status_msgs;
            std::string rec_info;

            if (use_multi_frame) {
                // 使用多帧处理
                if (parseLEDRegionMultiFrame(led_status_msgs, rec_info, 8)) {
                    response->success = true;
                    response->message = "识别LED区域成功(多帧模式)";
                    response->indicator_lights = led_status_msgs;
                } else {
                    response->success = false;
                    response->message = "识别LED区域失败: " + rec_info;
                }
            } else {
                // 使用单帧处理
                if (parseLEDRegion(led_status_msgs, rec_info)) {
                    response->success = true;
                    response->message = "识别LED区域成功(单帧模式)";
                    response->indicator_lights = led_status_msgs;
                } else {
                    response->success = false;
                    response->message = "识别LED区域失败: " + rec_info;
                }
            }
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "处理LED区域请求失败: %s", e.what());
            response->success = false;
            response->message = "处理LED区域请求失败: " + std::string(e.what());
        }
    }

    sensor_msgs::msg::Image getImageFromService()
    {
        auto request = std::make_shared<TriggerImageCapture::Request>();
        request->is_rectified = true;

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

    bool extractBlueROI(const cv::Mat &inputImage, cv::Mat &outputImage, std::string &info)
    {
        if (inputImage.empty())
        {
            std::cerr << "Input image is empty!" << std::endl;
            return false;
        }

        // 转换图像到 YUV 颜色空间
        cv::Mat yuvImage;
        cv::cvtColor(inputImage, yuvImage, cv::COLOR_BGR2YUV);

        // 分离 Y、U、V 通道
        std::vector<cv::Mat> channels;
        cv::split(yuvImage, channels);

        // 提取 U 通道（用于检测蓝色区域）
        cv::Mat uChannel = channels[1];

        // 使用阈值提取蓝色区域（U 通道在 130 到 255 的范围）
        cv::Mat blueMask;
        cv::inRange(uChannel, 150, 255, blueMask);

        // 查找轮廓以确定蓝色区域的边界框
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(blueMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // 如果没有找到轮廓，返回空图像
        if (contours.empty())
        {
            // std::cout << "No blue regions detected!" << std::endl;
            info = "未找到蓝色区域";
            return false;
        }

        // 找到最大的轮廓并获取其边界框
        double maxArea = 0;
        int maxIdx = 0;
        for (size_t i = 0; i < contours.size(); ++i)
        {
            double area = cv::contourArea(contours[i]);
            if (area > maxArea)
            {
                maxArea = area;
                maxIdx = i;
            }
        }

        // 判断最大轮廓面积是否小于 200x200
        if (maxArea < 200 * 200)
        {
            // std::cout << "Blue region area is too small!" << std::endl;
            info = "蓝色区域面积过小";
            outputImage = inputImage;
            return false;
        }

        cv::Rect boundingBox = cv::boundingRect(contours[maxIdx]);

        // 提取原始图像中的蓝色 ROI 区域
        outputImage = inputImage(boundingBox).clone();

        return true;
    }

    bool recScreenRegion(const cv::Mat &inputImage, ScreenInfoMsg &msg, std::string &info)
    {
        cv::Mat blueMask;
        std::string extract_info;
        if (!extractBlueROI(inputImage, blueMask, extract_info))
        {
            info = "提取蓝色区域失败: " + extract_info;
            return false;
        }

        // 解析OCR结果
        ScreenInfoStruct info_ = parse_screen.parseOCRResults(ppocr.ocr(blueMask));

        msg.header.stamp = rclcpp::Clock().now();
        msg.header.frame_id = "single_shot_rec";
        msg.current_page = static_cast<uint8_t>(info_.current_page);
        msg.active_alarm_id = info_.active_alarm_id;
        msg.active_alarm_time = info_.active_alarm_time;
        msg.active_alarm_type = info_.active_alarm_type;
        msg.first_alarm_id = info_.first_alarm_id;
        msg.first_alarm_time = info_.first_alarm_time;
        msg.first_alarm_type = info_.first_alarm_type;
        msg.total_alarms = info_.total_alarms;
        msg.has_activation = info_.has_activation;
        msg.has_delay = info_.has_delay;
        msg.has_fault = info_.has_fault;
        msg.has_feedback = info_.has_feedback;
        msg.has_shield = info_.has_shield;
        msg.has_supervision = info_.has_supervision;
        msg.total_starts = info_.total_starts;
        return true;
    }

    // 解析LED区域
    bool parseLEDRegion(std::vector<LEDStatusMsg> &msgs,
                        std::string &info)
    {
        auto img_msg = getImageFromService();
        cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
        cv::Mat frame = cv_ptr->image;
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);

        auto ocr_results = ppocr.ocr(frame);
        auto status_results = parse_led_region.detectLEDs(frame, ocr_results);
        msgs.clear();  // 清空之前的结果
        msgs.reserve(status_results.size());  // 预分配内存

        
        for (const auto &[text, state] : status_results)
        {
            LEDStatusMsg led_status_msg;
            led_status_msg.name = text;

            switch (state)
            {
            case LEDState::RED:
                led_status_msg.state = LEDStatusMsg::RED;
                break;
            case LEDState::GREEN:
                led_status_msg.state = LEDStatusMsg::GREEN;
                break;
            case LEDState::YELLOW:
                led_status_msg.state = LEDStatusMsg::YELLOW;
                break;
            case LEDState::OFF:
                led_status_msg.state = LEDStatusMsg::OFF;
                break;
            default:
                led_status_msg.state = LEDStatusMsg::UNKNOWN;
            }
            msgs.push_back(led_status_msg);
        }
        return true;
    }

    bool parseLEDRegionMultiFrame(std::vector<LEDStatusMsg>& msgs,
                                std::string& info,
                                int frame_count = 5)
    {
        // 采集多帧图像
        std::vector<cv::Mat> frames;
        std::vector<OCRPredictResult> first_ocr_results;
        
        parseLEDText(first_ocr_results);

        // 设置相机参数
        led_params.auto_exposure_enabled = false;  // 设置曝光参数
        led_params.manual_exposure_time = 500.0f;
        
        setCameraParams(led_params);
        // 延时等待相机参数生效
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 采集多帧
        for (int i = 0; i < frame_count; ++i) {
            auto img_msg = getImageFromService();
            cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
            cv::Mat frame = cv_ptr->image;
            cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
            cv::imwrite("/tmp/led_frame_" + std::to_string(i) + ".jpg", frame);
            cv::waitKey(1);
            frames.push_back(frame.clone());
            
            // 简短延时以确保采集到不同帧
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // 使用多帧处理
        auto status_results = parse_led_region.detectLEDsMultiFrame(frames, first_ocr_results);
        
        msgs.clear();
        msgs.reserve(status_results.size());
        
        for (const auto& [text, state] : status_results) {
            LEDStatusMsg led_status_msg;
            led_status_msg.name = text;
            
            switch (state) {
                case LEDState::RED:
                    led_status_msg.state = LEDStatusMsg::RED;
                    break;
                case LEDState::GREEN:
                    led_status_msg.state = LEDStatusMsg::GREEN;
                    break;
                case LEDState::YELLOW:
                    led_status_msg.state = LEDStatusMsg::YELLOW;
                    break;
                case LEDState::OFF:
                    led_status_msg.state = LEDStatusMsg::OFF;
                    break;
                default:
                    led_status_msg.state = LEDStatusMsg::UNKNOWN;
            }
            msgs.push_back(led_status_msg);
        }
        
        return true;
    }

    // 解析LED区域文字信息
    bool parseLEDText(std::vector<OCRPredictResult>& ocr_results)
    {
        
        led_params.auto_exposure_enabled = false;  // 设置曝光参数
        led_params.manual_exposure_time = 8764.0f;
        setCameraParams(led_params);
        // 延时等待相机参数生效
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        auto img_msg = getImageFromService();
        cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(img_msg, sensor_msgs::image_encodings::RGB8);
        cv::Mat frame = cv_ptr->image;
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
        // imshow("LED Text", frame);
        // waitKey(0);
        ocr_results = ppocr.ocr(frame);
        // 打印OCR结果
        std::cout << "OCR Results:" << std::endl;
        for (const auto& result : ocr_results) {
            std::cout << "Text: " << result.text << ", Score: " << result.score << std::endl;
        }
        return true;
    }

private:
    rclcpp::Node::SharedPtr client_node_;
    rclcpp::Service<GetPanelInfo>::SharedPtr service_;
    rclcpp::Client<TriggerImageCapture>::SharedPtr trigger_image_client_; // 触发图像捕获客户端
    rclcpp::Client<SetCameraParamsSrv>::SharedPtr set_camera_params_client_;

    CameraParams led_params;
    ParseScreen parse_screen;
    ParseLedRegion parse_led_region;
    PPOCR ppocr;
};

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SingleShotRec>(argc, argv);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
