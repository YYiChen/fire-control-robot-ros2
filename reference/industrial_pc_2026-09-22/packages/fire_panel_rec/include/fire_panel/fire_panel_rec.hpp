#ifndef FIRE_PANEL_REC_HPP_
#define FIRE_PANEL_REC_HPP_

#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "video_manager/video/videoreader.h"
#include <opencv2/opencv.hpp>

#include <args.h>
#include <paddleocr.h>
#include <paddlestructure.h>
#include <gflags/gflags.h>

#include "blockingconcurrentqueue.h"
#include "concurrentqueue.h"
#include "parse_screen.h"

#include "rclcpp/rclcpp.hpp"
#include "common_interfaces/msg/screen_info.hpp"

using namespace PaddleOCR;

class FirePanelRec : public rclcpp::Node
{

public:
    FirePanelRec(int argc, char *argv[]);
    ~FirePanelRec();
    void initNode();
    void startProcessing(const std::string& video_src);
    void stopProcessing();
  

private:
    void captureThreadFunc();
    void processingThreadFunc();
    void pubPanelInfoThreadFunc();
    cv::Mat extractBlueROI(const cv::Mat &inputImage);
    void checkParams();

    // 成员变量
    VideoReader reader_;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<cv::Mat>> frameQueue_{10};
    moodycamel::BlockingConcurrentQueue<std::pair<int, std::vector<OCRPredictResult>>> processedQueue_{10};
    
    std::atomic<bool> processingDone_;
    std::thread captureThread_;
    std::thread processingThread_;
    std::thread pubPanelInfoThread_;

    ParseScreen parse_screen;
    PPOCR ppocr;

    rclcpp::Publisher<common_interfaces::msg::ScreenInfo>::SharedPtr status_pub;

};

#endif // FIRE_PANEL_REC_HPP_

