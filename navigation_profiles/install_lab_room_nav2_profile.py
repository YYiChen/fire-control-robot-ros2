#!/usr/bin/env python3
"""Create an A* Nav2 profile from the TurtleBot3 profile installed in this WSL distro.

This script does not modify files under /opt/ros.  It copies the exact installed
Burger profile first, then changes only GridBased.use_astar.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def ros_package_prefix(package: str) -> Path:
    try:
        output = subprocess.check_output(
            ["ros2", "pkg", "prefix", package], text=True, stderr=subprocess.STDOUT
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise RuntimeError(f"Cannot locate ROS package '{package}': {error}") from error
    return Path(output.strip())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--workspace",
        type=Path,
        default=Path.home() / "ros2_ws",
        help="ROS 2 workspace that holds the copied profile (default: ~/ros2_ws)",
    )
    arguments = parser.parse_args()

    try:
        import yaml
    except ImportError:
        print("Missing PyYAML. Install it with: sudo apt install python3-yaml", file=sys.stderr)
        return 2

    package_prefix = ros_package_prefix("turtlebot3_navigation2")
    source_profile = package_prefix / "share" / "turtlebot3_navigation2" / "param" / "burger.yaml"
    if not source_profile.is_file():
        print(f"Installed Burger profile was not found: {source_profile}", file=sys.stderr)
        return 3

    destination_dir = arguments.workspace.expanduser() / "src" / "lab_room_navigation_profiles" / "generated"
    destination_dir.mkdir(parents=True, exist_ok=True)
    destination_profile = destination_dir / "lab_room_nav2_astar.yaml"
    diagnostic_source = Path(__file__).with_name("bspline_path_diagnostic.py")
    diagnostic_destination = destination_dir.parent / diagnostic_source.name

    with source_profile.open("r", encoding="utf-8") as source_file:
        profile = yaml.safe_load(source_file)

    try:
        grid_based = profile["planner_server"]["ros__parameters"]["GridBased"]
    except (KeyError, TypeError) as error:
        print("The installed Burger profile does not have planner_server.GridBased.", file=sys.stderr)
        return 4

    plugin = grid_based.get("plugin")
    if plugin != "nav2_navfn_planner::NavfnPlanner":
        print(
            "The installed profile uses a different planner plugin "
            f"({plugin!r}); no automatic change was made.",
            file=sys.stderr,
        )
        return 5

    grid_based["use_astar"] = True
    profile["lab_room_profile_metadata"] = {
        "ros__parameters": {
            "purpose": "A-star baseline copied from the installed TurtleBot3 Burger profile",
            "source_profile": str(source_profile),
            "bspline_note": "The separate /plan_bspline topic is diagnostic only and never controls cmd_vel.",
        }
    }

    with destination_profile.open("w", encoding="utf-8") as destination_file:
        yaml.safe_dump(profile, destination_file, sort_keys=False, allow_unicode=True)

    shutil.copy2(diagnostic_source, diagnostic_destination)
    diagnostic_destination.chmod(0o755)

    print(f"Created: {destination_profile}")
    print(f"Copied diagnostic node: {diagnostic_destination}")
    print("A* verification:")
    print("  ros2 param get /planner_server GridBased.use_astar")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
