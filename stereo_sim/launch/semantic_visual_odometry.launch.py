"""Stereo VO drives semantic panel localization, 3D occupancy and path advice."""

from pathlib import Path
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_odometry_audit.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui')}.items()),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'semantic_panel_node.py'),
                 '--ros-args', '-p', 'use_sim_time:=true', '-p', 'pose_source:=odometry'],
            output='screen'),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'vo_synchronized_cloud.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
        Node(
            package='octomap_server', executable='octomap_server_node',
            name='stereo_octomap', output='screen',
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
            remappings=[('cloud_in', '/vo/points2')]),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'semantic_approach_planner.py'),
                 '--ros-args', '-p', 'use_sim_time:=true', '-p', 'pose_source:=odometry'],
            output='screen'),
    ])
