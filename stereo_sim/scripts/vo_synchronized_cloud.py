#!/usr/bin/env python3
"""Forward only stereo clouds whose capture time has a measured VO pose."""

from collections import deque

import rclpy
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2


def stamp_seconds(stamp):
    return stamp.sec + stamp.nanosec * 1e-9


class VOSynchronizedCloud(Node):
    def __init__(self):
        super().__init__('vo_synchronized_cloud')
        self.clouds = deque(maxlen=45)
        self.odometry = deque(maxlen=45)
        self.last_published_stamp = -1.0
        self.publisher = self.create_publisher(
            PointCloud2, '/vo/points2', qos_profile_sensor_data)
        self.create_subscription(
            PointCloud2, '/stereo/points2', self.on_cloud, qos_profile_sensor_data)
        self.create_subscription(
            Odometry, '/vo/odom', self.on_odom, qos_profile_sensor_data)

    def on_cloud(self, msg):
        self.clouds.append(msg)
        self.pair()

    def on_odom(self, msg):
        if msg.header.frame_id != 'vo_odom' or msg.child_frame_id != 'stereo_base_link':
            return
        self.odometry.append(msg)
        self.pair()

    def pair(self):
        if not self.clouds or not self.odometry:
            return
        candidate = min(
            ((abs(stamp_seconds(cloud.header.stamp) - stamp_seconds(odom.header.stamp)),
              cloud, odom)
             for cloud in self.clouds for odom in self.odometry),
            key=lambda item: item[0])
        error, cloud, odom = candidate
        if error > 0.012:
            return
        self.clouds.remove(cloud)
        self.odometry.remove(odom)
        stamp = stamp_seconds(cloud.header.stamp)
        if stamp - self.last_published_stamp >= 0.18:
            self.publisher.publish(cloud)
            self.last_published_stamp = stamp


def main():
    rclpy.init()
    node = VOSynchronizedCloud()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
