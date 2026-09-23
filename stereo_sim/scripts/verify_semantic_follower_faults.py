#!/usr/bin/env python3
"""Check that simulated path following stops for stale or unsafe inputs."""

import time

import rclpy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Odometry, Path
from sensor_msgs.msg import LaserScan

from semantic_path_follower import SemanticPathFollower


def main():
    rclpy.init()
    node = SemanticPathFollower()
    results = []
    node.publish = lambda state, linear=0.0, angular=0.0, **_kw: results.append(
        (state, linear, angular))
    try:
        grid = OccupancyGrid()
        grid.header.frame_id = 'map'
        grid.info.resolution = 0.05
        grid.info.width = 30
        grid.info.height = 30
        grid.info.origin.position.x = -0.2
        grid.info.origin.position.y = -0.2
        grid.info.origin.orientation.w = 1.0
        grid.data = [0] * 900
        path = Path()
        path.header.frame_id = 'map'
        for x in (0.05, 0.20):
            pose = PoseStamped()
            pose.pose.position.x = x
            path.poses.append(pose)
        odom = Odometry()
        odom.header.frame_id = 'vo_odom'
        odom.child_frame_id = 'stereo_base_link'
        odom.pose.pose.orientation.w = 1.0
        scan = LaserScan()
        scan.angle_min = 0.0
        scan.angle_increment = 6.28 / 360.0
        scan.range_min = 0.12
        scan.range_max = 3.5
        scan.ranges = [1.0] * 360
        node.data = {
            'map': grid, 'path': path, 'odom': odom, 'scan': scan,
            'status': {'state': 'ready', 'target': 'reset',
                       'approach_xy_m': [0.20, 0.0]},
        }
        node.times = {name: time.monotonic() for name in node.data}

        node.step()
        assert results[-1][0] == 'moving' and results[-1][1] > 0

        node.data['status'] = {'state': 'no_known_free_path', 'target': 'reset'}
        node.step()
        assert results[-1] == ('stopped_no_safe_path', 0.0, 0.0)

        node.data['status'] = {'state': 'ready', 'target': 'reset',
                               'approach_xy_m': [0.20, 0.0]}
        node.times['scan'] = time.monotonic() - 2.0
        node.step()
        assert results[-1] == ('stopped_stale_input', 0.0, 0.0)

        node.times['scan'] = time.monotonic()
        scan.ranges[0] = 0.15
        node.step()
        assert results[-1] == ('stopped_obstacle', 0.0, 0.0)

        scan.ranges[0] = 1.0
        node.data['status'] = {'state': 'start_or_goal_not_free', 'target': 'reset'}
        node.step()
        assert results[-1] == ('bootstrap_scanning', 0.025, 0.0)
        scan.ranges[0] = 0.20
        node.step()
        assert results[-1] == ('stopped_bootstrap_limit', 0.0, 0.0)

        print('PASS: unsafe route, stale scan, near obstacle and blocked '
              'bootstrap all produce zero velocity.')
        return 0
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    raise SystemExit(main())
