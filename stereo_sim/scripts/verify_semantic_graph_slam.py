#!/usr/bin/env python3
"""Drive an isolated simulated camera loop and measure RTAB-Map graph outputs."""

import math
import sys
import time

import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from nav_msgs.msg import OccupancyGrid, Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from rtabmap_msgs.msg import Info, MapGraph


class GraphAudit(Node):
    def __init__(self):
        super().__init__('semantic_graph_slam_verifier')
        self.truth = []
        self.vo = []
        self.info = []
        self.graph = []
        self.graph_link_types = []
        self.maps = []
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        self.create_subscription(Odometry, '/vo/odom', self.on_odom,
                                 qos_profile_sensor_data)
        self.create_subscription(Info, '/info', self.on_info, 10)
        self.create_subscription(MapGraph, '/mapGraph', self.on_graph, 10)
        self.create_subscription(OccupancyGrid, '/map', self.on_map, 10)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')

    def on_truth(self, msg):
        if 'stereo_rig' in msg.name:
            p = msg.pose[msg.name.index('stereo_rig')].position
            self.truth.append((time.monotonic(), p.x, p.y))

    def on_odom(self, msg):
        if msg.header.frame_id == 'vo_odom':
            p = msg.pose.pose.position
            self.vo.append((time.monotonic(), p.x, p.y))

    def on_info(self, msg):
        self.info.append((time.monotonic(), msg.ref_id, msg.loop_closure_id,
                          msg.proximity_detection_id))

    def on_graph(self, msg):
        t = msg.map_to_odom.translation
        q = msg.map_to_odom.rotation
        self.graph.append((time.monotonic(), len(msg.poses_id), len(msg.links),
                           msg.header.frame_id, t.x, t.y, q.z, q.w))
        self.graph_link_types.append([link.type for link in msg.links])

    def on_map(self, msg):
        self.maps.append((time.monotonic(), msg.info.width, msg.info.height,
                          msg.header.frame_id))

    def observe(self, seconds):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            rclpy.spin_once(self, timeout_sec=0.1)

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


def main():
    rclpy.init()
    node = GraphAudit()
    try:
        if not node.move_client.wait_for_service(timeout_sec=10.0):
            print('FAIL: Gazebo state service unavailable')
            return 2
        node.observe(10.0)
        first_truth = node.truth[-1] if node.truth else None
        first_vo = node.vo[-1] if node.vo else None
        # Closed rectangle, 5 mm incremental moves: revisit the original view
        # after more than the 5-node short-term memory configured in the graph.
        vertices = [(0.15, 0.0), (0.15, 0.08), (0.0, 0.08), (0.0, 0.0)]
        current = (0.0, 0.0)
        for destination in vertices:
            steps = max(1, math.ceil(math.dist(current, destination) / 0.005))
            for index in range(1, steps + 1):
                fraction = index / steps
                point = tuple(current[axis] + fraction *
                              (destination[axis] - current[axis]) for axis in (0, 1))
                if not node.move(*point):
                    print(f'FAIL: Gazebo rejected camera pose {point}')
                    return 2
                node.observe(0.32)
            current = destination
            print(f'vertex={destination}, vo={len(node.vo)}, graph={len(node.graph)}, '
                  f'info={len(node.info)}, map={len(node.maps)}', flush=True)
        node.observe(6.0)
        final_truth = node.truth[-1] if node.truth else None
        final_vo = node.vo[-1] if node.vo else None
        loop_ids = sorted({item[2] for item in node.info if item[2] > 0})
        graph_nodes = max((item[1] for item in node.graph), default=0)
        graph_links = max((item[2] for item in node.graph), default=0)
        link_types = (node.graph_link_types[-1]
                      if node.graph_link_types else [])
        link_type_counts = {kind: link_types.count(kind) for kind in set(link_types)}
        proximity_ids = sorted({item[3] for item in node.info if item[3] > 0})
        nonempty_maps = [item for item in node.maps if item[1] and item[2]]
        truth_closure = (math.dist(first_truth[1:], final_truth[1:])
                         if first_truth and final_truth else float('inf'))
        vo_closure = (math.dist(first_vo[1:], final_vo[1:])
                      if first_vo and final_vo else float('inf'))
        if node.graph and first_vo and final_vo:
            initial, final = node.graph[0], node.graph[-1]
            def corrected(pose, transform):
                _time, _nodes, _links, _frame, tx, ty, z, w = transform
                heading = 2.0 * math.atan2(z, w)
                return (tx + math.cos(heading) * pose[1] - math.sin(heading) * pose[2],
                        ty + math.sin(heading) * pose[1] + math.cos(heading) * pose[2])
            graph_closure = math.dist(corrected(first_vo, initial),
                                      corrected(final_vo, final))
        else:
            graph_closure = float('inf')
        print(f'graph_audit: truth_closure_m={truth_closure:.4f}, '
              f'vo_closure_m={vo_closure:.4f}, graph_closure_m={graph_closure:.4f}, '
              f'vo_msgs={len(node.vo)}, '
              f'info_msgs={len(node.info)}, graph_msgs={len(node.graph)}, '
              f'graph_nodes={graph_nodes}, graph_links={graph_links}, '
              f'loop_ids={loop_ids}, proximity_ids={proximity_ids}, '
              f'link_types={link_type_counts}, '
              f'map_msgs={len(nonempty_maps)}, '
              f'last_graph={node.graph[-1] if node.graph else None}, '
              f'last_map={nonempty_maps[-1] if nonempty_maps else None}')
        success = (truth_closure < 0.02 and len(node.vo) >= 40 and
                   len(node.info) >= 8 and graph_nodes >= 8 and
                   graph_links >= graph_nodes - 1 and len(nonempty_maps) >= 1 and
                   bool(loop_ids) and graph_closure <= 0.02)
        print('PASS: stereo graph built an occupancy map and observed a loop closure.'
              if success else 'INCOMPLETE: graph, map or loop closure evidence missing.')
        return 0 if success else 2
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    sys.exit(main())
