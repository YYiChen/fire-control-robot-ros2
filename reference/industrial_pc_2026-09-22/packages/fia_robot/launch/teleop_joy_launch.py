import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    bringup_dir = get_package_share_directory('fia_robot')
    # launch_dir = os.path.join(bringup_dir, 'launch')
    joy_config = os.path.join(bringup_dir, 'config', 'joy_dev.yaml')
    teleop_config = os.path.join(bringup_dir, 'config', 'teleop_ps5.yaml')

    
    joy_node = Node(
        package="joy_linux",
        executable="joy_linux_node",
        name="joy_linux_node",
        parameters=[joy_config]  # 或加载 YAML
    )

    teleop_node = Node(
        package="teleop_twist_joy",
        executable="teleop_node",
        name="teleop_node",
        parameters=[teleop_config],
        remappings=[
            ("/cmd_vel", "/teleop/cmd_vel")
        ]
    )

    ld = LaunchDescription()
    ld.add_action(joy_node)
    ld.add_action(teleop_node)
    return ld
