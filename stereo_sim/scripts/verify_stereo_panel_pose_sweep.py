#!/usr/bin/env python3
"""Measure synthetic stereo panel perception across range and pose conditions."""

import argparse
import csv
import json
import math
from pathlib import Path
import statistics
import time

import cv2
import rclpy
from cv_bridge import CvBridge
from gazebo_msgs.msg import ModelStates
from gazebo_msgs.srv import SetEntityState
from rclpy.node import Node
from sensor_msgs.msg import Image
from geometry_msgs.msg import PoseStamped
from std_msgs.msg import String


LABEL_KEYS = {'火警': 'fire', '故障': 'fault', '主电工作': 'main_power'}
EXPECTED_LED = {'火警': 'red', '故障': 'off', '主电工作': 'off'}
PANEL_HALF_WIDTH_M = 0.480
PANEL_HALF_HEIGHT_M = 0.270
PANEL_FACE_X_M = -0.012
PANEL_Z_M = 0.500
MAX_POSITION_ERROR_M = 0.020


def stamp_tuple(stamp):
    return int(stamp.sec), int(stamp.nanosec)


def stamp_seconds(stamp):
    return stamp[0] + stamp[1] * 1e-9


def full_conditions():
    conditions = []
    distances = (0.65, 0.80, 0.888, 1.20, 1.50, 1.80, 2.10)
    for distance in distances:
        conditions.append({
            'condition_id': f'distance_{distance:.3f}',
            'axis': 'distance', 'distance_m': distance,
            'lateral_m': 0.0, 'yaw_deg': 0.0})
    for yaw in (-35.0, -25.0, -15.0, 15.0, 25.0, 35.0):
        conditions.append({
            'condition_id': f'yaw_{yaw:+.0f}_deg',
            'axis': 'yaw', 'distance_m': 0.888,
            'lateral_m': 0.0, 'yaw_deg': yaw})
    for lateral in (-0.25, -0.15, 0.15, 0.25):
        conditions.append({
            'condition_id': f'lateral_{lateral:+.2f}_m',
            'axis': 'lateral', 'distance_m': 1.20,
            'lateral_m': lateral, 'yaw_deg': 0.0})
    return conditions


def smoke_conditions():
    return [{
        'condition_id': 'smoke_front_0.888m', 'axis': 'smoke',
        'distance_m': 0.888, 'lateral_m': 0.0, 'yaw_deg': 0.0}]


def diagnostic_conditions():
    conditions = []
    for distance in (0.888, 1.20, 1.50, 1.80):
        conditions.append({
            'condition_id': f'distance_{distance:.3f}',
            'axis': 'distance', 'distance_m': distance,
            'lateral_m': 0.0, 'yaw_deg': 0.0})
    for yaw in (-35.0, -15.0, 15.0, 35.0):
        conditions.append({
            'condition_id': f'yaw_{yaw:+.0f}_deg',
            'axis': 'yaw', 'distance_m': 0.888,
            'lateral_m': 0.0, 'yaw_deg': yaw})
    for lateral in (-0.25, 0.25):
        conditions.append({
            'condition_id': f'lateral_{lateral:+.2f}_m',
            'axis': 'lateral', 'distance_m': 1.20,
            'yaw_deg': 0.0, 'lateral_m': lateral})
    return conditions


def expected_camera_xyz(truth_item, condition):
    """Transform the textured-face LED pixel into the left optical frame."""
    u, v = (float(value) for value in truth_item['led_center_px'])
    local_y = PANEL_HALF_WIDTH_M - u / 1000.0
    local_z = PANEL_HALF_HEIGHT_M - v / 1000.0
    yaw = math.radians(float(condition['yaw_deg']))
    world_y = float(condition['lateral_m']) + local_y * math.cos(yaw)
    world_x = float(condition['distance_m']) - local_y * math.sin(yaw)
    # The Gazebo camera faces +world-X. Its optical X axis points toward
    # -world-Y, optical Y points down, and optical Z points toward +world-X.
    return [-world_y, -(local_z), world_x]


def pose_yaw(pose):
    q = pose.orientation
    return math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                      1.0 - 2.0 * (q.y * q.y + q.z * q.z))


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(float(value) for value in values)
    index = (len(ordered) - 1) * fraction
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return round(ordered[lower], 6)
    weight = index - lower
    return round(ordered[lower] * (1.0 - weight) +
                 ordered[upper] * weight, 6)


