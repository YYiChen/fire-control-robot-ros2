#include "configuration_manager.hpp"
#include <rclcpp/rclcpp.hpp>
#include <stdexcept>

namespace task_manager {

ConfigurationManager::ConfigurationManager()
    : config_loaded_(false)
{
}

void ConfigurationManager::loadTaskConfig(const std::string& file_path)
{
    if (file_path.empty()) {
        RCLCPP_ERROR(rclcpp::get_logger("ConfigurationManager"), "任务配置文件路径为空");
        throw std::runtime_error("任务配置文件路径为空");
    }
    
    try {
        YAML::Node config = YAML::LoadFile(file_path);
        
        // 解析全局配置
        if (config["global"]) {
            global_config_ = GlobalConfig::fromYaml(config["global"]);
        } else {
            RCLCPP_WARN(rclcpp::get_logger("ConfigurationManager"), "配置文件中未找到全局配置");
        }
        
        // 解析任务点位
        task_points_.clear();
        if (config["task_points"]) {
            auto task_points_node = config["task_points"];
            for (const auto& point : task_points_node) {
                task_points_.push_back(TaskPoint::fromYaml(point));
            }
            RCLCPP_INFO(rclcpp::get_logger("ConfigurationManager"), 
                       "成功加载任务配置文件，共 %zu 个任务点位", task_points_.size());
        } else {
            RCLCPP_WARN(rclcpp::get_logger("ConfigurationManager"), "配置文件中未找到任务点位配置");
        }
        
        config_loaded_ = true;
        
    } catch (const YAML::Exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("ConfigurationManager"), "解析任务配置文件失败: %s", e.what());
        throw std::runtime_error("解析任务配置文件失败: " + std::string(e.what()));
    } catch (const std::exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("ConfigurationManager"), "加载任务配置文件时发生错误: %s", e.what());
        throw std::runtime_error("加载任务配置文件时发生错误: " + std::string(e.what()));
    }
}

const GlobalConfig& ConfigurationManager::getGlobalConfig() const
{
    return global_config_;
}

const std::vector<TaskPoint>& ConfigurationManager::getTaskPoints() const
{
    return task_points_;
}

const TaskPoint& ConfigurationManager::getTaskPoint(size_t index) const
{
    if (index >= task_points_.size()) {
        throw std::out_of_range("任务点位索引超出范围");
    }
    return task_points_[index];
}

size_t ConfigurationManager::getTaskPointCount() const
{
    return task_points_.size();
}

bool ConfigurationManager::isConfigLoaded() const
{
    return config_loaded_;
}

} // namespace task_manager