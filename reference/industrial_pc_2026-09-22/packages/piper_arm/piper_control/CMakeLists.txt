cmake_minimum_required(VERSION 3.8)
project(piper_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(common_interfaces REQUIRED)
find_package(moveit_ros_planning_interface)
find_package(moveit_msgs REQUIRED)

add_executable(execute_arm_task 
  src/main.cpp 
  src/arm_action_server.cpp
  
  )

foreach(target IN ITEMS execute_arm_task)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
  target_compile_features(${target} PUBLIC c_std_99 cxx_std_17)
  ament_target_dependencies(${target}
    moveit_ros_planning_interface
    rclcpp
    rclcpp_action
    geometry_msgs
    common_interfaces
    moveit_msgs
    )
endforeach()

install(TARGETS
  execute_arm_task
  DESTINATION lib/${PROJECT_NAME})


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
