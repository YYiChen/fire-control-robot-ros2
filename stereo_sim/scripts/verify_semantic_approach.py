#!/usr/bin/env python3
"""Verify that narrow stereo coverage does not become an unsafe drive path."""

import json
import math
from pathlib import Path as FilePath
import subprocess
import sys
import time

import numpy as np
import rclpy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Path
from rclpy.node import Node
from std_msgs.msg import String


class ApproachVerifier(Node):
    def __init__(self):
        super().__init__('semantic_approach_verifier')
        self.start = time.monotonic()
        self.statuses = []
        self.paths = []
        self.maps = []
        self.button = None
        self.create_subscription(String, '/semantic_panel/approach_status', self.on_status, 10)
        self.create_subscription(Path, '/semantic_panel/approach_path', self.on_path, 10)
        self.create_subscription(OccupancyGrid, '/projected_map', self.on_map, 10)
        self.create_subscription(PoseStamped, '/semantic_panel/button/reset/pose', self.on_button, 10)

    def on_status(self, msg):
        self.statuses.append((time.monotonic() - self.start, json.loads(msg.data)))

    def on_path(self, msg):
        if msg.poses:
            self.paths.append((time.monotonic() - self.start, msg))

    def on_map(self, msg):
        self.maps.append(msg)

    def on_button(self, msg):
        self.button = msg.pose.position


def main():
    rclpy.init()
    node = ApproachVerifier()
    verifier = FilePath(__file__).with_name('verify_semantic_panel.py')
    child = subprocess.Popen([sys.executable, str(verifier)], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic() + 35.0
        finished_at = None
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
            if child.poll() is not None:
                if finished_at is None:
                    finished_at = time.monotonic()
                if time.monotonic() - finished_at > 1.0:
                    break
        if child.poll() is None:
            child.terminate()
        output, _ = child.communicate(timeout=5.0)
        print(output, end='')
        ready = [(t, status) for t, status in node.statuses if status.get('state') == 'ready']
        reasons = {status.get('state') for _, status in node.statuses}
        before = [t for t, _ in ready if t < 6.0]
        after = [t for t, _ in ready if t >= 6.0]
        latest = node.paths[-1][1] if node.paths else None
        map_extent = None
        if node.maps:
            grid = node.maps[-1]
            values = np.asarray(grid.data)
            map_extent = (grid.info.width, grid.info.height, grid.info.resolution,
                          grid.info.origin.position.x, grid.info.origin.position.y,
                          int(np.count_nonzero(values == 0)),
                          int(np.count_nonzero(values > 0)))
        print(f'Approach: states={sorted(reasons)}, last_status={node.statuses[-1][1] if node.statuses else None}, '
              f'map_extent_and_free_occupied={map_extent}, ready_before={len(before)}, '
              f'ready_after={len(after)}, nonempty_paths={len(node.paths)}, '
              f'last_waypoints={len(latest.poses) if latest else 0}')
        explicit_refusals = sum(status.get('state') in
                                ('start_or_goal_not_free', 'no_known_free_path')
                                for _, status in node.statuses)
        success = (child.returncode == 0 and node.button is not None and
                   map_extent is not None and map_extent[5] > 10 and
                   explicit_refusals >= 2 and not ready and not node.paths)
        print('PASS: insufficient known free space correctly withheld the approach path.' if success
              else 'FAIL: narrow scene did not produce the expected safe refusal.')
        return 0 if success else 2
    finally:
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=5.0)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
