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
    fire_panel_rec_dir = get_package_share_directory('fire_panel_rec')
    # fire_panel.jpg
    # input_file = os.path.join(fire_panel_rec_dir, 'res', 'led_region.jpg')
    input_file = "/home/ubuntu/arm_ws/src/fire_panel_rec/res/OCR/4.jpg"
    det_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_PP-OCRv3_det_infer', 'inference.pdmodel')
    cls_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_ppocr_mobile_v2.0_cls_infer', 'inference.pdmodel')
    rec_model = os.path.join(fire_panel_rec_dir, 'models', 'ch_PP-OCRv3_rec_infer', 'inference.pdmodel')
    label_dir = os.path.join(fire_panel_rec_dir, 'models', 'label', 'ppocr_keys_v1.txt')                         
    
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

    return LaunchDescription([
        
        rec_demo_node

    ])
