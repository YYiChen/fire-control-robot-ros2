"""Run synthetic OCR/depth landmarks through VO and graph-map projection."""

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
            'world', default_value=str(root / 'worlds' / 'semantic_ocr_panel.world')),
        DeclareLaunchArgument('database_path', default_value=str(
            Path.home() / 'stereo_sim_generated' / 'semantic_ocr_graph.db')),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                str(root / 'launch' / 'semantic_graph_slam.launch.py')),
            launch_arguments={
                'gui': LaunchConfiguration('gui'),
                'world': LaunchConfiguration('world'),
                'database_path': LaunchConfiguration('database_path'),
            }.items()),
        ExecuteProcess(
            cmd=[sys.executable,
                 str(root / 'scripts' / 'stereo_panel_depth_node.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
        ExecuteProcess(
            cmd=[sys.executable,
                 str(root / 'scripts' / 'semantic_ocr_graph_projector.py'),
                 '--ros-args', '-p', 'use_sim_time:=true'],
            output='screen'),
    ])