def stats(values):
    values = [float(value) for value in values if value is not None and
              math.isfinite(float(value))]
    if not values:
        return {'count': 0, 'median': None, 'p95': None, 'max': None}
    return {'count': len(values),
            'median': round(statistics.median(values), 6),
            'p95': percentile(values, 0.95),
            'max': round(max(values), 6)}


class PoseSweepVerifier(Node):
    def __init__(self):
        super().__init__('stereo_panel_pose_sweep_verifier')
        self.report_records = []
        self.report_cursor = 0
        self.image_arrivals = {}
        self.image_messages = {}
        self.latest_image_stamp = None
        self.bridge = CvBridge()
        self.poses = {key: {} for key in LABEL_KEYS.values()}
        self.model_states = None
        self.move_client = self.create_client(SetEntityState, '/set_entity_state')
        self.create_subscription(Image, '/stereo/left/image_rect',
                                 self.on_image, 20)
        self.create_subscription(String, '/stereo_panel/observation',
                                 self.on_report, 20)
        self.create_subscription(ModelStates, '/model_states',
                                 self.on_model_states, 10)
        for key in LABEL_KEYS.values():
            self.create_subscription(
                PoseStamped, f'/stereo_panel/{key}/pose',
                lambda msg, label=key: self.on_pose(label, msg), 20)

    def on_image(self, message):
        stamp = stamp_tuple(message.header.stamp)
        self.latest_image_stamp = stamp
        self.image_arrivals[stamp] = time.monotonic()
        self.image_messages[stamp] = message
        if len(self.image_messages) > 75:
            for old_stamp in list(self.image_messages)[:-60]:
                self.image_messages.pop(old_stamp, None)
        if len(self.image_arrivals) > 1200:
            for old_stamp in list(self.image_arrivals)[:-900]:
                self.image_arrivals.pop(old_stamp, None)

    def on_report(self, message):
        try:
            report = json.loads(message.data)
        except json.JSONDecodeError:
            return
        self.report_records.append({
            'report': report, 'arrival_monotonic': time.monotonic()})

    def on_pose(self, key, message):
        p = message.pose.position
        self.poses[key][stamp_tuple(message.header.stamp)] = {
            'frame_id': message.header.frame_id,
            'xyz_m': [float(p.x), float(p.y), float(p.z)]}

    def on_model_states(self, message):
        self.model_states = message

    def move_panel(self, condition, timeout_sec):
        if not self.move_client.wait_for_service(timeout_sec=timeout_sec):
            raise RuntimeError('Gazebo /set_entity_state service is unavailable')
        yaw = math.radians(float(condition['yaw_deg']))
        request = SetEntityState.Request()
        request.state.name = 'perception_panel'
        request.state.reference_frame = 'world'
        request.state.pose.position.x = float(condition['distance_m']) - \
            PANEL_FACE_X_M * math.cos(yaw)
        request.state.pose.position.y = float(condition['lateral_m']) - \
            PANEL_FACE_X_M * math.sin(yaw)
        request.state.pose.position.z = PANEL_Z_M
        request.state.pose.orientation.z = math.sin(yaw / 2.0)
        request.state.pose.orientation.w = math.cos(yaw / 2.0)
        future = self.move_client.call_async(request)
        rclpy.spin_until_future_complete(self, future, timeout_sec=timeout_sec)
        if not future.done() or future.result() is None:
            raise RuntimeError('Gazebo did not answer the panel pose request')
        if not future.result().success:
            raise RuntimeError(f'Gazebo rejected panel pose: {future.result().status_message}')

        expected = request.state.pose
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            states = self.model_states
            if states is None or 'perception_panel' not in states.name:
                continue
            actual = states.pose[states.name.index('perception_panel')]
            position_error = math.dist(
                (actual.position.x, actual.position.y, actual.position.z),
                (expected.position.x, expected.position.y, expected.position.z))
            yaw_error = abs(math.atan2(
                math.sin(pose_yaw(actual) - yaw),
                math.cos(pose_yaw(actual) - yaw)))
            if position_error <= 0.005 and yaw_error <= 0.01:
                return {
                    'model_position_error_m': round(position_error, 6),
                    'model_yaw_error_rad': round(yaw_error, 6)}
        raise RuntimeError('Gazebo accepted the request but model_states did not confirm it')

    def wait_for_report(self, after_stamp, timeout_sec):
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            while self.report_cursor < len(self.report_records):
                record = self.report_records[self.report_cursor]
                self.report_cursor += 1
                stamp = tuple(record['report'].get('stamp', (0, 0)))
                if stamp > after_stamp:
                    return record
            rclpy.spin_once(self, timeout_sec=0.05)
        return None

    def condition_ready_stamp(self, settle_sec):
        deadline = time.monotonic() + settle_sec
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=min(0.05, deadline - time.monotonic()))
        if self.latest_image_stamp is None:
            raise RuntimeError('No rectified stereo image received')
        return self.latest_image_stamp

    def image_latency(self, stamp, report_arrival):
        deadline = time.monotonic() + 0.25
        while stamp not in self.image_arrivals and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.025)
        image_arrival = self.image_arrivals.get(stamp)
        if image_arrival is None:
            return None, 'image_stamp_not_seen_by_verifier'
        latency = report_arrival - image_arrival
        if latency < 0.0:
            return round(latency, 6), 'observer_callback_order_reversed'
        return round(latency, 6), None

    def save_image(self, stamp, destination):
        message = self.image_messages.get(stamp)
        if message is None:
            return None
        try:
            bgr = self.bridge.imgmsg_to_cv2(message, desired_encoding='bgr8')
            if not cv2.imwrite(str(destination), bgr):
                return None
        except Exception:
            return None
        return destination

    def wait_for_pose_messages(self, record, timeout_sec=0.20):
        report = record['report']
        stamp = tuple(report.get('stamp', (0, 0)))
        expected = [key for key, item in report.get('labels', {}).items()
                    if item.get('state') == 'ok']
        deadline = time.monotonic() + timeout_sec
        while (any(stamp not in self.poses.get(key, {}) for key in expected) and
               time.monotonic() < deadline):
            rclpy.spin_once(self, timeout_sec=0.025)


