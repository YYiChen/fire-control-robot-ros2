import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, GroupAction,
                            IncludeLaunchDescription, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
import launch_ros.actions

#def launch(launch_descriptor, argv):
def generate_launch_description():
    
    # -----------------------------------------
    # lidar launch
    # -----------------------------------------
    lidar_pkg_dir = get_package_share_directory('lslidar_driver')
    lidar_launch_dir = os.path.join(lidar_pkg_dir, 'launch')
   
    lidar_node = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(lidar_launch_dir, 'lsn10p_launch.py')),
    )

    # -----------------------------------------
    # camera launch
    # -----------------------------------------
    # camera_pkg_dir = get_package_share_directory('lslidar_driver')
    # camera_launch_dir = os.path.join(lidar_pkg_dir, 'launch')

    # front_depth_camera = IncludeLaunchDescription(
    #         PythonLaunchDescriptionSource(os.path.join(launch_dir, 'wheeltec_camera.launch.py')),
    # )

    astra_dir = get_package_share_directory('astra_camera')
    astra_launch_dir = os.path.join(astra_dir,'launch')

    # usbcam_dir=get_package_share_directory('usb_cam')
    # usbcam_launch_dir = os.path.join(usbcam_dir,'launch')

    usbcam_arg = DeclareLaunchArgument(
            'video_device', default_value='/dev/video0',
            description='video device serial number.')

    Astra_S = IncludeLaunchDescription(
    AnyLaunchDescriptionSource(os.path.join(astra_launch_dir,'astra.launch.xml')),)

    # Astra_Pro = IncludeLaunchDescription(
    # AnyLaunchDescriptionSource(os.path.join(astra_launch_dir,'astra_pro.launch.xml')),)

    # Dabai = IncludeLaunchDescription(
    # AnyLaunchDescriptionSource(os.path.join(astra_launch_dir,'dabai_u3.launch.xml')),)

    # Gemini = IncludeLaunchDescription(
    # AnyLaunchDescriptionSource(os.path.join(astra_launch_dir,'gemini.launch.xml')),)

    # #Select the UVC device {Rgbcam(C70)、Astra_Gemini、Astra_Dabai}
    # Wheeltec_Usbcam = IncludeLaunchDescription(
    # PythonLaunchDescriptionSource(os.path.join(usbcam_launch_dir,'demo.launch.py')),
    # launch_arguments={'video_device': '/dev/RgbCam'}.items())

    ld = LaunchDescription()
    
    ld.add_action(lidar_node)
#     ld.add_action(Astra_S)
    # ld.add_action(usbcam_arg)
    # ld.add_action(front_depth_camera)

    return ld

