#!/usr/bin/env python3
"""Verify that OctoMap contains stereo surfaces near live semantic targets."""

import math
import os
from pathlib import Path
import subprocess
import sys
import time

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2


BUTTONS = ('mute', 'reset', 'confirm')


class OccupancyVerifier(Node):
    def __init__(self):
        super().__init__('semantic_occupancy_verifier')
        self.start = time.monotonic()
        self.first_cloud = None
        self.last_cloud = None
        self.before_cloud = None
        self.after_cloud = None
        self.map_messages = 0
        self.input_messages = 0
        self.button_poses = {}
        self.create_subscription(PointCloud2, '/octomap_point_cloud_centers', self.on_map, 10)
        self.create_subscription(PointCloud2, '/stereo/points2', self.on_input, 10)
        for name in BUTTONS:
            self.create_subscription(
                PoseStamped, f'/semantic_panel/button/{name}/pose',
                lambda msg, label=name: self.on_button(label, msg), 10)

    def on_input(self, msg):
        self.input_messages += 1

    def on_button(self, name, msg):
        if msg.header.frame_id == 'map':
            p = msg.pose.position
            self.button_poses[name] = (p.x, p.y, p.z)

    def on_map(self, msg):
        if msg.header.frame_id != 'map':
            return
        cloud = {
            (round(float(x), 3), round(float(y), 3), round(float(z), 3))
            for x, y, z in point_cloud2.read_points(
                msg, field_names=('x', 'y', 'z'), skip_nans=True)
        }
        self.map_messages += 1
        if self.first_cloud is None:
            self.first_cloud = cloud
        self.last_cloud = cloud
        if time.monotonic() - self.start < 6.0:
            self.before_cloud = cloud
        else:
            self.after_cloud = cloud


def main():
    panel_x = float(os.environ.get('SEMANTIC_PANEL_X', '0.60'))
    panel_y = float(os.environ.get('SEMANTIC_PANEL_Y', '0'))
    rclpy.init()
    node = OccupancyVerifier()
    verifier = Path(__file__).with_name('verify_semantic_panel.py')
    child = subprocess.Popen([sys.executable, str(verifier)], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic() + 35.0
        finished_at = None
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
            if child.poll() is not None:
                if finished_at is None:
                    finished_at = time.monotonic()
                if time.monotonic() - finished_at >= 2.0:
                    break
        if child.poll() is None:
            child.terminate()
        output, _ = child.communicate(timeout=5.0)
        print(output, end='')
        success = child.returncode == 0

        panel_points = {
            p for p in (node.last_cloud or set())
            if panel_x - 0.16 < p[0] < panel_x + 0.04
            and abs(p[1] - panel_y) < 0.30 and 0.30 < p[2] < 0.68
        }
        distances = {}
        for name, pose in node.button_poses.items():
            distances[name] = min(
                (math.dist(p, pose) for p in panel_points), default=float('inf'))
        before_count = len(node.before_cloud or set())
        after_count = len(node.after_cloud or set())
        new_voxels = len((node.after_cloud or set()) - (node.before_cloud or set()))
        print(f'OctoMap: input_cloud_messages={node.input_messages}, '
              f'map_messages={node.map_messages}, before_voxels={before_count}, '
              f'after_voxels={after_count}, changed_voxels={new_voxels}, '
              f'panel_voxels={len(panel_points)}, '
              f'button_to_occupied_m={distances}')
        if (node.input_messages < 3 or node.map_messages < 2 or
                before_count < 10 or after_count < 10 or new_voxels < 1 or
                len(panel_points) < 10 or set(distances) != set(BUTTONS) or
                any(distance > 0.08 for distance in distances.values())):
            success = False
        print('PASS: live 3D occupancy is linked to named map-frame buttons.' if success
              else 'FAIL: stereo occupancy or semantic linkage did not meet checks.')
        return 0 if success else 2
    finally:
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=5.0)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
