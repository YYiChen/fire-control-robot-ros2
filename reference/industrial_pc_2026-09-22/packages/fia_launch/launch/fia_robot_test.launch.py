from launch import LaunchDescription
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch.actions import IncludeLaunchDescription, TimerAction, ExecuteProcess
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # 获取对应包的共享目录路径
    piper_moveit_dir = get_package_share_directory('piper_with_gripper_moveit')
    fia_launch_dir = get_package_share_directory('fia_launch')
    fire_panel_rec_dir = get_package_share_directory('fire_panel_rec')
    
    cert_file = os.path.join(fia_launch_dir, 'certs', 'cert.pem')
    key_file = os.path.join(fia_launch_dir, 'certs', 'key.pem')
    task_config_file = os.path.join(fia_launch_dir, 'configs', 'task_config_v2.yaml')
    
    input_file = os.path.join(fire_panel_rec_dir, 'res', 'led.jpg')
    det_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_PP-OCRv3_det_infer', 'inference.pdmodel')
    cls_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_ppocr_mobile_v2.0_cls_infer', 'inference.pdmodel')
    rec_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_PP-OCRv3_rec_infer', 'inference.pdmodel')
    label_dir = os.path.join(fire_panel_rec_dir, 'models', 'label', 'ppocr_keys_v1.txt')                         
    
    piper_dir = get_package_share_directory('piper')
    start_single_piper_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(piper_dir, 'launch', 'start_single_piper.launch.py')
        )
    )

    # 延迟启动 piper_demo_launch，确保 piper 启动完成
    piper_demo_launch = TimerAction(
        period=5.0,  # 延迟5秒启动
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(piper_moveit_dir, 'launch', 'demo.launch.py')
                )
            )
        ]
    )

    # 延迟启动 piper_production_launch，确保 piper 启动完成
    piper_production_launch = TimerAction(
        period=5.0,  # 延迟5秒启动
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(piper_moveit_dir, 'launch', 'production.launch.py')
                )
            )
        ]
    )

    # 节点：execute_arm_task
    execute_arm_task_node = Node(
        package='piper_control',
        executable='execute_arm_task',
        # name='execute_arm_task',
        output='screen'
    )

    video_manager_dir = get_package_share_directory('video_manager')
    ost_path = os.path.join(video_manager_dir, 'ost', 'hk_ost.txt')
    image_server_node = Node(
        package='video_manager',
        executable='image_server',
        name='image_server',
        output='screen',
        parameters=[
            {'ost_path': ost_path},
            {'is_rectified': False}
        ]
    )

    piper_perception_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(fia_launch_dir, 'launch', 'piper_perception_launch.py')
        )
    )

    lift_control_node = Node(
        package='lift_control',
        executable='lift_controller_node',
        name='lift_controller_node',
        output='screen'
    )
    
    rosbridge_websocket_node = Node(
        package='rosbridge_server',
        executable='rosbridge_websocket',
        name='rosbridge_websocket',
        parameters=[{
            'port': 9090,
            'ssl': True,
            'certfile': cert_file,
            'keyfile': key_file
        }],
        output='screen'
    )

    # 节点：task_manager（延迟 5 秒启动）
    task_manager_node = TimerAction(
        period=1.0,
        actions=[
            Node(
                package='task_manager',
                executable='task_manager',
                # name='task_manager',
                # namespace='robot1',  # 添加命名空间
                parameters=[
                    {'task_config_path': task_config_file}
                ],
                output='screen',
                # condition=IfCondition(LaunchConfiguration('lift_control_ready')),
            )
        ]
    )

    # 节点：rec_demo
    rec_demo_node = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'fire_panel_rec', 'rec_demo',
            '-input', input_file,
            '-label_dir', label_dir,
            '-det_model_dir', det_model,
            '-cls_model_dir', cls_model,
            '-rec_model_dir', rec_model,
        ],
        name='rec_demo',
        output='screen'
    )

    single_shot_rec_node = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'fire_panel_rec', 'single_shot_rec',
            '-input', input_file,
            '-label_dir', label_dir,
            '-det_model_dir', det_model,
            '-cls_model_dir', cls_model,
            '-rec_model_dir', rec_model,
        ],
        # name='single_shot_rec',
        output='screen'
    )
    
    export_display_node = ExecuteProcess(
        cmd=[
            'export DISPLAY=:0'
        ],
        name='export_display',
        output='screen'
    )

    return LaunchDescription([
        # export_display_node,
        # rosbridge_websocket_node,

        start_single_piper_launch,
        # piper_production_launch,
        piper_demo_launch,
        execute_arm_task_node,
        image_server_node,
        piper_perception_launch,
        # lift_control_node,
        task_manager_node,
        single_shot_rec_node,
    ])
