cmake_minimum_required(VERSION 3.8)
project(piper_aruco_localization)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 11) # C++11...
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
set(CMAKE_CXX_EXTENSIONS ON) #...with compiler extensions like gnu++11

set(THIS_PACKAGE_INCLUDE_DEPENDS
    OpenCV
    yaml-cpp
    cv_bridge
    geometry_msgs
    image_transport
    rclcpp
    rclpy
    tf2
    tf2_ros
    tf2_geometry_msgs
    aruco
    sensor_msgs
    visualization_msgs
    common_interfaces
)

find_package(ament_cmake REQUIRED)
foreach(Dependency IN ITEMS ${THIS_PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()


add_executable(single_shot_detector src/single_shot_detector.cpp
                      src/aruco_ros_utils.cpp)

target_include_directories(single_shot_detector
  PUBLIC
  include)

target_include_directories(single_shot_detector
  SYSTEM PUBLIC
  ${OpenCV_INCLUDE_DIRS}
)
ament_target_dependencies(single_shot_detector ${THIS_PACKAGE_INCLUDE_DEPENDS})
target_link_libraries(single_shot_detector ${OpenCV_LIBRARIES} ${YAML_CPP_LIBRARIES})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  list(APPEND AMENT_LINT_AUTO_EXCLUDE
    ament_cmake_copyright
  )
  ament_lint_auto_find_test_dependencies()
endif()

#############
## Install ##
#############

install(TARGETS single_shot_detector 
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY configs 
  DESTINATION share/${PROJECT_NAME})

ament_package()
