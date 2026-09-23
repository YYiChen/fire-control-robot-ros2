#!/usr/bin/env python3
"""Exercise fail-closed behaviour using synthetic ROS image/disparity messages."""

import sys

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from geometry_msgs.msg import Pose
from sensor_msgs.msg import CameraInfo
from stereo_msgs.msg import DisparityImage

from semantic_panel_node import SemanticPanelNode


def run_case(node, with_marker, valid_depth):
    image = np.full((480, 640, 3), 170, dtype=np.uint8)
    for x, color in ((200, (0, 0, 255)), (320, (0, 255, 0)), (440, (255, 0, 0))):
        cv2.circle(image, (x, 190), 22, color, -1)
    if with_marker:
        dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_ARUCO_ORIGINAL)
        tag = cv2.aruco.drawMarker(dictionary, 582, 140, borderBits=1)
        image[270:410, 250:390] = cv2.cvtColor(tag, cv2.COLOR_GRAY2BGR)

    stamp = node.get_clock().now().to_msg()
    camera = CameraInfo()
    camera.header.stamp = stamp
    camera.width, camera.height = 640, 480
    camera.k = [300.0, 0.0, 320.0, 0.0, 300.0, 240.0, 0.0, 0.0, 1.0]
    camera.p = [300.0, 0.0, 320.0, 0.0, 0.0, 300.0, 240.0, 0.0, 0.0, 0.0, 1.0, 0.0]
    camera.d = [0.0] * 5
    node.camera_info = camera
    rig = Pose()
    rig.orientation.w = 1.0
    node.rig_pose = rig
    node.rig_pose_time = node.get_clock().now().nanoseconds * 1e-9

    bridge = CvBridge()
    image_msg = bridge.cv2_to_imgmsg(image, encoding='bgr8')
    image_msg.header.stamp = stamp
    depth = np.full((480, 640), 30.0 if valid_depth else 0.0, dtype=np.float32)
    disparity = DisparityImage()
    disparity.header.stamp = stamp
    disparity.image = bridge.cv2_to_imgmsg(depth, encoding='32FC1')
    disparity.image.header.stamp = stamp
    disparity.f, disparity.t = 300.0, 0.06

    poses = []
    status = []
    for name in node.pose_publishers:
        node.pose_publishers[name] = type('Collector', (), {'publish': lambda self, msg: poses.append(msg)})()
    node.publish_status = lambda stamp_sec, buttons, state: status.append((state, buttons))
    node.process(image_msg, disparity)
    return poses, status[-1][1]


def main():
    rclpy.init()
    node = SemanticPanelNode()
    try:
        poses, status = run_case(node, with_marker=False, valid_depth=True)
        if poses or status.get('aruco_582') != 'not_detected' or any(
                status.get(name) != 'marker_not_detected'
                for name in ('mute', 'reset', 'confirm')):
            print(f'FAIL missing marker: poses={len(poses)}, status={status}')
            return 2
        print('PASS missing marker: all three button poses withheld')

        poses, status = run_case(node, with_marker=True, valid_depth=False)
        if poses or not isinstance(status.get('aruco_582'), dict) or any(
                status.get(name) != 'no_valid_depth'
                for name in ('mute', 'reset', 'confirm')):
            print(f'FAIL invalid disparity: poses={len(poses)}, status={status}')
            return 2
        print('PASS invalid disparity: all three button poses withheld')
        return 0
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
