#!/usr/bin/env python3
"""Transform stereo OCR landmarks into VO and RTAB-Map graph coordinates."""

import json
import math

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from rclpy.time import Time
from rtabmap_msgs.msg import Info, MapGraph
from std_msgs.msg import String
from tf2_ros import Buffer, TransformException, TransformListener


LABELS = ('fire', 'fault', 'main_power')


def quaternion_product(a, b):
    return (
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    )


def normalized_quaternion(q):
    values = (q.x, q.y, q.z, q.w)
    norm = math.sqrt(sum(value * value for value in values))
    if (not all(math.isfinite(value) for value in values) or
            not 0.9 <= norm <= 1.1):
        raise ValueError('invalid quaternion')
    return tuple(value / norm for value in values)


def rotate_vector(q, vector):
    point = (vector[0], vector[1], vector[2], 0.0)
    conjugate = (-q[0], -q[1], -q[2], q[3])
    return quaternion_product(quaternion_product(q, point), conjugate)[:3]


def transform_pose(source, transform, target_frame):
    """Apply a Transform or TransformStamped while retaining observation time."""
    value = transform.transform if hasattr(transform, 'transform') else transform
    rotation = normalized_quaternion(value.rotation)
    source_rotation = normalized_quaternion(source.pose.orientation)
    p = source.pose.position
    t = value.translation
    if not all(math.isfinite(component) for component in
               (p.x, p.y, p.z, t.x, t.y, t.z)):
        raise ValueError('nonfinite pose or transform translation')
    rotated = rotate_vector(rotation, (p.x, p.y, p.z))
    output = PoseStamped()
    output.header.stamp = source.header.stamp
    output.header.frame_id = target_frame
    output.pose.position.x = t.x + rotated[0]
    output.pose.position.y = t.y + rotated[1]
    output.pose.position.z = t.z + rotated[2]
    orientation = quaternion_product(rotation, source_rotation)
    norm = math.sqrt(sum(value * value for value in orientation))
    if not math.isfinite(norm) or norm < 1e-9:
        raise ValueError('invalid transformed orientation')
    (output.pose.orientation.x, output.pose.orientation.y,
     output.pose.orientation.z, output.pose.orientation.w) = tuple(
         value / norm for value in orientation)
    return output


def stamp_seconds(stamp):
    return stamp.sec + stamp.nanosec * 1e-9


class SemanticOcrGraphProjector(Node):
    def __init__(self):
        super().__init__('semantic_ocr_graph_projector')
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        self.camera_poses = {key: None for key in LABELS}
        self.vo_poses = {key: None for key in LABELS}
        self.tf_errors = {key: None for key in LABELS}
        self.graph_correction = None
        self.graph_stamp = None
        self.last_graph_signal = None
        self.vo_publishers = {
            key: self.create_publisher(
                PoseStamped, f'/semantic_panel/ocr/{key}/pose', 10)
            for key in LABELS}
        self.graph_publishers = {
            key: self.create_publisher(
                PoseStamped, f'/graph_semantic_panel/ocr/{key}/pose', 10)
            for key in LABELS}
        for key in LABELS:
            self.create_subscription(
                PoseStamped, f'/stereo_panel/{key}/pose',
                lambda msg, label=key: self.on_camera_pose(label, msg), 10)
        self.status_publisher = self.create_publisher(
            String, '/graph_semantic_panel/ocr/status', 10)
        self.create_subscription(MapGraph, '/mapGraph', self.on_graph, 10)
        self.create_subscription(Info, '/info', self.on_info, 10)
        self.create_timer(0.2, self.publish_current)

    def on_info(self, _message):
        self.last_graph_signal = self.get_clock().now().nanoseconds * 1e-9

    def on_graph(self, message):
        if message.header.frame_id != 'graph_map' or not message.poses_id:
            return
        stamp = stamp_seconds(message.header.stamp)
        if stamp <= 0 or (self.graph_stamp is not None and stamp < self.graph_stamp):
            return
        try:
            normalized_quaternion(message.map_to_odom.rotation)
            translation = message.map_to_odom.translation
            if not all(math.isfinite(value) for value in
                       (translation.x, translation.y, translation.z)):
                return
        except ValueError:
            return
        self.graph_correction = message.map_to_odom
        self.graph_stamp = stamp
        self.last_graph_signal = self.get_clock().now().nanoseconds * 1e-9

    def on_camera_pose(self, key, message):
        if (not message.header.frame_id or
                stamp_seconds(message.header.stamp) <= 0):
            self.tf_errors[key] = 'invalid_source_header'
            return
        previous = self.camera_poses[key]
        if (previous is not None and stamp_seconds(message.header.stamp) <
                stamp_seconds(previous.header.stamp)):
            return
        self.camera_poses[key] = message
        self.vo_poses[key] = None
        self.tf_errors[key] = None
        self.try_transform(key)

    def try_transform(self, key):
        source = self.camera_poses[key]
        if source is None:
            return
        try:
            source_time = Time.from_msg(
                source.header.stamp, clock_type=self.get_clock().clock_type)
            transform = self.tf_buffer.lookup_transform(
                'vo_odom', source.header.frame_id, source_time)
            output = transform_pose(source, transform, 'vo_odom')
        except (TransformException, ValueError) as exc:
            self.tf_errors[key] = str(exc)
            return
        current_vo = self.vo_poses[key]
        if (current_vo is not None and
                stamp_seconds(current_vo.header.stamp) ==
                stamp_seconds(output.header.stamp)):
            return
        self.vo_poses[key] = output
        self.tf_errors[key] = None
        self.vo_publishers[key].publish(output)

    def publish_current(self):
        now = self.get_clock().now().nanoseconds * 1e-9
        graph_fresh = (self.graph_correction is not None and
                       self.last_graph_signal is not None and
                       0 <= now - self.last_graph_signal <= 2.0)
        report = {'frame_id': 'graph_map', 'graph_fresh': graph_fresh,
                  'landmarks': {}}
        for key in LABELS:
            if self.vo_poses[key] is None:
                self.try_transform(key)
            source = self.vo_poses[key]
            if source is None:
                report['landmarks'][key] = (
                    'waiting_for_tf' if self.camera_poses[key] is not None
                    else 'unobserved')
                if self.tf_errors[key]:
                    report['landmarks'][key + '_tf_error'] = self.tf_errors[key]
                continue
            age = now - stamp_seconds(source.header.stamp)
            report['landmarks'][key] = 'fresh' if 0 <= age <= 0.7 else 'stored'
            if not graph_fresh:
                continue
            try:
                projected = transform_pose(
                    source, self.graph_correction, 'graph_map')
            except ValueError:
                report['landmarks'][key] = 'invalid_graph_transform'
                continue
            self.graph_publishers[key].publish(projected)
        status = String()
        status.data = json.dumps(report, sort_keys=True)
        self.status_publisher.publish(status)


def main():
    rclpy.init()
    node = SemanticOcrGraphProjector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
