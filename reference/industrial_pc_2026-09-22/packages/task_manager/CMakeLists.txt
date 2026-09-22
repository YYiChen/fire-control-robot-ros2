cmake_minimum_required(VERSION 3.8)
project(task_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

set(PACKAGE_INCLUDE_DEPENDS
    rclcpp
    rclcpp_action
    geometry_msgs
    common_interfaces
    fia_utils
    tf2
    tf2_geometry_msgs
    yaml-cpp
)

foreach(Dependency IN ITEMS ${PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()

file(GLOB_RECURSE TASK_MANAGER_SOURCE CONFIGURE_DEPENDS FOLLOW_SYMLINKS src/node/*.cpp)
add_executable(task_manager 
  # src/node/main.cpp 
  # src/node/task_manager.cpp
  ${TASK_MANAGER_SOURCE}
  )

foreach(target IN ITEMS task_manager)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:
    ${CMAKE_CURRENT_SOURCE_DIR}/include/node
    ${fia_utils_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:include>)
  target_compile_features(${target} PUBLIC c_std_99 cxx_std_17)
  ament_target_dependencies(${target} ${PACKAGE_INCLUDE_DEPENDS})
endforeach()
target_link_libraries(task_manager ${YAML_CPP_LIBRARIES})


add_executable(task_client_test src/test/task_client_test.cpp)
ament_target_dependencies(task_client_test rclcpp rclcpp_action common_interfaces)
install(TARGETS task_client_test DESTINATION lib/${PROJECT_NAME})

install(TARGETS
  task_manager
  DESTINATION lib/${PROJECT_NAME})

install(
    DIRECTORY include/
    DESTINATION include
  )
ament_package()
