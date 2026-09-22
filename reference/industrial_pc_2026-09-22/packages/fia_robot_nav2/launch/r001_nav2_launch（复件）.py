import os
from ament_index_python.packages import get_package_share_directory, get_package_prefix
from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')
    
    # pkg_path = get_package_share_directory('robot_nav2')  # 替换为你的ROS2包名
    # rviz_config_path = os.path.join(pkg_path, 'rviz', 'nav.rviz')  # 确保配置文件存在

    # robot_robot_dir = get_package_share_directory('turn_on_robot_robot')
    # robot_launch_dir = os.path.join(robot_robot_dir, 'launch')
        
    robot_nav_dir = get_package_share_directory('fia_robot_nav2')
    robot_nav_launchr = os.path.join(robot_nav_dir, 'launch')


    # map_dir = os.path.join(robot_nav_dir, 'maps')

    prefix_path = get_package_prefix('fia_robot_nav2')
    install_dir = os.path.dirname(prefix_path)
    workspace_root = os.path.dirname(install_dir)
    map_dir = os.path.join(workspace_root, 'data', 'map')

    map_file = LaunchConfiguration('map', default=os.path.join(
        map_dir, 'fcr_map.yaml'))


    #Modify the model parameter file, the options are:
    #param_mini_akm.yaml/param_mini_4wd.yaml/param_mini_diff.yaml/
    #param_mini_mec.yaml/param_mini_omni.yaml/param_mini_tank.yaml/
    #param_senior_akm.yaml/param_senior_diff.yaml/param_senior_mec_bs.yaml
    #param_senior_mec_dl.yaml/param_top_4wd_bs.yaml/param_top_4wd_dl.yaml
    #param_top_akm_dl.yaml/param_four_wheel_diff_dl.yaml/param_four_wheel_diff_bs.yaml

    param_dir = os.path.join(robot_nav_dir, 'params')
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
                [robot_nav_launchr, '/bringup_launch.py']),
            launch_arguments={
                'map': map_file,
                'use_sim_time': use_sim_time,
                'params_file': param_file}.items(),
        )
        # ,
        
        # Node(
        #     package='rviz2',
        #     executable='rviz2',
        #     name='rviz2',
        #     arguments=['-d', rviz_config_path],  # 加载配置文件
        #     output='screen'
        # ),

    ])
