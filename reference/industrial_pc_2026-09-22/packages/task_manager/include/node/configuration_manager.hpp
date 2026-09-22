#ifndef CONFIGURATION_MANAGER_HPP_
#define CONFIGURATION_MANAGER_HPP_

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include "task_config_types.hpp"

namespace task_manager {

class ConfigurationManager {
public:
    ConfigurationManager();
    ~ConfigurationManager() = default;
    
    // 加载任务配置文件
    void loadTaskConfig(const std::string& file_path);
    
    // 获取全局配置
    const GlobalConfig& getGlobalConfig() const;
    
    // 获取任务点位列表
    const std::vector<TaskPoint>& getTaskPoints() const;
    
    // 根据索引获取特定任务点位
    const TaskPoint& getTaskPoint(size_t index) const;
    
    // 获取任务点位数量
    size_t getTaskPointCount() const;
    
    // 检查配置是否已加载
    bool isConfigLoaded() const;

private:
    GlobalConfig global_config_;
    std::vector<TaskPoint> task_points_;
    bool config_loaded_;
};

} // namespace task_manager

#endif // CONFIGURATION_MANAGER_HPP_