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
        DeclareLaunchArgument(
            'left_image_topic', default_value='/stereo/stereo_rig/left/image_raw',
            description='Raw left image topic published by the Gazebo stereo rig.'),
        DeclareLaunchArgument(
            'left_camera_info_topic', default_value='/stereo/stereo_rig/left/camera_info',
            description='Left CameraInfo topic published by the Gazebo stereo rig.'),
        DeclareLaunchArgument(
            'right_image_topic', default_value='/stereo/stereo_rig/right/image_raw',
            description='Raw right image topic published by the Gazebo stereo rig.'),
        DeclareLaunchArgument(
            'right_camera_info_topic', default_value='/stereo/stereo_rig/right/camera_info',
            description='Right CameraInfo topic published by the Gazebo stereo rig.'),
        Node(
            package='image_proc',
            executable='image_proc',
            namespace='stereo/left',
            name='left_rectify',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
            remappings=[
                ('image', LaunchConfiguration('left_image_topic')),
                ('camera_info', LaunchConfiguration('left_camera_info_topic')),
                ('image_rect', '/stereo/left/image_rect'),
                ('image_rect_color', '/stereo/left/image_rect_color'),
            ]),
        Node(
            package='image_proc',
            executable='image_proc',
            namespace='stereo/right',
            name='right_rectify',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
            remappings=[
                ('image', LaunchConfiguration('right_image_topic')),
                ('camera_info', LaunchConfiguration('right_camera_info_topic')),
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
                ('left/camera_info', LaunchConfiguration('left_camera_info_topic')),
                ('left/image_rect', '/stereo/left/image_rect'),
                ('right/camera_info', LaunchConfiguration('right_camera_info_topic')),
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
                ('left/camera_info', LaunchConfiguration('left_camera_info_topic')),
                ('left/image_rect_color', '/stereo/left/image_rect'),
                ('right/camera_info', LaunchConfiguration('right_camera_info_topic')),
                ('disparity', '/stereo/disparity'),
                ('points2', '/stereo/points2'),
            ]),
    ])
