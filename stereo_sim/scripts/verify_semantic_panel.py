#!/usr/bin/env python3
"""Check live, named 3D button poses against Gazebo's configured geometry."""

import json
import math
import os
import sys
import time
from collections import Counter

import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from std_msgs.msg import String


BUTTON_OFFSETS = {'mute': -0.12, 'reset': 0.0, 'confirm': 0.12}


class Verifier(Node):
    def __init__(self):
        super().__init__('semantic_panel_verifier')
        self.samples = {name: [] for name in BUTTON_OFFSETS}
        self.phase = 'before_move'
        self.phase_samples = {
            'before_move': {name: [] for name in BUTTON_OFFSETS},
            'after_move': {name: [] for name in BUTTON_OFFSETS},
        }
        self.statuses = {'before_move': [], 'after_move': []}
        self.rig_positions = []
        self.marker_samples = []
        for name in BUTTON_OFFSETS:
            self.create_subscription(
                PoseStamped, f'/semantic_panel/button/{name}/pose',
                lambda msg, button=name: self.on_pose(button, msg), 10)
        self.create_subscription(String, '/semantic_panel/status', self.on_status, 10)
        self.create_subscription(PoseStamped, '/semantic_panel/marker_pose', self.on_marker, 10)
        self.create_subscription(ModelStates, '/model_states', self.on_models, 10)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_pose(self, name, msg):
        if msg.header.frame_id != 'map' or msg.header.stamp.sec == 0:
            return
        p = msg.pose.position
        sample = (p.x, p.y, p.z, msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9)
        self.samples[name].append(sample)
        self.phase_samples[self.phase][name].append(sample)

    def on_status(self, msg):
        self.statuses[self.phase].append(json.loads(msg.data))

    def on_marker(self, msg):
        p = msg.pose.position
        self.marker_samples.append((p.x, p.y, p.z))

    def on_models(self, msg):
        if 'stereo_rig' in msg.name:
            p = msg.pose[msg.name.index('stereo_rig')].position
            self.rig_positions.append((p.x, p.y))

    def move_rig(self):
        if not self.move_client.wait_for_service(timeout_sec=4.0):
            print('FAIL: Gazebo /set_entity_state service is unavailable.')
            return False
        request = SetEntityState.Request()
        request.state.name = 'stereo_rig'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = 0.06
        request.state.pose.position.y = -0.04
        request.state.pose.orientation.z = math.sin(0.08 / 2.0)
        request.state.pose.orientation.w = math.cos(0.08 / 2.0)
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        if not future.done() or future.result() is None or not future.result().success:
            print('FAIL: Gazebo did not accept the camera movement.')
            return False
        return True


def main():
    panel_x = float(os.environ.get('SEMANTIC_PANEL_X', '0.60'))
    panel_y = float(os.environ.get('SEMANTIC_PANEL_Y', '0'))
    panel_yaw = float(os.environ.get('SEMANTIC_PANEL_YAW', '0'))
    rig_x = float(os.environ.get('SEMANTIC_RIG_X', '0'))
    rig_y = float(os.environ.get('SEMANTIC_RIG_Y', '0'))
    rclpy.init()
    node = Verifier()
    move_ok = False
    try:
        deadline = time.monotonic() + 6.0
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
        move_ok = node.move_rig()
        node.phase = 'after_move'
        deadline = time.monotonic() + 6.0
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
    finally:
        node.destroy_node()
        rclpy.shutdown()

    success = move_ok
    for phase, statuses in node.statuses.items():
        state_counts = Counter(status.get('state') for status in statuses)
        reasons = Counter(str(status.get('buttons', {}).get('reset'))
                          for status in statuses if not isinstance(status.get('buttons', {}).get('reset'), dict))
        print(f'{phase}: state_counts={state_counts}, status_reasons={reasons}')
    for name, y_offset in BUTTON_OFFSETS.items():
        measurements = node.samples[name]
        truth = (panel_x - 0.039 * math.cos(panel_yaw) - y_offset * math.sin(panel_yaw),
                 panel_y - 0.039 * math.sin(panel_yaw) + y_offset * math.cos(panel_yaw),
                 0.545)
        errors = [math.dist(sample[:3], truth) for sample in measurements]
        median_error = sorted(errors)[len(errors) // 2] if errors else float('inf')
        pixel_count = sum(name in status.get('buttons', {}) and
                          isinstance(status['buttons'][name], dict)
                          for phase in node.statuses.values() for status in phase)
        moving_in_time = len({round(sample[3], 2) for sample in measurements}) >= 3
        before = node.phase_samples['before_move'][name]
        after = node.phase_samples['after_move'][name]
        before_pixels = [s['buttons'][name]['pixel'] for s in node.statuses['before_move']
                         if isinstance(s.get('buttons', {}).get(name), dict)]
        after_pixels = [s['buttons'][name]['pixel'] for s in node.statuses['after_move']
                        if isinstance(s.get('buttons', {}).get(name), dict)]
        pixel_shift = math.dist(before_pixels[-1], after_pixels[-1]) if before_pixels and after_pixels else 0.0
        residuals = [s['buttons'][name]['marker_residual_m']
                     for phase in node.statuses.values() for s in phase
                     if isinstance(s.get('buttons', {}).get(name), dict) and
                     s['buttons'][name].get('marker_residual_m') is not None]
        marker_residual = sorted(residuals)[len(residuals) // 2] if residuals else float('inf')
        example_prediction = next(
            (s['buttons'][name].get('marker_prediction_xyz_m')
             for s in node.statuses['before_move']
             if isinstance(s.get('buttons', {}).get(name), dict) and
             s['buttons'][name].get('marker_prediction_xyz_m') is not None), None)
        print(f'{name}: poses={len(measurements)}, image_matches={pixel_count}, '
              f'median_3d_error_m={median_error:.3f}, pixel_shift={pixel_shift:.1f}, '
              f'marker_residual_m={marker_residual:.3f}, predicted={example_prediction}, truth={truth}')
        before_errors = [math.dist(sample[:3], truth) for sample in before]
        after_errors = [math.dist(sample[:3], truth) for sample in after]
        max_phase_error = max(
            sorted(before_errors)[len(before_errors) // 2] if before_errors else float('inf'),
            sorted(after_errors)[len(after_errors) // 2] if after_errors else float('inf'))
        if (len(before) < 3 or len(after) < 3 or pixel_count < 3 or
                not moving_in_time or max_phase_error > 0.050 or
                pixel_shift < 3.0 or marker_residual > 0.025):
            success = False
    if not any(abs(x - 0.06) < 0.01 and abs(y + 0.04) < 0.01
               for x, y in node.rig_positions):
        print('FAIL: moved Gazebo rig pose was not observed.')
        success = False
    if node.marker_samples:
        marker_truth = (panel_x - 0.022 * math.cos(panel_yaw),
                        panel_y - 0.022 * math.sin(panel_yaw), 0.425)
        marker_errors = sorted(math.dist(p, marker_truth) for p in node.marker_samples)
        marker_error = marker_errors[len(marker_errors) // 2]
    else:
        marker_error = float('inf')
    print(f'aruco_582: poses={len(node.marker_samples)}, median_3d_error_m={marker_error:.3f}')
    if len(node.marker_samples) < 3 or marker_error > 0.050:
        success = False
    if not success:
        print('FAIL: named button detection, temporal update, or 3D ground-truth check failed.')
        return 2
    print('PASS: all named buttons remain map-anchored while camera image positions change.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
