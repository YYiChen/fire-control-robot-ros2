#!/usr/bin/env bash
# One-time setup: make every terminal (incl. VS Code) auto-load ROS2,
# add handy aliases, and generate ~/ros2_ws/.vscode configs.
# Run INSIDE WSL Ubuntu 22.04, ONCE.
#
# Usage:
#   bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/setup_ros2_dev_env.sh
#
set -e

WS="$HOME/ros2_ws"

# ---------- 1) .bashrc auto-source (idempotent) ----------
if ! grep -q ">>> ros2 auto-source >>>" "$HOME/.bashrc" 2>/dev/null; then
  cat >> "$HOME/.bashrc" << 'EOF'

# >>> ros2 auto-source >>>
source /opt/ros/humble/setup.bash
if [ -f ~/ros2_ws/install/setup.bash ]; then
  source ~/ros2_ws/install/setup.bash
fi
# <<< ros2 auto-source <<<
EOF
  echo "[OK] 已写入 ~/.bashrc：新终端自动 source ROS2"
else
  echo "[SKIP] ~/.bashrc 已配置自动 source"
fi

# ---------- 2) handy aliases ----------
if ! grep -q ">>> ros2 aliases >>>" "$HOME/.bashrc" 2>/dev/null; then
  cat >> "$HOME/.bashrc" << 'EOF'

# >>> ros2 aliases >>>
alias rosbuild='cd ~/ros2_ws && colcon build --symlink-install && source install/setup.bash'
alias roscd='cd ~/ros2_ws'
alias rossrc='source /opt/ros/humble/setup.bash && [ -f ~/ros2_ws/install/setup.bash ] && source ~/ros2_ws/install/setup.bash'
alias rosnode='ros2 node list'
alias rostopic='ros2 topic list'
# <<< ros2 aliases <<<
EOF
  echo "[OK] 已添加别名：rosbuild / roscd / rossrc / rosnode / rostopic"
else
  echo "[SKIP] 别名已存在"
fi

# ---------- 3) generate ~/ros2_ws/.vscode ----------
mkdir -p "$WS/.vscode"

cat > "$WS/.vscode/settings.json" << 'EOF'
{
  "python.analysis.extraPaths": [
    "/opt/ros/humble/lib/python3.10/site-packages",
    "/opt/ros/humble/local/lib/python3.10/dist-packages"
  ],
  "python.autoComplete.extraPaths": [
    "/opt/ros/humble/lib/python3.10/site-packages"
  ],
  "files.associations": {
    "*.urdf": "xml",
    "*.xacro": "xml"
  },
  "C_Cpp.default.includePath": [
    "/opt/ros/humble/include/**"
  ],
  "terminal.integrated.defaultProfile.linux": "bash"
}
EOF

cat > "$WS/.vscode/tasks.json" << 'EOF'
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "colcon: build (all)  [Ctrl+Shift+B]",
      "type": "shell",
      "command": "source /opt/ros/humble/setup.bash && colcon build --symlink-install",
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": []
    },
    {
      "label": "colcon: build (one package)",
      "type": "shell",
      "command": "source /opt/ros/humble/setup.bash && colcon build --symlink-install --packages-select ${input:packageName}",
      "problemMatcher": []
    },
    {
      "label": "colcon: clean (rm build install log)",
      "type": "shell",
      "command": "rm -rf build install log",
      "problemMatcher": []
    }
  ],
  "inputs": [
    { "id": "packageName", "type": "promptString", "description": "输入要单独编译的包名" }
  ]
}
EOF

cat > "$WS/.vscode/launch.json" << 'EOF'
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "ROS2: 调试当前 Python 文件",
      "type": "debugpy",
      "request": "launch",
      "program": "${file}",
      "console": "integratedTerminal",
      "env": {
        "PYTHONPATH": "/opt/ros/humble/lib/python3.10/site-packages:${workspaceFolder}/install/my_first_pkg/lib/python3.10/site-packages"
      }
    }
  ]
}
EOF

echo "[OK] 已生成 $WS/.vscode/{settings,tasks,launch}.json"
echo ""
echo "======================================================"
echo "完成！执行一次让配置生效："
echo "    source ~/.bashrc"
echo ""
echo "之后："
echo "  - 任何新终端（含 VS Code 集成终端）自动带 ROS2 环境"
echo "  - 编译只需输入： rosbuild"
echo "  - VS Code 里 Ctrl+Shift+B 一键 colcon build"
echo "======================================================"
