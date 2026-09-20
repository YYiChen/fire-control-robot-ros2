#!/usr/bin/env bash
# ROS2 Humble dependency health check.
# Run INSIDE WSL Ubuntu 22.04 (prompt like: yyyyc001@DESKTOP-...:~$), NOT in PowerShell.
#
# Usage:
#   bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/check_ros2_env.sh
#
source /opt/ros/humble/setup.bash 2>/dev/null

PASS=0; FAIL=0
ok(){ echo "  [PASS] $1"; PASS=$((PASS+1)); }
bad(){ echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }

echo "================ ROS2 Humble 依赖体检 ================"
echo ""

echo "[1] 基础"
[ -f /opt/ros/humble/setup.bash ] && ok "setup.bash 存在" || bad "找不到 /opt/ros/humble/setup.bash"
[ "$ROS_DISTRO" = "humble" ] && ok "ROS_DISTRO = humble" || bad "ROS_DISTRO 不是 humble (当前: '$ROS_DISTRO')"
command -v ros2 >/dev/null 2>&1 && ok "ros2 命令在 PATH" || bad "找不到 ros2 命令"
python3 -c "import rclpy" >/dev/null 2>&1 && ok "python rclpy 可导入" || bad "rclpy 导入失败"

echo ""
echo "[2] ROS2 CLI 子命令"
for c in run topic node pkg launch param service action bag doctor; do
  if ros2 "$c" -h >/dev/null 2>&1; then ok "ros2 $c"; else bad "ros2 $c"; fi
done

echo ""
echo "[3] 构建 / 依赖工具"
for t in colcon rosdep; do
  command -v "$t" >/dev/null 2>&1 && ok "$t" || bad "$t 未安装"
done
dpkg -l python3-colcon-common-extensions >/dev/null 2>&1 && ok "colcon-common-extensions" || bad "colcon-common-extensions 未安装"

echo ""
echo "[4] C++ 编译工具链"
for t in gcc g++ cmake make; do
  command -v "$t" >/dev/null 2>&1 && ok "$t" || bad "$t 未安装"
done

echo ""
echo "[5] 图形工具 (WSLg)"
command -v rviz2 >/dev/null 2>&1 && ok "rviz2" || bad "rviz2 未安装"
command -v rqt   >/dev/null 2>&1 && ok "rqt"   || bad "rqt 未安装"
[ -n "$DISPLAY" ] && ok "DISPLAY = $DISPLAY (WSLg 就绪)" || bad "DISPLAY 未设置"

echo ""
echo "[6] 已安装 ros-humble-* 包数量"
echo "  $(dpkg -l 2>/dev/null | grep -c '^ii  ros-humble-') 个"
echo ""

echo "================ 汇总: PASS=$PASS   FAIL=$FAIL ================"
if [ "$FAIL" -gt 0 ]; then
  echo ">>> 有失败项。常见修复：sudo apt install -y ros-humble-desktop python3-colcon-common-extensions"
fi
echo ""
echo "================ 官方自检: ros2 doctor ================"
ros2 doctor --report 2>&1 || true
