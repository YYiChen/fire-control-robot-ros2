#!/usr/bin/env python3
"""Check A* detour and blocked-path behaviour on a known synthetic map."""

import json
import math
import sys

import numpy as np
import rclpy
from geometry_msgs.msg import Pose, PoseStamped
from nav_msgs.msg import OccupancyGrid

from semantic_approach_planner import ApproachPlanner


class Collector:
    def __init__(self):
        self.messages = []

    def publish(self, message):
        self.messages.append(message)


def make_map(blocked):
    grid = OccupancyGrid()
    grid.header.frame_id = 'map'
    grid.info.width = grid.info.height = 40
    grid.info.resolution = 0.025
    grid.info.origin.position.x = -0.1
    grid.info.origin.position.y = -0.5
    grid.info.origin.orientation.w = 1.0
    cells = np.full((40, 40), -1, dtype=np.int8)
    for row in range(40):
        for col in range(40):
            x = -0.1 + (col + 0.5) * 0.025
            y = -0.5 + (row + 0.5) * 0.025
            if -0.1 <= x <= 0.5 and abs(y) <= 0.3:
                cells[row, col] = 0
            if 0.12 <= x <= 0.18 and abs(y) <= (0.3 if blocked else 0.04):
                cells[row, col] = 100
    grid.data = cells.ravel().tolist()
    return grid


def main():
    rclpy.init()
    planner = ApproachPlanner()
    paths, statuses = Collector(), Collector()
    planner.path_publisher = paths
    planner.status_publisher = statuses
    try:
        button = PoseStamped()
        button.header.frame_id = 'map'
        button.pose.position.x = 0.56
        button.pose.position.z = 0.545
        marker = PoseStamped()
        marker.header.frame_id = 'map'
        marker.pose.orientation.y = -math.sqrt(0.5)
        marker.pose.orientation.w = math.sqrt(0.5)
        rig = Pose()
        rig.orientation.w = 1.0
        planner.remember('button', button)
        planner.remember('marker', marker)
        planner.remember('rig', rig)
        planner.remember('map', make_map(blocked=False))
        planner.plan()
        open_status = json.loads(statuses.messages[-1].data)
        path = paths.messages[-1]
        detour = max((abs(p.pose.position.y) for p in path.poses), default=0.0)
        goal = path.poses[-1].pose.position if path.poses else None
        if (open_status['state'] != 'ready' or len(path.poses) < 5 or
                detour < 0.10 or goal is None or abs(goal.x - 0.31) > 0.05 or
                abs(goal.y) > 0.05):
            print(f'FAIL open map: status={open_status}, waypoints={len(path.poses)}, '
                  f'detour_y={detour:.3f}, goal={goal}')
            return 2
        print(f'PASS open map: {len(path.poses)} waypoints, detour_y={detour:.3f} m, '
              f'goal=({goal.x:.3f}, {goal.y:.3f})')

        planner.remember('map', make_map(blocked=True))
        planner.plan()
        blocked_status = json.loads(statuses.messages[-1].data)
        if blocked_status['state'] == 'ready' or paths.messages[-1].poses:
            print(f'FAIL blocked map: status={blocked_status}')
            return 2
        print(f"PASS blocked map: state={blocked_status['state']}, no path published")
        return 0
    finally:
        planner.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
