#!/usr/bin/env python3
"""Publish a clamped cubic B-spline rendering of Nav2's global /plan.

The output is deliberately diagnostic only: it is published on /plan_bspline and
is never remapped to Nav2's controller.  A B-spline may cut a corner into an
obstacle, so a collision-tested Nav2 smoother plugin is required before a
smoothed path is allowed to control the robot.
"""

from __future__ import annotations

import math
from typing import Iterable, List, Tuple

import rclpy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Path
from rclpy.node import Node


Point2 = Tuple[float, float]


def cubic_basis(t: float) -> Tuple[float, float, float, float]:
    """Uniform cubic B-spline basis for t in [0, 1]."""
    one_minus_t = 1.0 - t
    return (
        (one_minus_t**3) / 6.0,
        (3.0 * t**3 - 6.0 * t**2 + 4.0) / 6.0,
        (-3.0 * t**3 + 3.0 * t**2 + 3.0 * t + 1.0) / 6.0,
        (t**3) / 6.0,
    )


def select_control_points(poses: Iterable[PoseStamped], stride: int) -> List[Point2]:
    raw = [(pose.pose.position.x, pose.pose.position.y) for pose in poses]
    if len(raw) < 2:
        return raw
    selected = raw[::stride]
    if selected[-1] != raw[-1]:
        selected.append(raw[-1])
    return selected


def clamped_cubic_bspline(points: List[Point2], samples_per_span: int) -> List[Point2]:
    """Sample a clamped cubic uniform B-spline through endpoint-repeated controls."""
    if len(points) < 4:
        return points

    controls = [points[0], points[0], points[0], *points, points[-1], points[-1], points[-1]]
    result: List[Point2] = []
    for index in range(len(controls) - 3):
        for sample in range(samples_per_span):
            t = sample / float(samples_per_span)
            weights = cubic_basis(t)
            x = sum(weights[offset] * controls[index + offset][0] for offset in range(4))
            y = sum(weights[offset] * controls[index + offset][1] for offset in range(4))
            result.append((x, y))
    result.append(points[-1])
    return result


class BSplinePathDiagnostic(Node):
    def __init__(self) -> None:
        super().__init__("bspline_path_diagnostic")
        self.declare_parameter("control_point_stride", 6)
        self.declare_parameter("samples_per_span", 6)
        self.subscription = self.create_subscription(Path, "/plan", self.on_plan, 10)
        self.publisher = self.create_publisher(Path, "/plan_bspline", 10)
        self.get_logger().info("Waiting for Nav2 global plans on /plan; output is /plan_bspline only.")

    def on_plan(self, plan: Path) -> None:
        stride = max(1, int(self.get_parameter("control_point_stride").value))
        samples = max(2, int(self.get_parameter("samples_per_span").value))
        controls = select_control_points(plan.poses, stride)
        samples_xy = clamped_cubic_bspline(controls, samples)
        if len(samples_xy) < 2:
            return

        output = Path()
        output.header = plan.header
        for index, (x, y) in enumerate(samples_xy):
            pose = PoseStamped()
            pose.header = plan.header
            pose.pose.position.x = x
            pose.pose.position.y = y
            pose.pose.orientation.w = 1.0
            if index + 1 < len(samples_xy):
                next_x, next_y = samples_xy[index + 1]
                heading = math.atan2(next_y - y, next_x - x)
                pose.pose.orientation.z = math.sin(heading / 2.0)
                pose.pose.orientation.w = math.cos(heading / 2.0)
            output.poses.append(pose)

        self.publisher.publish(output)
        self.get_logger().info(
            f"Published {len(output.poses)} B-spline samples from {len(plan.poses)} Nav2 path poses."
        )


def main() -> None:
    rclpy.init()
    node = BSplinePathDiagnostic()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
