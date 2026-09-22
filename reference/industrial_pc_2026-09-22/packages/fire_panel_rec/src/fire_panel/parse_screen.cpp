#include <parse_screen.h>
#include <regex>

ParseScreen::ParseScreen() {}

ParseScreen::~ParseScreen() {}

// 辅助函数：去除字符串空格和特殊字符
std::string ParseScreen::cleanOcrText(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (!std::isspace(static_cast<unsigned char>(c)) && 
            c != '\n' && c != '\r' && c != '\t') {
            output += c;
        }
    }
    return output;
}

// 替换全角标点为半角
void ParseScreen::toHalfWidth(std::string &str) {
    for (char &c : str) {
        if (c == '：') c = ':';
        else if (c == '，') c = ',';
        else if (c == '“' || c == '”') c = '"';
        // 可以继续添加其他替换规则
    }
}

int ParseScreen::editDistance(const std::string &s1, const std::string &s2) {
    int m = s1.size(), n = s2.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (int i = 0; i <= m; ++i) dp[i][0] = i;
    for (int j = 0; j <= n; ++j) dp[0][j] = j;

    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }

    return dp[m][n];
}

bool ParseScreen::isKeywordSimilar(const std::string &input, const std::string &target, int max_distance) {
    auto it = keyword_correction.find(input);
    if (it != keyword_correction.end()) {
        return it->second == target;
    }
    return editDistance(input, target) <= max_distance;
}

// 辅助函数：按照Y坐标分组行
std::map<int, std::vector<OCRPredictResult>> 
ParseScreen::groupTextByRows(
    const std::vector<OCRPredictResult>& ocr_results, 
    int row_threshold) {
    
    std::map<int, std::vector<OCRPredictResult>> rows;
    
    for (const auto& result : ocr_results) {
        if (result.box.empty() || result.box[0].size() < 2) continue;
        
        int y_pos = result.box[0][1]; // 左上角Y坐标
        
        // 寻找最近的Y坐标分组
        auto it = rows.lower_bound(y_pos - row_threshold);
        if (it != rows.end() && y_pos - it->first < row_threshold) {
            it->second.push_back(result);
        } else {
            rows[y_pos] = {result};
        }
    }
    return rows;
}

// 合并行文本
std::string ParseScreen::mergeRowText(const std::vector<OCRPredictResult>& row_results) {
    std::string row_text;
    for (const auto& item : row_results) {
        row_text += cleanOcrText(item.text);
    }
    return row_text;
}

// 判断页面类型
void ParseScreen::determineCurrentPage(ScreenInfoStruct& status, const std::string& text) {
    if (text.find("请输入密码") != std::string::npos) {
        status.current_page = ScreenInfoStruct::CurrentPage::PASSWORD_INPUT_PAGE;
    } else if (text.find("按确认键生效") != std::string::npos) {
        status.current_page = ScreenInfoStruct::CurrentPage::CONFIRM_CLICK_PAGE;
    } else if (text.find("火灾报警控制器") != std::string::npos) {
        status.current_page = ScreenInfoStruct::CurrentPage::NORMAL_PAGE;
    }
}

// 检查是否进入报警状态
void ParseScreen::checkAlarmState(ScreenInfoStruct& status) {
    bool is_alarm = (status.total_alarms > 0) ||
                   (!status.active_alarm_id.empty() && status.active_alarm_id != "无");

    if (!is_alarm) {
        status.first_alarm_id.clear();
        status.first_alarm_time.clear();
        status.first_alarm_type.clear();
        status.active_alarm_id.clear();
        status.active_alarm_time.clear();
        status.active_alarm_type.clear();
        status.total_alarms = 0;
        status.total_starts = 0;
    } else {
        status.current_page = ScreenInfoStruct::CurrentPage::ALERT_PAGE;
    }
}

