#!/usr/bin/env python3
"""Check graph-frame button landmarks against isolated stereo simulation."""

from collections import defaultdict
import argparse
import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from geometry_msgs.msg import PoseStamped, Transform
from nav_msgs.msg import OccupancyGrid
from rclpy.node import Node
from rtabmap_msgs.msg import MapGraph
from tf2_ros import Buffer, TransformListener

from graph_semantic_projector import projected_pose


BUTTONS = {'mute': -0.12, 'reset': 0.0, 'confirm': 0.12}


def check_projection_math():
    source = PoseStamped()
    source.header.stamp.sec = 7
    source.header.frame_id = 'vo_odom'
    source.pose.position.x = 1.0
    source.pose.position.y = 2.0
    source.pose.orientation.w = 1.0
    transform = Transform()
    transform.translation.x = 3.0
    transform.translation.y = -1.0
    transform.rotation.z = math.sin(math.pi / 4)
    transform.rotation.w = math.cos(math.pi / 4)
    result = projected_pose(source, transform)
    assert result.header.frame_id == 'graph_map' and result.header.stamp.sec == 7
    assert math.isclose(result.pose.position.x, 1.0, abs_tol=1e-6)
    assert math.isclose(result.pose.position.y, 0.0, abs_tol=1e-6)


class Verifier(Node):
    def __init__(self):
        super().__init__('graph_semantic_verifier')
        self.phase = 'initial'
        self.raw = defaultdict(lambda: defaultdict(list))
        self.projected = defaultdict(lambda: defaultdict(list))
        self.truth = []
        self.graph = []
        self.maps = []
        self.last_map = None
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        for name in BUTTONS:
            self.create_subscription(
                PoseStamped, f'/semantic_panel/button/{name}/pose',
                lambda msg, key=name: self.on_pose(self.raw, key, msg), 10)
            self.create_subscription(
                PoseStamped, f'/graph_semantic_panel/button/{name}/pose',
                lambda msg, key=name: self.on_pose(self.projected, key, msg), 10)
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(MapGraph, '/mapGraph', self.on_graph, 10)
        self.create_subscription(OccupancyGrid, '/map', self.on_map, 10)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_pose(self, store, name, msg):
        p = msg.pose.position
        store[self.phase][name].append((p.x, p.y, p.z, msg.header.frame_id,
                                       msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9))

    def on_truth(self, msg):
        if 'stereo_rig' in msg.name:
            p = msg.pose[msg.name.index('stereo_rig')].position
            self.truth.append((p.x, p.y, p.z))

    def on_graph(self, msg):
        self.graph.append((len(msg.poses_id), msg.header.frame_id))

    def on_map(self, msg):
        self.last_map = msg
        self.maps.append((msg.info.width, msg.info.height, msg.header.frame_id))

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.15)

    def move(self, x, y):
        request = SetEntityState.Request()
        request.state.name = 'stereo_rig'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = x
        request.state.pose.position.y = y
        request.state.pose.orientation.w = 1.0
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        return future.done() and future.result() is not None and future.result().success


def median(values):
    return sorted(values)[len(values) // 2] if values else float('inf')


def nearest_occupied_xy(grid, x, y):
    if grid is None:
        return float('inf')
    resolution = grid.info.resolution
    origin = grid.info.origin.position
    occupied = (
        (column, row) for row in range(grid.info.height)
        for column in range(grid.info.width)
        if grid.data[row * grid.info.width + column] > 50)
    return min((math.hypot(origin.x + (column + 0.5) * resolution - x,
                           origin.y + (row + 0.5) * resolution - y)
                for column, row in occupied), default=float('inf'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=('base', 'rotated', 'occluded'), default='base')
    parser.add_argument('--panel-x', type=float, default=0.60)
    parser.add_argument('--panel-y', type=float, default=0.0)
    parser.add_argument('--panel-yaw', type=float, default=0.0)
    args = parser.parse_args()
    check_projection_math()
    rclpy.init()
    node = Verifier()
    try:
        if not node.move_client.wait_for_service(timeout_sec=12.0):
            print('FAIL: Gazebo motion service absent')
            return 2
        node.observe(10.0)
        if args.mode == 'occluded':
            raw_count = sum(len(node.raw['initial'][name]) for name in BUTTONS)
            graph_count = sum(len(node.projected['initial'][name]) for name in BUTTONS)
            passed = raw_count == 0 and graph_count == 0
            print(f'occluded: raw_buttons={raw_count}, graph_buttons={graph_count}, '
                  f'graph_msgs={len(node.graph)}')
            print('PASS: no hidden button mapped.' if passed else
                  'FAIL: an occluded button was mapped.')
            return 0 if passed else 2
        node.phase = 'moved'
        for step in range(1, 21):
            if not node.move(step * 0.005, 0.0):
                print(f'FAIL: x step {step}')
                return 2
            node.observe(0.34)
        for step in range(1, 11):
            if not node.move(0.10, step * 0.005):
                print(f'FAIL: y step {step}')
                return 2
            node.observe(0.34)
        node.observe(5.0)
        results = {}
        passed = bool(node.graph and node.maps and
                      max(item[0] for item in node.graph) >= 3 and
                      all(item[2] == 'graph_map' for item in node.maps))
        for phase in ('initial', 'moved'):
            results[phase] = {}
            for name, y in BUTTONS.items():
                samples = node.projected[phase][name]
                raw = node.raw[phase][name]
                truth = (args.panel_x - 0.039 * math.cos(args.panel_yaw) -
                         y * math.sin(args.panel_yaw),
                         args.panel_y - 0.039 * math.sin(args.panel_yaw) +
                         y * math.cos(args.panel_yaw), 0.545)
                recent = ([value for value in samples
                           if raw and value[4] >= max(item[4] for item in raw) - 1.0]
                          if phase == 'moved' else samples)
                error = median([math.dist(value[:3], truth) for value in recent
                                if value[3] == 'graph_map'])
                result = {'raw': len(raw), 'graph': len(samples),
                          'recent_graph': len(recent),
                          'median_truth_error_m': round(error, 4),
                          'raw_frames': sorted({item[3] for item in raw}),
                          'graph_frames': sorted({item[3] for item in samples})}
                if phase == 'moved' and recent:
                    point = recent[-1]
                    result['nearest_map_occupied_m'] = round(
                        nearest_occupied_xy(node.last_map, point[0], point[1]), 4)
                    if result['nearest_map_occupied_m'] > 0.10:
                        passed = False
                results[phase][name] = result
                if (len(raw) < 3 or len(recent) < 3 or error > 0.06 or
                        result['raw_frames'] != ['vo_odom'] or
                        result['graph_frames'] != ['graph_map']):
                    passed = False
        tf_ok = node.tf_buffer.can_transform('graph_map', 'stereo_base_link',
                                             rclpy.time.Time())
        passed = passed and tf_ok
        print(f'graph_semantic: {results}; graph_msgs={len(node.graph)}, '
              f'graph_nodes={max((item[0] for item in node.graph), default=0)}, '
              f'map_msgs={len(node.maps)}, tf_ok={tf_ok}, '
              f'truth_final={node.truth[-1] if node.truth else None}')
        print('PASS: button detections reproject into graph coordinates.' if passed else
              'INCOMPLETE: graph semantic coordinate evidence missing.')
        return 0 if passed else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
