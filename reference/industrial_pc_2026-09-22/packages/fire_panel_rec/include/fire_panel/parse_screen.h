
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <map>

#include "utility.h"

using namespace PaddleOCR;

// 火灾报警状态消息结构
struct ScreenInfoStruct {
    enum class CurrentPage : uint8_t {
        NORMAL_PAGE, 
        ALERT_PAGE, 
        PASSWORD_INPUT_PAGE, 
        CONFIRM_CLICK_PAGE
    };

    CurrentPage current_page;
    std::string first_alarm_id;
    std::string first_alarm_time;
    std::string first_alarm_type;
    std::string active_alarm_id;
    std::string active_alarm_time;
    std::string active_alarm_type;
    bool has_supervision = false;
    bool has_activation = false;
    bool has_feedback = false;
    bool has_delay = false;
    bool has_fault = false;
    bool has_shield = false;
    uint32_t total_alarms = 0;
    uint32_t total_starts = 0;
};

class ParseScreen {
public:
    ParseScreen();
    ~ParseScreen();

    std::string cleanOcrText(const std::string& input);
    void toHalfWidth(std::string &str);
    std::map<int, std::vector<OCRPredictResult>> groupTextByRows(
                        const std::vector<OCRPredictResult>& ocr_results, 
                        int row_threshold = 30);
    std::string mergeRowText(const std::vector<OCRPredictResult>& row_results);
    void determineCurrentPage(ScreenInfoStruct& status, const std::string& text);
    void checkAlarmState(ScreenInfoStruct& status);
    ScreenInfoStruct parseOCRResults(const std::vector<OCRPredictResult>& ocr_result);

private:
    // 白名单替换规则
    const std::map<std::string, std::string> keyword_correction = {
        {"火臀", "火警"},
        {"首臀", "首火"},
        {"监臂", "监管"},
        {"故陈", "故障"},
        {"反债", "反馈"},
        {"屏敲", "屏蔽"},
        {"启协", "启动"},
        {"延寺", "延时"},
        {"总或", "总火"}
    };
    // 编辑距离计算
    int editDistance(const std::string &s1, const std::string &s2);
    // 模糊匹配关键字
    bool isKeywordSimilar(const std::string &input, const std::string &target, int max_distance = 1);
};






