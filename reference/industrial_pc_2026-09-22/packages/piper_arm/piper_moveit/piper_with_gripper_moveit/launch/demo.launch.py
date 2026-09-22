from moveit_configs_utils import MoveItConfigsBuilder
from moveit_configs_utils.launches import generate_demo_launch
from launch_ros.actions import Node

joint_state_publisher = Node(
    package="joint_state_publisher",
    executable="joint_state_publisher",
    parameters=[{"publish_rate": 500}],  # 修改频率
    output='log'
)

def generate_launch_description():
    moveit_config = MoveItConfigsBuilder("piper", package_name="piper_with_gripper_moveit").to_moveit_configs()
    moveit_config.planning_pipelines['ompl']['max_velocity_scaling_factor'] = 1.0
    moveit_config.planning_pipelines['ompl']['max_acceleration_scaling_factor'] = 1.0
    
    return generate_demo_launch(moveit_config)
