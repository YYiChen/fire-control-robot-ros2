// Config structures v2.1 (2025-08)


#pragma once

#include "geometry_msgs/msg/pose.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h> 

namespace task_manager {

inline geometry_msgs::msg::Pose parsePose(const std::vector<double>& values) 
{
    if (values.size() != 7) {
        throw std::runtime_error("Invalid pose format: expected 7 values [x, y, z, qx, qy, qz, qw]");
    }
    
    geometry_msgs::msg::Pose pose;
    pose.position.x = values[0];
    pose.position.y = values[1];
    pose.position.z = values[2];
    pose.orientation.x = values[3];
    pose.orientation.y = values[4];
    pose.orientation.z = values[5];
    pose.orientation.w = values[6];
    return pose;
}

inline std::vector<double> poseToArray(const geometry_msgs::msg::Pose& pose) {
    return {
        pose.position.x, pose.position.y, pose.position.z,
        pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w
    };
}

// 参数解析助手类
class ParameterParser {
public:
    // 解析字符串为整数
    static int parseInt(const std::string& str, int default_value = 0) {
        try {
            return std::stoi(str);
        } catch (...) {
            return default_value;
        }
    }
    
    // 解析字符串为双精度浮点数
    static double parseDouble(const std::string& str, double default_value = 0.0) {
        try {
            return std::stod(str);
        } catch (...) {
            return default_value;
        }
    }
    
    // 解析字符串为布尔值
    static bool parseBool(const std::string& str, bool default_value = false) {
        if (str == "true" || str == "True" || str == "1") {
            return true;
        } else if (str == "false" || str == "False" || str == "0") {
            return false;
        }
        return default_value;
    }
    
    // 解析字符串为位姿
    static geometry_msgs::msg::Pose parsePose(const std::string& str) {
        try {
            // 移除方括号
            std::string clean_str = str;
            if (!clean_str.empty() && clean_str.front() == '[') {
                clean_str = clean_str.substr(1);
            }
            if (!clean_str.empty() && clean_str.back() == ']') {
                clean_str = clean_str.substr(0, clean_str.length() - 1);
            }
            
            // 分割字符串
            std::vector<double> values;
            std::stringstream ss(clean_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // 去除空格
                item.erase(0, item.find_first_not_of(' '));
                item.erase(item.find_last_not_of(' ') + 1);
                values.push_back(std::stod(item));
            }
            
            return task_manager::parsePose(values);
        } catch (...) {
            throw std::runtime_error("Failed to parse pose from string: " + str);
        }
    }
    
    // 解析字符串为双精度浮点数向量
    static std::vector<double> parseDoubleVector(const std::string& str) {
        std::vector<double> result;
        try {
            // 移除方括号
            std::string clean_str = str;
            if (!clean_str.empty() && clean_str.front() == '[') {
                clean_str = clean_str.substr(1);
            }
            if (!clean_str.empty() && clean_str.back() == ']') {
                clean_str = clean_str.substr(0, clean_str.length() - 1);
            }
            
            // 分割字符串
            std::stringstream ss(clean_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // 去除空格
                item.erase(0, item.find_first_not_of(' '));
                item.erase(item.find_last_not_of(' ') + 1);
                result.push_back(std::stod(item));
            }
        } catch (...) {
            throw std::runtime_error("Failed to parse double vector from string: " + str);
        }
        return result;
    }
};

// 任务步骤配置
struct TaskStep {
    std::string task_type;      // 任务类型: "lift_control", "aruco_localization" 等
    std::string description;    // 步骤描述
    std::unordered_map<std::string, std::string> parameters; // 参数键值对
    
    YAML::Node toYaml() const {
        YAML::Node node;
        node["task_type"] = task_type;
        node["description"] = description;
        if (!parameters.empty()) {
            for (const auto& param : parameters) {
                node["parameters"][param.first] = param.second;
            }
        }
        return node;
    }
    
    static TaskStep fromYaml(const YAML::Node& node) {
        TaskStep step;
        step.task_type = node["task_type"].as<std::string>();
        step.description = node["description"].as<std::string>("");
        
        if (node["parameters"]) {
            for (const auto& param_node : node["parameters"]) {
                std::string key = param_node.first.as<std::string>();
                // 处理不同类型的参数值
                if (param_node.second.IsSequence()) {
                    // 如果是数组，转换为字符串
                    std::stringstream ss;
                    ss << "[";
                    bool first = true;
                    for (const auto& val : param_node.second) {
                        if (!first) ss << ", ";
                        ss << val.as<std::string>();
                        first = false;
                    }
                    ss << "]";
                    step.parameters[key] = ss.str();
                } else {
                    step.parameters[key] = param_node.second.as<std::string>();
                }
            }
        }
        return step;
    }

