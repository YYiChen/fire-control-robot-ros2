#!/usr/bin/env python3
"""Reproject observed panel landmarks when RTAB-Map corrects its map-to-odom pose.

Input detections stay in the VO odometry frame. An old observation retains its
capture timestamp when republished, so a controller cannot mistake a graph
correction for a new camera sighting.
"""

import json
import math

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from rtabmap_msgs.msg import Info, MapGraph
from std_msgs.msg import String


NAMES = ('mute', 'reset', 'confirm', 'marker_582')


def stamp_seconds(stamp):
    return stamp.sec + stamp.nanosec * 1e-9


def quaternion_product(a, b):
    return (
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    )


def projected_pose(source, correction):
    """Return T_graph_odom * T_odom_landmark, preserving observation time."""
    q = correction.rotation
    norm = math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)
    if not 0.9 <= norm <= 1.1:
        raise ValueError('invalid graph rotation')
    graph_q = (q.x / norm, q.y / norm, q.z / norm, q.w / norm)
    p = source.pose.position
    point = (p.x, p.y, p.z, 0.0)
    conjugate = (-graph_q[0], -graph_q[1], -graph_q[2], graph_q[3])
    rotated = quaternion_product(quaternion_product(graph_q, point), conjugate)
    if not all(math.isfinite(value) for value in (*rotated[:3],
                                                correction.translation.x,
                                                correction.translation.y,
                                                correction.translation.z)):
        raise ValueError('nonfinite landmark transform')
    output = PoseStamped()
    output.header.stamp = source.header.stamp
    output.header.frame_id = 'graph_map'
    output.pose.position.x = correction.translation.x + rotated[0]
    output.pose.position.y = correction.translation.y + rotated[1]
    output.pose.position.z = correction.translation.z + rotated[2]
    source_q = source.pose.orientation
    orientation = quaternion_product(graph_q, (
        source_q.x, source_q.y, source_q.z, source_q.w))
    (output.pose.orientation.x, output.pose.orientation.y,
     output.pose.orientation.z, output.pose.orientation.w) = orientation
    return output


class GraphSemanticProjector(Node):
    def __init__(self):
        super().__init__('graph_semantic_projector')
        self.raw = {}
        self.correction = None
        self.graph_stamp = None
        self.last_graph_signal = None
        self.landmark_publishers = {}
        for name in NAMES:
            input_topic = ('/semantic_panel/marker_pose' if name == 'marker_582'
                           else f'/semantic_panel/button/{name}/pose')
            output_topic = (f'/graph_semantic_panel/{name}/pose' if name == 'marker_582'
                            else f'/graph_semantic_panel/button/{name}/pose')
            self.landmark_publishers[name] = self.create_publisher(PoseStamped, output_topic, 10)
            self.create_subscription(PoseStamped, input_topic,
                                     lambda msg, key=name: self.on_landmark(key, msg), 10)
        self.status_publisher = self.create_publisher(
            String, '/graph_semantic_panel/status', 10)
        self.create_subscription(MapGraph, '/mapGraph', self.on_graph, 10)
        self.create_subscription(Info, '/info', self.on_info, 10)
        self.create_timer(0.2, self.publish_projected)

    def on_info(self, _msg):
        # No new keyframe while stationary is normal. /info still reports the
        # graph node's processing heartbeat; its absence means no new output.
        self.last_graph_signal = self.get_clock().now().nanoseconds * 1e-9

    def on_landmark(self, name, msg):
        if msg.header.frame_id != 'vo_odom':
            return
        stamp = stamp_seconds(msg.header.stamp)
        if stamp <= 0 or (name in self.raw and
                          stamp < stamp_seconds(self.raw[name].header.stamp)):
            return
        self.raw[name] = msg
        self.publish_projected(name)

    def on_graph(self, msg):
        if msg.header.frame_id != 'graph_map' or not msg.poses_id:
            return
        stamp = stamp_seconds(msg.header.stamp)
        if stamp <= 0 or (self.graph_stamp is not None and stamp < self.graph_stamp):
            return
        self.correction = msg.map_to_odom
        self.graph_stamp = stamp
        self.last_graph_signal = self.get_clock().now().nanoseconds * 1e-9
        self.publish_projected()

    def publish_projected(self, name_filter=None):
        now = self.get_clock().now().nanoseconds * 1e-9
        fresh_graph = (self.correction is not None and self.last_graph_signal is not None and
                       0 <= now - self.last_graph_signal <= 2.0)
        status = {'graph_fresh': fresh_graph, 'frame_id': 'graph_map', 'landmarks': {}}
        for name in NAMES:
            source = self.raw.get(name)
            if source is None:
                status['landmarks'][name] = 'unobserved'
                continue
            age = now - stamp_seconds(source.header.stamp)
            status['landmarks'][name] = ('fresh' if 0 <= age <= 0.7 else 'stored')
            if not fresh_graph or (name_filter is not None and name != name_filter):
                continue
            try:
                output = projected_pose(source, self.correction)
            except ValueError:
                status['landmarks'][name] = 'invalid_transform'
                continue
            # Repeated publication is useful for map correction, but it is
            # never a new camera observation: output keeps the source stamp.
            self.landmark_publishers[name].publish(output)
        report = String()
        report.data = json.dumps(status, sort_keys=True)
        self.status_publisher.publish(report)


def main():
    rclpy.init()
    node = GraphSemanticProjector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
