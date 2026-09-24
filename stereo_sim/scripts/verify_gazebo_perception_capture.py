#!/usr/bin/env python3
"""Capture a Gazebo ROS camera frame and audit the panel perception result."""

import argparse
import json
import math
from pathlib import Path
import time

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image

from evaluate_panel_perception import infer, marker_homography


class Capture(Node):
    def __init__(self, topic):
        super().__init__('gazebo_perception_capture')
        self.bridge = CvBridge()
        self.frames_seen = 0
        self.best = None
        self.create_subscription(Image, topic, self.on_image,
                                 qos_profile_sensor_data)

    def on_image(self, msg):
        self.frames_seen += 1
        if self.frames_seen % 10 != 0:
            return
        bgr = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        if bgr.size == 0 or msg.header.stamp.sec == 0 and msg.header.stamp.nanosec == 0:
            return
        marker = marker_homography(bgr) is not None
        candidate = {'image': bgr.copy(), 'marker_found': marker,
                     'stamp': [msg.header.stamp.sec, msg.header.stamp.nanosec],
                     'frame_id': msg.header.frame_id,
                     'encoding': msg.encoding}
        if self.best is None or marker or not self.best['marker_found']:
            self.best = candidate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/stereo/stereo_rig/left/image_raw')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tesseract', type=Path, required=True)
    parser.add_argument('--tessdata', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=28)
    args = parser.parse_args()
    rclpy.init()
    node = Capture(args.topic)
    end = time.monotonic() + args.timeout
    try:
        while time.monotonic() < end:
            rclpy.spin_once(node, timeout_sec=0.25)
            if node.best is not None and node.best['marker_found']:
                break
        if node.best is None:
            raise RuntimeError(f'no nonzero-stamp camera frame on {args.topic}')
        args.output.mkdir(parents=True, exist_ok=True)
        image_path = args.output / 'gazebo_left_raw.png'
        cv2.imwrite(str(image_path), node.best['image'])
        ocr, marker_found, predictions = infer(
            image_path, args.tesseract, args.tessdata)
        generation = json.loads((args.output / 'generation.json').read_text(
            encoding='utf-8'))
        truth = {item['label']: item for item in generation['truth']}
        visible = {label: item for label, item in truth.items()
                   if item.get('visible_text', True)}
        recognized = set(predictions).intersection(visible)
        scored_leds = [label for label, item in visible.items()
                       if item.get('visible_led', True)]
        correct_leds = [label for label in scored_leds
                        if label in predictions and
                        predictions[label]['led_state'] == truth[label]['led_state']]
        position_errors = {
            label: round(math.dist(predictions[label]['marker_local_xyz_m'],
                                   truth[label]['marker_local_xyz_m']), 5)
            for label in visible
            if label in predictions and
            predictions[label]['marker_local_xyz_m'] is not None}
        metrics = {
            'truth_domain': generation['domain'],
            'visible_labels': sorted(visible),
            'recognized_labels': sorted(recognized),
            'text_recall': round(len(recognized) / max(len(visible), 1), 4),
            'led_state_accuracy': round(
                len(correct_leds) / max(len(scored_leds), 1), 4),
            'position_errors_m': position_errors,
            'max_marker_local_error_m': (max(position_errors.values())
                                         if position_errors else None)}
        frame = node.best['image']
        result = {'source_topic': args.topic, 'stamp': node.best['stamp'],
                  'frame_id': node.best['frame_id'],
                  'encoding': node.best['encoding'],
                  'width': int(frame.shape[1]), 'height': int(frame.shape[0]),
                  'pixel_std': round(float(np.std(frame)), 2),
                  'frames_seen': node.frames_seen,
                  'marker_found': marker_found,
                  'ocr_raw': ocr, 'predictions': predictions,
                  'metrics': metrics,
                  'image': str(image_path)}
        (args.output / 'gazebo_capture_report.json').write_text(
            json.dumps(result, ensure_ascii=False, indent=2) + '\n',
            encoding='utf-8')
        print(json.dumps({key: result[key] for key in (
            'source_topic', 'stamp', 'frame_id', 'width', 'height',
            'pixel_std', 'frames_seen', 'marker_found')},
            ensure_ascii=False))
        print(f'predictions: {predictions}')
        print(f'metrics: {metrics}')
        max_error = metrics['max_marker_local_error_m']
        if (not marker_found or not visible or recognized != set(visible) or
                correct_leds != scored_leds or set(position_errors) != set(visible) or
                max_error is None or max_error > 0.02):
            raise RuntimeError(
                'Gazebo frame failed text, LED, or marker-local position checks; see saved report and image')
        print('PASS: Gazebo camera frame matches synthetic text, LED, and position truth within 0.02 m.')
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
