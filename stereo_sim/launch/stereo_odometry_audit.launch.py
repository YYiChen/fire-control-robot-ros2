"""Run RTAB-Map stereo odometry beside, not inside, the truth-based pipeline."""

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
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_test_world.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui'),
                              'world': LaunchConfiguration('world')}.items()),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_processing.launch.py')),
            launch_arguments={'use_sim_time': 'true'}.items()),
        Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='stereo_base_to_optical',
            arguments=['--x', '0', '--y', '0', '--z', '0.5',
                       '--qx', '0.5', '--qy', '-0.5', '--qz', '0.5', '--qw', '-0.5',
                       '--frame-id', 'stereo_base_link',
                       '--child-frame-id', 'stereo_left_camera_optical_frame']),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'normalize_stereo_camera_info.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
        Node(
            package='rtabmap_odom',
            executable='stereo_odometry',
            name='stereo_odometry_audit',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'frame_id': 'stereo_base_link',
                'odom_frame_id': 'vo_odom',
                'publish_tf': False,
                'approx_sync': False,
                'qos': 2,
                'qos_camera_info': 2,
                'wait_imu_to_init': False,
                'Reg/Force3DoF': 'true',
            }],
            remappings=[
                ('left/image_rect', '/stereo/left/image_rect'),
                ('right/image_rect', '/stereo/right/image_rect'),
                ('left/camera_info', '/vo/left/camera_info'),
                ('right/camera_info', '/stereo/stereo_rig/right/camera_info'),
                ('odom', '/vo/odom'),
            ]),
    ])
