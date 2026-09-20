#!/usr/bin/env bash
# Create a minimal ROS2 Humble workspace + publisher/subscriber sample package.
# Run INSIDE WSL Ubuntu 22.04 (prompt looks like: user@host:~$), NOT in Windows PowerShell.
#
# Usage (from WSL):
#   bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/create_first_ros2_pkg.sh
#
set -e

# ROS2 environment is required for 'ros2 pkg create' and 'colcon build'
source /opt/ros/humble/setup.bash

WS="$HOME/ros2_ws"
PKG="my_first_pkg"

echo "[*] Creating workspace $WS/src ..."
mkdir -p "$WS/src"
cd "$WS/src"

if [ ! -d "$PKG" ]; then
  echo "[*] Creating package $PKG ..."
  ros2 pkg create "$PKG" --build-type ament_python --dependencies rclpy std_msgs
else
  echo "[*] Package $PKG already exists, reusing it (files will be overwritten)."
fi

# ---------- publisher node ----------
cat > "$WS/src/$PKG/$PKG/my_publisher.py" << 'PYEOF'
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class MyPublisher(Node):
    def __init__(self):
        super().__init__('my_publisher')
        self.publisher_ = self.create_publisher(String, 'my_topic', 10)
        self.timer = self.create_timer(0.5, self.timer_callback)
        self.i = 0

    def timer_callback(self):
        msg = String()
        msg.data = 'Hello ROS2: %d' % self.i
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing: "%s"' % msg.data)
        self.i += 1


def main(args=None):
    rclpy.init(args=args)
    node = MyPublisher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
PYEOF

# ---------- subscriber node ----------
cat > "$WS/src/$PKG/$PKG/my_subscriber.py" << 'PYEOF'
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class MySubscriber(Node):
    def __init__(self):
        super().__init__('my_subscriber')
        self.subscription = self.create_subscription(
            String, 'my_topic', self.listener_callback, 10)

    def listener_callback(self, msg):
        self.get_logger().info('I heard: "%s"' % msg.data)


def main(args=None):
    rclpy.init(args=args)
    node = MySubscriber()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
PYEOF

# ---------- setup.py with entry points ----------
cat > "$WS/src/$PKG/setup.py" << 'PYEOF'
from setuptools import setup

package_name = 'my_first_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='yyyyyc001',
    maintainer_email='yyyyyc001@todo.todo',
    description='My first ROS2 package',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'my_publisher = my_first_pkg.my_publisher:main',
            'my_subscriber = my_first_pkg.my_subscriber:main',
        ],
    },
)
PYEOF

echo "[*] Building with colcon ..."
cd "$WS"
colcon build --packages-select "$PKG"

echo ""
echo "[OK] Build finished. Now open TWO terminals and run:"
echo ""
echo "  Terminal 1 (publisher):"
echo "    source /opt/ros/humble/setup.bash"
echo "    source ~/ros2_ws/install/setup.bash"
echo "    ros2 run my_first_pkg my_publisher"
echo ""
echo "  Terminal 2 (subscriber):"
echo "    source /opt/ros/humble/setup.bash"
echo "    source ~/ros2_ws/install/setup.bash"
echo "    ros2 run my_first_pkg my_subscriber"
echo ""
echo "  Optional - watch the raw topic in a 3rd terminal:"
echo "    ros2 topic echo /my_topic"
