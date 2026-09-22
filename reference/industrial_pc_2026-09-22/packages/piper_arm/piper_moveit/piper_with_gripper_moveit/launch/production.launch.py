from moveit_configs_utils import MoveItConfigsBuilder
from moveit_configs_utils.launches import generate_demo_launch
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import Node

joint_state_publisher = Node(
    package="joint_state_publisher",
    executable="joint_state_publisher",
    parameters=[{"publish_rate": 500}],  # 修改频率
    output='log'
)

def generate_launch_description():
    moveit_config = MoveItConfigsBuilder("piper", package_name="piper_with_gripper_moveit").to_moveit_configs()
    ld = generate_demo_launch(moveit_config)
    
    # 覆盖 use_rviz 参数为 false
    ld.entities.insert(0, DeclareLaunchArgument("use_rviz", default_value="false"))
    
    return ld