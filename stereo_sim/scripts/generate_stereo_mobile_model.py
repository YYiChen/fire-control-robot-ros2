#!/usr/bin/env python3
"""Build a temporary TurtleBot3 Burger SDF with the existing stereo sensor."""

from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from ament_index_python.packages import get_package_share_directory


def generate_model(project_root, output):
    package = Path(get_package_share_directory('turtlebot3_gazebo'))
    original = package / 'models' / 'turtlebot3_burger' / 'model.sdf'
    camera = project_root / 'models' / 'stereo_rig' / 'model.sdf'
    robot_tree = ET.parse(original)
    camera_tree = ET.parse(camera)
    robot = robot_tree.find('./model')
    base = robot.find("./link[@name='base_link']")
    stereo = camera_tree.find("./model/link/sensor[@name='stereo_rig']")
    if robot is None or base is None or stereo is None:
        raise ValueError('TurtleBot3 base or stereo sensor is missing')
    robot.set('name', 'stereo_mobile_bot')
    robot_tree.getroot().set('version', '1.7')
    pose = stereo.find('pose')
    if pose is None:
        pose = ET.SubElement(stereo, 'pose')
    pose.text = '0 0 0.50 0 0 0'
    base.append(stereo)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Humble spawn_entity.py parses file text as Unicode and rejects an XML
    # encoding declaration even when the same XML parses as bytes.
    robot_tree.write(output, encoding='unicode', xml_declaration=False)
    return output


def main():
    project_root = Path(__file__).resolve().parents[1]
    output = Path(sys.argv[1]).expanduser() if len(sys.argv) > 1 else (
        Path.home() / 'stereo_sim_generated' / 'stereo_mobile_bot.sdf')
    print(generate_model(project_root, output))


if __name__ == '__main__':
    main()
