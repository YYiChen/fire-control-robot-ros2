#!/usr/bin/env python3
"""Measure named-button and panel-normal errors during autonomous motion."""

import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from std_msgs.msg import String


BUTTONS = {'mute': -0.12, 'reset': 0.0, 'confirm': 0.12}


class RobustnessVerifier(Node):
    def __init__(self):
        super().__init__('semantic_robustness_verifier')
        self.buttons = {name: [] for name in BUTTONS}
        self.normals = []
        for name in BUTTONS:
            self.create_subscription(
                PoseStamped, f'/semantic_panel/button/{name}/pose',
                lambda msg, label=name: self.on_button(label, msg), 10)
        self.create_subscription(String, '/semantic_panel/status', self.on_status, 10)

    def on_button(self, name, msg):
        if msg.header.frame_id == 'map':
            p = msg.pose.position
            self.buttons[name].append((p.x, p.y, p.z))

    def on_status(self, msg):
        item = json.loads(msg.data)
        marker = item.get('buttons', {}).get('aruco_582')
        if isinstance(marker, dict) and marker.get('normal_map_xyz'):
            self.normals.append(marker['normal_map_xyz'])


def median(values):
    return sorted(values)[len(values) // 2] if values else float('inf')


def main():
    panel_x = float(os.environ['SEMANTIC_PANEL_X'])
    panel_y = float(os.environ['SEMANTIC_PANEL_Y'])
    panel_yaw = float(os.environ['SEMANTIC_PANEL_YAW'])
    rclpy.init()
    node = RobustnessVerifier()
    child = subprocess.Popen(
        [sys.executable, str(Path(__file__).with_name('verify_semantic_autonomous.py'))],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic() + 85.0
        finished_at = None
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
            if child.poll() is not None:
                if finished_at is None:
                    finished_at = time.monotonic()
                if time.monotonic() - finished_at > 0.5:
                    break
        if child.poll() is None:
            child.terminate()
        output, _ = child.communicate(timeout=5.0)
        print(output, end='')
        success = child.returncode == 0
        for name, offset in BUTTONS.items():
            truth = (panel_x - 0.039 * math.cos(panel_yaw) - offset * math.sin(panel_yaw),
                     panel_y - 0.039 * math.sin(panel_yaw) + offset * math.cos(panel_yaw),
                     0.545)
            errors = [math.dist(p, truth) for p in node.buttons[name]]
            final_errors = errors[-10:]
            print(f'{name}: samples={len(errors)}, median_error_m={median(errors):.4f}, '
                  f'final_median_error_m={median(final_errors):.4f}, truth={truth}')
            if len(errors) < 15 or median(errors) > 0.05 or median(final_errors) > 0.06:
                success = False
        true_normal_yaw = math.atan2(-math.sin(panel_yaw), -math.cos(panel_yaw))
        angular_errors = [abs(math.atan2(math.sin(math.atan2(n[1], n[0]) - true_normal_yaw),
                                         math.cos(math.atan2(n[1], n[0]) - true_normal_yaw)))
                          for n in node.normals]
        print(f'panel_normal: samples={len(node.normals)}, '
              f'median_yaw_error_rad={median(angular_errors):.4f}, '
              f'final_median_yaw_error_rad={median(angular_errors[-10:]):.4f}')
        if len(node.normals) < 15 or median(angular_errors) > 0.10 or \
                median(angular_errors[-10:]) > 0.10:
            success = False
        print('PASS: shifted panel detection and autonomous approach met bounds.' if success
              else 'FAIL: panel variation exposed a localization or approach failure.')
        return 0 if success else 2
    finally:
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=5.0)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
