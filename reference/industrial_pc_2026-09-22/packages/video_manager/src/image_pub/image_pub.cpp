#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "opencv2/opencv.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "cv_bridge/cv_bridge.h"

#include "video_manager/hikvision/hk_camera_reader.h"

using namespace std::chrono_literals;

class ImagePub : public rclcpp::Node
{

public:
    ImagePub()
        : Node("image_pub")
    {
        loadParam();
        initNode();
        startProcessing();
    }
    ~ImagePub() 
    {
        if (captureThread_.joinable())
            captureThread_.join();
    };

    void customShutdown() {
        processingDone_ = true;
        if (captureThread_.joinable()) {
            captureThread_.join();
        }
    }

    void loadParam()
    {
        this->declare_parameter<std::string>("image_pub_topic_name", "/hk_cam/image_raw");
        // 将视频URL参数改为设备ID参数
        this->declare_parameter<std::string>("camera_device_id", ""); // 海康相机设备ID
        this->declare_parameter<std::string>("ost_path", "");
        this->declare_parameter<bool>("is_rectified", false);

        this->get_parameter("image_pub_topic_name", image_pub_topic_name);
        this->get_parameter("camera_device_id", camera_device_id_); // 获取设备ID
        this->get_parameter("ost_path", ost_path_);
        this->get_parameter("is_rectified", is_rectified_);

        if (is_rectified_ && !ost_path_.empty()) 
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
        image_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
            image_pub_topic_name, 3);
        camera_info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>(
            "/hk_cam/camera_info", 3);
        // cp_image_pub_ = this->create_publisher<sensor_msgs::msg::CompressedImage>(
        //     image_pub_topic_name, 3);
    }

    void startProcessing()
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
            captureThread_ = std::thread(&ImagePub::captureThreadFunc, this);
        }
    }

    void captureThreadFunc()
    {
        cv::Mat frame;
        
        // 读取第一帧以设置相机信息
        // if (reader_.read(frame)) {
        //     setCameraInfo(frame);
        // }
        
        while (reader_.read(frame) && !processingDone_ && rclcpp::ok())
        {
            if (!frame.empty()) // 避免帧差引起发布空图
            {
                pubFrame(frame);
                camera_info_msg.header.stamp = this->now();
                camera_info_pub_->publish(camera_info_msg);
                // cv::imshow("frame", frame);
                // cv::imshow("un_img", un_img);
                cv::waitKey(1);
            }
        }
    }

    void pubFrame(cv::Mat frame)
    { 
        sensor_msgs::msg::Image img_msg;
        if(is_rectified_)
        {
            cv::Mat un_img;
            cv::undistort(frame, un_img, camera_matrix, dist_coeffs); // 做畸变矫正
            cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", un_img).toImageMsg(img_msg);
        }
        else
        {
            cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg(img_msg);
        }
        
        img_msg.header.stamp = this->now();
        img_msg.header.frame_id = "camera";
        image_pub_->publish(img_msg);
    }
    
    void setCameraInfo(cv::Mat frame)
    {
        if (!ost_path_.empty() && (frame.rows != image_height_ || frame.cols != image_width_)) {
            RCLCPP_WARN(this->get_logger(), "Frame size (%dx%d) does not match camera info size (%dx%d). Using frame size.",
                        frame.cols, frame.rows, image_width_, image_height_);
            image_width_ = frame.cols;
            image_height_ = frame.rows;
        }
        
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
        camera_info_msg.roi.height = frame.rows;
        camera_info_msg.roi.width = frame.cols;
        camera_info_msg.roi.do_rectify = false;
    }


    bool isNum(std::string str)
    {
        std::stringstream sin(str);
        double d;
        char c;
        if (!(sin >> d))
        {
            return false;
        }
        if (sin >> c)
        {
            return false;
        }
        return true;
    }

private:
    
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
    rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_pub_;
    // rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr cp_image_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    sensor_msgs::msg::CameraInfo camera_info_msg;
    
    HKCameraReader reader_; // 使用海康相机读取器
    std::atomic<bool> processingDone_;
    std::thread captureThread_;

    cv::Mat camera_matrix;  //内参矩阵，需初始化
    cv::Mat dist_coeffs;    //畸变矩阵，需初始化
    cv::Mat rectification_matrix;   // 校正矩阵，需初始化
    cv::Mat projection_matrix;      //投影矩阵，需初始化

    std::string image_pub_topic_name;
    std::string ost_path_;
    std::string camera_device_id_; // 海康相机设备ID
    bool is_rectified_;
    int image_width_;
    int image_height_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImagePub>());
    rclcpp::shutdown();
    return 0;
}