#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <map>

#include "utility.h"
#include "opencv2/opencv.hpp"

using namespace PaddleOCR;
// LED状态定义
enum class LEDState { OFF, RED, GREEN, YELLOW, UNKNOWN };

class ParseLedRegion { 

public:
    ParseLedRegion();
    ~ParseLedRegion();
    
    std::vector<std::pair<std::string, LEDState>> detectLEDs(
        cv::Mat frame,
        const std::vector<OCRPredictResult> &ocr_results);
    cv::Mat preprocessImage(const cv::Mat &img);
    bool isValidPanelText(const std::string &text);
    int calculateLedSize(const std::vector<std::vector<int>> &box);
    cv::Rect locateLED(const std::vector<std::vector<int>> &box, int led_size);
    LEDState analyzeLED(const cv::Mat &led_region);
    LEDState verifyConsistency(const std::string &text, LEDState state);
    bool validateROI(const cv::Rect &roi, const cv::Size &img_size);
    std::vector<OCRPredictResult> mergeRelatedTexts(const std::vector<OCRPredictResult> &ocr_results);
    bool shouldMergeTexts(const OCRPredictResult &text1, const OCRPredictResult &text2);
    bool areTextsAdjacent(const std::vector<std::vector<int>> &box1,
                      const std::vector<std::vector<int>> &box2);
    OCRPredictResult mergeTwoTexts(const OCRPredictResult &text1, const OCRPredictResult &text2);

    // 新增的多帧处理方法
    std::vector<std::pair<std::string, LEDState>> detectLEDsMultiFrame(
        const std::vector<cv::Mat>& input_images,
        const std::vector<OCRPredictResult>& ocr_results);

    // 亮度分析方法
    LEDState analyzeLEDBrightness(const cv::Mat& led_region, const std::string& text);
    
    // 设置识别模式
    void setBrightnessOnlyMode(bool brightness_only) { brightness_only_mode_ = brightness_only; }    


private:
    std::unordered_map<std::string, std::string> text_mapping_;
    std::set<std::string> valid_keywords_;  // 有效指示灯关键词
    std::unordered_map<std::string, std::vector<std::pair<cv::Scalar, cv::Scalar>>> color_thresholds_;

    bool brightness_only_mode_ = false; // 仅亮度模式     
};



