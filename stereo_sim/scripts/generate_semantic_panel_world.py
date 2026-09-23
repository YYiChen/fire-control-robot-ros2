#!/usr/bin/env python3
"""Create a reproducible semantic-panel view without editing the base world."""

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--template', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--panel-x', type=float, default=0.60)
    parser.add_argument('--panel-y', type=float, default=0.0)
    parser.add_argument('--panel-yaw', type=float, default=0.0)
    args = parser.parse_args()
    if not 0.45 <= args.panel_x <= 1.0 or abs(args.panel_y) > 0.12 or abs(args.panel_yaw) > 0.20:
        parser.error('panel must remain within the stereo test camera field of view')
    tree = ET.parse(args.template)
    panel = next((item for item in tree.findall('.//include')
                  if item.findtext('uri') == 'model://semantic_panel'), None)
    if panel is None:
        raise ValueError('semantic_panel include not found')
    panel.find('pose').text = f'{args.panel_x:.4f} {args.panel_y:.4f} 0.50 0 0 {args.panel_yaw:.6f}'
    ET.indent(tree, space='  ')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    tree.write(args.output, encoding='utf-8', xml_declaration=True)
    print(f'Generated semantic panel at x={args.panel_x:.3f}, y={args.panel_y:.3f}, yaw={args.panel_yaw:.3f}')


if __name__ == '__main__':
    main()
