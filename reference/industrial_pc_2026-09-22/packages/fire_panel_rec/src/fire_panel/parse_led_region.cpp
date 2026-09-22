#include "parse_led_region.h"

ParseLedRegion::ParseLedRegion()
{
    // 初始化颜色阈值参数
    color_thresholds_ = {
        {"red", {{cv::Scalar(0, 100, 80), cv::Scalar(8, 255, 255)}, {cv::Scalar(170, 100, 80), cv::Scalar(180, 255, 255)}}},
        {"green", {{cv::Scalar(36, 50, 70), cv::Scalar(85, 255, 255)}}},
        {"yellow", {{cv::Scalar(20, 50, 70), cv::Scalar(35, 255, 255)}}}};

    // 初始化有效关键词（根据面板描述）
    valid_keywords_ = {
        "火", "火警", "监管报警", "公共故障",
        "屏蔽指示", "消", "消音", "系统故障",
        "警报屏蔽", "警报消音", "警报故障",
        "联动请求", "延时指示", "手动允许",
        "启动指示", "反馈指示", "自动允许",
        "主电工作", "备电工作", "气体喷洒",
        "主电故障", "备电故障", "传输故障",
        "接收指示", "发送指示", "传输屏蔽"};
    // 初始化文本映射
    text_mapping_ = {
        {"火", "火警"},
        {"消", "消音"},
        {"警", "火警"},
        {"音", "消音"}
        // 可以继续添加更多映射
    };
}
ParseLedRegion::~ParseLedRegion() {}

std::vector<std::pair<std::string, LEDState>> 
ParseLedRegion::detectLEDs(
    cv::Mat input_image,
    const std::vector<OCRPredictResult> &ocr_results)
{
    // ========== 核心新增：先执行文本拼接，得到合并后的OCR结果 ==========
    std::vector<OCRPredictResult> merged_ocr = mergeRelatedTexts(ocr_results);

    // 预处理优化图像质量
    cv::Mat processed = preprocessImage(input_image);
    cv::Mat hsv;
    cv::cvtColor(processed, hsv, cv::COLOR_BGR2HSV);

    std::vector<std::pair<std::string, LEDState>> results;

    // ========== 遍历合并后的OCR结果（而非原始OCR结果） ==========
    for (const auto &ocr : merged_ocr)
    {
        if (!isValidPanelText(ocr.text))
            continue; // 跳过非面板文本

        // 使用映射表处理文本显示
        std::string display_text = ocr.text;
        auto it = text_mapping_.find(ocr.text);
        if (it != text_mapping_.end())
        {
            display_text = it->second;
        }

        // 动态计算LED尺寸（基于【合并后】文本高度）
        int led_size = calculateLedSize(ocr.box);
        cv::Rect led_roi = locateLED(ocr.box, led_size);

        if (!validateROI(led_roi, input_image.size()))
        {
            results.emplace_back(display_text, LEDState::UNKNOWN);
            continue;
        }

        // 状态检测
        LEDState state = analyzeLED(hsv(led_roi));

        // 显示LED区域图像
        // cv::imshow("LED", processed(led_roi));
        // cv::waitKey(0);

        // 逻辑一致性校验
        // state = verifyConsistency(display_text, state);
        results.emplace_back(display_text, state);
    }

    return results;
}

