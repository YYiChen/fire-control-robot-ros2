#!/usr/bin/env python3
"""Sweep a simulated stereo camera and inspect map coverage and safe paths."""

import json
import math
import sys
import time

import numpy as np
import rclpy
from gazebo_msgs.srv import SetEntityState
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Path
from rclpy.node import Node
from std_msgs.msg import String


VIEWS = [
    (0.00, -0.22, 0.35),
    (0.00, -0.15, 0.22),
    (0.00, -0.08, 0.12),
    (0.00, 0.00, 0.00),
    (0.00, 0.08, -0.12),
    (0.00, 0.15, -0.22),
    (0.00, 0.22, -0.35),
    (0.12, 0.00, 0.00),
]


class MultiviewVerifier(Node):
    def __init__(self):
        super().__init__('multiview_approach_verifier')
        self.map = None
        self.statuses = []
        self.paths = []
        self.button = None
        self.create_subscription(OccupancyGrid, '/projected_map', self.on_map, 10)
        self.create_subscription(String, '/semantic_panel/approach_status', self.on_status, 10)
        self.create_subscription(Path, '/semantic_panel/approach_path', self.on_path, 10)
        self.create_subscription(PoseStamped, '/semantic_panel/button/reset/pose', self.on_button, 10)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_map(self, msg):
        self.map = msg

    def on_status(self, msg):
        self.statuses.append(json.loads(msg.data))

    def on_path(self, msg):
        if msg.poses:
            self.paths.append(msg)

    def on_button(self, msg):
        self.button = msg.pose.position

    def move(self, x, y, yaw):
        request = SetEntityState.Request()
        request.state.name = 'stereo_rig'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = x
        request.state.pose.position.y = y
        request.state.pose.orientation.z = math.sin(yaw / 2.0)
        request.state.pose.orientation.w = math.cos(yaw / 2.0)
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        return future.done() and future.result() is not None and future.result().success

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.2)

    def counts(self):
        if self.map is None:
            return 0, 0, 0
        data = np.asarray(self.map.data)
        return (int(np.count_nonzero(data == 0)),
                int(np.count_nonzero(data > 0)),
                int(np.count_nonzero(data < 0)))


def main():
    rclpy.init()
    node = MultiviewVerifier()
    try:
        if not node.move_client.wait_for_service(timeout_sec=8.0):
            print('FAIL: Gazebo set_entity_state unavailable')
            return 2
        node.observe(4.0)
        print(f'initial: free/occupied/unknown={node.counts()}, '
              f'last_state={node.statuses[-1] if node.statuses else None}')
        for index, (x, y, yaw) in enumerate(VIEWS, 1):
            if not node.move(x, y, yaw):
                print(f'FAIL: camera view {index} was not accepted by Gazebo')
                return 2
            node.observe(2.0 if index < len(VIEWS) else 5.0)
            state = node.statuses[-1]['state'] if node.statuses else 'missing'
            print(f'view {index}: x={x:.2f}, y={y:.2f}, yaw={yaw:.2f}, '
                  f'free/occupied/unknown={node.counts()}, '
                  f'state={state}, paths={len(node.paths)}')
        if not node.paths or node.button is None or node.map is None:
            print('INCOMPLETE: multiple views did not yet establish a safe approach path.')
            return 2
        path = node.paths[-1]
        goal = path.poses[-1].pose.position
        distance = math.hypot(goal.x - node.button.x, goal.y - node.button.y)
        grid = node.map
        values = np.asarray(grid.data).reshape(grid.info.height, grid.info.width)
        unsafe = 0
        for waypoint in path.poses:
            p = waypoint.pose.position
            col = math.floor((p.x - grid.info.origin.position.x) / grid.info.resolution)
            row = math.floor((p.y - grid.info.origin.position.y) / grid.info.resolution)
            if not (0 <= col < grid.info.width and 0 <= row < grid.info.height and
                    values[row, col] == 0):
                unsafe += 1
        success = (path.header.frame_id == 'map' and len(path.poses) >= 2 and
                   0.18 < distance < 0.32 and unsafe == 0 and
                   node.statuses[-1]['state'] == 'ready')
        print(f'final: waypoints={len(path.poses)}, goal_to_button_m={distance:.3f}, '
              f'unsafe_waypoints={unsafe}, success={success}')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
