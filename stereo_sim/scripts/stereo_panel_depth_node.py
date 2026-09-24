#!/usr/bin/env python3
"""Join panel OCR/LED detections to pixel-aligned stereo point-cloud samples."""

from collections import deque
import json
import math
import os
from pathlib import Path
import struct
import tempfile
import time

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image, PointCloud2, PointField
from std_msgs.msg import String

from evaluate_panel_perception import infer_multipage as infer, marker_homography


LABEL_TOPICS = {
    '火警': 'fire',
    '故障': 'fault',
    '主电工作': 'main_power',
}


def stamp_seconds(stamp):
    return stamp.sec + stamp.nanosec * 1e-9


def median_xyz_patch(cloud, u, v, radius, minimum_points):
    """Read an organized PointCloud2 patch without assuming packed rows."""
    if cloud.height <= 1 or cloud.width <= 1:
        return None, 0, 'unorganized_cloud'
    if cloud.point_step <= 0 or cloud.row_step < cloud.width * cloud.point_step:
        return None, 0, 'invalid_cloud_layout'
    fields = {field.name: field for field in cloud.fields}
    if any(name not in fields for name in ('x', 'y', 'z')):
        return None, 0, 'missing_xyz_fields'
    if any(fields[name].datatype != PointField.FLOAT32
           for name in ('x', 'y', 'z')):
        return None, 0, 'unsupported_xyz_datatype'
    if any(fields[name].offset < 0 or
           fields[name].offset + 4 > cloud.point_step
           for name in ('x', 'y', 'z')):
        return None, 0, 'invalid_xyz_field_offset'
    if len(cloud.data) < cloud.row_step * cloud.height:
        return None, 0, 'short_cloud_buffer'

    centre_u, centre_v = int(round(u)), int(round(v))
    if not 0 <= centre_u < cloud.width or not 0 <= centre_v < cloud.height:
        return None, 0, 'pixel_outside_cloud'
    x0, x1 = max(0, centre_u - radius), min(cloud.width, centre_u + radius + 1)
    y0, y1 = max(0, centre_v - radius), min(cloud.height, centre_v + radius + 1)
    endian = '>' if cloud.is_bigendian else '<'
    raw = memoryview(cloud.data)
    points = []
    for row in range(y0, y1):
        row_offset = row * cloud.row_step
        for col in range(x0, x1):
            point_offset = row_offset + col * cloud.point_step
            xyz = tuple(struct.unpack_from(
                endian + 'f', raw, point_offset + fields[name].offset)[0]
                        for name in ('x', 'y', 'z'))
            if (all(math.isfinite(value) for value in xyz) and
                    0.10 < xyz[2] < 2.50):
                points.append(xyz)
    if len(points) < minimum_points:
        return None, len(points), 'too_few_valid_cloud_points'
    median = np.median(np.asarray(points, dtype=np.float64), axis=0)
    return tuple(float(value) for value in median), len(points), 'ok'


