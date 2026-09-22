#pragma once

#include "common_interfaces/msg/arm_task_type.hpp"
#include <string>

namespace fia_robot {

std::string taskTypeToString(const common_interfaces::msg::ArmTaskType& task_type);

} // namespace fia_robot