// 多帧LED检测
std::vector<std::pair<std::string, LEDState>> 
ParseLedRegion::detectLEDsMultiFrame(
    const std::vector<cv::Mat>& input_images,
    const std::vector<OCRPredictResult>& ocr_results)
{
    if (input_images.empty() || ocr_results.empty()) {
        return {};
    }
    // cv::imshow("input_images", input_images[0]);
    // cv::waitKey(0);

    // ========== 核心新增：先执行文本拼接，得到合并后的OCR结果 ==========
    std::vector<OCRPredictResult> merged_ocr = mergeRelatedTexts(ocr_results);
    
    std::vector<std::pair<std::string, std::vector<LEDState>>> state_collections;
    
    // 初始化状态收集器（基于【合并后】的OCR结果）
    for (const auto& ocr : merged_ocr) {
        if (isValidPanelText(ocr.text)) {
            std::string display_text = ocr.text;
            auto it = text_mapping_.find(ocr.text);
            if (it != text_mapping_.end()) {
                display_text = it->second;
            }
            state_collections.emplace_back(display_text, std::vector<LEDState>());
        }
    }
    
    // 对每一帧进行处理并收集状态
    for (size_t frame_idx = 0; frame_idx < input_images.size(); ++frame_idx) {
        const cv::Mat& frame = input_images[frame_idx];
        // const auto& ocr_results = ocr_results_list[frame_idx];
        
        cv::Mat processed = preprocessImage(frame);
        cv::Mat hsv;
        cv::cvtColor(processed, hsv, cv::COLOR_BGR2HSV);
        
        // ========== 遍历合并后的OCR结果（而非原始OCR结果） ==========
        for (size_t i = 0; i < merged_ocr.size() && i < state_collections.size(); ++i) {
            const auto& ocr = merged_ocr[i];
            if (!isValidPanelText(ocr.text)) continue;
            
            int led_size = calculateLedSize(ocr.box);
            cv::Rect led_roi = locateLED(ocr.box, led_size);
            
            if (!validateROI(led_roi, frame.size())) {
                state_collections[i].second.push_back(LEDState::UNKNOWN);
                continue;
            }
            
            LEDState state;
            if (brightness_only_mode_) {
                // 获取对应的文本内容（基于合并后的文本）
                std::string display_text = merged_ocr[i].text;
                auto it = text_mapping_.find(merged_ocr[i].text);
                if (it != text_mapping_.end()) {
                    display_text = it->second;
                }
                state = analyzeLEDBrightness(processed(led_roi), display_text);
            } else {
                state = analyzeLED(hsv(led_roi));
            }
            state_collections[i].second.push_back(state);
        }
    }
    
    // 统计分析，确定最终状态
    std::vector<std::pair<std::string, LEDState>> final_results;
    for (const auto& collection : state_collections) {
        const auto& states = collection.second;

        // 判断逻辑：只要出现过亮起状态就判定为亮起
        LEDState final_state = LEDState::OFF; // 默认为熄灭状态
        
        // 检查是否有亮起的状态
        for (const auto& state : states) {
            if (state != LEDState::OFF && state != LEDState::UNKNOWN) {
                final_state = state;
                break;
            }
        }
        
        // 如果没有找到亮起状态，但有未知状态，则标记为未知
        if (final_state == LEDState::OFF) {
            for (const auto& state : states) {
                if (state == LEDState::UNKNOWN) {
                    final_state = LEDState::UNKNOWN;
                    break;
                }
            }
        }
        
        // // 统计各状态出现次数
        // std::map<LEDState, int> state_counts;
        // for (const auto& state : states) {
        //     state_counts[state]++;
        // }
        
        // // 选择出现次数最多的状态作为最终结果
        // LEDState final_state = LEDState::UNKNOWN;
        // int max_count = 0;
        // for (const auto& pair : state_counts) {
        //     if (pair.second > max_count) {
        //         max_count = pair.second;
        //         final_state = pair.first;
        //     }
        // }
        
        // // 如果大多数帧都是UNKNOWN，则使用任意非UNKNOWN状态
        // if (final_state == LEDState::UNKNOWN) {
        //     for (const auto& state : states) {
        //         if (state != LEDState::UNKNOWN) {
        //             final_state = state;
        //             break;
        //         }
        //     }
        // }
        
        final_results.emplace_back(collection.first, final_state);
    }
    
    return final_results;
}

cv::Mat ParseLedRegion::preprocessImage(const cv::Mat &img)
{
    cv::Mat processed;
    // 消除反光干扰
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    std::vector<cv::Mat> channels;
    cv::split(img, channels);
    for (int i = 0; i < 3; ++i)
    {
        clahe->apply(channels[i], channels[i]);
    }
    cv::merge(channels, processed);

    // 抗眩光处理 (屏蔽反光点)
    cv::Mat mask;
    cv::inRange(processed, cv::Scalar(220, 220, 220), cv::Scalar(255, 255, 255), mask);
    processed.setTo(cv::Scalar(180, 180, 180), mask);
    return processed;
}

