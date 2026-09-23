#!/usr/bin/env python3
"""Normalize Gazebo multicamera's duplicated Tx only for the VO audit branch."""

import copy

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo


class StereoCameraInfoNormalizer(Node):
    def __init__(self):
        super().__init__('stereo_camera_info_normalizer')
        self.right = None
        self.reported = False
        self.publisher = self.create_publisher(
            CameraInfo, '/vo/left/camera_info', qos_profile_sensor_data)
        self.create_subscription(
            CameraInfo, '/stereo/stereo_rig/left/camera_info',
            self.on_left, qos_profile_sensor_data)
        self.create_subscription(
            CameraInfo, '/stereo/stereo_rig/right/camera_info',
            self.on_right, qos_profile_sensor_data)

    def on_right(self, msg):
        self.right = msg

    def on_left(self, msg):
        if self.right is None or msg.p[0] <= 0:
            return
        baseline = -self.right.p[3] / self.right.p[0]
        if not 0.055 <= baseline <= 0.065:
            if not self.reported:
                self.get_logger().error(f'Unexpected right-camera baseline: {baseline:.4f} m')
                self.reported = True
            return
        if abs(msg.p[3] - self.right.p[3]) > 1.0 and abs(msg.p[3]) > 1.0:
            if not self.reported:
                self.get_logger().error('Left-camera Tx differs from both canonical and Gazebo values')
                self.reported = True
            return
        original_tx = msg.p[3]
        corrected = copy.deepcopy(msg)
        projection = list(corrected.p)
        projection[3] = 0.0
        corrected.p = projection
        self.publisher.publish(corrected)
        if not self.reported:
            self.get_logger().info(
                f'VO-only CameraInfo correction: left Tx={original_tx:.3f} -> 0, '
                f'right Tx={self.right.p[3]:.3f}, baseline={baseline:.3f} m')
            self.reported = True


def main():
    rclpy.init()
    node = StereoCameraInfoNormalizer()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
