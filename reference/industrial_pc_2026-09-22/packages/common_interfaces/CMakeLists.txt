cmake_minimum_required(VERSION 3.8)
project(common_interfaces)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

set(rosidl_generate_interfaces_DEPENDENCIES
  "common_interfaces__msg__BaseTaskMode"
  "common_interfaces__msg__CurrentTask"
  "common_interfaces__msg__TaskStatus"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  
  # 消防主机识别业务相关
  "msg/ScreenInfo.msg"
  "msg/LEDStatus.msg"
  "msg/PanelRegion.msg"
  
  # 升降台相关
  "srv/LiftCommand.srv"
  "msg/LiftQuery.msg"
  "msg/LiftStatus.msg"

  # 机器人相关
  "srv/RobotCommand.srv"
  "msg/RobotStatus.msg"

  # 图像处理相关
  "srv/SetCameraParams.srv"
  "srv/TriggerImageCapture.srv"
  "srv/GetCameraInfo.srv"
  "srv/GetPanelRegionPose.srv"
  "srv/GetPanelInfo.srv"


  # 机械臂相关
  "msg/ArmTaskType.msg"
  "msg/TaskStatus.msg"
  "srv/ControlGripper.srv"
  
  "action/ExecuteArmTask.action"
  "action/TaskJs.action"

  DEPENDENCIES builtin_interfaces std_msgs geometry_msgs sensor_msgs
)


ament_export_dependencies(rosidl_default_runtime)

ament_package()
