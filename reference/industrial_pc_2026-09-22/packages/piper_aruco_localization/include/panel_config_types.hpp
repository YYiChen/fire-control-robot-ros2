// panel_config_types.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include "common_interfaces/msg/panel_region.hpp"

using PanelRegionMsg = common_interfaces::msg::PanelRegion;

namespace panel_config
{

// 区域配置结构
struct Region
{
    std::string name;
    std::vector<double> rel_position; // [x, y, z]
    std::string type;                 // 对应 PanelRegion.msg 中的类型字符串
    int type_id;                      // 对应 PanelRegion.msg 中的枚举值

    YAML::Node toYaml() const
    {
        YAML::Node node;
        node["rel_position"] = rel_position;
        node["type"] = type;
        return node;
    }

    static Region fromYaml(const std::string &name, const YAML::Node &node)
    {
        Region region;
        region.name = name;
        region.rel_position = node["rel_position"].as<std::vector<double>>();
        region.type = node["type"].as<std::string>();
        // 将字符串类型映射到枚举值
        region.type_id = mapTypeToEnum(region.type);
        return region;
    }

private:
    static int mapTypeToEnum(const std::string &type_str)
    {
        static const std::unordered_map<std::string, int> type_map = {
            {"ORIGIN", common_interfaces::msg::PanelRegion::ORIGIN},
            {"BUTTON_MUTE", common_interfaces::msg::PanelRegion::BUTTON_MUTE},
            {"BUTTON_RESET", common_interfaces::msg::PanelRegion::BUTTON_RESET},
            {"BUTTON_1", common_interfaces::msg::PanelRegion::BUTTON_1},
            {"BUTTON_CONFIRM", common_interfaces::msg::PanelRegion::BUTTON_CONFIRM},
            {"BUTTON_SELF_CHECK", common_interfaces::msg::PanelRegion::BUTTON_SELF_CHECK},
            {"BUTTON_MENU", common_interfaces::msg::PanelRegion::BUTTON_MENU},
            {"SCREEN", common_interfaces::msg::PanelRegion::SCREEN},
            {"LED", common_interfaces::msg::PanelRegion::LED},
            {"KEY_ENABLE", common_interfaces::msg::PanelRegion::KEY_ENABLE}
        };

        auto it = type_map.find(type_str);
        if (it != type_map.end())
        {
            return it->second;
        }
        return PanelRegionMsg::UNKNOWN;
    }
};

// 区域组配置结构
struct RegionGroup
{
    std::string name;
    std::unordered_map<std::string, Region> regions;

    YAML::Node toYaml() const
    {
        YAML::Node node;
        for (const auto &pair : regions)
        {
            node[pair.first] = pair.second.toYaml();
        }
        return node;
    }

    static RegionGroup fromYaml(const std::string &name, const YAML::Node &node)
    {
        RegionGroup group;
        group.name = name;
        for (const auto &region_node : node)
        {
            std::string region_name = region_node.first.as<std::string>();
            group.regions[region_name] = Region::fromYaml(region_name, region_node.second);
        }
        return group;
    }
};

// 面板配置结构
struct PanelConfig
{
    std::string panel_model;
    int main_aruco_id;
    int aux_aruco_id;
    std::vector<double> main_aux_offset; // aux相对于main的偏移，单位米
    std::vector<double> origin; // 坐标系原点
    std::string orientation;    // 坐标系方向
    std::unordered_map<std::string, RegionGroup> region_groups;

    YAML::Node toYaml() const
    {
        YAML::Node node;
        node["panel_model"] = panel_model;
        node["main_aruco_id"] = main_aruco_id;
        node["aux_aruco_id"] = aux_aruco_id;
        node["main_aux_offset"] = main_aux_offset;
        node["coordinate_system"]["origin"] = origin;
        node["coordinate_system"]["orientation"] = orientation;

        YAML::Node regions_node;
        for (const auto &pair : region_groups)
        {
            regions_node[pair.first] = pair.second.toYaml();
        }
        node["regions"] = regions_node;
        return node;
    }

    static PanelConfig fromYaml(const YAML::Node &node)
    {
        PanelConfig config;
        config.panel_model = node["panel_model"].as<std::string>();
        config.main_aruco_id = node["main_aruco_id"].as<int>();
        config.aux_aruco_id = node["aux_aruco_id"].as<int>();
        config.main_aux_offset = node["main_aux_offset"].as<std::vector<double>>();
        config.origin = node["coordinate_system"]["origin"].as<std::vector<double>>();
        config.orientation = node["coordinate_system"]["orientation"].as<std::string>();

        const auto &regions_node = node["regions"];
        for (const auto &group_node : regions_node)
        {
            std::string group_name = group_node.first.as<std::string>();
            config.region_groups[group_name] = RegionGroup::fromYaml(group_name, group_node.second);
        }
        return config;
    }
};

} // namespace panel_config