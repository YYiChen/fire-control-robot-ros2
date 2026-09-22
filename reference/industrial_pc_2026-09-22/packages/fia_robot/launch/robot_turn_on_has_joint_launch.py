import os
from pathlib import Path
import launch
from launch.actions import SetEnvironmentVariable
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, GroupAction,
                            IncludeLaunchDescription, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import PushRosNamespace
import launch_ros.actions
from launch.conditions import IfCondition
from launch.conditions import UnlessCondition
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
from launch.actions import LogInfo

def generate_launch_description():
    # Get the launch directory
    bringup_dir = get_package_share_directory('fia_robot')
    launch_dir = os.path.join(bringup_dir, 'launch')
    
    imu_config = Path(get_package_share_directory('fia_robot'), 'config', 'imu.yaml')

    
    carto_slam = LaunchConfiguration('carto_slam', default='false')
    
    carto_slam_dec = DeclareLaunchArgument('carto_slam',default_value='false')
            
    chassis_node = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'chassis_base_launch.py')),
    )

    sensors_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'robot_sensors_launch.py')),
    )
     
    robot_ekf = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'robot_location_ekf_launch.py')),
            launch_arguments={'carto_slam':carto_slam}.items(),            
    )
    
#     base_to_link = launch_ros.actions.Node(
#             package='tf2_ros', 
#             executable='static_transform_publisher', 
#             name='base_to_link',
#         #     arguments=['0', '0', '0','0', '0','0','base_footprint','base_link'],
#             arguments=[
#                 '--x', '0', '--y', '0', '--z', '0',
#                 '--roll', '0', '--pitch', '0', '--yaw', '0',
#                 '--frame-id', 'base_footprint',
#                 '--child-frame-id', 'base_link'
#             ],
#     )

# generate_static_transform_publisher_node(['0.10115', '0.00', '0.23618'], ['0', '0', '0'], 'base_footprint', 'laser'),
# generate_static_transform_publisher_node(['0.19024', '0.00024', '0.22022'], ['0', '0', '0'], 'base_footprint', 'camera_link'),
     

    base_to_gyro = launch_ros.actions.Node(
            package='tf2_ros', 
            executable='static_transform_publisher', 
            name='base_to_gyro',
        #     arguments=['0', '0', '0','0', '0','0','base_footprint','gyro_link'],
            arguments=[
                '--x', '0', '--y', '0', '--z', '0',
                '--roll', '0', '--pitch', '0', '--yaw', '0',
                '--frame-id', 'base_footprint',
                '--child-frame-id', 'gyro_link'
            ],

    )
    
    imu_filter_node =  launch_ros.actions.Node(
        package='imu_filter_madgwick',
        executable='imu_filter_madgwick_node',
        parameters=[imu_config]
    )
    
                           
    joint_state_publisher_node = launch_ros.actions.Node(
            package='joint_state_publisher', 
            executable='joint_state_publisher', 
            name='joint_state_publisher',
    )
    
    #select a robot model,the default model is fia r001 
    robot_urdf = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'robot_description_xacro_launch.py')),
            launch_arguments={'fia_diff': 'true'}.items(),
    )

    twist_mux_node = launch_ros.actions.Node(
            package="twist_mux",
            executable="twist_mux",
            name="twist_mux_node",
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('fia_robot'),
                    'config',
                    'twist_mux_locks.yaml'
                ]),
                PathJoinSubstitution([
                    FindPackageShare('fia_robot'),
                    'config',
                    'twist_mux_topics.yaml'
                ])
            ],
            remappings=[
                ("cmd_vel_out", "/cmd_vel")  # 输出话题
            ]
        )
    # twist_mux = IncludeLaunchDescription(
    #         PythonLaunchDescriptionSource(os.path.join(launch_dir, 'twist_mux_launch.py')),
    # )

    teleop_joy = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'teleop_joy_launch.py')),
    )

    ld = LaunchDescription()

    ld.add_action(robot_urdf)
    #ld.add_action(flagship_type)
    ld.add_action(carto_slam_dec)
    ld.add_action(chassis_node)
#     ld.add_action(base_to_link)
    ld.add_action(twist_mux_node)
    ld.add_action(base_to_gyro)
    ld.add_action(joint_state_publisher_node)
    ld.add_action(imu_filter_node)
    ld.add_action(sensors_launch)    
    ld.add_action(robot_ekf)
    ld.add_action(teleop_joy)

    return ld

