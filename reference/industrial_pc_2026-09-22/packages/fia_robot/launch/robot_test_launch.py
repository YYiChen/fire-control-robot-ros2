import os
from pathlib import Path
import launch
from launch.actions import SetEnvironmentVariable
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, GroupAction,
                            IncludeLaunchDescription, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import PushRosNamespace
import launch_ros.actions
from launch.conditions import IfCondition
from launch.conditions import UnlessCondition
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
from launch.actions import LogInfo

def generate_launch_description():
    # Get the launch directory
    bringup_dir = get_package_share_directory('fia_robot')
    launch_dir = os.path.join(bringup_dir, 'launch')
    
    imu_config = Path(get_package_share_directory('fia_robot'), 'config', 'imu.yaml')

    
    twist_mux_node = launch_ros.actions.Node(
            package="twist_mux",
            executable="twist_mux",
            name="twist_mux_node",
            # parameters=[
            #     os.path.join(launch_dir, 'config', 'twist_mux_locks.yaml'),
            #     os.path.join(launch_dir, 'config', 'twist_mux_topics.yaml'),
            #     {'use_sim_time': False}
            # ],

            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('fia_robot'),
                    'config',
                    'twist_mux_locks.yaml'
                ]),
                PathJoinSubstitution([
                    FindPackageShare('fia_robot'),
                    'config',
                    'twist_mux_topics.yaml'
                ])
            ]
    )

    # twist_mux = IncludeLaunchDescription(
    #         PythonLaunchDescriptionSource(os.path.join(launch_dir, 'twist_mux_launch.py')),
    # )

    ld = LaunchDescription()

    ld.add_action(twist_mux_node)

    return ld

