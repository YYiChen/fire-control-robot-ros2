@echo off
REM ============================================================
REM One-click launcher: open the WSL ROS2 workspace in VS Code.
REM Uses the Remote-WSL URI so it connects in WSL mode (not the UNC path).
REM Double-click to start.
REM ============================================================
start "" "vscode://vscode-remote/wsl+Ubuntu-22.04/home/yyyyyc001/ros2_ws"