// 判断是否为有效的面板文本
bool ParseLedRegion::isValidPanelText(const std::string &text)
{
    // 空文本过滤
    if (text.empty())
        return false;

    // 关键词检查（允许部分匹配）
    for (const auto &keyword : valid_keywords_)
    {
        if (text.find(keyword) != std::string::npos)
        {
            return true;
        }
    }

    // 面板特有的文本特征检查
    // 1. 状态词检查（如"故障""报警"等）
    const static std::vector<std::string> status_terms = {
        "故障", "报警", "工作", "允许", "启动"};
    for (const auto &term : status_terms)
    {
        if (text.find(term) != std::string::npos)
        {
            return true;
        }
    }

    // 2. 排除HTML/XML标签文本（如用户描述的<pFig>）
    if (text.find('<') != std::string::npos && text.find('>') != std::string::npos)
    {
        return false;
    }

    // 3. 排除位置标记文本（如<pos_86>）
    if (text.find("pos_") != std::string::npos)
    {
        return false;
    }

    // 4. 排除纯数字（如999）
    bool all_digits = !text.empty() &&
                      std::all_of(text.begin(), text.end(), ::isdigit);
    if (all_digits)
        return false;

    return false; // 默认视为无效文本
}

// 计算LED尺寸（基于文本高度）
int ParseLedRegion::calculateLedSize(const std::vector<std::vector<int>> &box)
{
    int min_y = std::min({box[0][1], box[1][1], box[2][1], box[3][1]});
    int max_y = std::max({box[0][1], box[1][1], box[2][1], box[3][1]});
    return static_cast<int>((max_y - min_y) * 0.8); // LED大小是文本高度的80%
}

// 定位LED区域
cv::Rect ParseLedRegion::locateLED(const std::vector<std::vector<int>> &box, int led_size)
{
    // 找到最左边的两个点（x坐标最小）
    std::vector<std::pair<int, int>> points = {
        {box[0][0], box[0][1]},
        {box[1][0], box[1][1]},
        {box[2][0], box[2][1]},
        {box[3][0], box[3][1]}};

    // 按x坐标排序
    std::sort(points.begin(), points.end());

    // 取最左边两个点计算中心点
    int left_center_x = (points[0].first + points[1].first) / 2;
    int left_center_y = (points[0].second + points[1].second) / 2;

    // 确定LED位置（左侧水平偏移）
    return cv::Rect(
        left_center_x - led_size,
        left_center_y - led_size / 2,
        led_size,
        led_size);
}

// 分析LED状态
LEDState ParseLedRegion::analyzeLED(const cv::Mat &led_region)
{
    // 1. 亮度检测
    cv::Mat gray;
    cv::cvtColor(led_region, gray, cv::COLOR_BGR2GRAY);
    double mean_brightness = cv::mean(gray)[0];

    // std::cout << "平均亮度: " << mean_brightness << std::endl;

    // 检测熄灭状态（考虑背景补偿）
    if (mean_brightness < 50)
        return LEDState::OFF;

    // 2. 色彩分析（HSV空间）
    for (const auto &[color, ranges] : color_thresholds_)
    {
        cv::Mat mask = cv::Mat::zeros(led_region.size(), CV_8UC1);
        for (const auto &range : ranges)
        {
            cv::Mat temp_mask;
            cv::inRange(led_region, range.first, range.second, temp_mask);
            mask = mask | temp_mask;
        }

        // 色彩有效性判断
        double coverage = cv::countNonZero(mask) / static_cast<double>(mask.total());
        // std::cout << "颜色 " << color << " 覆盖率: " << coverage << std::endl;

        if (coverage > 0.3)
        { // 30%面积覆盖即视为有效
            if (color == "red")
                return LEDState::RED;
            if (color == "green")
                return LEDState::GREEN;
            if (color == "yellow")
                return LEDState::YELLOW;
        }
    }

    return LEDState::OFF; // 默认熄灭状态
}

