#!/usr/bin/env bash
# Read-only dependency probe for the ROS 2 Humble Gazebo Classic stereo test.

if [[ -f /opt/ros/humble/setup.bash ]]; then
  # shellcheck disable=SC1091
  source /opt/ros/humble/setup.bash
else
  echo 'ERROR: /opt/ros/humble/setup.bash was not found.'
  exit 2
fi

# ROS 2's setup scripts read optional AMENT_* state.  Enable strict handling
# only after they have populated the environment.
set -u

missing=0

check_package() {
  local package_name="$1"
  if ros2 pkg prefix "$package_name" >/dev/null 2>&1; then
    echo "PASS package: $package_name"
  else
    echo "MISS package: $package_name"
    missing=1
  fi
}

check_executable() {
  local package_name="$1"
  local executable_name="$2"
  if ros2 pkg executables "$package_name" 2>/dev/null | awk '{print $2}' | grep -Fxq "$executable_name"; then
    echo "PASS executable: $package_name/$executable_name"
  else
    echo "MISS executable: $package_name/$executable_name"
    missing=1
  fi
}

check_required_library() {
  local library_name="$1"
  local library_path="/opt/ros/${ROS_DISTRO}/lib/${library_name}"
  if [[ -f "$library_path" ]]; then
    echo "PASS Gazebo plugin: $library_name"
  else
    echo "MISS Gazebo plugin: $library_name"
    missing=1
  fi
}

echo "ROS_DISTRO=${ROS_DISTRO:-unset}"
echo "Gazebo=$(gazebo --version 2>/dev/null | head -n 1 || true)"

check_package gazebo_ros
check_package gazebo_plugins
check_package image_proc
check_package stereo_image_proc
check_package image_view

check_executable image_proc image_proc
check_executable stereo_image_proc disparity_node
check_executable stereo_image_proc point_cloud_node

check_required_library libgazebo_ros_camera.so

# In ROS 2 Humble, multicamera support is provided by libgazebo_ros_camera.so.
# A separate libgazebo_ros_multicamera.so is a ROS 1 convention and is not a
# dependency of this test.
echo 'INFO multicamera implementation: libgazebo_ros_camera.so (ROS 2 Humble)'

if [[ "$missing" -eq 0 ]]; then
  echo 'RESULT: READY (processing dependencies are present)'
  exit 0
fi

echo 'RESULT: NOT READY (install only the missing ROS packages, then run this check again)'
exit 2
