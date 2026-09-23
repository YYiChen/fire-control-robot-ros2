"""Launch only the isolated Gazebo stereo test world; no robot is spawned."""

import os
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, SetEnvironmentVariable
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    root = Path(__file__).resolve().parents[1]
    default_world = str(root / 'worlds' / 'stereo_test.world')
    world = LaunchConfiguration('world')
    models = str(root / 'models')
    turtlebot_models = str(Path(get_package_share_directory('turtlebot3_gazebo')) / 'models')
    existing_models = os.environ.get('GAZEBO_MODEL_PATH', '')
    gazebo_share = get_package_share_directory('gazebo_ros')

    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='true'),
        DeclareLaunchArgument(
            'world', default_value=default_world,
            description='SDF world file containing the stereo rig and known-distance target.'),
        SetEnvironmentVariable(
            'GAZEBO_MODEL_PATH', ':'.join(
                part for part in (models, turtlebot_models, existing_models) if part)),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(Path(gazebo_share) / 'launch' / 'gzserver.launch.py')),
            launch_arguments={'world': world}.items()),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(Path(gazebo_share) / 'launch' / 'gzclient.launch.py')),
            condition=IfCondition(LaunchConfiguration('gui'))),
    ])
