cmake_minimum_required(VERSION 3.8)
project(video_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 添加 RPATH 支持
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(common_interfaces REQUIRED)

# 添加FFmpeg支持
include(FindPkgConfig)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavdevice libavutil libswscale)
if(FFMPEG_FOUND)
    message(STATUS "FFmpeg found: ${FFMPEG_INCLUDE_DIRS}")
    message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
endif()


# 添加OpenCV支持
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV and ensure it is in your PATH.")
endif()

#--------------海康相机库-----------------#
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/MVS/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/MVS/lib/x64)
set(HIK_CAMERA_LIBS 
    MvCameraControl
    MediaProcess
)
file(GLOB_RECURSE HIKVISION_SOURCE CONFIGURE_DEPENDS FOLLOW_SYMLINKS src/hikvision/*.cpp)



#--------------视频管理库-----------------#
file(GLOB_RECURSE VIDEO_MANAGER_SOURCE CONFIGURE_DEPENDS FOLLOW_SYMLINKS src/video/*.cpp)

add_library(video_manager SHARED ${VIDEO_MANAGER_SOURCE} ${HIKVISION_SOURCE})
target_include_directories(video_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${PROJECT_SOURCE_DIR}/3rdparty/MVS/include
    ${FFMPEG_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(video_manager PUBLIC
    ${FFMPEG_LIBRARIES} 
    avfilter 
    ${OpenCV_LIBS}
    ${HIK_CAMERA_LIBS}
)

#--------------图像发布节点-----------------#
add_executable(image_pub 
    src/image_pub/image_pub.cpp
)
ament_target_dependencies(image_pub rclcpp sensor_msgs cv_bridge )
target_include_directories(image_pub PRIVATE
    $<BUILD_INTERFACE:
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/3rdparty/MVS/include
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(image_pub 
    ${OpenCV_LIBS}
    video_manager
)

#--------------图像服务节点-----------------#
add_executable(image_server 
    src/image_server/image_server.cpp
)
ament_target_dependencies(image_server rclcpp sensor_msgs cv_bridge common_interfaces)
target_include_directories(image_server PRIVATE
    $<BUILD_INTERFACE:
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/3rdparty/MVS/include
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(image_server 
    ${OpenCV_LIBS}
    video_manager
)

install(TARGETS video_manager
  EXPORT video_manager
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(TARGETS image_pub image_server
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include
)
install(DIRECTORY ost DESTINATION share/${PROJECT_NAME}
  PATTERN "check.py" EXCLUDE) # 排除


# 导出头文件路径
ament_export_include_directories(include)
# 导出依赖项
# ament_export_dependencies(${PROJECT_NAME})
# 导出目标、库、插件
# ament_export_targets(${PROJECT_NAME})
ament_export_libraries(video_manager)
ament_package()
