#include "fia_utils/task_utils.hpp"
#include <unordered_map>

#include "common_interfaces/msg/arm_task_type.hpp"
namespace fia_robot{

std::string taskTypeToString(const common_interfaces::msg::ArmTaskType& task_type) {
    // switch (task_type.value) {
    //     case common_interfaces::msg::ArmTaskType::SILENCING:
    //         return "SILENCING";
    //     case common_interfaces::msg::ArmTaskType::RESETTING:
    //         return "RESETTING";
    //     default:
    //         return "UNKNOWN_TASK_TYPE";
    // }
    return "UNKNOWN_TASK_TYPE";
}
} // namespace common_interfaces