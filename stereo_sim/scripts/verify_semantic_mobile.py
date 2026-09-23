#!/usr/bin/env python3
"""Check real Gazebo wheel motion while stereo VO tracks the panel."""

import json
import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from geometry_msgs.msg import PoseStamped, Twist
from nav_msgs.msg import OccupancyGrid, Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from std_msgs.msg import String


def yaw(q):
    return math.atan2(2 * (q.w * q.z + q.x * q.y),
                      1 - 2 * (q.y * q.y + q.z * q.z))


class MobileVerifier(Node):
    def __init__(self):
        super().__init__('semantic_mobile_verifier')
        self.truth = []
        self.vo = []
        self.buttons = []
        self.map_times = []
        self.map_free = []
        self.statuses = []
        self.approach = []
        self.command = self.create_publisher(Twist, '/cmd_vel', 10)
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(Odometry, '/vo/odom', self.on_odom,
                                 qos_profile_sensor_data)
        self.create_subscription(PoseStamped, '/semantic_panel/button/reset/pose',
                                 self.on_button, 10)
        self.create_subscription(OccupancyGrid, '/projected_map', self.on_map, 10)
        self.create_subscription(String, '/semantic_panel/status',
                                 lambda msg: self.statuses.append(json.loads(msg.data)), 10)
        self.create_subscription(String, '/semantic_panel/approach_status',
                                 lambda msg: self.approach.append(json.loads(msg.data)), 10)

    def on_truth(self, msg):
        if 'stereo_mobile_bot' in msg.name:
            pose = msg.pose[msg.name.index('stereo_mobile_bot')]
            p = pose.position
            self.truth.append((p.x, p.y, p.z, yaw(pose.orientation)))

    def on_odom(self, msg):
        pose = msg.pose.pose
        p = pose.position
        self.vo.append((p.x, p.y, p.z, yaw(pose.orientation)))

    def on_button(self, msg):
        p = msg.pose.position
        self.buttons.append((p.x, p.y, p.z, msg.header.frame_id))

    def on_map(self, msg):
        self.map_times.append(time.monotonic())
        self.map_free.append(sum(value == 0 for value in msg.data))

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)

    def drive(self, linear, angular, seconds):
        command = Twist()
        command.linear.x = linear
        command.angular.z = angular
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.command.publish(command)
            rclpy.spin_once(self, timeout_sec=0.1)
        self.stop()

    def stop(self):
        zero = Twist()
        for _ in range(5):
            self.command.publish(zero)
            rclpy.spin_once(self, timeout_sec=0.08)


def median(values):
    return sorted(values)[len(values) // 2] if values else float('inf')


def main():
    rclpy.init()
    node = MobileVerifier()
    try:
        node.observe(10.0)
        if not node.truth or not node.vo or not node.buttons:
            print(f'FAIL: initial truth/VO/button counts '
                  f'{len(node.truth)}/{len(node.vo)}/{len(node.buttons)}')
            return 2
        before_truth = node.truth[-1]
        before_vo = node.vo[-1]
        initial_map = max(node.map_free, default=0)
        initial_buttons = len(node.buttons)
        print(f'initial: truth={before_truth}, vo={before_vo}, '
              f'buttons={initial_buttons}, free_cells={initial_map}, '
              f'status={node.statuses[-1] if node.statuses else None}')
        node.drive(0.05, 0.0, 3.0)
        node.observe(1.0)
        node.drive(0.0, 0.12, 1.0)
        node.observe(3.0)
        after_truth = node.truth[-1]
        after_vo = node.vo[-1]
        truth_delta = tuple(after_truth[i] - before_truth[i] for i in range(3))
        vo_delta = tuple(after_vo[i] - before_vo[i] for i in range(3))
        vector_error = math.dist(truth_delta, vo_delta)
        truth_yaw = after_truth[3] - before_truth[3]
        vo_yaw = after_vo[3] - before_vo[3]
        truth_button = (0.561, 0.0, 0.545)
        moved_buttons = node.buttons[initial_buttons:]
        button_error = median([math.dist(item[:3], truth_button)
                               for item in moved_buttons if item[3] == 'map'])
        map_age = time.monotonic() - node.map_times[-1] if node.map_times else float('inf')
        print(f'result: truth_delta={truth_delta}, vo_delta={vo_delta}, '
              f'vector_error_m={vector_error:.4f}, truth_yaw_rad={truth_yaw:.3f}, '
              f'vo_yaw_rad={vo_yaw:.3f}, button_messages_after_drive={len(moved_buttons)}, '
              f'button_median_error_m={button_error:.4f}, '
              f'free_cells_initial/final={initial_map}/{max(node.map_free, default=0)}, '
              f'map_age_s={map_age:.2f}, '
              f'last_approach={node.approach[-1] if node.approach else None}')
        success = (0.04 <= math.hypot(*truth_delta[:2]) <= 0.25 and
                   abs(truth_yaw) >= 0.03 and vector_error <= 0.05 and
                   abs(vo_yaw - truth_yaw) <= 0.08 and
                   len(moved_buttons) >= 5 and button_error <= 0.07 and
                   initial_map >= 50 and map_age <= 3.0 and bool(node.approach))
        print('PASS: wheel-driven stereo robot retained VO, named target, and map.' if success
              else 'FAIL: mobile stereo baseline did not meet checks.')
        return 0 if success else 2
    finally:
        node.stop()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
