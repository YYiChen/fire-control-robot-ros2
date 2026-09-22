"""Process a simulated stereo pair after Gazebo publishes its image topics.

This launch file deliberately has no Gazebo, navigation, velocity, or hardware
driver action.  It only consumes image and CameraInfo messages and produces a
disparity image plus a PointCloud2 topic.
"""

from pathlib import Path

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    config_file = Path(__file__).resolve().parents[1] / 'config' / 'stereo_processing.yaml'
    use_sim_time = LaunchConfiguration('use_sim_time')

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time', default_value='true',
            description='Use Gazebo /clock for all image-processing nodes.'),
        Node(
            package='image_proc',
            executable='image_proc',
            namespace='stereo/left',
            name='rectify',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
            remappings=[
                ('image', '/stereo/left/image_raw'),
                ('camera_info', '/stereo/left/camera_info'),
                ('image_rect', '/stereo/left/image_rect'),
                ('image_rect_color', '/stereo/left/image_rect_color'),
            ]),
        Node(
            package='image_proc',
            executable='image_proc',
            namespace='stereo/right',
            name='rectify',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
            remappings=[
                ('image', '/stereo/right/image_raw'),
                ('camera_info', '/stereo/right/camera_info'),
                ('image_rect', '/stereo/right/image_rect'),
                ('image_rect_color', '/stereo/right/image_rect_color'),
            ]),
        Node(
            package='stereo_image_proc',
            executable='disparity_node',
            namespace='stereo',
            name='disparity',
            output='screen',
            parameters=[str(config_file), {'use_sim_time': use_sim_time}],
            remappings=[
                ('left/camera_info', '/stereo/left/camera_info'),
                ('left/image_rect', '/stereo/left/image_rect'),
                ('right/camera_info', '/stereo/right/camera_info'),
                ('right/image_rect', '/stereo/right/image_rect'),
                ('disparity', '/stereo/disparity'),
            ]),
        Node(
            package='stereo_image_proc',
            executable='point_cloud_node',
            namespace='stereo',
            name='point_cloud',
            output='screen',
            parameters=[str(config_file), {'use_sim_time': use_sim_time}],
            remappings=[
                ('left/camera_info', '/stereo/left/camera_info'),
                ('left/image_rect_color', '/stereo/left/image_rect_color'),
                ('right/camera_info', '/stereo/right/camera_info'),
                ('disparity', '/stereo/disparity'),
                ('points2', '/stereo/points2'),
            ]),
    ])
