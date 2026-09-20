#!/usr/bin/env bash
# Install ROS2 Humble (desktop) on Ubuntu 22.04 -- direct connection / no proxy.
# Run this INSIDE the WSL2 Ubuntu 22.04 terminal (NOT in Windows).
# Prerequisite: Ubuntu 22.04 booted, you have set your user password.
set -e

echo "[*] Updating apt..."
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository universe -y
sudo apt update

echo "[*] Installing curl, gnupg, lsb-release..."
sudo apt install -y curl gnupg lsb-release

echo "[*] Adding ROS2 apt key..."
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "[*] Adding ROS2 apt source (jammy)..."
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

sudo apt update
echo "[*] Installing ros-humble-desktop (this is large, keep direct connection)..."
sudo apt install -y ros-humble-desktop

echo "[*] Sourcing ROS2 in ~/.bashrc..."
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc

echo "[OK] ROS2 Humble installed."
echo "Verify with:  source ~/.bashrc && ros2 run demo_nodes_cpp talker"
echo "Then in another terminal:  source ~/.bashrc && ros2 run demo_nodes_cpp listener"
