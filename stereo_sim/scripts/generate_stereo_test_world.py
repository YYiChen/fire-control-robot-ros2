#!/usr/bin/env python3
"""Generate a known-distance copy of the stereo test world."""

import argparse
import xml.etree.ElementTree as element_tree


def model_by_name(root, name):
    for model in root.findall('.//model'):
        if model.get('name') == name:
            return model
    raise ValueError(f'model not found: {name}')


def set_pose(model, x_position):
    pose = model.find('pose')
    if pose is None:
        raise ValueError(f'pose not found for {model.get("name")}')
    values = pose.text.split()
    if len(values) != 6:
        raise ValueError(f'unexpected pose format for {model.get("name")}')
    values[0] = f'{x_position:.2f}'
    pose.text = ' '.join(values)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--template', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--distance-m', required=True, type=float)
    args = parser.parse_args()
    if not 0.20 <= args.distance_m <= 1.50:
        raise ValueError('distance must be within 0.20 to 1.50 m')

    tree = element_tree.parse(args.template)
    root = tree.getroot()
    panel = model_by_name(root, 'distance_panel_0_60m')
    panel.set('name', f'distance_panel_{args.distance_m:.2f}m')
    set_pose(panel, args.distance_m)
    set_pose(model_by_name(root, 'left_marker'), args.distance_m - 0.025)
    set_pose(model_by_name(root, 'right_marker'), args.distance_m - 0.025)
    element_tree.indent(tree, space='  ')
    tree.write(args.output, encoding='utf-8', xml_declaration=True)


if __name__ == '__main__':
    main()
