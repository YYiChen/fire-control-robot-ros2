#include "fire_panel_rec.hpp"
#include "rclcpp/rclcpp.hpp"
FirePanelRec::FirePanelRec(int argc, char *argv[])
    : Node("fire_panel_rec")
{
    checkParams();
    initNode();
    std::string video_src = "rtsp://<camera-host>/...";
    startProcessing(video_src);
}

// 析构函数
FirePanelRec::~FirePanelRec()
{
    if (captureThread_.joinable())
        captureThread_.join();
    if (processingThread_.joinable())
        processingThread_.join();
    if (pubPanelInfoThread_.joinable())
        pubPanelInfoThread_.join();
}

void FirePanelRec::checkParams()
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
void FirePanelRec::initNode()
{
    status_pub = this->create_publisher<common_interfaces::msg::ScreenInfo>(
        "/fire_alarm/status", 1000);
}

// 启动视频处理
void FirePanelRec::startProcessing(const std::string &video_src)
{
    if (!reader_.open(video_src))
    {
        std::cerr << "Failed to open video file" << std::endl;
        return;
    }

    captureThread_ = std::thread(&FirePanelRec::captureThreadFunc, this);
    processingThread_ = std::thread(&FirePanelRec::processingThreadFunc, this);
    pubPanelInfoThread_ = std::thread(&FirePanelRec::pubPanelInfoThreadFunc, this);
}

// 停止处理
void FirePanelRec::stopProcessing()
{
    processingDone_ = true;
    if (captureThread_.joinable())
        captureThread_.join();
    if (processingThread_.joinable())
        processingThread_.join();
    if (pubPanelInfoThread_.joinable())
        pubPanelInfoThread_.join();

    reader_.close();
    cv::destroyAllWindows();
    std::cout << "Video processing completed successfully." << std::endl;
}

cv::Mat FirePanelRec::extractBlueROI(const cv::Mat &inputImage)
{
    if (inputImage.empty())
    {
        std::cerr << "Input image is empty!" << std::endl;
        return cv::Mat();
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
        return inputImage;
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
        return inputImage;
    }

    cv::Rect boundingBox = cv::boundingRect(contours[maxIdx]);

    // 提取原始图像中的蓝色 ROI 区域
    cv::Mat roiImage = inputImage(boundingBox).clone();

    return roiImage;
}

// 捕获线程函数
void FirePanelRec::captureThreadFunc()
{
    cv::Mat frame;
    const size_t max_queue_size = 5;
    while (reader_.read(frame) && !processingDone_ && rclcpp::ok())
    {
        // 队列过载时跳过
        // if (frameQueue_.size_approx() > 3)
        //     continue;
        if (frameQueue_.size_approx() > max_queue_size)
        {
            std::shared_ptr<cv::Mat> oldFrame;
            frameQueue_.try_dequeue(oldFrame); // 弹出并释放最老的一帧
        }
        auto framePtr = std::make_shared<cv::Mat>(frame.clone());
        frameQueue_.enqueue(framePtr);
    }
}

// 处理线程函数
void FirePanelRec::processingThreadFunc() 
{
    std::shared_ptr<cv::Mat> framePtr;
    int frameIndex = 0;
    PPOCR ppocr;

    while (!processingDone_ && rclcpp::ok()) {
        frameQueue_.wait_dequeue(framePtr);
        // std::cout << "Frame queue size: " << frameQueue_.size_approx() << std::endl;
        auto start = std::chrono::steady_clock::now();
        cv::Mat blueMask = extractBlueROI(*framePtr);
        // 翻转图像 180 度
        cv::Mat flippedFrame;
        cv::flip(blueMask, flippedFrame, -1); // -1 表示绕中心翻转 180 度
        std::vector<OCRPredictResult> ocr_result = ppocr.ocr(flippedFrame);
        Utility::VisualizeBboxes(flippedFrame, ocr_result, " ");
        auto end = std::chrono::steady_clock::now();
        // 延迟200ms
        // std::this_thread::sleep_for(std::chrono::milliseconds(200));
        cv::imshow("frame", flippedFrame);
        if (cv::waitKey(1) == 27) processingDone_ = true;

        processedQueue_.enqueue(std::make_pair(frameIndex++, ocr_result));
    }
}

// OCR解析线程函数
void FirePanelRec::pubPanelInfoThreadFunc() 
{
    std::pair<int, std::vector<OCRPredictResult>> processedFrame;
    common_interfaces::msg::ScreenInfo status_msg;
    const size_t max_processed_queue_size = 5;
    while (!processingDone_ && rclcpp::ok()) {
        if (processedQueue_.size_approx() > max_processed_queue_size) {
            std::pair<int, std::vector<OCRPredictResult>> oldData;
            processedQueue_.try_dequeue(oldData); // 丢弃旧数据
        }
        processedQueue_.wait_dequeue(processedFrame);
        std::cout << "processe queue size: " << processedQueue_.size_approx() << std::endl;
        ScreenInfoStruct status = parse_screen.parseOCRResults(processedFrame.second);
        
        status_msg.header.stamp = rclcpp::Clock().now();
        status_msg.header.frame_id = "fire_panel_rec";
        status_msg.model = "JB-TG-HJ9000";
        status_msg.current_page = static_cast<uint8_t>(status.current_page);
        status_msg.active_alarm_id = status.active_alarm_id;
        status_msg.active_alarm_time = status.active_alarm_time;
        status_msg.active_alarm_type = status.active_alarm_type;
        status_msg.first_alarm_id = status.first_alarm_id;
        status_msg.first_alarm_time = status.first_alarm_time;
        status_msg.first_alarm_type = status.first_alarm_type;
        status_msg.total_alarms = status.total_alarms;
        status_msg.has_activation = status.has_activation;
        status_msg.has_delay = status.has_delay;
        status_msg.has_fault = status.has_fault;
        status_msg.has_feedback = status.has_feedback;
        status_msg.has_shield = status.has_shield;
        status_msg.has_supervision = status.has_supervision;
        status_msg.total_starts = status.total_starts;

        status_pub->publish(status_msg);
    
    }
}

