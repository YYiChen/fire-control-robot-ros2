from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch.utilities import perform_substitutions
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def launch_setup(context, *args, **kwargs):

    piper_aruco_localization_dir = get_package_share_directory('piper_aruco_localization')
    arm_eih_path = os.path.join(piper_aruco_localization_dir, 'configs', 'piper_hk_eih_33.calib')
    aruco_panel_path = os.path.join(piper_aruco_localization_dir, 'configs', 'aruco_panel_new.yaml')
    
    aruco_single_params = {
        # 手眼标定参数文件路径
        'arm_eih_path': arm_eih_path,
        'aruco_panel_path': aruco_panel_path,
        # 图像是否已去畸变
        'image_is_rectified': False, 
        # 标记大小
        'marker_size': LaunchConfiguration('marker_size'),
        # 识别的标记ID
        'marker_id': LaunchConfiguration('marker_id'),
        # 角点优化方法
        'corner_refinement': LaunchConfiguration('corner_refinement'),
        'parameter_preset': LaunchConfiguration('parameter_preset'),
    }

    aruco_single = Node(
        package='piper_aruco_localization',
        executable='single_shot_detector',
        parameters=[aruco_single_params],
    )

    return [aruco_single]

# 定义所有可配置的启动参数
def generate_launch_description():
    # ArUco 标记的 ID，默认为 582
    marker_id_arg = DeclareLaunchArgument(
        'marker_id', default_value='582',
        description='Marker ID. '
    )
    # 标记的物理大小（单位：米）
    marker_size_arg = DeclareLaunchArgument(
        'marker_size', default_value='0.04',
        description='Marker size in m. '
    )
    
    
    # 角点优化方法，默认为 LINES，支持 NONE, HARRIS, LINES, SUBPIX
    corner_refinement_arg = DeclareLaunchArgument(
        'corner_refinement', default_value='LINES',
        description='Corner Refinement. ',
        choices=['NONE', 'HARRIS', 'LINES', 'SUBPIX'],
    )

    parameter_preset_arg = DeclareLaunchArgument(
        'parameter_preset', default_value='robust',
        description='Parameter preset for ArUco detection. Options: robust, high_precision, custom',
        choices=['robust', 'high_precision', 'custom'],
    )

    # Create the launch description and populate
    ld = LaunchDescription()
    # 添加所有定义的参数
    ld.add_action(marker_id_arg)
    ld.add_action(marker_size_arg)
    ld.add_action(corner_refinement_arg)
    ld.add_action(parameter_preset_arg)
    # 执行动态逻辑以生成节点
    ld.add_action(OpaqueFunction(function=launch_setup))

    return ld
