#!/usr/bin/env python3
"""Suggest a map-checked approach path to a named panel button, without driving."""

import heapq
import json
import math

import cv2
import numpy as np
import rclpy
from gazebo_msgs.msg import ModelStates
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Odometry, Path
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from std_msgs.msg import String


BUTTONS = ('mute', 'reset', 'confirm')


def rotate(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    uv = np.cross((x, y, z), (vx, vy, vz))
    uuv = np.cross((x, y, z), uv)
    return np.array((vx, vy, vz)) + 2.0 * (w * uv + uuv)


def grid_cell(grid, x, y):
    info = grid.info
    return (math.floor((x - info.origin.position.x) / info.resolution),
            math.floor((y - info.origin.position.y) / info.resolution))


def nearest_free(free, cell, limit):
    cx, cy = cell
    candidates = []
    for dy in range(-limit, limit + 1):
        for dx in range(-limit, limit + 1):
            x, y = cx + dx, cy + dy
            if 0 <= y < free.shape[0] and 0 <= x < free.shape[1] and free[y, x]:
                candidates.append((dx * dx + dy * dy, (x, y)))
    return min(candidates)[1] if candidates else None


def astar(free, start, goal):
    height, width = free.shape
    queue = [(0.0, 0.0, start)]
    best = {start: 0.0}
    previous = {}
    moves = ((1, 0), (-1, 0), (0, 1), (0, -1),
             (1, 1), (1, -1), (-1, 1), (-1, -1))
    while queue:
        _, distance, cell = heapq.heappop(queue)
        if distance > best.get(cell, float('inf')):
            continue
        if cell == goal:
            path = [cell]
            while path[-1] != start:
                path.append(previous[path[-1]])
            return list(reversed(path))
        for dx, dy in moves:
            x, y = cell[0] + dx, cell[1] + dy
            if not (0 <= x < width and 0 <= y < height and free[y, x]):
                continue
            if dx and dy and not (free[cell[1], x] and free[y, cell[0]]):
                continue
            candidate = distance + math.hypot(dx, dy)
            neighbour = (x, y)
            if candidate >= best.get(neighbour, float('inf')):
                continue
            best[neighbour] = candidate
            previous[neighbour] = cell
            heuristic = math.hypot(goal[0] - x, goal[1] - y)
            heapq.heappush(queue, (candidate + heuristic, candidate, neighbour))
    return None


class ApproachPlanner(Node):
    def __init__(self):
        super().__init__('semantic_approach_planner')
        self.declare_parameter('target_button', 'reset')
        self.declare_parameter('stand_off_m', 0.25)
        self.declare_parameter('footprint_radius_m', 0.07)
        self.declare_parameter('pose_source', 'gazebo')
        self.pose_source = self.get_parameter('pose_source').value
        if self.pose_source not in ('gazebo', 'odometry'):
            raise ValueError('pose_source must be gazebo or odometry')
        self.target = self.get_parameter('target_button').value
        if self.target not in BUTTONS:
            raise ValueError(f'target_button must be one of {BUTTONS}')
        self.map = None
        self.button = None
        self.marker = None
        self.rig = None
        self.seen = {}
        self.path_publisher = self.create_publisher(Path, '/semantic_panel/approach_path', 10)
        self.status_publisher = self.create_publisher(String, '/semantic_panel/approach_status', 10)
        self.create_subscription(OccupancyGrid, '/projected_map', self.on_map, 10)
        self.create_subscription(PoseStamped, f'/semantic_panel/button/{self.target}/pose',
                                 self.on_button, 10)
        self.create_subscription(PoseStamped, '/semantic_panel/marker_pose', self.on_marker, 10)
        if self.pose_source == 'gazebo':
            self.create_subscription(ModelStates, '/model_states', self.on_models, 10)
        else:
            self.create_subscription(Odometry, '/vo/odom', self.on_odom,
                                     qos_profile_sensor_data)
        self.create_timer(0.5, self.plan)

    def remember(self, name, value):
        setattr(self, name, value)
        self.seen[name] = self.get_clock().now().nanoseconds * 1e-9

    def on_map(self, msg):
        self.remember('map', msg)

    def on_button(self, msg):
        self.remember('button', msg)

    def on_marker(self, msg):
        self.remember('marker', msg)

    def on_models(self, msg):
        if 'stereo_rig' in msg.name:
            self.remember('rig', msg.pose[msg.name.index('stereo_rig')])

    def on_odom(self, msg):
        if msg.header.frame_id == 'vo_odom' and msg.child_frame_id == 'stereo_base_link':
            self.remember('rig', msg.pose.pose)

    def publish(self, state, path=None, **data):
        result = String()
        result.data = json.dumps({'state': state, 'target': self.target, **data}, sort_keys=True)
        self.status_publisher.publish(result)
        if path is None:
            path = Path()
            path.header.frame_id = 'map'
            path.header.stamp = self.get_clock().now().to_msg()
        self.path_publisher.publish(path)

    def plan(self):
        now = self.get_clock().now().nanoseconds * 1e-9
        max_age = {'map': 3.0, 'button': 0.6, 'marker': 0.6, 'rig': 0.6}
        stale = [name for name, limit in max_age.items()
                 if getattr(self, name) is None or now - self.seen.get(name, -1e9) > limit]
        if stale:
            self.publish('missing_or_stale_input', inputs=stale)
            return
        grid = self.map
        if grid.header.frame_id != 'map' or self.button.header.frame_id != 'map' or \
                self.marker.header.frame_id != 'map':
            self.publish('frame_mismatch')
            return
        origin_q = grid.info.origin.orientation
        if (abs(origin_q.x) > 1e-4 or abs(origin_q.y) > 1e-4 or
                abs(origin_q.z) > 1e-4 or abs(origin_q.w - 1.0) > 1e-4):
            self.publish('rotated_grid_not_supported')
            return
        q = self.marker.pose.orientation
        normal = rotate((q.x, q.y, q.z, q.w), (0.0, 0.0, 1.0))[:2]
        norm = np.linalg.norm(normal)
        if norm < 0.9:
            self.publish('invalid_marker_orientation')
            return
        normal /= norm
        button = self.button.pose.position
        stand_off = self.get_parameter('stand_off_m').value
        goal_xy = (button.x + stand_off * normal[0], button.y + stand_off * normal[1])
        start_xy = (self.rig.position.x, self.rig.position.y)
        width, height = grid.info.width, grid.info.height
        if width == 0 or height == 0 or len(grid.data) != width * height:
            self.publish('empty_map')
            return
        occupancy = np.asarray(grid.data, dtype=np.int16).reshape(height, width)
        radius = max(1, math.ceil(self.get_parameter('footprint_radius_m').value /
                                  grid.info.resolution))
        known_obstacle = (occupancy != 0).astype(np.uint8)
        padded = np.pad(known_obstacle, radius, constant_values=1)
        expanded = cv2.dilate(padded,
                              cv2.getStructuringElement(cv2.MORPH_ELLIPSE,
                                                         (2 * radius + 1, 2 * radius + 1)))
        unsafe = expanded[radius:-radius, radius:-radius]
        free = unsafe == 0
        start = nearest_free(free, grid_cell(grid, *start_xy), 3)
        goal = nearest_free(free, grid_cell(grid, *goal_xy), 3)
        if start is None or goal is None:
            self.publish('start_or_goal_not_free', goal_xy_m=list(goal_xy))
            return
        cells = astar(free, start, goal)
        if cells is None:
            self.publish('no_known_free_path', goal_xy_m=list(goal_xy))
            return
        path = Path()
        path.header.frame_id = 'map'
        path.header.stamp = self.get_clock().now().to_msg()
        heading = math.atan2(button.y - goal_xy[1], button.x - goal_xy[0])
        for x, y in cells:
            pose = PoseStamped()
            pose.header = path.header
            pose.pose.position.x = grid.info.origin.position.x + (x + 0.5) * grid.info.resolution
            pose.pose.position.y = grid.info.origin.position.y + (y + 0.5) * grid.info.resolution
            pose.pose.orientation.z = math.sin(heading / 2.0)
            pose.pose.orientation.w = math.cos(heading / 2.0)
            path.poses.append(pose)
        self.publish('ready', path, waypoint_count=len(cells),
                     approach_xy_m=[round(x, 4) for x in goal_xy],
                     button_xyz_m=[round(button.x, 4), round(button.y, 4), round(button.z, 4)])


def main():
    rclpy.init()
    node = ApproachPlanner()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
