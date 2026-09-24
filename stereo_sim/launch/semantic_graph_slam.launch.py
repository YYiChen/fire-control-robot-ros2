"""Isolated RTAB-Map graph-SLAM audit over the existing stereo/VO inputs."""

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
        DeclareLaunchArgument('world', default_value=str(root / 'worlds' / 'semantic_panel.world')),
        DeclareLaunchArgument('database_path', default_value=str(
            Path.home() / 'stereo_sim_generated' / 'graph_slam_audit.db')),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_odometry_audit.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui'),
                              'world': LaunchConfiguration('world')}.items()),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'vo_odom_tf.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
        Node(
            package='rtabmap_slam', executable='rtabmap', name='graph_slam',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'frame_id': 'stereo_base_link',
                'map_frame_id': 'graph_map',
                'publish_tf': True,
                'subscribe_stereo': True,
                'subscribe_depth': False,
                'subscribe_rgbd': False,
                'subscribe_scan': False,
                'approx_sync': False,
                'qos_image': 2,
                'qos_camera_info': 2,
                'qos_odom': 2,
                'database_path': LaunchConfiguration('database_path'),
                'Reg/Force3DoF': 'true',
                'RGBD/OptimizeFromGraphEnd': 'false',
                'Mem/STMSize': '5',
                'Rtabmap/DetectionRate': '2',
                'RGBD/LinearUpdate': '0.015',
                'Grid/3D': 'true',
                'Grid/RayTracing': 'true',
                'Grid/CellSize': '0.025',
                'Grid/RangeMax': '1.2',
            }],
            remappings=[
                ('left/image_rect', '/stereo/left/image_rect'),
                ('right/image_rect', '/stereo/right/image_rect'),
                ('left/camera_info', '/vo/left/camera_info'),
                ('right/camera_info', '/stereo/stereo_rig/right/camera_info'),
                ('odom', '/graph/vo_odom'),
            ]),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'semantic_panel_node.py'),
                 '--ros-args', '-p', 'use_sim_time:=true', '-p', 'pose_source:=odometry',
                 '-p', 'pose_frame_id:=vo_odom', '-p', 'publish_pose_tf:=false'],
            output='screen'),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'graph_semantic_projector.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
    ])
