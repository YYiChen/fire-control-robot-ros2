CMAKE_MINIMUM_REQUIRED(VERSION 3.6)
project(fire_panel_rec CXX C)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_BUILD_TYPE Debug)

# openvino支持
# include(${PROJECT_SOURCE_DIR}/3rdparty/linux-x64/openvino/runtime/cmake/OpenVINOTargets.cmake)
find_package(OpenVINO)

find_package(gflags REQUIRED)

# find ROS2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(common_interfaces REQUIRED)
find_package(video_manager REQUIRED)

# 添加OpenCV支持
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV and ensure it is in your PATH.")
endif()

#----------------fire_panel_rec_demo----------------#
aux_source_directory(src/ppocr PPOCR_SRC)
add_executable(rec_demo 
    ${PPOCR_SRC}
    src/fire_panel/parse_screen.cpp
    src/fire_panel/parse_led_region.cpp
    src/rec_demo/rec_demo.cpp
)
ament_target_dependencies(rec_demo rclcpp common_interfaces)
target_include_directories(rec_demo PRIVATE
    $<BUILD_INTERFACE:
    ${PROJECT_SOURCE_DIR}/include/fire_panel
    ${PROJECT_SOURCE_DIR}/include/ppocr
    ${PROJECT_SOURCE_DIR}/3rdparty/linux-x64/concurrentqueue>

    $<INSTALL_INTERFACE:include>
)
target_link_libraries(rec_demo 
    ${OpenCV_LIBS}
    openvino::runtime
    gflags
)

#----------------fire_panel_rec----------------#
add_executable(fire_panel_rec 
    ${PPOCR_SRC}
    src/fire_panel/parse_screen.cpp
    src/fire_panel/fire_panel_rec.cpp
    src/fire_panel/main.cpp
)
ament_target_dependencies(fire_panel_rec rclcpp common_interfaces video_manager)
target_include_directories(fire_panel_rec PRIVATE
    $<BUILD_INTERFACE:
      ${PROJECT_SOURCE_DIR}/include/fire_panel
      ${PROJECT_SOURCE_DIR}/include/ppocr
      ${PROJECT_SOURCE_DIR}/3rdparty/linux-x64/concurrentqueue
    >

    $<INSTALL_INTERFACE:include>
)
target_link_libraries(fire_panel_rec 
    ${OpenCV_LIBS}
    openvino::runtime
    gflags
)

#-----------------single_shot_rec_node-----------------#
add_executable(single_shot_rec 
    ${PPOCR_SRC}
    src/fire_panel/parse_screen.cpp
    src/fire_panel/parse_led_region.cpp
    src/fire_panel/single_shot_rec_node.cpp
)
ament_target_dependencies(single_shot_rec rclcpp common_interfaces sensor_msgs cv_bridge)
target_include_directories(single_shot_rec PRIVATE
    $<BUILD_INTERFACE:
      ${PROJECT_SOURCE_DIR}/include/fire_panel
      ${PROJECT_SOURCE_DIR}/include/ppocr
    >
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(single_shot_rec 
    ${OpenCV_LIBS}
    openvino::runtime
    gflags
)


install(TARGETS rec_demo
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS fire_panel_rec
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS single_shot_rec
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY models DESTINATION share/${PROJECT_NAME})
install(DIRECTORY res DESTINATION share/${PROJECT_NAME}
  PATTERN "cmd.txt" EXCLUDE) # 排除


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()