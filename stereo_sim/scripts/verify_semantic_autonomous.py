#!/usr/bin/env python3
"""Verify the simulated robot autonomously approaches and stops at reset."""

import json
import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from geometry_msgs.msg import Twist
from nav_msgs.msg import OccupancyGrid, Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from std_msgs.msg import String


class AutonomousVerifier(Node):
    def __init__(self):
        super().__init__('semantic_autonomous_verifier')
        self.truth = []
        self.vo = []
        self.commands = []
        self.control = []
        self.approach = []
        self.semantic = []
        self.map_times = []
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(Odometry, '/vo/odom', self.on_odom,
                                 qos_profile_sensor_data)
        self.create_subscription(Twist, '/cmd_vel', self.on_command, 10)
        self.create_subscription(String, '/semantic_panel/control_status',
                                 lambda msg: self.control.append((time.monotonic(), json.loads(msg.data))), 10)
        self.create_subscription(String, '/semantic_panel/approach_status',
                                 lambda msg: self.approach.append(json.loads(msg.data)), 10)
        self.create_subscription(String, '/semantic_panel/status',
                                 lambda msg: self.semantic.append(json.loads(msg.data)), 10)
        self.create_subscription(OccupancyGrid, '/projected_map',
                                 lambda _msg: self.map_times.append(time.monotonic()), 10)

    def on_truth(self, msg):
        if 'stereo_mobile_bot' in msg.name:
            p = msg.pose[msg.name.index('stereo_mobile_bot')].position
            self.truth.append((time.monotonic(), p.x, p.y, p.z))

    def on_odom(self, msg):
        p = msg.pose.pose.position
        self.vo.append((time.monotonic(), p.x, p.y))

    def on_command(self, msg):
        self.commands.append((time.monotonic(), msg.linear.x, msg.angular.z))

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.1)


def main():
    rclpy.init()
    node = AutonomousVerifier()
    try:
        node.observe(5.0)
        if not node.truth:
            print('FAIL: mobile robot truth not observed')
            return 2
        # The controller may begin its laser-guarded mapping crawl during the
        # five-second warmup, so compare against the first robot observation.
        initial = node.truth[0]
        deadline = time.monotonic() + 60.0
        arrived_time = None
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
            if node.control and node.control[-1][1].get('state') == 'arrived':
                arrived_time = node.control[-1][0]
                break
        if arrived_time is not None:
            node.observe(2.0)
        final = node.truth[-1] if node.truth else initial
        moved = math.hypot(final[1] - initial[1], final[2] - initial[2])
        ready = [item for item in node.approach if item.get('state') == 'ready']
        goal = ready[-1].get('approach_xy_m') if ready else None
        recorded_goal = (next((item[1].get('approach_xy_m') for item in reversed(node.control)
                               if item[1].get('state') == 'arrived'), None))
        vo_goal_distance = (math.hypot(node.vo[-1][1] - goal[0], node.vo[-1][2] - goal[1])
                            if goal is not None and node.vo else float('inf'))
        recorded_goal_distance = (math.hypot(node.vo[-1][1] - recorded_goal[0],
                                             node.vo[-1][2] - recorded_goal[1])
                                  if recorded_goal is not None and node.vo else float('inf'))
        nonzero = sum(abs(v) > 0.005 or abs(w) > 0.01 for _, v, w in node.commands)
        stopped_after = [item for item in node.commands if arrived_time is not None and
                         item[0] >= arrived_time + 0.2]
        stopped = (len(stopped_after) >= 5 and
                   all(abs(v) < 1e-6 and abs(w) < 1e-6 for _, v, w in stopped_after))
        drift_after_stop = (math.hypot(final[1] - sample[1], final[2] - sample[2])
                            if arrived_time is not None and
                            (sample := next((p for p in node.truth if p[0] >= arrived_time), None))
                            else float('inf'))
        map_age = time.monotonic() - node.map_times[-1] if node.map_times else float('inf')
        states = sorted({item[1]['state'] for item in node.control})
        planner_states = sorted({item['state'] for item in node.approach})
        normals = [item['buttons']['aruco_582'].get('normal_map_xyz')
                   for item in node.semantic
                   if isinstance(item.get('buttons', {}).get('aruco_582'), dict) and
                   item['buttons']['aruco_582'].get('normal_map_xyz')]
        print(f'autonomous: states={states}, initial_truth_xy={initial[1:3]}, '
              f'final_truth_xy={final[1:3]}, moved_m={moved:.3f}, '
              f'goal_xy={goal}, recorded_goal_xy={recorded_goal}, '
              f'vo_to_goal_m={vo_goal_distance:.3f}, '
              f'vo_to_recorded_goal_m={recorded_goal_distance:.3f}, '
              f'nonzero_commands={nonzero}, stopped_after_arrival={stopped}, '
              f'drift_after_stop_m={drift_after_stop:.3f}, map_age_s={map_age:.2f}, '
              f'planner_states={planner_states}, '
              f'normal_first_last={normals[0] if normals else None}/{normals[-1] if normals else None}, '
              f'last_control={node.control[-1][1] if node.control else None}')
        success = (arrived_time is not None and 0.18 <= moved <= 0.42 and
                   final[1] < 0.43 and vo_goal_distance <= 0.05 and
                   recorded_goal_distance <= 0.035 and
                   nonzero >= 10 and stopped and drift_after_stop <= 0.02 and
                   map_age <= 3.0)
        print('PASS: simulated robot followed the sensed path and stopped.' if success
              else 'FAIL: autonomous approach did not meet the safety and arrival checks.')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
