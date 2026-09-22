cmake_minimum_required(VERSION 3.8)
project(lift_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(common_interfaces REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libserial REQUIRED IMPORTED_TARGET libserial)
pkg_check_modules(UDEV REQUIRED libudev)

add_executable(lift_controller_node src/lift_controller_node.cpp)
target_include_directories(lift_controller_node PRIVATE
  include
  ${libserial_INCLUDE_DIRS}
)
target_link_libraries(lift_controller_node
  ${rclcpp_LIBRARIES}
  ${libserial_LIBRARIES}
  ${UDEV_LIBRARIES}
)
ament_target_dependencies(lift_controller_node rclcpp common_interfaces)

# 安装规则
install(TARGETS lift_controller_node
  DESTINATION lib/${PROJECT_NAME}
)

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
