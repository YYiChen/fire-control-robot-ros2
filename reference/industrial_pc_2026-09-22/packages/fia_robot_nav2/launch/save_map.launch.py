#ros2 run nav2_map_server map_saver_cli -f ~/map
import os

from launch import LaunchDescription
from launch.actions import ExecuteProcess
import launch_ros.actions

from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import Node
from pathlib import Path


def generate_launch_description():
    pkg_share_path = Path(get_package_share_directory('fia_robot_nav'))
    root_path = pkg_share_path.parents[3]
    data_map_path = os.path.join(root_path, 'data', 'fcr_map')

    map_saver = launch_ros.actions.Node(
        package='nav2_map_server',
        executable='map_saver_cli',
        output='screen',
        arguments=['-f', data_map_path],
        
        parameters=[{'save_map_timeout': 20000.0},
                    {'free_thresh_default': 0.196}]

        )
    ld = LaunchDescription()

    ld.add_action(map_saver)
    return ld
 
