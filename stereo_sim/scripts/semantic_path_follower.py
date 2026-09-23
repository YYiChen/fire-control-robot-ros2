#!/usr/bin/env python3
"""Conservative simulation-only follower for the named button approach path."""

import json
import math
import time

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import OccupancyGrid, Odometry, Path
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan
from std_msgs.msg import String


def yaw(q):
    return math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                      1.0 - 2.0 * (q.y * q.y + q.z * q.z))


def wrap(angle):
    return math.atan2(math.sin(angle), math.cos(angle))


def clamp(value, limit):
    return max(-limit, min(limit, value))


class SemanticPathFollower(Node):
    def __init__(self):
        super().__init__('semantic_path_follower')
        self.data = {}
        self.times = {}
        self.arrived = False
        self.arrival_goal = None
        self.last_ready_goal = None
        self.last_ready_goal_time = None
        self.started_at = None
        self.bootstrap_origin = None
        self.status_pub = self.create_publisher(String, '/semantic_panel/control_status', 10)
        self.command_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        self.create_subscription(Path, '/semantic_panel/approach_path',
                                 lambda msg: self.remember('path', msg), 10)
        self.create_subscription(String, '/semantic_panel/approach_status',
                                 self.on_status, 10)
        self.create_subscription(OccupancyGrid, '/projected_map',
                                 lambda msg: self.remember('map', msg), 10)
        self.create_subscription(Odometry, '/vo/odom',
                                 lambda msg: self.remember('odom', msg),
                                 qos_profile_sensor_data)
        self.create_subscription(LaserScan, '/scan',
                                 lambda msg: self.remember('scan', msg),
                                 qos_profile_sensor_data)
        self.create_timer(0.1, self.step)
        self.last_status = 0.0

    def remember(self, name, msg):
        self.data[name] = msg
        self.times[name] = time.monotonic()

    def on_status(self, msg):
        try:
            status = json.loads(msg.data)
            if not isinstance(status, dict):
                raise ValueError('planner status must be an object')
            self.remember('status', status)
            goal = status.get('approach_xy_m')
            if (status.get('state') == 'ready' and isinstance(goal, list) and
                    len(goal) == 2 and all(isinstance(value, (int, float)) and
                    math.isfinite(value) for value in goal)):
                self.last_ready_goal = goal
                self.last_ready_goal_time = time.monotonic()
        except (TypeError, ValueError):
            self.data.pop('status', None)

    def publish(self, state, linear=0.0, angular=0.0, **details):
        command = Twist()
        command.linear.x = linear
        command.angular.z = angular
        self.command_pub.publish(command)
        now = time.monotonic()
        if now - self.last_status >= 0.45:
            msg = String()
            msg.data = json.dumps({'state': state, 'linear_mps': round(linear, 4),
                                   'angular_radps': round(angular, 4), **details},
                                  sort_keys=True)
            self.status_pub.publish(msg)
            self.last_status = now

    @staticmethod
    def free(grid, x, y):
        if grid.info.resolution <= 0:
            return False
        col = math.floor((x - grid.info.origin.position.x) / grid.info.resolution)
        row = math.floor((y - grid.info.origin.position.y) / grid.info.resolution)
        return (0 <= col < grid.info.width and 0 <= row < grid.info.height and
                grid.data[row * grid.info.width + col] == 0)

    @staticmethod
    def front_clearance(scan):
        readings = []
        for index, value in enumerate(scan.ranges):
            angle = scan.angle_min + index * scan.angle_increment
            if abs(wrap(angle)) > 0.35:
                continue
            if math.isnan(value) or value < scan.range_min:
                return None
            readings.append(min(value, scan.range_max) if math.isfinite(value)
                            else scan.range_max)
        return min(readings) if len(readings) >= 5 else None

    def step(self):
        if self.arrived:
            self.publish('arrived', approach_xy_m=self.arrival_goal)
            return
        now = time.monotonic()
        limits = {'status': 1.5, 'path': 1.5, 'map': 3.0, 'odom': 0.6,
                  'scan': 0.6}
        missing = [name for name, age in limits.items()
                   if name not in self.data or now - self.times[name] > age]
        if missing:
            self.publish('stopped_stale_input', inputs=missing)
            return
        status = self.data['status']
        path = self.data['path']
        grid = self.data['map']
        odom = self.data['odom']
        scan = self.data['scan']
        p = odom.pose.pose.position
        heading = yaw(odom.pose.pose.orientation)
        variance = odom.pose.covariance[0]
        if (grid.header.frame_id != 'map' or
                odom.header.frame_id != 'vo_odom' or
                odom.child_frame_id != 'stereo_base_link' or
                not all(math.isfinite(value) for value in (p.x, p.y, heading)) or
                not math.isfinite(variance) or variance < 0 or variance > 0.04):
            self.publish('stopped_invalid_pose_or_map')
            return
        clearance = self.front_clearance(scan)
        if clearance is None:
            self.publish('stopped_invalid_scan')
            return
        if (self.last_ready_goal is not None and
                now - self.last_ready_goal_time <= 3.0 and
                math.hypot(p.x - self.last_ready_goal[0],
                           p.y - self.last_ready_goal[1]) <= 0.025):
            self.arrived = True
            self.arrival_goal = [round(value, 4) for value in self.last_ready_goal]
            self.publish('arrived', approach_xy_m=self.arrival_goal)
            return
        if status.get('state') == 'start_or_goal_not_free' and status.get('target') == 'reset':
            if self.bootstrap_origin is None:
                self.bootstrap_origin = (p.x, p.y)
            explored = math.hypot(p.x - self.bootstrap_origin[0],
                                  p.y - self.bootstrap_origin[1])
            if explored < 0.12 and clearance >= 0.38:
                self.publish('bootstrap_scanning', 0.025, 0.0,
                             explored_m=round(explored, 4),
                             front_clearance_m=round(clearance, 3))
            else:
                self.publish('stopped_bootstrap_limit', explored_m=round(explored, 4),
                             front_clearance_m=round(clearance, 3))
            return
        if (status.get('state') != 'ready' or status.get('target') != 'reset' or
                path.header.frame_id != 'map' or
                len(path.poses) < 2):
            self.publish('stopped_no_safe_path')
            return
        goal = status.get('approach_xy_m')
        if not isinstance(goal, list) or len(goal) != 2 or not all(
                isinstance(value, (int, float)) and math.isfinite(value) for value in goal):
            self.publish('stopped_invalid_goal')
            return
        if any(not self.free(grid, pose.pose.position.x, pose.pose.position.y)
               for pose in path.poses):
            self.publish('stopped_path_not_free')
            return
        if clearance < 0.22:
            self.publish('stopped_obstacle', front_clearance_m=round(clearance, 3))
            return
        distance_goal = math.hypot(goal[0] - p.x, goal[1] - p.y)
        if distance_goal <= 0.02:
            self.arrived = True
            self.arrival_goal = [round(goal[0], 4), round(goal[1], 4)]
            self.publish('arrived', approach_xy_m=self.arrival_goal,
                         distance_to_goal_m=round(distance_goal, 4))
            return
        if self.started_at is None:
            self.started_at = now
        if now - self.started_at > 30.0:
            self.publish('stopped_timeout', distance_to_goal_m=round(distance_goal, 4))
            return
        target = path.poses[-1].pose.position
        for pose in path.poses:
            candidate = pose.pose.position
            if math.hypot(candidate.x - p.x, candidate.y - p.y) >= 0.08:
                target = candidate
                break
        error = wrap(math.atan2(target.y - p.y, target.x - p.x) - heading)
        angular = clamp(2.0 * error, 0.30)
        linear = min(0.055, max(0.018, 0.6 * distance_goal))
        if abs(error) > 0.5:
            linear = 0.0
        else:
            linear *= max(0.25, 1.0 - abs(error) / 0.5)
        self.publish('moving', linear, angular,
                     distance_to_goal_m=round(distance_goal, 4))


def main():
    rclpy.init()
    node = SemanticPathFollower()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.publish('stopped_shutdown')
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
