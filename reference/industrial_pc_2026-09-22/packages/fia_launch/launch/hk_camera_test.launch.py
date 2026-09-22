from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os
def generate_launch_description():
    video_manager_dir = get_package_share_directory('video_manager')
    ost_path = os.path.join(video_manager_dir, 'ost', 'hk_ost.txt')
    
    image_server_node = Node(
            package='video_manager',
            executable='image_server',
            name='image_server',
            output='screen',
            parameters=[
                {'ost_path': ost_path},
                {'is_rectified': False}
            ]
        )
    image_pub_node = Node(
            package='video_manager',
            executable='image_pub',
            name='image_pub',
            output='screen',
            parameters=[
                {'image_pub_topic_name': '/hk_cam/image_raw'},
                {'ost_path': ost_path},
                {'is_rectified': False}
            ]
        )

    return LaunchDescription([
        # image_pub_node,
        image_server_node,
    ])