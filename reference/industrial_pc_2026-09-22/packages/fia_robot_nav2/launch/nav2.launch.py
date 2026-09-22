import os
from glob import glob
import yaml
import tempfile
from ament_index_python.packages import get_package_share_directory
from ament_index_python.packages import get_package_prefix
from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def merge_yaml_files(file_list):
    merged = {}
    for file in file_list:
        with open(file, 'r') as f:
            data = yaml.safe_load(f)
            if data:
                merged.update(data)
    # 写入临时文件
    tmp_file = tempfile.NamedTemporaryFile(delete=False, suffix='.yaml')
    with open(tmp_file.name, 'w') as f:
        yaml.dump(merged, f)
    return tmp_file.name

def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')
    
    fia_nav_dir = get_package_share_directory('fia_robot_nav2')
    fia_nav_launchr = os.path.join(fia_nav_dir, 'launch')

    prefix_path = get_package_prefix('fia_robot_nav2')
    install_dir = os.path.dirname(prefix_path)
    workspace_root = os.path.dirname(install_dir)
    map_dir = os.path.join(workspace_root, 'data', 'map')

    print(f"[DEBUG] map_dir path = {map_dir}")

    #map_dir = os.path.join(fia_nav_dir, 'map')
    map_file = LaunchConfiguration('map', default=os.path.join(
        map_dir, 'fcr_map.yaml'))

    param_dir = os.path.join(fia_nav_dir, 'params')
    param_files = sorted(glob(os.path.join(param_dir, '*.yaml')))
    merged_param_file = merge_yaml_files(param_files)

    return LaunchDescription([
        DeclareLaunchArgument(
            'map',
            default_value=map_file,
            description='Full path to map file to load'),

        DeclareLaunchArgument(
            'params',
            default_value=merged_param_file,
            description='Full path to all nav param file to load'),
        Node(
            name='waypoint_cycle',
            package='nav2_waypoint_cycle',
            executable='nav2_waypoint_cycle',
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                [fia_nav_launchr, '/bringup_launch.py']),
            launch_arguments={
                'map': map_file,
                'use_sim_time': use_sim_time,
                'params_file': merged_param_file}.items(),
        ),
    ])