def make_trial(node, condition, repeat_index, report_record, generation):
    if report_record is None:
        return {
            **condition, 'repeat': repeat_index,
            'status': 'timeout', 'report_state': None,
            'stamp': None, 'image_cloud_delta_sec': None,
            'end_to_end_latency_sec': None,
            'node_timings_sec': None, 'failure_image': None,
            'labels': {key: {'text': label,
                             'expected_led_state': EXPECTED_LED[label],
                             'recognized_text': None, 'text_detected': False,
                             'detected': False, 'led_center_detected': False,
                             'predicted_led_state': None,
                             'led_correct': False, 'pose_xyz_m': None,
                             'position_error_m': None,
                             'valid_points': 0, 'label_pass': False}
                       for label, key in LABEL_KEYS.items()},
            'complete_pass': False}

    report = report_record['report']
    stamp = tuple(report.get('stamp', (0, 0)))
    truth = {item['label']: item for item in generation['truth']}
    output_labels = report.get('labels', {})
    latency, latency_note = node.image_latency(
        stamp, report_record['arrival_monotonic'])
    labels = {}
    for label, key in LABEL_KEYS.items():
        item = output_labels.get(key, {})
        recognized_text = item.get('text')
        predicted_led = item.get('led_state')
        text_detected = recognized_text == label
        led_center_detected = item.get('pixel_rectified') is not None
        led_correct = predicted_led == EXPECTED_LED[label]
        pose = node.poses[key].get(stamp)
        expected_xyz = expected_camera_xyz(truth[label], condition)
        observed_xyz = pose['xyz_m'] if pose else None
        frame_matches = pose is not None and pose['frame_id'] == report.get('frame_id')
        position_error = (math.dist(expected_xyz, observed_xyz)
                          if observed_xyz is not None and frame_matches else None)
        valid_points = int(item.get('valid_points', 0))
        label_pass = bool(
            text_detected and led_center_detected and led_correct and
            position_error is not None and
            position_error <= MAX_POSITION_ERROR_M and valid_points >= 5)
        labels[key] = {
            'text': label,
            'recognized_text': recognized_text,
            'detection_state': item.get('state'),
            'text_detected': text_detected,
            'detected': text_detected,
            'led_center_detected': led_center_detected,
            'expected_led_state': EXPECTED_LED[label],
            'predicted_led_state': predicted_led,
            'led_correct': led_correct,
            'expected_camera_xyz_m': [round(value, 6) for value in expected_xyz],
            'pose_xyz_m': ([round(value, 6) for value in observed_xyz]
                           if observed_xyz is not None else None),
            'pose_frame_id': pose['frame_id'] if pose else None,
            'pose_frame_matches_report': bool(frame_matches),
            'position_error_m': round(position_error, 6)
            if position_error is not None else None,
            'valid_points': valid_points,
            'label_pass': label_pass}
    return {
        **condition, 'repeat': repeat_index,
        'status': 'measured', 'report_state': report.get('state'),
        'stamp': list(stamp),
        'image_cloud_delta_sec': report.get('image_cloud_delta_sec'),
        'end_to_end_latency_sec': latency,
        'node_timings_sec': report.get('timings_sec'),
        'failure_image': None,
        'latency_note': latency_note,
        'labels': labels,
        'complete_pass': all(item['label_pass'] for item in labels.values())}


