#!/usr/bin/env bash
# Install recommended VS Code extensions for ROS2 development.
# Run INSIDE WSL Ubuntu 22.04 (installs to the WSL side automatically).
#
# Usage:
#   bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/install_vscode_extensions.sh
#
set -e

echo "[*] Installing VS Code extensions (WSL side) ..."

# Recommended: extension pack = ROS2 ext + URDF editor + more, in one go.
# (Successor of the deprecated ms-iot.vscode-ros.)
code --install-extension Ranch-Hand-Robotics.rde-pack

# Language support
code --install-extension ms-vscode.cpptools
code --install-extension ms-python.python
code --install-extension ms-python.vscode-pylance
code --install-extension ms-vscode.cmake-tools

echo "[OK] Core extensions installed."
echo ""
echo "[i] Optional: visual URDF/Xacro editor (view/edit robot 3D models)."
echo "    Install via the Extensions panel: search 'Robot Developer Extension for URDF'"
echo "    (published by Ranch Hand Robotics). It does NOT require ROS to render."
echo ""
echo "[i] You can uninstall the old deprecated one: ms-iot.vscode-ros"
