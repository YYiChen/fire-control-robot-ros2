import os
from ament_index_python.packages import get_package_share_directory
from ament_index_python.packages import get_package_prefix
from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')

    fia_nav_dir = get_package_share_directory('fia_robot_nav2')
    fia_nav_launchr = os.path.join(fia_nav_dir, 'launch')

    prefix_path = get_package_prefix('fia_robot_nav2')
    install_dir = os.path.dirname(prefix_path)
    workspace_root = os.path.dirname(install_dir)
    map_dir = os.path.join(workspace_root, 'data', 'map')

    print(f"[DEBUG] map_dir path = {map_dir}")

    map_file = LaunchConfiguration('map', default=os.path.join(
        map_dir, 'fcr_map.yaml'))
    
    # map_file = os.path.join(map_dir, 'fcr_map.yaml')
    
    print(f"[DEBUG] map_file path = {map_file}")


    #Modify the model parameter file, the options are:
    #param_mini_akm.yaml/param_mini_4wd.yaml/param_mini_diff.yaml/
    #param_mini_mec.yaml/param_mini_omni.yaml/param_mini_tank.yaml/
    #param_senior_akm.yaml/param_senior_diff.yaml/param_senior_mec_bs.yaml
    #param_senior_mec_dl.yaml/param_top_4wd_bs.yaml/param_top_4wd_dl.yaml
    #param_top_akm_dl.yaml/param_four_wheel_diff_dl.yaml/param_four_wheel_diff_bs.yaml

    param_dir = os.path.join(fia_nav_dir, 'params')
    param_file = LaunchConfiguration('params', default=os.path.join(
        param_dir, 'yh_fiar0001_params.yaml'))


    return LaunchDescription([
        DeclareLaunchArgument(
            'map',
            default_value=map_file,
            description='Full path to map file to load'),

        DeclareLaunchArgument(
            'params',
            default_value=param_file,
            description='Full path to param file to load'),
        Node(
            name='waypoint_cycle',
            package='nav2_waypoint_cycle',
            executable='nav2_waypoint_cycle',
        ),      
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                [fia_nav_launchr, '/bringup_launchssssss.py']),
            launch_arguments={
                'map': map_file,
                'use_sim_time': use_sim_time,
                'params_file': param_file}.items(),
        ),

    ])
