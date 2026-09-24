#!/usr/bin/env python3
"""Compare live stereo semantic XYZ output with synthetic panel geometry."""

import argparse
import json
import math
from pathlib import Path
import time

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from std_msgs.msg import String


LABEL_KEYS = {'火警': 'fire', '故障': 'fault', '主电工作': 'main_power'}
PANEL_WIDTH_PX = 960
PANEL_HEIGHT_PX = 540
PANEL_SURFACE_DEPTH_M = 0.888
MAX_POSITION_ERROR_M = 0.02


def stamp_tuple(stamp):
    return stamp.sec, stamp.nanosec


class DepthVerifier(Node):
    def __init__(self):
        super().__init__('stereo_panel_depth_verifier')
        self.reports = []
        self.poses = {key: [] for key in LABEL_KEYS.values()}
        self.create_subscription(String, '/stereo_panel/observation',
                                 self.on_report, 10)
        for key in LABEL_KEYS.values():
            self.create_subscription(
                PoseStamped, f'/stereo_panel/{key}/pose',
                lambda msg, label=key: self.on_pose(label, msg), 10)

    def on_report(self, message):
        try:
            self.reports.append(json.loads(message.data))
        except json.JSONDecodeError:
            return

    def on_pose(self, key, message):
        position = message.pose.position
        self.poses[key].append({
            'stamp': stamp_tuple(message.header.stamp),
            'frame_id': message.header.frame_id,
            'xyz_m': [position.x, position.y, position.z]})


def expected_camera_xyz(truth):
    u, v = truth['led_center_px']
    return [(float(u) - PANEL_WIDTH_PX / 2.0) / 1000.0,
            (float(v) - PANEL_HEIGHT_PX / 2.0) / 1000.0,
            PANEL_SURFACE_DEPTH_M]


def match_poses(node, report):
    stamp = tuple(report['stamp'])
    matched = {}
    for key in LABEL_KEYS.values():
        candidates = [item for item in node.poses[key]
                      if item['stamp'] == stamp]
        if not candidates:
            return None
        matched[key] = candidates[-1]
    return matched


def score_report(node, report, generation):
    truth = {item['label']: item for item in generation['truth']}
    detections = report.get('labels', {})
    poses = match_poses(node, report)
    if poses is None:
        return None
    if report.get('state') != 'ok':
        return None
    if report.get('frame_id') != 'stereo_left_camera_optical_frame':
        raise RuntimeError(f'unexpected point-cloud frame: {report.get("frame_id")}')
    if float(report.get('image_cloud_delta_sec', math.inf)) > 0.025:
        raise RuntimeError('image and point-cloud timestamps are not aligned')

    per_label = {}
    for label, key in LABEL_KEYS.items():
        item = detections.get(key, {})
        expected_state = truth[label]['led_state']
        if item.get('state') != 'ok' or item.get('led_state') != expected_state:
            raise RuntimeError(
                f'{label}: perception/depth status {item.get("state")!r}, '
                f'LED {item.get("led_state")!r}, expected {expected_state!r}')
        valid_points = int(item.get('valid_points', 0))
        if valid_points < 5:
            raise RuntimeError(f'{label}: only {valid_points} valid point-cloud samples')
        pose = poses[key]
        if pose['frame_id'] != report['frame_id']:
            raise RuntimeError(f'{label}: pose frame differs from point-cloud frame')
        observed = pose['xyz_m']
        expected = expected_camera_xyz(truth[label])
        error = math.dist(observed, expected)
        per_label[key] = {
            'text': label,
            'led_state': item['led_state'],
            'pixel_rectified': item['pixel_rectified'],
            'valid_points': valid_points,
            'expected_camera_xyz_m': [round(value, 5) for value in expected],
            'observed_camera_xyz_m': [round(value, 5) for value in observed],
            'position_error_m': round(error, 5),
            'pose_matches_status_stamp': True}
    maximum_error = max(item['position_error_m'] for item in per_label.values())
    result = {
        'case': generation['case'],
        'domain': generation['domain'],
        'source_image_topic': '/stereo/left/image_rect',
        'source_point_cloud_topic': '/stereo/points2',
        'stamp': report['stamp'],
        'frame_id': report['frame_id'],
        'image_cloud_delta_sec': report['image_cloud_delta_sec'],
        'labels': per_label,
        'label_count': len(per_label),
        'max_position_error_m': maximum_error,
        'acceptance_threshold_m': MAX_POSITION_ERROR_M,
        'pass': maximum_error <= MAX_POSITION_ERROR_M}
    if maximum_error > MAX_POSITION_ERROR_M:
        raise RuntimeError(
            f'maximum XYZ error {maximum_error:.3f} m exceeds '
            f'{MAX_POSITION_ERROR_M:.3f} m')
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=25.0)
    args = parser.parse_args()
    generation = json.loads((args.output / 'generation.json').read_text(
        encoding='utf-8'))
    rclpy.init()
    node = DepthVerifier()
    deadline = time.monotonic() + args.timeout
    latest_failure = None
    result = None
    try:
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.20)
            for report in reversed(node.reports):
                try:
                    scored = score_report(node, report, generation)
                    if scored is not None:
                        result = scored
                        break
                except RuntimeError as exc:
                    latest_failure = str(exc)
            if result is not None:
                break
    finally:
        node.destroy_node()
        rclpy.shutdown()
    if result is None:
        result = {
            'case': generation['case'], 'domain': generation['domain'],
            'pass': False, 'reports_seen': len(node.reports),
            'latest_failure': latest_failure or 'no complete synchronized observation'}
    output_path = args.output / 'stereo_panel_depth_report.json'
    output_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                           encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if not result['pass']:
        return 2
    print('PASS: OCR labels and LED states are associated with stereo XYZ within 0.02 m.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