class StereoPanelDepthNode(Node):
    def __init__(self):
        super().__init__('stereo_panel_depth')
        self.declare_parameter('image_topic', '/stereo/left/image_rect')
        self.declare_parameter('cloud_topic', '/stereo/points2')
        self.declare_parameter(
            'tesseract', str(Path.home() / 'stereo_sim_deps/tesseract_overlay/usr/bin/tesseract'))
        self.declare_parameter(
            'tessdata', str(Path.home() /
                             'stereo_sim_deps/tesseract_overlay/usr/share/tesseract-ocr/4.00/tessdata'))
        self.declare_parameter('maximum_pair_delta_sec', 0.025)
        self.declare_parameter('minimum_process_interval_sec', 0.75)
        self.declare_parameter('sample_radius_px', 6)
        self.declare_parameter('minimum_points', 5)
        self.bridge = CvBridge()
        self.images = deque(maxlen=6)
        self.clouds = deque(maxlen=6)
        self.image_arrivals = {}
        self.active_timing = None
        self.last_process_wall = 0.0
        self.poses = {
            key: self.create_publisher(PoseStamped, f'/stereo_panel/{key}/pose', 10)
            for key in LABEL_TOPICS.values()}
        self.status_publisher = self.create_publisher(
            String, '/stereo_panel/observation', 10)
        self.create_subscription(
            Image, self.get_parameter('image_topic').value,
            self.on_image, qos_profile_sensor_data)
        self.create_subscription(
            PointCloud2, self.get_parameter('cloud_topic').value,
            self.on_cloud, qos_profile_sensor_data)

    def on_image(self, msg):
        stamp = (msg.header.stamp.sec, msg.header.stamp.nanosec)
        self.image_arrivals[stamp] = time.monotonic()
        if len(self.image_arrivals) > 120:
            for old_stamp in list(self.image_arrivals)[:-90]:
                self.image_arrivals.pop(old_stamp, None)
        self.images.append(msg)
        self.try_process()

    def on_cloud(self, msg):
        self.clouds.append(msg)
        self.try_process()

    def try_process(self):
        if not self.images or not self.clouds:
            return
        latest = max(stamp_seconds(self.images[-1].header.stamp),
                     stamp_seconds(self.clouds[-1].header.stamp))
        while self.images and stamp_seconds(self.images[0].header.stamp) < latest - 0.15:
            self.images.popleft()
        while self.clouds and stamp_seconds(self.clouds[0].header.stamp) < latest - 0.15:
            self.clouds.popleft()
        if not self.images or not self.clouds:
            return
        choices = [(abs(stamp_seconds(image.header.stamp) -
                        stamp_seconds(cloud.header.stamp)), image_index, cloud_index)
                   for image_index, image in enumerate(self.images)
                   for cloud_index, cloud in enumerate(self.clouds)]
        delta, image_index, cloud_index = min(choices)
        if delta > self.get_parameter('maximum_pair_delta_sec').value:
            return
        if time.monotonic() - self.last_process_wall < \
                self.get_parameter('minimum_process_interval_sec').value:
            return
        image = self.images[image_index]
        cloud = self.clouds[cloud_index]
        del self.images[image_index]
        del self.clouds[cloud_index]
        self.last_process_wall = time.monotonic()
        stamp = (image.header.stamp.sec, image.header.stamp.nanosec)
        self.active_timing = {
            'pair_started_monotonic': self.last_process_wall,
            'image_arrival_monotonic': self.image_arrivals.get(stamp),
            'image_conversion_sec': None,
            'marker_detection_sec': None,
            'ocr_sec': None,
            'point_cloud_association_sec': None,
        }
        self.process_pair(image, cloud, delta)

    def publish_status(self, stamp, frame_id, state, labels=None, **extra):
        report = {'stamp': [stamp.sec, stamp.nanosec], 'frame_id': frame_id,
                  'state': state, 'labels': labels or {}, **extra}
        timing = self.active_timing
        if timing is not None:
            now = time.monotonic()
            report['timings_sec'] = {
                'input_to_publish': round(
                    now - timing['image_arrival_monotonic'], 6)
                if timing['image_arrival_monotonic'] is not None else None,
                'pair_processing': round(
                    now - timing['pair_started_monotonic'], 6),
                'image_conversion': timing['image_conversion_sec'],
                'marker_detection': timing['marker_detection_sec'],
                'ocr': timing['ocr_sec'],
                'point_cloud_association': timing['point_cloud_association_sec'],
            }
        message = String()
        message.data = json.dumps(report, ensure_ascii=False, sort_keys=True)
        self.status_publisher.publish(message)

    def process_pair(self, image_msg, cloud, pair_delta):
        stamp = image_msg.header.stamp
        if (image_msg.width != cloud.width or image_msg.height != cloud.height or
                not cloud.header.frame_id):
            self.publish_status(stamp, cloud.header.frame_id,
                                'image_cloud_geometry_mismatch',
                                image_size=[image_msg.width, image_msg.height],
                                cloud_size=[cloud.width, cloud.height])
            return
        conversion_started = time.monotonic()
        try:
            bgr = self.bridge.imgmsg_to_cv2(image_msg, desired_encoding='bgr8')
        except Exception as exc:  # cv_bridge error types vary across ROS builds.
            if self.active_timing is not None:
                self.active_timing['image_conversion_sec'] = round(
                    time.monotonic() - conversion_started, 6)
            self.publish_status(stamp, cloud.header.frame_id,
                                'image_conversion_failed', error=str(exc))
            return

        if self.active_timing is not None:
            self.active_timing['image_conversion_sec'] = round(
                time.monotonic() - conversion_started, 6)
        marker_started = time.monotonic()
        marker_transform = marker_homography(bgr)
        if self.active_timing is not None:
            self.active_timing['marker_detection_sec'] = round(
                time.monotonic() - marker_started, 6)
        if marker_transform is None:
            self.publish_status(stamp, cloud.header.frame_id, 'marker_not_detected',
                                image_cloud_delta_sec=round(pair_delta, 6))
            return
        temporary_path = None
        ocr_started = time.monotonic()
        try:
            with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as temp:
                temporary_path = Path(temp.name)
            if not cv2.imwrite(str(temporary_path), bgr):
                raise RuntimeError('could not encode rectified camera image')
            _ocr, marker_found, predictions = infer(
                temporary_path, Path(self.get_parameter('tesseract').value),
                Path(self.get_parameter('tessdata').value))
        except Exception as exc:
            if self.active_timing is not None:
                self.active_timing['ocr_sec'] = round(
                    time.monotonic() - ocr_started, 6)
            self.publish_status(stamp, cloud.header.frame_id, 'perception_failed',
                                error=str(exc),
                                image_cloud_delta_sec=round(pair_delta, 6))
            return
        finally:
            if temporary_path is not None:
                temporary_path.unlink(missing_ok=True)
        if self.active_timing is not None:
            self.active_timing['ocr_sec'] = round(
                time.monotonic() - ocr_started, 6)
        if not marker_found:
            self.publish_status(stamp, cloud.header.frame_id, 'marker_not_detected',
                                image_cloud_delta_sec=round(pair_delta, 6))
            return

        try:
            inverse = np.linalg.inv(marker_transform)
        except np.linalg.LinAlgError:
            self.publish_status(stamp, cloud.header.frame_id,
                                'invalid_marker_transform',
                                image_cloud_delta_sec=round(pair_delta, 6))
            return
        if not np.isfinite(inverse).all():
            self.publish_status(stamp, cloud.header.frame_id,
                                'invalid_marker_transform',
                                image_cloud_delta_sec=round(pair_delta, 6))
            return
        labels = {}
        association_started = time.monotonic()
        for chinese_label, topic_key in LABEL_TOPICS.items():
            item = predictions.get(chinese_label)
            if item is None:
                labels[topic_key] = {'state': 'text_or_led_not_detected'}
                continue
            canonical = item.get('led_center_px')
            if canonical is None:
                labels[topic_key] = {
                    'text': chinese_label,
                    'led_state': item['led_state'],
                    'state': 'led_center_not_detected'}
                continue
            point = cv2.perspectiveTransform(
                np.asarray([[canonical]], dtype=np.float32), inverse)[0, 0]
            xyz, valid_count, sample_state = median_xyz_patch(
                cloud, float(point[0]), float(point[1]),
                int(self.get_parameter('sample_radius_px').value),
                int(self.get_parameter('minimum_points').value))
            if xyz is None:
                labels[topic_key] = {
                    'text': chinese_label, 'led_state': item['led_state'],
                    'pixel_rectified': [round(float(point[0]), 2),
                                        round(float(point[1]), 2)],
                    'valid_points': valid_count, 'state': sample_state}
                continue
            pose = PoseStamped()
            pose.header.stamp = stamp
            pose.header.frame_id = cloud.header.frame_id
            pose.pose.position.x, pose.pose.position.y, pose.pose.position.z = xyz
            pose.pose.orientation.w = 1.0
            self.poses[topic_key].publish(pose)
            labels[topic_key] = {
                'text': chinese_label,
                'led_state': item['led_state'],
                'pixel_rectified': [round(float(point[0]), 2),
                                    round(float(point[1]), 2)],
                'valid_points': valid_count,
                'xyz_m': [round(value, 5) for value in xyz],
                'depth_m': round(xyz[2], 5),
                'state': 'ok'}
        state = ('ok' if all(labels.get(key, {}).get('state') == 'ok'
                             for key in LABEL_TOPICS.values()) else 'partial')
        if self.active_timing is not None:
            self.active_timing['point_cloud_association_sec'] = round(
                time.monotonic() - association_started, 6)
        self.publish_status(
            stamp, cloud.header.frame_id, state, labels,
            image_cloud_delta_sec=round(pair_delta, 6),
            cloud_point_step=cloud.point_step,
            cloud_row_step=cloud.row_step)


def main():
    rclpy.init()
    node = StereoPanelDepthNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