    // 获取参数值
    std::string getStringParam(const std::string& key, const std::string& default_value = "") const {
        auto it = parameters.find(key);
        return (it != parameters.end()) ? it->second : default_value;
    }
    
    int getIntParam(const std::string& key, int default_value = 0) const {
        auto it = parameters.find(key);
        return (it != parameters.end()) ? ParameterParser::parseInt(it->second, default_value) : default_value;
    }
    
    double getDoubleParam(const std::string& key, double default_value = 0.0) const {
        auto it = parameters.find(key);
        return (it != parameters.end()) ? ParameterParser::parseDouble(it->second, default_value) : default_value;
    }
    
    bool getBoolParam(const std::string& key, bool default_value = false) const {
        auto it = parameters.find(key);
        return (it != parameters.end()) ? ParameterParser::parseBool(it->second, default_value) : default_value;
    }
    
    geometry_msgs::msg::Pose getPoseParam(const std::string& key) const {
        auto it = parameters.find(key);
        if (it != parameters.end()) {
            return ParameterParser::parsePose(it->second);
        }
        throw std::runtime_error("Pose parameter '" + key + "' not found");
    }
    
    std::vector<double> getDoubleVectorParam(const std::string& key) const {
        auto it = parameters.find(key);
        if (it != parameters.end()) {
            return ParameterParser::parseDoubleVector(it->second);
        }
        throw std::runtime_error("Double vector parameter '" + key + "' not found");
    }
};


// 任务点位配置
struct TaskPoint {
  int id;
  geometry_msgs::msg::Pose nav_pose;
  std::string facp_model;
  std::vector<TaskStep> task_sequence;  // 任务序列

  YAML::Node toYaml() const {
    YAML::Node node;
    node["id"] = id;
    node["nav_pose"] = poseToArray(nav_pose);
    node["FACP_model"] = facp_model;
    
    // 序列化任务序列
    for (const auto& step : task_sequence) {
        node["task_sequence"].push_back(step.toYaml());
    }
    return node;
  }

  static TaskPoint fromYaml(const YAML::Node& node) {
    TaskPoint point;
    point.id = node["id"].as<int>();
    
    auto nav_pose_values = node["nav_pose"].as<std::vector<double>>();
    point.nav_pose = parsePose(nav_pose_values);

    point.facp_model = node["FACP_model"].as<std::string>();

    // 解析任务序列
    if (node["task_sequence"]) {
        for (const auto& step_node : node["task_sequence"]) {
            point.task_sequence.push_back(TaskStep::fromYaml(step_node));
        }
    }
    return point;
  }
};

// 全局配置
struct GlobalConfig {
  geometry_msgs::msg::Pose charge_pose;
  int default_lift_height;
  geometry_msgs::msg::Pose arm_start_pose;
  double default_button_hover_distance;
  double default_button_press_distance;

  YAML::Node toYaml() const { 
    YAML::Node node;
    node["charge_pose"] = poseToArray(charge_pose);
    node["default_lift_height"] = default_lift_height;
    node["arm_start_pose"] = poseToArray(arm_start_pose);
    node["default_button_hover_distance"] = default_button_hover_distance;
    node["default_button_press_distance"] = default_button_press_distance;
    return node;
  }

  static GlobalConfig fromYaml(const YAML::Node& node) {
    GlobalConfig config;
    auto charge_pose_values = node["charge_pose"].as<std::vector<double>>();
    config.charge_pose = parsePose(charge_pose_values);
    config.default_lift_height = node["default_lift_height"].as<int>();
    auto arm_start_pose_values = node["arm_start_pose"].as<std::vector<double>>();
    config.arm_start_pose = parsePose(arm_start_pose_values);
    config.default_button_hover_distance = node["default_button_hover_distance"].as<double>(0.14508);
    config.default_button_press_distance = node["default_button_press_distance"].as<double>(0.015);
    
    return config;
  }

};

// 动作模板库
// using ActionTemplate = std::vector<Operation>;
// using ActionTemplateMap = std::unordered_map<std::string, ActionTemplate>;

} // namespace task_manager