// 亮度分析方法
LEDState ParseLedRegion::analyzeLEDBrightness(const cv::Mat& led_region, const std::string& text)
{
    // 转换为灰度图
    cv::Mat gray, binary;
    cv::cvtColor(led_region, gray, cv::COLOR_BGR2GRAY);

    // 自适应阈值分割
    cv::adaptiveThreshold(gray, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, 
                         cv::THRESH_BINARY, 11, 2);
    
    // 计算亮区域占比
    double brightRatio = cv::countNonZero(binary) / (double)(binary.rows * binary.cols);
    
    // 如果亮区域占比超过一定阈值，则判断为亮
    if (brightRatio < 0.05) {  // 5%的像素为亮
        return LEDState::OFF;
    }
    else {
        // 根据文本内容确定LED颜色
        if (text.find("火") != std::string::npos || 
            text.find("火警") != std::string::npos ||
            text.find("监管报警") != std::string::npos ||
            text.find("屏蔽指示") != std::string::npos)
        {
            return LEDState::RED;
        }
        else{
            return LEDState::GREEN; // 默认绿色
        }
    }

    // 调试
    // std::cout << "Text: " << text << std::endl;
    // cv::imshow("LED Brightness", gray);
    // cv::waitKey(0);
    
    // 计算平均亮度
    // double mean_brightness = cv::mean(gray)[0];
    // // std::cout << "平均亮度: " << mean_brightness << std::endl;
    // // 根据亮度判断状态
    // if (mean_brightness < 50) {
    //     return LEDState::OFF;
    // } else {
    //     // 根据文本内容确定LED颜色
    //     if (text.find("火") != std::string::npos || 
    //         text.find("火警") != std::string::npos ||
    //         text.find("监管报警") != std::string::npos ||
    //         text.find("屏蔽指示") != std::string::npos)
    //     {
    //         return LEDState::RED;
    //     }
    //     else{
    //         return LEDState::GREEN; // 默认绿色
    //     }
    // }
}

// 逻辑一致性验证
LEDState ParseLedRegion::verifyConsistency(const std::string &text, LEDState state)
{
    // 关键状态逻辑规则
    static const std::unordered_map<std::string, LEDState> expected_states = {
        {"火警", LEDState::RED},
        {"故障", LEDState::YELLOW},
        {"监管报警", LEDState::RED},
        {"启动", LEDState::GREEN}};

    if (auto it = expected_states.find(text); it != expected_states.end())
    {
        if (state == LEDState::OFF || state == it->second)
        {
            return state; // 状态符合或熄灭则保留
        }
        return it->second; // 强制修正为预期状态
    }
    return state;
}

// ROI区域有效性检查
bool ParseLedRegion::validateROI(const cv::Rect &roi, const cv::Size &img_size)
{
    return roi.x >= 0 && roi.y >= 0 &&
           roi.x + roi.width <= img_size.width &&
           roi.y + roi.height <= img_size.height;
}

std::vector<OCRPredictResult> ParseLedRegion::mergeRelatedTexts(const std::vector<OCRPredictResult> &ocr_results)
{
    std::vector<OCRPredictResult> merged_results;
    std::vector<bool> processed(ocr_results.size(), false);

    for (size_t i = 0; i < ocr_results.size(); ++i)
    {
        if (processed[i])
            continue;

        const auto &current = ocr_results[i];
        processed[i] = true;

        // 检查是否需要与其他文本合并
        bool merged = false;
        for (size_t j = i + 1; j < ocr_results.size(); ++j)
        {
            if (processed[j])
                continue;

            const auto &next = ocr_results[j];

            // 判断是否需要合并（基于位置和内容）
            if (shouldMergeTexts(current, next))
            {
                // 合并文本
                OCRPredictResult merged_text = mergeTwoTexts(current, next);
                merged_results.push_back(merged_text);
                processed[j] = true;
                merged = true;
                break;
            }
        }

        // 如果没有合并，则直接添加
        if (!merged)
        {
            merged_results.push_back(current);
        }
    }

    return merged_results;
}