def condition_summary(trials, key):
    selected = [item for item in trials if item['condition_id'] == key]
    labels = [label for trial in selected for label in trial['labels'].values()]
    total_labels = len(selected) * len(LABEL_KEYS)
    text_detected = sum(item['text_detected'] for item in labels)
    led_centres = sum(item['led_center_detected'] for item in labels)
    led_correct = sum(item['led_correct'] for item in labels)
    poses = [item for item in labels if item['pose_xyz_m'] is not None and
             item['pose_frame_matches_report']]
    errors = [item['position_error_m'] for item in poses
              if item['position_error_m'] is not None]
    within = sum(error <= MAX_POSITION_ERROR_M for error in errors)
    return {
        'condition_id': key,
        'axis': selected[0]['axis'] if selected else None,
        'distance_m': selected[0]['distance_m'] if selected else None,
        'lateral_m': selected[0]['lateral_m'] if selected else None,
        'yaw_deg': selected[0]['yaw_deg'] if selected else None,
        'trials': len(selected),
        'status_timeouts': sum(item['status'] == 'timeout' for item in selected),
        'complete_trial_rate': round(sum(item['complete_pass'] for item in selected) /
                                      len(selected), 4) if selected else 0.0,
        'ocr_text_detection_rate_all_required': round(text_detected / total_labels, 4)
        if total_labels else 0.0,
        'led_center_detection_rate_all_required': round(led_centres / total_labels, 4)
        if total_labels else 0.0,
        'led_accuracy_all_required': round(led_correct / total_labels, 4)
        if total_labels else 0.0,
        'valid_pose_rate_all_required': round(len(poses) / total_labels, 4)
        if total_labels else 0.0,
        'position_within_2cm_rate_all_required': round(within / total_labels, 4)
        if total_labels else 0.0,
        'position_error_m': stats(errors),
        'end_to_end_latency_sec': stats([
            item['end_to_end_latency_sec'] for item in selected]),
        'node_input_to_publish_sec': stats([
            (item.get('node_timings_sec') or {}).get('input_to_publish')
            for item in selected]),
        'node_pair_processing_sec': stats([
            (item.get('node_timings_sec') or {}).get('pair_processing')
            for item in selected]),
        'node_ocr_sec': stats([
            (item.get('node_timings_sec') or {}).get('ocr')
            for item in selected]),
        'image_cloud_delta_sec': stats([
            item['image_cloud_delta_sec'] for item in selected]),
        'valid_points': stats([item['valid_points'] for item in labels])}


