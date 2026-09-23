"""Run the isolated stereo and semantic panel simulation without hardware."""

from pathlib import Path
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    return LaunchDescription([
        DeclareLaunchArgument('world', default_value=str(root / 'worlds' / 'semantic_panel.world')),
        DeclareLaunchArgument('gui', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_test_world.launch.py')),
            launch_arguments={'world': LaunchConfiguration('world'),
                              'gui': LaunchConfiguration('gui')}.items()),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'stereo_processing.launch.py')),
            launch_arguments={'use_sim_time': 'true'}.items()),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'semantic_panel_node.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
    ])