// 判断两个文本是否应该合并
bool ParseLedRegion::shouldMergeTexts(const OCRPredictResult &text1, const OCRPredictResult &text2)
{
    // 1. 检查内容是否可以组成有效词汇
    std::string combined1 = text1.text + text2.text;
    std::string combined2 = text2.text + text1.text;

    for (const auto &keyword : valid_keywords_)
    {
        if (keyword == combined1 || keyword == combined2)
        {
            // 2. 检查位置是否相邻
            if (areTextsAdjacent(text1.box, text2.box))
            {
                return true;
            }
        }
    }

    return false;
}

// 判断两个文本框是否相邻
bool ParseLedRegion::areTextsAdjacent(const std::vector<std::vector<int>> &box1,
                      const std::vector<std::vector<int>> &box2)
{
    // 计算两个文本框的中心点
    int center1_x = (box1[0][0] + box1[1][0] + box1[2][0] + box1[3][0]) / 4;
    int center1_y = (box1[0][1] + box1[1][1] + box1[2][1] + box1[3][1]) / 4;
    int center2_x = (box2[0][0] + box2[1][0] + box2[2][0] + box2[3][0]) / 4;
    int center2_y = (box2[0][1] + box2[1][1] + box2[2][1] + box2[3][1]) / 4;

    // 计算两个文本框的高度
    int height1 = std::max({box1[0][1], box1[1][1], box1[2][1], box1[3][1]}) -
                  std::min({box1[0][1], box1[1][1], box1[2][1], box1[3][1]});
    int height2 = std::max({box2[0][1], box2[1][1], box2[2][1], box2[3][1]}) -
                  std::min({box2[0][1], box2[1][1], box2[2][1], box2[3][1]});
    int avg_height = (height1 + height2) / 2;

    // 检查水平距离是否在合理范围内（例如，不超过平均高度的2倍）
    int horizontal_distance = std::abs(center1_x - center2_x);
    int vertical_distance = std::abs(center1_y - center2_y);

    return (horizontal_distance < avg_height * 3) && (vertical_distance < avg_height * 0.5);
}

// 合并两个文本框
OCRPredictResult ParseLedRegion::mergeTwoTexts(const OCRPredictResult &text1, const OCRPredictResult &text2)
{
    OCRPredictResult merged;

    // 合并文本内容
    // 根据位置确定合并顺序
    int center1_x = (text1.box[0][0] + text1.box[1][0] + text1.box[2][0] + text1.box[3][0]) / 4;
    int center2_x = (text2.box[0][0] + text2.box[1][0] + text2.box[2][0] + text2.box[3][0]) / 4;

    if (center1_x < center2_x)
    {
        merged.text = text1.text + text2.text;
    }
    else
    {
        merged.text = text2.text + text1.text;
    }

    // 合并边界框（创建包含两个框的新框）
    int min_x = std::min({text1.box[0][0], text1.box[1][0], text1.box[2][0], text1.box[3][0],
                          text2.box[0][0], text2.box[1][0], text2.box[2][0], text2.box[3][0]});
    int max_x = std::max({text1.box[0][0], text1.box[1][0], text1.box[2][0], text1.box[3][0],
                          text2.box[0][0], text2.box[1][0], text2.box[2][0], text2.box[3][0]});
    int min_y = std::min({text1.box[0][1], text1.box[1][1], text1.box[2][1], text1.box[3][1],
                          text2.box[0][1], text2.box[1][1], text2.box[2][1], text2.box[3][1]});
    int max_y = std::max({text1.box[0][1], text1.box[1][1], text1.box[2][1], text1.box[3][1],
                          text2.box[0][1], text2.box[1][1], text2.box[2][1], text2.box[3][1]});

    // 创建新的边界框
    merged.box = {{min_x, min_y}, {max_x, min_y}, {max_x, max_y}, {min_x, max_y}};

    return merged;
}
