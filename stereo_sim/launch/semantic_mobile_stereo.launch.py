"""Spawn a physical TurtleBot3 Burger carrying the stereo semantic sensor."""

from pathlib import Path
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / 'scripts'))
    from generate_stereo_mobile_model import generate_model

    model_path = generate_model(root, Path.home() / 'stereo_sim_generated' / 'stereo_mobile_bot.sdf')
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'semantic_visual_odometry.launch.py')),
            launch_arguments={
                'gui': LaunchConfiguration('gui'),
                'world': str(root / 'worlds' / 'semantic_panel_mobile.world'),
            }.items()),
        Node(
            package='gazebo_ros', executable='spawn_entity.py',
            name='spawn_stereo_mobile_bot', output='screen',
            arguments=['-entity', 'stereo_mobile_bot', '-file', str(model_path),
                       '-x', '0', '-y', '0', '-z', '0.01', '-timeout', '120']),
    ])
