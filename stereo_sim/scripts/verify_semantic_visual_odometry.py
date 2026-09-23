#!/usr/bin/env python3
"""Audit VO-driven semantic positions and occupancy against test-only truth."""

import json
import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Odometry, Path
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2
from std_msgs.msg import String
from tf2_ros import Buffer, TransformListener


BUTTONS = {'mute': -0.12, 'reset': 0.0, 'confirm': 0.12}


class Verifier(Node):
    def __init__(self):
        super().__init__('semantic_visual_odometry_verifier')
        self.phase = 'initial'
        self.buttons = {phase: {name: [] for name in BUTTONS}
                        for phase in ('initial', 'moved')}
        self.statuses = []
        self.approach = []
        self.odom = []
        self.truth = []
        self.map_free = []
        self.map_times = []
        self.last_map = None
        self.last_path = None
        self.cloud_times = []
        self.transform_ok = 0
        self.transform_bad = 0
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        for name in BUTTONS:
            self.create_subscription(
                PoseStamped, f'/semantic_panel/button/{name}/pose',
                lambda msg, label=name: self.on_button(label, msg), 10)
        self.create_subscription(String, '/semantic_panel/status',
                                 lambda msg: self.statuses.append(json.loads(msg.data)), 10)
        self.create_subscription(String, '/semantic_panel/approach_status',
                                 lambda msg: self.approach.append(json.loads(msg.data)), 10)
        self.create_subscription(OccupancyGrid, '/projected_map', self.on_map, 10)
        self.create_subscription(Path, '/semantic_panel/approach_path', self.on_path, 10)
        self.create_subscription(PointCloud2, '/stereo/points2',
                                 self.on_cloud, 10)
        self.create_subscription(Odometry, '/vo/odom', self.on_odom, qos_profile_sensor_data)
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_button(self, name, msg):
        p = msg.pose.position
        self.buttons[self.phase][name].append((p.x, p.y, p.z, msg.header.frame_id))

    def on_map(self, msg):
        self.last_map = msg
        self.map_free.append(sum(value == 0 for value in msg.data))
        self.map_times.append(time.monotonic())

    def on_cloud(self, msg):
        self.cloud_times.append(time.monotonic())
        if len(self.cloud_times) % 10 == 0:
            if self.tf_buffer.can_transform('map', msg.header.frame_id,
                                            rclpy.time.Time.from_msg(msg.header.stamp)):
                self.transform_ok += 1
            else:
                self.transform_bad += 1

    def on_path(self, msg):
        self.last_path = msg

    def on_odom(self, msg):
        p = msg.pose.pose.position
        self.odom.append((p.x, p.y, p.z))

    def on_truth(self, msg):
        if 'stereo_rig' in msg.name:
            p = msg.pose[msg.name.index('stereo_rig')].position
            self.truth.append((p.x, p.y, p.z))

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.2)

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


def main():
    rclpy.init()
    node = Verifier()
    try:
        if not node.move_client.wait_for_service(timeout_sec=10.0):
            print('FAIL: Gazebo state service unavailable')
            return 2
        node.observe(8.0)
        before_vo = node.odom[-1] if node.odom else None
        before_truth = node.truth[-1] if node.truth else None
        before_free = max(node.map_free, default=0)
        print(f'initial: vo={len(node.odom)}, map_free={before_free}, '
              f'buttons={[(name, len(node.buttons["initial"][name])) for name in BUTTONS]}, '
              f'state={node.statuses[-1] if node.statuses else None}')
        node.phase = 'moved'
        for step in range(1, 21):
            if not node.move(step * 0.005, 0.0):
                print(f'FAIL: x move {step}')
                return 2
            node.observe(0.4)
            if step % 5 == 0:
                print(f'x={step * 0.005:.3f}: map_msgs={len(node.map_times)}, '
                      f'free={node.map_free[-1] if node.map_free else 0}, '
                      f'odom={len(node.odom)}, cloud={len(node.cloud_times)}')
        for step in range(1, 11):
            if not node.move(0.10, step * 0.005):
                print(f'FAIL: y move {step}')
                return 2
            node.observe(0.4)
            if step % 5 == 0:
                print(f'y={step * 0.005:.3f}: map_msgs={len(node.map_times)}, '
                      f'free={node.map_free[-1] if node.map_free else 0}, '
                      f'odom={len(node.odom)}, cloud={len(node.cloud_times)}')
        node.observe(4.0)
        after_vo = node.odom[-1] if node.odom else None
        after_truth = node.truth[-1] if node.truth else None
        after_free = max(node.map_free, default=0)
        vo_error = (math.dist(tuple(after_vo[i] - before_vo[i] for i in range(3)),
                              tuple(after_truth[i] - before_truth[i] for i in range(3)))
                    if all(x is not None for x in (before_vo, after_vo, before_truth, after_truth))
                    else float('inf'))
        all_valid = True
        for name, offset in BUTTONS.items():
            truth = (0.561, offset, 0.545)
            results = []
            for phase in ('initial', 'moved'):
                values = node.buttons[phase][name]
                errors = [math.dist(sample[:3], truth) for sample in values
                          if sample[3] == 'map']
                results.append((len(values), median(errors)))
                if len(values) < 3 or median(errors) > 0.06:
                    all_valid = False
            print(f'{name}: initial/moved=(count, median_error_m) {results}')
        states = sorted({item['state'] for item in node.approach})
        now = time.monotonic()
        map_age = now - node.map_times[-1] if node.map_times else float('inf')
        safe_waypoints = False
        goal_distance = float('inf')
        if node.last_map is not None and node.last_path is not None and node.last_path.poses:
            grid = node.last_map
            safe_waypoints = True
            for waypoint in node.last_path.poses:
                p = waypoint.pose.position
                col = math.floor((p.x - grid.info.origin.position.x) / grid.info.resolution)
                row = math.floor((p.y - grid.info.origin.position.y) / grid.info.resolution)
                if not (0 <= col < grid.info.width and 0 <= row < grid.info.height and
                        grid.data[row * grid.info.width + col] == 0):
                    safe_waypoints = False
                    break
            reset = node.buttons['moved']['reset'][-1]
            p = node.last_path.poses[-1].pose.position
            goal_distance = math.hypot(p.x - reset[0], p.y - reset[1])
        print(f'result: odom_msgs={len(node.odom)}, vector_error_m={vo_error:.4f}, '
              f'free_cells_initial/max={before_free}/{after_free}, '
              f'map_msgs={len(node.map_times)}, map_age_s={map_age:.2f}, '
              f'cloud_msgs={len(node.cloud_times)}, cloud_age_s={now - node.cloud_times[-1] if node.cloud_times else None}, '
              f'tf_cloud_ok/bad={node.transform_ok}/{node.transform_bad}, '
              f'approach_states={states}, last_approach={node.approach[-1] if node.approach else None}, '
              f'path_waypoints={len(node.last_path.poses) if node.last_path else 0}, '
              f'safe_waypoints={safe_waypoints}, goal_distance_m={goal_distance:.3f}')
        success = (all_valid and len(node.odom) >= 20 and vo_error <= 0.025
                   and after_free > before_free and after_free >= 100
                   and map_age <= 3.0 and node.approach and
                   node.approach[-1]['state'] == 'ready' and safe_waypoints and
                   0.18 <= goal_distance <= 0.32)
        print('PASS: stereo VO drives buttons, OctoMap, and approach status.' if success
              else 'FAIL: integrated visual-odometry chain did not meet checks.')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
