#!/usr/bin/env python3
"""Compare measured stereo-odometry translation to Gazebo model-state truth."""

import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo, Image


def yaw_from_quaternion(q):
    return math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                      1.0 - 2.0 * (q.y * q.y + q.z * q.z))


class OdometryAudit(Node):
    def __init__(self):
        super().__init__('stereo_odometry_audit_verifier')
        self.odom = []
        self.truth = []
        self.images = 0
        self.left_p3 = None
        self.right_p3 = None
        self.corrected_left_p3 = None
        self.create_subscription(Odometry, '/vo/odom', self.on_odom, qos_profile_sensor_data)
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(Image, '/stereo/left/image_rect', self.on_image, 10)
        self.create_subscription(CameraInfo, '/stereo/stereo_rig/left/camera_info',
                                 self.on_left_info, qos_profile_sensor_data)
        self.create_subscription(CameraInfo, '/stereo/stereo_rig/right/camera_info',
                                 self.on_right_info, qos_profile_sensor_data)
        self.create_subscription(CameraInfo, '/vo/left/camera_info',
                                 self.on_corrected_left_info, qos_profile_sensor_data)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_image(self, _msg):
        self.images += 1

    def on_left_info(self, msg):
        self.left_p3 = msg.p[3]

    def on_right_info(self, msg):
        self.right_p3 = msg.p[3]

    def on_corrected_left_info(self, msg):
        self.corrected_left_p3 = msg.p[3]

    def on_odom(self, msg):
        p = msg.pose.pose.position
        values = (p.x, p.y, p.z)
        if all(math.isfinite(value) for value in values):
            self.odom.append((time.monotonic(), values, msg.header.frame_id,
                              msg.child_frame_id, msg.pose.covariance[0],
                              yaw_from_quaternion(msg.pose.pose.orientation)))

    def on_truth(self, msg):
        if 'stereo_rig' in msg.name:
            pose = msg.pose[msg.name.index('stereo_rig')]
            p = pose.position
            self.truth.append((time.monotonic(), (p.x, p.y, p.z),
                               yaw_from_quaternion(pose.orientation)))

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.2)

    def move(self, x, y=0.0, yaw=0.0):
        request = SetEntityState.Request()
        request.state.name = 'stereo_rig'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = x
        request.state.pose.position.y = y
        request.state.pose.orientation.z = math.sin(yaw / 2.0)
        request.state.pose.orientation.w = math.cos(yaw / 2.0)
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        return future.done() and future.result() is not None and future.result().success


def main():
    rclpy.init()
    node = OdometryAudit()
    try:
        if not node.move_client.wait_for_service(timeout_sec=8.0):
            print('FAIL: Gazebo state service unavailable')
            return 2
        node.observe(8.0)
        print(f'initial: images={node.images}, left_P3={node.left_p3}, '
              f'right_P3={node.right_p3}, corrected_left_P3={node.corrected_left_p3}, '
              f'odom_msgs={len(node.odom)}, '
              f'odom={node.odom[-1] if node.odom else None}')
        before_odom = node.odom[-1][1] if node.odom else None
        before_odom_yaw = node.odom[-1][5] if node.odom else None
        before_truth = node.truth[-1][1] if node.truth else None
        before_truth_yaw = node.truth[-1][2] if node.truth else None
        for step in range(1, 21):
            x = step * 0.005
            if not node.move(x):
                print(f'FAIL: Gazebo rejected camera movement to x={x}')
                return 2
            node.observe(0.4)
            if step % 4 == 0:
                print(f'view x={x:.2f}: images={node.images}, odom_msgs={len(node.odom)}, '
                      f'odom_xyz={node.odom[-1][1] if node.odom else None}')
        for step in range(1, 11):
            y = step * 0.005
            if not node.move(0.10, y):
                print(f'FAIL: Gazebo rejected lateral camera movement to y={y}')
                return 2
            node.observe(0.4)
            if step % 5 == 0:
                print(f'view y={y:.3f}: images={node.images}, odom_msgs={len(node.odom)}, '
                      f'odom_xyz={node.odom[-1][1] if node.odom else None}')
        for step in range(1, 9):
            yaw = step * 0.01
            if not node.move(0.10, 0.05, yaw):
                print(f'FAIL: Gazebo rejected camera yaw={yaw}')
                return 2
            node.observe(0.4)
            if step % 4 == 0:
                print(f'view yaw={yaw:.3f}: odom_yaw={node.odom[-1][5] if node.odom else None}')
        node.observe(3.0)
        after_odom = node.odom[-1][1] if node.odom else None
        after_odom_yaw = node.odom[-1][5] if node.odom else None
        after_truth = node.truth[-1][1] if node.truth else None
        after_truth_yaw = node.truth[-1][2] if node.truth else None
        vo_delta = math.dist(before_odom, after_odom) if before_odom and after_odom else float('nan')
        truth_delta = math.dist(before_truth, after_truth) if before_truth and after_truth else float('nan')
        expected_base = tuple(after_truth[i] - before_truth[i] for i in range(3)) if before_truth and after_truth else None
        measured_base = tuple(after_odom[i] - before_odom[i] for i in range(3)) if before_odom and after_odom else None
        vector_error = math.dist(expected_base, measured_base) if expected_base and measured_base else float('nan')
        yaw_error = abs((after_odom_yaw - before_odom_yaw) -
                        (after_truth_yaw - before_truth_yaw)) if before_odom_yaw is not None and after_odom_yaw is not None and before_truth_yaw is not None and after_truth_yaw is not None else float('nan')
        print(f'result: images={node.images}, odom_msgs={len(node.odom)}, '
              f'vo_translation_m={vo_delta:.4f}, truth_translation_m={truth_delta:.4f}, '
              f'expected_base_m={expected_base}, measured_base_m={measured_base}, '
              f'vector_error_m={vector_error:.4f}, '
              f'vo_yaw_rad={after_odom_yaw - before_odom_yaw if before_odom_yaw is not None and after_odom_yaw is not None else None}, '
              f'truth_yaw_rad={after_truth_yaw - before_truth_yaw if before_truth_yaw is not None and after_truth_yaw is not None else None}, '
              f'yaw_error_rad={yaw_error:.4f}, '
              f'frame={node.odom[-1][2:4] if node.odom else None}, '
              f'cov_x={node.odom[-1][4] if node.odom else None}')
        success = (node.images >= 20 and len(node.odom) >= 10 and
                   math.isfinite(vo_delta) and vector_error <= 0.015 and yaw_error <= 0.03 and
                   0.11 <= truth_delta <= 0.115)
        print('PASS: independent stereo odometry resolved the controlled translation.' if success
              else 'FAIL: stereo odometry is not yet reliable enough to replace Gazebo truth.')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
