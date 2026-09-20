#!/usr/bin/env python3
# 在自定义场景 fire_control_room.world 中启动 TurtleBot3（供建图 / 导航用）
#
# 用法（WSL 内）：
#   1) 把 fire_control_room.world 和本文件放在同一目录
#   2) ros2 launch /该目录/fire_control_room.launch.py
#
# 说明：Gazebo 启动较慢时，生成机器人最多等待 120 秒。

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    model = os.environ.setdefault('TURTLEBOT3_MODEL', 'burger')
    launch_file_dir = os.path.join(get_package_share_directory('turtlebot3_gazebo'), 'launch')
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    model_file = os.path.join(
        get_package_share_directory('turtlebot3_gazebo'),
        'models', 'turtlebot3_' + model, 'model.sdf')
    if not os.path.isfile(model_file):
        raise FileNotFoundError(model_file)

    use_sim_time = LaunchConfiguration('use_sim_time')
    x_pose = LaunchConfiguration('x_pose')
    y_pose = LaunchConfiguration('y_pose')
    world = LaunchConfiguration('world')

    world_file = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), 'fire_control_room.world')

    gzserver_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')),
        launch_arguments={'world': world}.items())

    gzclient_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py')),
        condition=IfCondition(LaunchConfiguration('gui')))

    robot_state_publisher_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(launch_file_dir, 'robot_state_publisher.launch.py')),
        launch_arguments={'use_sim_time': use_sim_time}.items())

    spawn_turtlebot_cmd = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=[
            '-entity', model, '-file', model_file,
            '-x', x_pose, '-y', y_pose, '-z', '0.01', '-timeout', '120'],
        output='screen')

    ld = LaunchDescription()
    ld.add_action(DeclareLaunchArgument('world', default_value=world_file))
    ld.add_action(DeclareLaunchArgument('gui', default_value='true'))
    ld.add_action(DeclareLaunchArgument('use_sim_time', default_value='true'))
    ld.add_action(DeclareLaunchArgument('x_pose', default_value='0.5'))
    ld.add_action(DeclareLaunchArgument('y_pose', default_value='0.5'))
    ld.add_action(gzserver_cmd)
    ld.add_action(gzclient_cmd)
    ld.add_action(robot_state_publisher_cmd)
    ld.add_action(spawn_turtlebot_cmd)
    return ld
