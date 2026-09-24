#!/usr/bin/env python3
"""Publish the exact VO odom-to-base transform for graph-SLAM TF lookups."""

import rclpy
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from tf2_ros import TransformBroadcaster


class VisualOdomTF(Node):
    def __init__(self):
        super().__init__('visual_odom_tf')
        self.broadcaster = TransformBroadcaster(self)
        self.create_subscription(Odometry, '/vo/odom', self.on_odom,
                                 qos_profile_sensor_data)

    def on_odom(self, msg):
        if (msg.header.frame_id != 'vo_odom' or
                msg.child_frame_id != 'stereo_base_link'):
            return
        transform = TransformStamped()
        transform.header = msg.header
        transform.child_frame_id = msg.child_frame_id
        p = msg.pose.pose.position
        transform.transform.translation.x = p.x
        transform.transform.translation.y = p.y
        transform.transform.translation.z = p.z
        transform.transform.rotation = msg.pose.pose.orientation
        self.broadcaster.sendTransform(transform)


def main():
    rclpy.init()
    node = VisualOdomTF()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