// 主解析函数
ScreenInfoStruct ParseScreen::parseOCRResults(const std::vector<OCRPredictResult>& ocr_result) {
    ScreenInfoStruct status;
    std::map<std::string, std::string> parsed_data;
    std::smatch match;
    std::regex alarm_pattern(R"(^([A-Za-z0-9]{4})(\d{2}:\d{2})(.*))");  // 匹配ID，时间，描述

    // 1. 按文本行分组
    auto row_map = groupTextByRows(ocr_result);
    
    // 2. 处理每行文本
    for (auto& [y_pos, row_results] : row_map) {
        // 按X坐标排序（从左到右）
        std::sort(row_results.begin(), row_results.end(),
            [](const OCRPredictResult& a, const OCRPredictResult& b) {
                return a.box[0][0] < b.box[0][0]; // 比较左上角X坐标
            });
        
        // 合并整行文本
        std::string row_text = mergeRowText(row_results);
        // std::cout << "Row Text: " << row_text << std::endl;
        determineCurrentPage(status, row_text);

        // 解析关键字段 (基于已知面板格式)
        std::string prefix;
        size_t pos;

        // 提取前缀（如“火警：”，最多取6个字符）
        if (row_text.length() >= 6) {
            prefix = row_text.substr(0, 6);
        } else {
            continue;
        }
        // std::cout << "prefix: " << prefix << std::endl;
        // 解析关键字段 (基于已知面板格式)
        // if (row_text.find("首火：") != std::string::npos) 
        if (isKeywordSimilar(prefix, "首火"))
        {
            pos = row_text.find("：") + 3; // 跳过"首火:"
            std::string content = row_text.substr(pos);
            if (std::regex_search(content, match, alarm_pattern) && match.size() > 2) {
                // parsed_data["首火_ID"] = match[1].str();
                // parsed_data["首火_TIME"] = match[2].str();
                // parsed_data["首火_TYPE"] = match[3].str();
                status.first_alarm_id = match[1].str();
                status.first_alarm_time = match[2].str();
                status.first_alarm_type = match[3].str();
            }
        }
        // else if (row_text.find("火警：") != std::string::npos) 
        else if (isKeywordSimilar(prefix, "火警"))
        {
            pos = row_text.find("：") + 3; // 跳过"火警:"
            std::string content = row_text.substr(pos);
            // std::cout << "content: " << content << std::endl;
            if (std::regex_search(content, match, alarm_pattern) && match.size() > 2) {
                // parsed_data["火警_ID"] = match[1].str();
                // parsed_data["火警_TIME"] = match[2].str();
                // parsed_data["火警_TYPE"] = match[3].str();
                status.active_alarm_id = match[1].str();
                status.active_alarm_time = match[2].str();
                status.active_alarm_type = match[3].str();
            }
        }
        else if (row_text.find("总火：") != std::string::npos) {
            size_t pos = row_text.find("总火：") + 9; // 跳过"总火:"
            // 提取数字部分
            std::string num_str;
            for (size_t i = pos; i < row_text.size(); ++i) {
                if (std::isdigit(row_text[i])) num_str += row_text[i];
                else break;
            }
            parsed_data["总火"] = num_str;
        }
        else if (row_text.find("总启：") != std::string::npos) {
            size_t pos = row_text.find("总启：") + 9; // 跳过"总启:"
            // 提取数字部分
            std::string num_str;
            for (size_t i = pos; i < row_text.size(); ++i) {
                if (std::isdigit(row_text[i])) num_str += row_text[i];
                else break;
            }
            parsed_data["总启"] = num_str;
        }
        else if (row_text.find("监管：") != std::string::npos) {
            parsed_data["监管"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("启动：") != std::string::npos) {
            parsed_data["启动"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("反馈：") != std::string::npos) {
            parsed_data["反馈"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("延时：") != std::string::npos) {
            parsed_data["延时"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("故障：") != std::string::npos) {
            parsed_data["故障"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("屏蔽：") != std::string::npos) {
            parsed_data["屏蔽"] = (row_text.find("无") != std::string::npos) ? "0" : "1";
        }
        else if (row_text.find("火警总数：") != std::string::npos) {
            size_t pos = row_text.find("火警总数：") + 15; // 跳过"火警总数:"
            // 提取数字部分
            std::string num_str;
            for (size_t i = pos; i < row_text.size(); ++i) {
                if (std::isdigit(row_text[i])) num_str += row_text[i];
                else break;
            }
            parsed_data["火警总数"] = num_str;
        }
    }
    
    // 3. 填充状态结构体
    
    // 处理状态标志
    if (parsed_data.count("监管")) status.has_supervision = (parsed_data["监管"] == "1");
    if (parsed_data.count("启动")) status.has_activation = (parsed_data["启动"] == "1");
    if (parsed_data.count("反馈")) status.has_feedback = (parsed_data["反馈"] == "1");
    if (parsed_data.count("延时")) status.has_delay = (parsed_data["延时"] == "1");
    if (parsed_data.count("故障")) status.has_fault = (parsed_data["故障"] == "1");
    if (parsed_data.count("屏蔽")) status.has_shield = (parsed_data["屏蔽"] == "1");
    
    // 处理火警总数
    if (parsed_data.count("火警总数")) {
        try {
            status.total_alarms = std::stoul(parsed_data["火警总数"]);
        } catch (...) {
            // 转换失败时使用默认值0
            status.total_alarms = 0;
        }
    }
    // 如果火警总数缺失但总火存在，使用总火值
    else if (parsed_data.count("总火")) {
        try {
            status.total_alarms = std::stoul(parsed_data["总火"]);
        } catch (...) {
            status.total_alarms = 0;
        }
    }
    else if (parsed_data.count("总启")) {
        try {
            status.total_starts = std::stoul(parsed_data["总启"]);
        } catch (...) {
            status.total_starts = 0;
        }
    }
    
    checkAlarmState(status);
    
    return status;
}