def write_reports(output, trials, conditions, repeats, profile, movement_checks):
    output.mkdir(parents=True, exist_ok=True)
    raw_path = output / 'trials.jsonl'
    with raw_path.open('w', encoding='utf-8', newline='\n') as stream:
        for trial in trials:
            stream.write(json.dumps(trial, ensure_ascii=False, sort_keys=True) + '\n')

    csv_path = output / 'trials.csv'
    columns = [
        'condition_id', 'axis', 'repeat', 'distance_m', 'lateral_m', 'yaw_deg',
        'status', 'report_state', 'stamp', 'label', 'detection_state', 'detected',
        'recognized_text', 'text_detected', 'led_center_detected',
        'expected_led_state', 'predicted_led_state', 'led_correct',
        'expected_camera_xyz_m', 'pose_xyz_m', 'position_error_m',
        'valid_points', 'image_cloud_delta_sec', 'end_to_end_latency_sec',
        'node_input_to_publish_sec', 'node_pair_processing_sec', 'node_ocr_sec',
        'latency_note', 'failure_image', 'label_pass', 'complete_pass']
    with csv_path.open('w', encoding='utf-8-sig', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for trial in trials:
            for key, item in trial['labels'].items():
                writer.writerow({
                    'condition_id': trial['condition_id'], 'axis': trial['axis'],
                    'repeat': trial['repeat'], 'distance_m': trial['distance_m'],
                    'lateral_m': trial['lateral_m'], 'yaw_deg': trial['yaw_deg'],
                    'status': trial['status'], 'report_state': trial['report_state'],
                    'stamp': trial['stamp'], 'label': item['text'],
                    'recognized_text': item.get('recognized_text'),
                    'text_detected': item['text_detected'],
                    'led_center_detected': item['led_center_detected'],
                    'detection_state': item.get('detection_state'),
                    'detected': item['detected'],
                    'expected_led_state': item['expected_led_state'],
                    'predicted_led_state': item['predicted_led_state'],
                    'led_correct': item['led_correct'],
                    'expected_camera_xyz_m': item.get('expected_camera_xyz_m'),
                    'pose_xyz_m': item.get('pose_xyz_m'),
                    'position_error_m': item.get('position_error_m'),
                    'valid_points': item['valid_points'],
                    'image_cloud_delta_sec': trial['image_cloud_delta_sec'],
                    'end_to_end_latency_sec': trial['end_to_end_latency_sec'],
                    'node_input_to_publish_sec':
                        (trial.get('node_timings_sec') or {}).get('input_to_publish'),
                    'node_pair_processing_sec':
                        (trial.get('node_timings_sec') or {}).get('pair_processing'),
                    'node_ocr_sec': (trial.get('node_timings_sec') or {}).get('ocr'),
                    'latency_note': trial.get('latency_note'),
                    'failure_image': trial.get('failure_image'),
                    'label_pass': item['label_pass'],
                    'complete_pass': trial['complete_pass']})

    conditions_summary = [condition_summary(trials, condition['condition_id'])
                          for condition in conditions]
    all_labels = [label for trial in trials for label in trial['labels'].values()]
    total_labels = len(all_labels)
    text_detected_count = sum(item['text_detected'] for item in all_labels)
    led_center_count = sum(item['led_center_detected'] for item in all_labels)
    led_correct_count = sum(item['led_correct'] for item in all_labels)
    valid_pose = [item for item in all_labels
                  if item['pose_xyz_m'] is not None and item['pose_frame_matches_report']]
    errors = [item['position_error_m'] for item in valid_pose
              if item['position_error_m'] is not None]
    within = sum(error <= MAX_POSITION_ERROR_M for error in errors)
    confusion = {}
    for item in all_labels:
        expected = item['expected_led_state']
        predicted = item['predicted_led_state'] or 'missing'
        confusion.setdefault(expected, {})[predicted] = \
            confusion.setdefault(expected, {}).get(predicted, 0) + 1
    summary = {
        'schema': 'stereo_panel_pose_sweep_v1', 'domain': 'synthetic_only',
        'profile': profile, 'planned_conditions': len(conditions),
        'repeats_per_condition': repeats,
        'planned_trials': len(conditions) * repeats,
        'completed_trials': len(trials),
        'acquisition_complete': len(trials) == len(conditions) * repeats,
        'acceptance_position_error_m': MAX_POSITION_ERROR_M,
        'panel_case': 'fire_on',
        'distance_definition': 'textured-face center depth along camera optical Z, metres',
        'repeat_definition': 'fresh camera observations at a fixed pose in one Gazebo run; not independent process restarts',
        'total_ocr_text_detection_rate_all_required':
            round(text_detected_count / total_labels, 4)
        if total_labels else 0.0,
        'total_led_center_detection_rate_all_required':
            round(led_center_count / total_labels, 4)
        if total_labels else 0.0,
        'total_led_accuracy_all_required': round(led_correct_count / total_labels, 4)
        if total_labels else 0.0,
        'total_valid_pose_rate_all_required': round(len(valid_pose) / total_labels, 4)
        if total_labels else 0.0,
        'total_position_within_2cm_rate_all_required': round(within / total_labels, 4)
        if total_labels else 0.0,
        'position_error_m': stats(errors),
        'end_to_end_latency_sec': stats([
            item['end_to_end_latency_sec'] for item in trials]),
        'node_input_to_publish_sec': stats([
            (item.get('node_timings_sec') or {}).get('input_to_publish')
            for item in trials]),
        'node_pair_processing_sec': stats([
            (item.get('node_timings_sec') or {}).get('pair_processing')
            for item in trials]),
        'node_image_conversion_sec': stats([
            (item.get('node_timings_sec') or {}).get('image_conversion')
            for item in trials]),
        'node_marker_detection_sec': stats([
            (item.get('node_timings_sec') or {}).get('marker_detection')
            for item in trials]),
        'node_ocr_sec': stats([
            (item.get('node_timings_sec') or {}).get('ocr')
            for item in trials]),
        'node_point_cloud_association_sec': stats([
            (item.get('node_timings_sec') or {}).get('point_cloud_association')
            for item in trials]),
        'image_cloud_delta_sec': stats([
            item['image_cloud_delta_sec'] for item in trials]),
        'valid_points': stats([item['valid_points'] for item in all_labels]),
        'led_confusion_expected_to_predicted': confusion,
        'gazebo_model_pose_checks': movement_checks,
        'conditions': conditions_summary,
        'failed_trials': [
            {'condition_id': item['condition_id'], 'repeat': item['repeat'],
             'status': item['status'], 'report_state': item['report_state'],
             'failure_image': item.get('failure_image'),
             'label_states': {key: value.get('detection_state')
                              for key, value in item['labels'].items()}}
            for item in trials if not item['complete_pass']],
        'source_files': ['trials.jsonl', 'trials.csv'],
    }
    (output / 'summary.json').write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=('smoke', 'diagnostic', 'full'),
                        default='full')
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--report-timeout', type=float, default=5.0)
    parser.add_argument('--service-timeout', type=float, default=6.0)
    parser.add_argument('--settle-sec', type=float, default=0.20)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error('--repeats must be at least 1')
    output = args.output.expanduser().resolve()
    generation_path = output / 'generation.json'
    if not generation_path.is_file():
        parser.error(f'missing generated panel truth: {generation_path}')
    generation = json.loads(generation_path.read_text(encoding='utf-8'))
    conditions = {
        'smoke': smoke_conditions(),
        'diagnostic': diagnostic_conditions(),
        'full': full_conditions(),
    }[args.profile]
    rclpy.init()
    node = PoseSweepVerifier()
    trials = []
    movement_checks = []
    infrastructure_error = None
    try:
        if not node.move_client.wait_for_service(timeout_sec=args.service_timeout):
            raise RuntimeError('Gazebo /set_entity_state service is unavailable')
        deadline = time.monotonic() + args.service_timeout
        while node.latest_image_stamp is None and time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.1)
        if node.latest_image_stamp is None:
            raise RuntimeError('No rectified camera images arrived')

        for condition in conditions:
            movement_check = node.move_panel(condition, args.service_timeout)
            movement_check['condition_id'] = condition['condition_id']
            movement_checks.append(movement_check)
            after_stamp = node.condition_ready_stamp(args.settle_sec)
            for repeat_index in range(1, args.repeats + 1):
                fresh = node.wait_for_report(after_stamp, args.report_timeout)
                if fresh is not None:
                    node.wait_for_pose_messages(fresh)
                trial = make_trial(node, condition, repeat_index, fresh, generation)
                if fresh is not None and not trial['complete_pass']:
                    stamp = tuple(fresh['report'].get('stamp', (0, 0)))
                    image_dir = output / 'failure_images'
                    image_dir.mkdir(parents=True, exist_ok=True)
                    image_name = (f"{condition['condition_id']}_r{repeat_index}_"
                                  f"{stamp[0]}_{stamp[1]}.png")
                    saved = node.save_image(stamp, image_dir / image_name)
                    if saved is not None:
                        trial['failure_image'] = str(saved.relative_to(output))
                trials.append(trial)
                if fresh is not None:
                    after_stamp = tuple(fresh['report'].get('stamp', (0, 0)))
                line = {
                    'condition_id': condition['condition_id'], 'repeat': repeat_index,
                    'status': trial['status'], 'report_state': trial['report_state'],
                    'complete_pass': trial['complete_pass']}
                print(json.dumps(line, ensure_ascii=False), flush=True)
    except Exception as exc:
        infrastructure_error = str(exc)
        print(f'INFRASTRUCTURE ERROR: {infrastructure_error}')
    finally:
        node.destroy_node()
        rclpy.shutdown()

    summary = write_reports(
        output, trials, conditions, args.repeats, args.profile, movement_checks)
    summary['infrastructure_error'] = infrastructure_error
    (output / 'summary.json').write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if infrastructure_error or not summary['acquisition_complete']:
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
