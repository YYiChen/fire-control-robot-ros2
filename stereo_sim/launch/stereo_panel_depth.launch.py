"""Run OCR/LED localization beside the existing simulated stereo cloud."""

from pathlib import Path
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='false'),
        DeclareLaunchArgument(
            'world', default_value=str(root / 'worlds' / 'perception_panel.world')),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                str(root / 'launch' / 'stereo_test_world.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui'),
                              'world': LaunchConfiguration('world')}.items()),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                str(root / 'launch' / 'stereo_processing.launch.py')),
            launch_arguments={'use_sim_time': 'true'}.items()),
        ExecuteProcess(
            cmd=[sys.executable,
                 str(root / 'scripts' / 'stereo_panel_depth_node.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
    ])
