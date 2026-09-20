#!/usr/bin/env bash
# One-click: build the ROS2 workspace, then run a given command.
# Run INSIDE WSL Ubuntu 22.04 (or called by Run-ROS2.bat).
#
# Usage:
#   bash run_ros2.sh                                       # build only, then print hint
#   bash run_ros2.sh "ros2 run demo_nodes_cpp talker"      # build, then run this
#   bash run_ros2.sh "ros2 launch my_pkg my_launch.py"     # build, then launch this
#
set -e

# ROS2 base environment
source /opt/ros/humble/setup.bash

WS="$HOME/ros2_ws"
if [ -d "$WS/src" ]; then
  echo "[*] colcon build @ $WS ..."
  cd "$WS"
  colcon build --symlink-install
  # shellcheck disable=SC1091
  source "$WS/install/setup.bash"
else
  echo "[!] $WS/src 不存在，跳过编译（先用 create_first_ros2_pkg.sh 建工作区）"
fi

if [ -n "$1" ]; then
  echo "[*] RUN: $1"
  eval "$1"
else
  echo "[OK] ROS2 环境已就绪。例如：ros2 run demo_nodes_cpp talker"
fi
