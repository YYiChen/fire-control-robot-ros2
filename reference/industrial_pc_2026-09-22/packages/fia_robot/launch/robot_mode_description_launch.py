import os

from ament_index_python.packages import get_package_share_directory
import launch_ros.actions
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, SetEnvironmentVariable
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import LoadComposableNodes
from launch_ros.actions import Node
from launch_ros.descriptions import ComposableNode
from nav2_common.launch import RewrittenYaml

def generate_robot_node(robot_urdf, child):
    urdf_file = os.path.join(
        get_package_share_directory('fia_robot'),
        'urdf',
        robot_urdf
    )
    
    # print(f"[DEBUG] URDF file path: {urdf_file}")
    if not os.path.exists(urdf_file):
        raise FileNotFoundError(f"URDF file not found at: {urdf_file}")

    with open(urdf_file, 'r') as infp:
        robot_description_content = infp.read()

    return launch_ros.actions.Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name=f'robot_state_publisher_{child}',
        output='screen',
        # arguments=[os.path.join(get_package_share_directory('fia_robot'), 'urdf', robot_urdf)],
        parameters=[{'robot_description': robot_description_content}]
    )

def generate_static_transform_publisher_node(translation, rotation, parent, child):
    return launch_ros.actions.Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name=f'base_to_{child}',
        arguments=[translation[0], translation[1], translation[2], rotation[0], rotation[1], rotation[2], parent, child],
    )

def create_joint_state_publisher_nodes(use_gui):
    return [
        Node(
            package='joint_state_publisher_gui',
            executable='joint_state_publisher_gui',
            name='joint_state_publisher_gui',
            output='screen',
            condition=IfCondition(use_gui)
        ),
        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            name='joint_state_publisher',
            output='screen',
            condition=UnlessCondition(use_gui)
        )
    ]
    
def generate_launch_description():

    fia_diff = LaunchConfiguration('fia_diff', default='true')
    use_gui_arg = DeclareLaunchArgument(
        'use_gui',
        default_value='false',
        description='Whether to launch joint_state_publisher_gui'
    )
    use_gui = LaunchConfiguration('use_gui')
    # brushless_senior_diff = LaunchConfiguration('brushless_senior_diff', default='false')
         
    fia_r001 = GroupAction(
        condition=IfCondition(fia_diff),
        actions=[
            generate_robot_node('YH_FIA_R001.urdf','fia')
            # generate_static_transform_publisher_node(['0.10115', '0.00', '0.23618'], ['0', '0', '0'], 'base_footprint', 'laser'),
            # generate_static_transform_publisher_node(['0.19024', '0.00024', '0.22022'], ['0', '0', '0'], 'base_footprint', 'camera_link'),
    ])            
    # brushless_senior_diff_ = GroupAction(
    #     condition=IfCondition(brushless_senior_diff),
    #     actions=[
    #         generate_robot_node('brushless_senior_diff_robot.urdf','brushless_senior_diff'),
    #         generate_static_transform_publisher_node(['0.10115', '0.00', '0.23618'], ['0', '0', '0'], 'base_footprint', 'laser'),
    #         generate_static_transform_publisher_node(['0.19024', '0.00024', '0.22022'], ['0', '0', '0'], 'base_footprint', 'camera_link'),        
    # ])

    # joint_state_publisher_gui node
    joint_state_nodes = create_joint_state_publisher_nodes(use_gui)
    
    # Create the launch description and populate
    ld = LaunchDescription()

    # Set environment variables
    # Declare the launch options

    # Add the actions to launch all of the localiztion nodes
    ld.add_action(use_gui_arg)
    ld.add_action(fia_r001)
    for node in joint_state_nodes:
        ld.add_action(node)
    # ld.add_action(brushless_senior_diff_)

    return ld
