#!/usr/bin/env python3
"""Fuse simulated coloured button detections with stereo disparity and pose.

The default mode uses Gazebo pose for legacy simulation checks. The odometry
mode instead uses stereo VO and never subscribes to Gazebo ModelStates.
"""

from collections import deque
import json
import math

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from gazebo_msgs.msg import ModelStates
from geometry_msgs.msg import PoseStamped, TransformStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo, Image
from std_msgs.msg import String
from stereo_msgs.msg import DisparityImage
from tf2_ros import TransformBroadcaster
from visualization_msgs.msg import Marker, MarkerArray


BUTTONS = {
    'mute': ([(0, 12), (170, 179)], (1.0, 0.1, 0.1)),
    'reset': ([(38, 88)], (0.1, 1.0, 0.1)),
    'confirm': ([(95, 140)], (0.1, 0.2, 1.0)),
}
MARKER_ID = 582
MARKER_SIDE_M = 0.084
BUTTON_Y_OFFSETS = {'mute': -0.12, 'reset': 0.0, 'confirm': 0.12}


def stamp_seconds(stamp):
    return stamp.sec + stamp.nanosec * 1e-9


def quat_multiply(a, b):
    return (
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    )


def rotate(q, v):
    qv = (v[0], v[1], v[2], 0.0)
    conj = (-q[0], -q[1], -q[2], q[3])
    return quat_multiply(quat_multiply(q, qv), conj)[:3]


def optical_to_model_quat():
    # Optical X right, Y down, Z forward -> model X forward, Y left, Z up.
    # Equivalent to the rotation matrix [[0,0,1],[-1,0,0],[0,-1,0]].
    return (0.5, -0.5, 0.5, -0.5)


def matrix_to_quat(matrix):
    matrix = np.asarray(matrix, dtype=np.float64)
    trace = float(np.trace(matrix))
    if trace > 0:
        scale = math.sqrt(trace + 1.0) * 2.0
        q = ((matrix[2, 1] - matrix[1, 2]) / scale,
             (matrix[0, 2] - matrix[2, 0]) / scale,
             (matrix[1, 0] - matrix[0, 1]) / scale,
             0.25 * scale)
    else:
        index = int(np.argmax(np.diag(matrix)))
        if index == 0:
            scale = math.sqrt(1.0 + matrix[0, 0] - matrix[1, 1] - matrix[2, 2]) * 2.0
            q = (0.25 * scale, (matrix[0, 1] + matrix[1, 0]) / scale,
                 (matrix[0, 2] + matrix[2, 0]) / scale,
                 (matrix[2, 1] - matrix[1, 2]) / scale)
        elif index == 1:
            scale = math.sqrt(1.0 + matrix[1, 1] - matrix[0, 0] - matrix[2, 2]) * 2.0
            q = ((matrix[0, 1] + matrix[1, 0]) / scale, 0.25 * scale,
                 (matrix[1, 2] + matrix[2, 1]) / scale,
                 (matrix[0, 2] - matrix[2, 0]) / scale)
        else:
            scale = math.sqrt(1.0 + matrix[2, 2] - matrix[0, 0] - matrix[1, 1]) * 2.0
            q = ((matrix[0, 2] + matrix[2, 0]) / scale,
                 (matrix[1, 2] + matrix[2, 1]) / scale, 0.25 * scale,
                 (matrix[1, 0] - matrix[0, 1]) / scale)
    norm = math.sqrt(sum(value * value for value in q))
    return tuple(value / norm for value in q)


