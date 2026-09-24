#!/usr/bin/env python3
"""Check camera-to-VO-to-graph OCR points against synthetic world truth."""

from collections import defaultdict
import argparse
import json
import math
from pathlib import Path
import time

from builtin_interfaces.msg import Time as RosTime
import rclpy
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from geometry_msgs.msg import PoseStamped, Transform
from rclpy.node import Node
from rclpy.time import Time
from rtabmap_msgs.msg import MapGraph
from std_msgs.msg import String
from tf2_ros import Buffer, TransformException, TransformListener

from semantic_ocr_graph_projector import (
    normalized_quaternion, quaternion_product, rotate_vector, transform_pose)


LABELS = {'火警': 'fire', '故障': 'fault', '主电工作': 'main_power'}
PANEL_FACE_X = 0.888
PANEL_CENTER_Y = 0.0
PANEL_CENTER_Z = 0.50
WORLD_ERROR_LIMIT_M = 0.06
MAP_DRIFT_LIMIT_M = 0.04


def stamp_tuple(stamp):
    return stamp.sec, stamp.nanosec


def stamp_to_msg(stamp):
    message = RosTime()
    message.sec, message.nanosec = stamp
    return message


def world_truth(truth):
    u, v = truth['led_center_px']
    return (PANEL_FACE_X,
            PANEL_CENTER_Y + 0.480 - float(u) / 1000.0,
            PANEL_CENTER_Z + 0.270 - float(v) / 1000.0)


class Verifier(Node):
    def __init__(self):
        super().__init__('semantic_ocr_graph_verifier')
        self.phase = 'initial'
        self.camera = defaultdict(dict)
        self.vo = defaultdict(dict)
        self.graph = defaultdict(lambda: defaultdict(list))
        self.camera_phase = defaultdict(dict)
        self.reports = []
        self.graph_nodes = []
        self.rig_truth = None
        self.world_to_graph = None
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')
        self.create_subscription(String, '/stereo_panel/observation',
                                 self.on_report, 10)
        self.create_subscription(MapGraph, '/mapGraph', self.on_graph, 10)
        self.create_subscription(ModelStates, '/model_states', self.on_truth, 10)
        for key in LABELS.values():
            self.create_subscription(
                PoseStamped, f'/stereo_panel/{key}/pose',
                lambda msg, label=key: self.on_pose(self.camera, label, msg), 10)
            self.create_subscription(
                PoseStamped, f'/semantic_panel/ocr/{key}/pose',
                lambda msg, label=key: self.on_pose(self.vo, label, msg), 10)
            self.create_subscription(
                PoseStamped, f'/graph_semantic_panel/ocr/{key}/pose',
                lambda msg, label=key: self.on_graph_pose(label, msg), 10)

    def on_pose(self, store, label, message):
        store[label][stamp_tuple(message.header.stamp)] = (
            message.pose.position.x, message.pose.position.y,
            message.pose.position.z, message.header.frame_id)
        if store is self.camera:
            self.camera_phase[label][stamp_tuple(message.header.stamp)] = self.phase

    def on_graph_pose(self, label, message):
        stamp = stamp_tuple(message.header.stamp)
        p = message.pose.position
        self.graph[label][stamp].append(
            (p.x, p.y, p.z, message.header.frame_id))

    def on_report(self, message):
        try:
            self.reports.append(json.loads(message.data))
        except json.JSONDecodeError:
            return

    def on_graph(self, message):
        self.graph_nodes.append(len(message.poses_id))

    def on_truth(self, message):
        if 'stereo_rig' in message.name:
            self.rig_truth = message.pose[message.name.index('stereo_rig')]

    def observe(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.10)

    def move_rig(self, x, y):
        request = SetEntityState.Request()
        request.state.name = 'stereo_rig'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = x
        request.state.pose.position.y = y
        request.state.pose.position.z = 0.0
        request.state.pose.orientation.w = 1.0
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        return future.done() and future.result() is not None and future.result().success


class TfUnavailableProbe(Node):
    def __init__(self):
        super().__init__('semantic_ocr_tf_failure_probe')
        self.publisher = self.create_publisher(
            PoseStamped, '/stereo_panel/fire/pose', 10)
        self.vo_count = 0
        self.graph_count = 0
        self.statuses = []
        self.sent = 0
        self.create_subscription(
            PoseStamped, '/semantic_panel/ocr/fire/pose',
            lambda _message: self.count_vo(), 10)
        self.create_subscription(
            PoseStamped, '/graph_semantic_panel/ocr/fire/pose',
            lambda _message: self.count_graph(), 10)
        self.create_subscription(
            String, '/graph_semantic_panel/ocr/status', self.on_status, 10)
        self.create_timer(0.25, self.publish_invalid_frame_pose)

    def publish_invalid_frame_pose(self):
        if self.sent >= 8:
            return
        message = PoseStamped()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = 'deliberately_missing_camera_frame'
        message.pose.position.x = 0.25
        message.pose.position.y = -0.10
        message.pose.position.z = 0.75
        message.pose.orientation.w = 1.0
        self.publisher.publish(message)
        self.sent += 1

    def count_vo(self):
        self.vo_count += 1

    def count_graph(self):
        self.graph_count += 1

    def on_status(self, message):
        try:
            self.statuses.append(json.loads(message.data))
        except json.JSONDecodeError:
            return


