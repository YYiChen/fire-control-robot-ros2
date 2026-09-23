"""Add OctoMap occupancy accumulation to the isolated semantic stereo scene."""

from pathlib import Path

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    return LaunchDescription([
        DeclareLaunchArgument('world', default_value=str(root / 'worlds' / 'semantic_panel.world')),
        DeclareLaunchArgument('gui', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'semantic_panel.launch.py')),
            launch_arguments={'world': LaunchConfiguration('world'),
                              'gui': LaunchConfiguration('gui')}.items()),
        Node(
            package='octomap_server',
            executable='octomap_server_node',
            name='stereo_octomap',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'frame_id': 'map',
                'resolution': 0.025,
                'sensor_model.max_range': 1.2,
                'pointcloud_min_z': 0.15,
                'pointcloud_max_z': 0.95,
                'occupancy_min_z': 0.15,
                'occupancy_max_z': 0.95,
                'filter_ground': False,
            }],
            remappings=[('cloud_in', '/stereo/points2')]),
    ])
