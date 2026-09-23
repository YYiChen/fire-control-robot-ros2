#!/usr/bin/env python3
"""Check that a visually hidden panel cannot trigger an approach."""

import json
import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from geometry_msgs.msg import PoseStamped, Twist
from rclpy.node import Node
from std_msgs.msg import String


class OcclusionVerifier(Node):
    def __init__(self):
        super().__init__('semantic_occlusion_verifier')
        self.truth = []
        self.commands = []
        self.states = []
        self.button_count = 0
        self.marker_seen = 0
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(Twist, '/cmd_vel', self.on_command, 10)
        self.create_subscription(String, '/semantic_panel/control_status',
                                 lambda msg: self.states.append(json.loads(msg.data)['state']), 10)
        self.create_subscription(String, '/semantic_panel/status', self.on_semantic, 10)
        for name in ('mute', 'reset', 'confirm'):
            self.create_subscription(PoseStamped, f'/semantic_panel/button/{name}/pose',
                                     self.on_button, 10)

    def on_truth(self, msg):
        if 'stereo_mobile_bot' in msg.name:
            p = msg.pose[msg.name.index('stereo_mobile_bot')].position
            self.truth.append((p.x, p.y))

    def on_command(self, msg):
        self.commands.append((msg.linear.x, msg.angular.z))

    def on_button(self, _msg):
        self.button_count += 1

    def on_semantic(self, msg):
        marker = json.loads(msg.data).get('buttons', {}).get('aruco_582')
        if isinstance(marker, dict):
            self.marker_seen += 1


def main():
    rclpy.init()
    node = OcclusionVerifier()
    try:
        deadline = time.monotonic() + 27.0
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.2)
        moved = (math.dist(node.truth[0], node.truth[-1])
                 if len(node.truth) >= 2 else float('inf'))
        stopped = (len(node.commands) >= 5 and
                   all(abs(v) < 1e-6 and abs(w) < 1e-6
                       for v, w in node.commands[-5:]))
        print(f'occluded: truth_samples={len(node.truth)}, moved_m={moved:.3f}, '
              f'button_poses={node.button_count}, marker_observations={node.marker_seen}, '
              f'control_states={sorted(set(node.states))}, stopped={stopped}')
        success = (len(node.truth) >= 10 and node.button_count == 0 and
                   node.marker_seen == 0 and 'arrived' not in node.states and
                   'moving' not in node.states and moved <= 0.14 and stopped)
        print('PASS: hidden panel caused bounded scanning then stop.' if success
              else 'FAIL: hidden panel caused unsafe or unverified behavior.')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