def xyz_median(samples):
    if not samples:
        return None
    return tuple(sorted(sample[axis] for sample in samples)[len(samples) // 2]
                 for axis in range(3))


def graph_world_transform(graph_base, world_base):
    """Compute T_graph_world from T_graph_base and Gazebo's T_world_base."""
    graph_rotation = normalized_quaternion(graph_base.transform.rotation)
    world_rotation = normalized_quaternion(world_base.orientation)
    world_inverse_rotation = (-world_rotation[0], -world_rotation[1],
                              -world_rotation[2], world_rotation[3])
    output_rotation = quaternion_product(graph_rotation, world_inverse_rotation)
    world_position = world_base.position
    base_origin_in_world = rotate_vector(
        world_inverse_rotation,
        (-world_position.x, -world_position.y, -world_position.z))
    rotated_origin = rotate_vector(graph_rotation, base_origin_in_world)
    result = Transform()
    result.translation.x = graph_base.transform.translation.x + rotated_origin[0]
    result.translation.y = graph_base.transform.translation.y + rotated_origin[1]
    result.translation.z = graph_base.transform.translation.z + rotated_origin[2]
    (result.rotation.x, result.rotation.y, result.rotation.z,
     result.rotation.w) = output_rotation
    return result


def aligned_world_truth(node, truth, stamp):
    if node.world_to_graph is None:
        if node.rig_truth is None:
            raise RuntimeError('no Gazebo stereo_rig initial pose was received')
        source_time = Time.from_msg(stamp, clock_type=node.get_clock().clock_type)
        try:
            graph_base = node.tf_buffer.lookup_transform(
                'graph_map', 'stereo_base_link', source_time)
        except TransformException as exc:
            raise RuntimeError(f'cannot align graph_map to SDF world at source time: {exc}')
        node.world_to_graph = graph_world_transform(graph_base, node.rig_truth)
    source = PoseStamped()
    source.header.stamp = stamp
    source.header.frame_id = 'world'
    source.pose.position.x, source.pose.position.y, source.pose.position.z = truth
    source.pose.orientation.w = 1.0
    return transform_pose(source, node.world_to_graph, 'graph_map')


def check_transform_math():
    source = PoseStamped()
    source.header.stamp.sec = 9
    source.header.frame_id = 'camera'
    source.pose.position.x = 1.0
    source.pose.orientation.w = 1.0
    transform = Transform()
    transform.translation.x = 1.0
    transform.translation.y = 2.0
    transform.translation.z = 3.0
    transform.rotation.z = math.sin(math.pi / 4.0)
    transform.rotation.w = math.cos(math.pi / 4.0)
    moved = transform_pose(source, transform, 'target')
    assert moved.header.frame_id == 'target' and moved.header.stamp.sec == 9
    assert math.isclose(moved.pose.position.x, 1.0, abs_tol=1e-6)
    assert math.isclose(moved.pose.position.y, 3.0, abs_tol=1e-6)
    assert math.isclose(moved.pose.position.z, 3.0, abs_tol=1e-6)

    graph_base = type('TransformStampedLike', (), {'transform': transform})()
    world_base = PoseStamped().pose
    world_base.position.x = 0.5
    world_base.position.z = 1.0
    world_base.orientation.w = 1.0
    aligned = graph_world_transform(graph_base, world_base)
    assert math.isclose(aligned.translation.x, 1.0, abs_tol=1e-6)
    assert math.isclose(aligned.translation.y, 1.5, abs_tol=1e-6)
    assert math.isclose(aligned.translation.z, 2.0, abs_tol=1e-6)


def check_base(node, generation):
    truth = {entry['label']: entry for entry in generation['truth']}
    report = next((item for item in reversed(node.reports)
                   if item.get('state') == 'ok'), None)
    if report is None:
        raise RuntimeError('no complete OCR/depth observation arrived')
    if report.get('frame_id') != 'stereo_left_camera_optical_frame':
        raise RuntimeError(f'unexpected camera frame {report.get("frame_id")!r}')
    if report.get('image_cloud_delta_sec', math.inf) > 0.025:
        raise RuntimeError('image and point-cloud timestamps exceed 25 ms')
    observed_stamp = tuple(report['stamp'])
    result = {'case': generation['case'], 'labels': {},
              'source_stamp': observed_stamp, 'failures': []}
    for chinese, key in LABELS.items():
        item = report.get('labels', {}).get(key, {})
        expected_state = truth[chinese]['led_state']
        if item.get('state') != 'ok' or item.get('led_state') != expected_state:
            raise RuntimeError(f'{chinese}: expected LED {expected_state}, got {item}')
        if observed_stamp not in node.camera[key] or observed_stamp not in node.vo[key]:
            raise RuntimeError(f'{key}: source and VO poses did not preserve OCR timestamp')
        graph_candidates = node.graph[key].get(observed_stamp, [])
        if not graph_candidates:
            raise RuntimeError(f'{key}: no graph_map pose at the OCR timestamp')
        camera = node.camera[key][observed_stamp]
        vo = node.vo[key][observed_stamp]
        graph = graph_candidates[-1]
        if camera[3] != report['frame_id'] or vo[3] != 'vo_odom' or graph[3] != 'graph_map':
            raise RuntimeError(f'{key}: unexpected frame chain {camera[3]}, {vo[3]}, {graph[3]}')
        expected_pose = aligned_world_truth(node, world_truth(truth[chinese]),
                                            stamp_to_msg(observed_stamp))
        expected = (expected_pose.pose.position.x, expected_pose.pose.position.y,
                    expected_pose.pose.position.z)
        graph_error = math.dist(graph[:3], expected)
        result['labels'][key] = {
            'led_state': item['led_state'],
            'camera_xyz_m': [round(value, 5) for value in camera[:3]],
            'vo_xyz_m': [round(value, 5) for value in vo[:3]],
            'graph_xyz_m': [round(value, 5) for value in graph[:3]],
            'expected_graph_xyz_m': [round(value, 5) for value in expected],
            'graph_truth_error_m': round(graph_error, 5),
            'graph_truth_delta_m': [round(graph[i] - expected[i], 5)
                                    for i in range(3)],
            'stamp_preserved': True}
        if graph_error > WORLD_ERROR_LIMIT_M:
            result['failures'].append(
                f'{key}: graph truth error {graph_error:.3f} m exceeds '
                f'{WORLD_ERROR_LIMIT_M:.3f} m')
    result['max_graph_truth_error_m'] = max(
        item['graph_truth_error_m'] for item in result['labels'].values())
    alignment = node.world_to_graph
    result['graph_map_to_world_alignment'] = {
        'translation_m': [round(alignment.translation.x, 5),
                          round(alignment.translation.y, 5),
                          round(alignment.translation.z, 5)],
        'rotation_xyzw': [round(alignment.rotation.x, 6),
                          round(alignment.rotation.y, 6),
                          round(alignment.rotation.z, 6),
                          round(alignment.rotation.w, 6)]}
    result['pass'] = not result['failures']
    return result


def check_motion(node, generation):
    truth = {entry['label']: entry for entry in generation['truth']}
    result = {'case': generation['case'], 'labels': {},
              'graph_nodes': max(node.graph_nodes, default=0)}
    final = node.rig_truth
    moved_distance = (math.hypot(final.position.x, final.position.y)
                      if final is not None else 0.0)
    if moved_distance < 0.08:
        raise RuntimeError(f'Gazebo rig moved only {moved_distance:.3f} m')
    if result['graph_nodes'] < 3:
        raise RuntimeError(f'RTAB-Map graph has only {result["graph_nodes"]} nodes')
    errors = []
    failures = []
    for chinese, key in LABELS.items():
        stamps = [stamp for stamp, phase in node.camera_phase[key].items()
                  if phase == 'moved' and stamp in node.graph[key]]
        if not stamps:
            raise RuntimeError(f'{key}: no new graph pose was seen after camera movement')
        stamp = max(stamps)
        graph = node.graph[key][stamp][-1]
        expected_pose = aligned_world_truth(
            node, world_truth(truth[chinese]), stamp_to_msg(stamp))
        expected = (expected_pose.pose.position.x, expected_pose.pose.position.y,
                    expected_pose.pose.position.z)
        error = math.dist(graph[:3], expected)
        initial_stamps = [source_stamp for source_stamp, phase in
                          node.camera_phase[key].items()
                          if phase == 'initial' and source_stamp in node.graph[key]]
        if not initial_stamps:
            raise RuntimeError(f'{key}: no initial graph observation')
        initial_xyz = xyz_median([sample[:3] for source_stamp in initial_stamps
                                  for sample in node.graph[key][source_stamp]])
        drift = math.dist(graph[:3], initial_xyz)
        result['labels'][key] = {
            'new_observation_stamp': stamp,
            'graph_xyz_m': [round(value, 5) for value in graph[:3]],
            'expected_graph_xyz_m': [round(value, 5) for value in expected],
            'graph_truth_error_m': round(error, 5),
            'graph_truth_delta_m': [round(graph[i] - expected[i], 5)
                                    for i in range(3)],
            'initial_to_moved_drift_m': round(drift, 5)}
        errors.append(error)
        if graph[3] != 'graph_map':
            failures.append(f'{key}: output frame is {graph[3]!r}, expected graph_map')
        if error > WORLD_ERROR_LIMIT_M:
            failures.append(f'{key}: moved graph truth error {error:.3f} m exceeds '
                            f'{WORLD_ERROR_LIMIT_M:.3f} m')
        if drift > MAP_DRIFT_LIMIT_M:
            failures.append(f'{key}: initial-to-moved drift {drift:.3f} m exceeds '
                            f'{MAP_DRIFT_LIMIT_M:.3f} m')
    result['rig_displacement_m'] = round(moved_distance, 5)
    result['max_graph_truth_error_m'] = round(max(errors), 5)
    result['failures'] = failures
    result['pass'] = not failures
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=('base', 'occluded', 'tf_unavailable'),
                        default='base')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=18.0)
    args = parser.parse_args()
    check_transform_math()
    if args.mode == 'tf_unavailable':
        rclpy.init()
        probe = TfUnavailableProbe()
        deadline = time.monotonic() + 4.0
        try:
            while time.monotonic() < deadline:
                rclpy.spin_once(probe, timeout_sec=0.10)
            waiting_states = [item.get('landmarks', {}).get('fire')
                              for item in probe.statuses]
            waiting_reported = 'waiting_for_tf' in waiting_states
            result = {
                'mode': args.mode,
                'domain': 'no_gazebo_no_tf',
                'input_messages_sent': probe.sent,
                'waiting_for_tf_reported': waiting_reported,
                'vo_pose_count': probe.vo_count,
                'graph_pose_count': probe.graph_count,
                'pass': (probe.sent >= 3 and waiting_reported and
                         probe.vo_count == 0 and probe.graph_count == 0)}
        finally:
            probe.destroy_node()
            rclpy.shutdown()
        report_path = args.output / 'semantic_ocr_graph_report.json'
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                               encoding='utf-8')
        print(json.dumps(result, ensure_ascii=False, indent=2))
        if result['pass']:
            print('PASS: missing exact-time TF holds the semantic point outside the map.')
            return 0
        return 2
    generation = json.loads((args.output / 'generation.json').read_text(
        encoding='utf-8'))
    rclpy.init()
    node = Verifier()
    result = {'mode': args.mode, 'domain': 'synthetic_only'}
    try:
        if not node.move_client.wait_for_service(timeout_sec=15.0):
            raise RuntimeError('Gazebo /set_entity_state service is absent')
        if args.mode == 'occluded':
            node.observe(args.timeout)
            counts = {key: len(node.camera[key]) for key in LABELS.values()}
            graph_counts = {key: len(node.graph[key]) for key in LABELS.values()}
            marker_missing = any(item.get('state') == 'marker_not_detected'
                                 for item in node.reports)
            result.update({'camera_pose_counts': counts,
                           'graph_pose_counts': graph_counts,
                           'marker_missing_reported': marker_missing})
            if any(counts.values()) or any(graph_counts.values()) or not marker_missing:
                raise RuntimeError('marker occlusion emitted a semantic map point')
            result['pass'] = True
        else:
            node.observe(8.0)
            result['initial'] = check_base(node, generation)
            node.phase = 'moved'
            for step in range(1, 21):
                if not node.move_rig(step * 0.005, 0.0):
                    raise RuntimeError(f'Gazebo camera x motion failed at step {step}')
                node.observe(0.34)
            for step in range(1, 11):
                if not node.move_rig(0.10, step * 0.005):
                    raise RuntimeError(f'Gazebo camera y motion failed at step {step}')
                node.observe(0.34)
            node.observe(6.0)
            result['motion'] = check_motion(node, generation)
            result['pass'] = (result['initial']['pass'] and
                              result['motion']['pass'])
    except RuntimeError as exc:
        result['pass'] = False
        result['failure'] = str(exc)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    report_path = args.output / 'semantic_ocr_graph_report.json'
    report_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                           encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if result['pass']:
        print('PASS: OCR/depth landmarks remain aligned in graph_map during camera motion.'
              if args.mode == 'base' else
              'PASS: an occluded panel produces no semantic graph points.')
        return 0
    return 2


if __name__ == '__main__':
    raise SystemExit(main())