class SemanticPanelNode(Node):
    def __init__(self):
        super().__init__('semantic_panel')
        self.declare_parameter('left_image_topic', '/stereo/stereo_rig/left/image_raw')
        self.declare_parameter('left_camera_info_topic', '/stereo/stereo_rig/left/camera_info')
        self.declare_parameter('rig_model', 'stereo_rig')
        self.declare_parameter('camera_frame', 'stereo_left_camera_optical_frame')
        self.declare_parameter('minimum_depth_pixels', 8)
        self.declare_parameter('pose_source', 'gazebo')
        self.pose_source = self.get_parameter('pose_source').value
        if self.pose_source not in ('gazebo', 'odometry'):
            raise ValueError('pose_source must be gazebo or odometry')
        self.bridge = CvBridge()
        self.images = deque(maxlen=15)
        self.disparities = deque(maxlen=15)
        self.odometry = deque(maxlen=30)
        self.camera_infos = deque(maxlen=30)
        self.camera_info = None
        self.rig_pose = None
        self.rig_pose_time = None
        self.last_status_time = -1.0
        # Marker 582 belongs to one stationary panel. Keep its yaw in the VO
        # map frame, where several views constrain the weak frontal PnP tilt.
        self.panel_normal_yaws = deque(maxlen=240)
        self.panel_anchor_xy = None
        self.button_positions = {name: deque(maxlen=240) for name in BUTTONS}
        self.tf_broadcaster = TransformBroadcaster(self)
        self.pose_publishers = {
            name: self.create_publisher(PoseStamped, f'/semantic_panel/button/{name}/pose', 10)
            for name in BUTTONS
        }
        self.marker_publisher = self.create_publisher(MarkerArray, '/semantic_panel/markers', 10)
        self.image_publisher = self.create_publisher(Image, '/semantic_panel/annotated_image', 10)
        self.status_publisher = self.create_publisher(String, '/semantic_panel/status', 10)
        self.marker_pose_publisher = self.create_publisher(PoseStamped, '/semantic_panel/marker_pose', 10)
        self.aruco_dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_ARUCO_ORIGINAL)
        self.create_subscription(
            Image, self.get_parameter('left_image_topic').value,
            self.on_image, qos_profile_sensor_data)
        self.create_subscription(
            CameraInfo, self.get_parameter('left_camera_info_topic').value,
            self.on_camera_info, qos_profile_sensor_data)
        self.create_subscription(
            DisparityImage, '/stereo/disparity', self.on_disparity,
            qos_profile_sensor_data)
        if self.pose_source == 'gazebo':
            self.create_subscription(
                ModelStates, '/model_states', self.on_models,
                qos_profile_sensor_data)
        else:
            self.create_subscription(
                Odometry, '/vo/odom', self.on_odom,
                qos_profile_sensor_data)

    def on_camera_info(self, msg):
        self.camera_info = msg
        self.camera_infos.append(msg)

    def on_models(self, msg):
        name = self.get_parameter('rig_model').value
        if name in msg.name:
            self.rig_pose = msg.pose[msg.name.index(name)]
            self.rig_pose_time = self.get_clock().now().nanoseconds * 1e-9

    def on_odom(self, msg):
        if msg.header.frame_id != 'vo_odom' or msg.child_frame_id != 'stereo_base_link':
            return
        self.odometry.append(msg)
        rig = msg.pose.pose
        q = rig.orientation
        self.publish_camera_tf(
            msg.header.stamp,
            (rig.position.x, rig.position.y, rig.position.z),
            (q.x, q.y, q.z, q.w), 'stereo_base_link')
        self.try_pair()

    def on_image(self, msg):
        self.images.append(msg)
        self.try_pair()

    def on_disparity(self, msg):
        self.disparities.append(msg)
        self.try_pair()

    def try_pair(self):
        if not self.images or not self.disparities:
            return
        best = min(
            ((abs(stamp_seconds(i.header.stamp) - stamp_seconds(d.header.stamp)), i, d)
             for i in self.images for d in self.disparities),
            key=lambda item: item[0])
        if best[0] > 0.012:
            return
        _, image, disparity = best
        if self.pose_source == 'odometry':
            if not self.odometry:
                return
            stamp = stamp_seconds(image.header.stamp)
            pose = min(self.odometry,
                       key=lambda msg: abs(stamp_seconds(msg.header.stamp) - stamp))
            pose_stamp = stamp_seconds(pose.header.stamp)
            if abs(pose_stamp - stamp) > 0.025:
                return
            if not self.camera_infos:
                return
            info = min(self.camera_infos,
                       key=lambda msg: abs(stamp_seconds(msg.header.stamp) - stamp))
            if abs(stamp_seconds(info.header.stamp) - stamp) > 0.025:
                return
            self.camera_info = info
            self.rig_pose = pose.pose.pose
            self.rig_pose_time = pose_stamp
        self.images.remove(image)
        self.disparities.remove(disparity)
        self.process(image, disparity)

    def detect_buttons(self, bgr):
        hsv = cv2.cvtColor(bgr, cv2.COLOR_BGR2HSV)
        detections = {}
        for name, (hue_ranges, _colour) in BUTTONS.items():
            mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
            for lower, upper in hue_ranges:
                mask |= cv2.inRange(hsv, (lower, 90, 45), (upper, 255, 255))
            mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            if not contours:
                continue
            contour = max(contours, key=cv2.contourArea)
            if cv2.contourArea(contour) < 45:
                continue
            isolated = np.zeros_like(mask)
            cv2.drawContours(isolated, [contour], -1, 255, -1)
            moments = cv2.moments(contour)
            if moments['m00'] <= 0:
                continue
            u = moments['m10'] / moments['m00']
            v = moments['m01'] / moments['m00']
            detections[name] = (u, v, isolated, cv2.boundingRect(contour))
        return detections

    def detect_marker(self, bgr):
        corners, ids, _rejected = cv2.aruco.detectMarkers(bgr, self.aruco_dictionary)
        if ids is None:
            return None
        matches = np.flatnonzero(ids.ravel() == MARKER_ID)
        if matches.size != 1:
            return None
        half = MARKER_SIDE_M / 2.0
        object_points = np.array([
            [-half, half, 0.0], [half, half, 0.0],
            [half, -half, 0.0], [-half, -half, 0.0],
        ], dtype=np.float32)
        image_points = corners[int(matches[0])].reshape(4, 2).astype(np.float32)
        intrinsic = np.asarray(self.camera_info.k, dtype=np.float64).reshape(3, 3)
        distortion = np.asarray(self.camera_info.d, dtype=np.float64)
        solved, rotation_vector, translation = cv2.solvePnP(
            object_points, image_points, intrinsic, distortion,
            flags=cv2.SOLVEPNP_ITERATIVE)
        if not solved or not np.isfinite(translation).all() or translation[2, 0] <= 0:
            return None
        pose_rotation = cv2.Rodrigues(rotation_vector)[0]
        if not np.isfinite(pose_rotation).all():
            return None
        # A small, nearly frontal square can have an unstable PnP rotation.
        # Use the observed corner directions to define its in-plane axes.
        depth = float(translation[2, 0])
        fx, fy, cx, cy = intrinsic[0, 0], intrinsic[1, 1], intrinsic[0, 2], intrinsic[1, 2]
        corners_3d = np.column_stack((
            (image_points[:, 0] - cx) * depth / fx,
            (image_points[:, 1] - cy) * depth / fy,
            np.full(4, depth),
        ))
        horizontal = (corners_3d[1] + corners_3d[2] - corners_3d[0] - corners_3d[3]) / 2.0
        horizontal /= np.linalg.norm(horizontal)
        vertical = (corners_3d[0] + corners_3d[1] - corners_3d[2] - corners_3d[3]) / 2.0
        vertical -= np.dot(vertical, horizontal) * horizontal
        vertical /= np.linalg.norm(vertical)
        normal = np.cross(horizontal, vertical)
        marker_rotation = np.column_stack((horizontal, vertical, normal))
        return marker_rotation, pose_rotation, translation[:, 0], image_points

    def process(self, image, disparity):
        stamp = stamp_seconds(image.header.stamp)
        statuses = {}
        if self.camera_info is None or self.rig_pose is None:
            self.publish_status(stamp, {}, 'missing_camera_info_or_pose')
            return
        if self.rig_pose_time is None or abs(stamp - self.rig_pose_time) > 0.20:
            self.publish_status(stamp, {}, 'stale_pose')
            return
        if abs(stamp - stamp_seconds(self.camera_info.header.stamp)) > 0.10:
            self.publish_status(stamp, {}, 'stale_camera_info')
            return
        if disparity.image.encoding != '32FC1':
            self.publish_status(stamp, {}, 'unsupported_disparity_encoding')
            return
        bgr = self.bridge.imgmsg_to_cv2(image, desired_encoding='bgr8')
        if bgr.shape[:2] != (disparity.image.height, disparity.image.width):
            self.publish_status(stamp, {}, 'image_disparity_size_mismatch')
            return
        disp = np.ndarray(
            (disparity.image.height, disparity.image.width), dtype='<f4',
            buffer=bytes(disparity.image.data), strides=(disparity.image.step, 4))
        detections = self.detect_buttons(bgr)
        marker = self.detect_marker(bgr)
        frame = self.get_parameter('camera_frame').value
        rig = self.rig_pose
        q_rig = (rig.orientation.x, rig.orientation.y, rig.orientation.z, rig.orientation.w)
        camera_offset = rotate(q_rig, (0.0, 0.0, 0.50))
        camera_origin = (
            rig.position.x + camera_offset[0],
            rig.position.y + camera_offset[1],
            rig.position.z + camera_offset[2])
        if self.pose_source == 'gazebo':
            self.publish_camera_tf(image.header.stamp, camera_origin,
                                   quat_multiply(q_rig, optical_to_model_quat()), frame)
        if marker is not None:
            marker_rotation, pose_rotation, marker_optical, marker_corners = marker
            p_model = (marker_optical[2], -marker_optical[0], -marker_optical[1])
            p_rotated = rotate(q_rig, p_model)
            marker_world = tuple(camera_origin[i] + p_rotated[i] for i in range(3))
            marker_pose = PoseStamped()
            marker_pose.header.stamp = image.header.stamp
            marker_pose.header.frame_id = 'map'
            marker_pose.pose.position.x, marker_pose.pose.position.y, marker_pose.pose.position.z = marker_world
            marker_quat = quat_multiply(
                quat_multiply(q_rig, optical_to_model_quat()),
                matrix_to_quat(pose_rotation))
            raw_normal = rotate(marker_quat, (0.0, 0.0, 1.0))
            raw_yaw = math.atan2(raw_normal[1], raw_normal[0])
            if (self.panel_anchor_xy is None or
                    math.dist(marker_world[:2], self.panel_anchor_xy) > 0.10):
                self.panel_normal_yaws.clear()
                self.panel_anchor_xy = marker_world[:2]
                for samples in self.button_positions.values():
                    samples.clear()
            if not self.panel_normal_yaws:
                self.panel_normal_yaws.append(raw_yaw)
            else:
                reference = float(np.median(self.panel_normal_yaws))
                unwrapped = reference + math.atan2(
                    math.sin(raw_yaw - reference), math.cos(raw_yaw - reference))
                if abs(unwrapped - reference) < 0.30:
                    self.panel_normal_yaws.append(unwrapped)
            filtered_yaw = float(np.median(self.panel_normal_yaws))
            correction = filtered_yaw - raw_yaw
            marker_quat = quat_multiply(
                (0.0, 0.0, math.sin(correction / 2.0),
                 math.cos(correction / 2.0)), marker_quat)
            (marker_pose.pose.orientation.x, marker_pose.pose.orientation.y,
             marker_pose.pose.orientation.z, marker_pose.pose.orientation.w) = marker_quat
            self.marker_pose_publisher.publish(marker_pose)
            panel_tf = TransformStamped()
            panel_tf.header = marker_pose.header
            panel_tf.child_frame_id = 'panel_marker_582'
            panel_tf.transform.translation.x, panel_tf.transform.translation.y, panel_tf.transform.translation.z = marker_world
            panel_tf.transform.rotation = marker_pose.pose.orientation
            self.tf_broadcaster.sendTransform(panel_tf)
            cv2.polylines(bgr, [marker_corners.astype(np.int32)], True, (0, 255, 255), 2)
            normal = rotate(marker_quat, (0.0, 0.0, 1.0))
            statuses['aruco_582'] = {
                'map_xyz_m': [round(x, 4) for x in marker_world],
                'normal_map_xyz': [round(x, 4) for x in normal],
                'normal_samples': len(self.panel_normal_yaws)}
        else:
            statuses['aruco_582'] = 'not_detected'
        markers = MarkerArray()
        for index, (name, (_hues, colour)) in enumerate(BUTTONS.items()):
            detected = detections.get(name)
            if detected is None:
                statuses[name] = 'not_detected'
                continue
            u, v, mask, (x, y, width, height) = detected
            core = cv2.erode(mask, np.ones((3, 3), np.uint8))
            values = disp[(core > 0) & np.isfinite(disp) & (disp > 0)]
            if values.size < self.get_parameter('minimum_depth_pixels').value:
                statuses[name] = 'no_valid_depth'
                continue
            depth = float(np.median(disparity.f * disparity.t / values))
            if not 0.20 < depth < 1.50:
                statuses[name] = 'depth_out_of_range'
                continue
            if marker is None:
                statuses[name] = 'marker_not_detected'
                continue
            fx = self.camera_info.p[0]
            fy = self.camera_info.p[5]
            cx = self.camera_info.p[2]
            cy = self.camera_info.p[6]
            if fx <= 0 or fy <= 0:
                statuses[name] = 'invalid_camera_projection'
                continue
            p_optical = ((u - cx) * depth / fx, (v - cy) * depth / fy, depth)
            p_model = (p_optical[2], -p_optical[0], -p_optical[1])
            p_rotated = rotate(q_rig, p_model)
            position = tuple(camera_origin[i] + p_rotated[i] for i in range(3))
            if marker is not None:
                button_in_marker = np.array(
                    [-BUTTON_Y_OFFSETS[name], 0.120, 0.017], dtype=np.float64)
                predicted_optical = marker_rotation @ button_in_marker + marker_optical
                predicted_model = (predicted_optical[2], -predicted_optical[0], -predicted_optical[1])
                predicted_rotated = rotate(q_rig, predicted_model)
                predicted_world = tuple(camera_origin[i] + predicted_rotated[i] for i in range(3))
                marker_residual = math.dist(position, predicted_world)
            else:
                marker_residual = None
            if marker_residual is None or marker_residual > 0.025:
                statuses[name] = f'marker_stereo_disagreement:{marker_residual:.3f}'
                continue
            samples = self.button_positions[name]
            if samples:
                centre = np.median(np.asarray(samples), axis=0)
                if math.dist(position, centre) > 0.06:
                    statuses[name] = 'inconsistent_static_button_position'
                    continue
            samples.append(position)
            stable_position = tuple(float(value) for value in
                                    np.median(np.asarray(samples), axis=0))
            pose = PoseStamped()
            pose.header.stamp = image.header.stamp
            pose.header.frame_id = 'map'
            (pose.pose.position.x, pose.pose.position.y,
             pose.pose.position.z) = stable_position
            pose.pose.orientation.w = 1.0
            self.pose_publishers[name].publish(pose)
            display_marker = Marker()
            display_marker.header = pose.header
            display_marker.ns = 'semantic_buttons'
            display_marker.id = index
            display_marker.type = Marker.SPHERE
            display_marker.action = Marker.ADD
            display_marker.pose = pose.pose
            display_marker.scale.x = display_marker.scale.y = display_marker.scale.z = 0.035
            display_marker.color.r, display_marker.color.g, display_marker.color.b = colour
            display_marker.color.a = 1.0
            display_marker.lifetime.sec = 0
            display_marker.lifetime.nanosec = 200000000
            markers.markers.append(display_marker)
            cv2.rectangle(bgr, (x, y), (x + width, y + height), tuple(int(c * 255) for c in reversed(colour)), 2)
            cv2.putText(bgr, f'{name} {depth:.3f}m', (x, max(12, y - 5)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
            statuses[name] = {'pixel': [round(u, 1), round(v, 1)],
                              'depth_m': round(depth, 4),
                              'map_xyz_m': [round(x, 4) for x in stable_position],
                              'raw_map_xyz_m': [round(x, 4) for x in position],
                              'position_samples': len(samples),
                              'marker_prediction_xyz_m': ([round(x, 4) for x in predicted_world]
                                                          if marker is not None else None),
                              'marker_residual_m': (round(marker_residual, 4)
                                                    if marker_residual is not None else None)}
        self.marker_publisher.publish(markers)
        annotated = self.bridge.cv2_to_imgmsg(bgr, encoding='bgr8')
        annotated.header = image.header
        self.image_publisher.publish(annotated)
        self.publish_status(stamp, statuses, 'ok')

    def publish_camera_tf(self, stamp, origin, orientation, child_frame):
        transform = TransformStamped()
        transform.header.stamp = stamp
        transform.header.frame_id = 'map'
        transform.child_frame_id = child_frame
        transform.transform.translation.x = origin[0]
        transform.transform.translation.y = origin[1]
        transform.transform.translation.z = origin[2]
        (transform.transform.rotation.x, transform.transform.rotation.y,
         transform.transform.rotation.z, transform.transform.rotation.w) = orientation
        self.tf_broadcaster.sendTransform(transform)

    def publish_status(self, stamp, buttons, state):
        if stamp - self.last_status_time < 0.10:
            return
        message = String()
        message.data = json.dumps({'stamp': round(stamp, 6), 'state': state,
                                   'buttons': buttons}, sort_keys=True)
        self.status_publisher.publish(message)
        self.last_status_time = stamp


def main():
    rclpy.init()
    node = SemanticPanelNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
