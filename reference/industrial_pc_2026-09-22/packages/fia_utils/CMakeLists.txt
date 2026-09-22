cmake_minimum_required(VERSION 3.8)
project(fia_utils)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
# 添加 RPATH 支持
# set(CMAKE_INSTALL_RPATH "\$ORIGIN")
# set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(common_interfaces REQUIRED)

# 添加一个工具库
add_library(fia_utils SHARED
  src/task_utils.cpp
)

target_include_directories(fia_utils PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(fia_utils
    rclcpp
    common_interfaces
    )

# 安装目标（供其他包使用）
install(TARGETS fia_utils
  EXPORT fia_utils
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)
ament_export_libraries(fia_utils)
ament_export_dependencies(common_interfaces)
ament_package()
