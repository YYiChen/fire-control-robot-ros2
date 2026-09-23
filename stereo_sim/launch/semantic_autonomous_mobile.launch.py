"""Run the isolated stereo mobile base with guarded approach path following."""

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
        DeclareLaunchArgument('world', default_value=str(root / 'worlds' / 'semantic_panel_mobile.world')),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(root / 'launch' / 'semantic_mobile_stereo.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui'),
                              'world': LaunchConfiguration('world'),
                              'stand_off_m': '0.28'}.items()),
        ExecuteProcess(
            cmd=[sys.executable, str(root / 'scripts' / 'semantic_path_follower.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
    ])
