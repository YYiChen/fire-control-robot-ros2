#!/usr/bin/env python3
"""Verify that the simulated stereo pipeline produces usable depth data."""

import math
import os
import statistics
import struct
import sys
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2
from stereo_msgs.msg import DisparityImage


class StereoOutputVerifier(Node):
    def __init__(self):
        super().__init__('stereo_output_verifier')
        self.disparity_count = 0
        self.point_count = 0
        self.central_depths = []
        self.create_subscription(
            DisparityImage, '/stereo/disparity', self.on_disparity,
            qos_profile_sensor_data)
        self.create_subscription(
            PointCloud2, '/stereo/points2', self.on_points,
            qos_profile_sensor_data)

    def on_points(self, _message):
        self.point_count += 1

    def on_disparity(self, message):
        self.disparity_count += 1
        image = message.image
        if image.encoding != '32FC1' or image.width < 40 or image.height < 40:
            return
        if message.f <= 0.0 or message.t <= 0.0:
            return

        start_x = image.width // 2 - 20
        start_y = image.height // 2 - 20
        samples = []
        raw = bytes(image.data)
        for row in range(start_y, start_y + 40):
            for col in range(start_x, start_x + 40):
                offset = row * image.step + col * 4
                disparity = struct.unpack_from('<f', raw, offset)[0]
                if math.isfinite(disparity) and disparity > 0.0:
                    samples.append((message.f * message.t) / disparity)
        if samples:
            self.central_depths.append(statistics.median(samples))


def main():
    expected_depth = float(os.environ.get('STEREO_EXPECTED_DEPTH_M', '0.60'))
    lower_bound = expected_depth * 0.58
    upper_bound = expected_depth * 1.50
    rclpy.init()
    node = StereoOutputVerifier()
    deadline = time.monotonic() + 8.0
    try:
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
    finally:
        node.destroy_node()
        rclpy.shutdown()

    if node.disparity_count < 3 or node.point_count < 3:
        print(
            'FAIL: insufficient output messages '
            f'(disparity={node.disparity_count}, points2={node.point_count})')
        return 2
    if not node.central_depths:
        print('FAIL: central 40x40 disparity region has no positive finite depth.')
        return 2

    depth = statistics.median(node.central_depths)
    print(
        'Stereo output: '
        f'disparity={node.disparity_count}, points2={node.point_count}, '
        f'central_depth_m={depth:.3f}')
    if not lower_bound <= depth <= upper_bound:
        print(
            'FAIL: central depth is outside the acceptance band '
            f'{lower_bound:.3f}-{upper_bound:.3f} m for the {expected_depth:.2f} m panel.')
        return 2

    print(f'PASS: stereo depth is plausible for the {expected_depth:.2f} m test panel.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
