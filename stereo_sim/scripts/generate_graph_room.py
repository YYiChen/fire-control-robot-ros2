#!/usr/bin/env python3
"""Add distinct visual landmarks to a copied, isolated stereo test world."""

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


PATTERNS = (
    ('left', -0.235, ('1100', '1001', '0110', '0011')),
    ('right', 0.235, ('1011', '0010', '1101', '0100')),
)


def add_pattern(world, name, y, rows):
    model = ET.SubElement(world, 'model', {'name': f'graph_landmark_{name}'})
    ET.SubElement(model, 'static').text = 'true'
    ET.SubElement(model, 'pose').text = f'0.58 {y} 0.67 0 0 0'
    link = ET.SubElement(model, 'link', {'name': 'pattern'})
    base = ET.SubElement(link, 'visual', {'name': 'white_backing'})
    ET.SubElement(base, 'pose').text = '0 0 0 0 0 0'
    geometry = ET.SubElement(base, 'geometry')
    ET.SubElement(ET.SubElement(geometry, 'box'), 'size').text = '0.004 0.132 0.132'
    material = ET.SubElement(base, 'material')
    ET.SubElement(material, 'ambient').text = '1 1 1 1'
    ET.SubElement(material, 'diffuse').text = '1 1 1 1'
    for row, bits in enumerate(rows):
        for col, bit in enumerate(bits):
            if bit != '1':
                continue
            visual = ET.SubElement(link, 'visual', {'name': f'black_{row}_{col}'})
            ET.SubElement(visual, 'pose').text = (
                f'-0.003 {(-1.5 + col) * 0.03:.3f} {(1.5 - row) * 0.03:.3f} 0 0 0')
            geometry = ET.SubElement(visual, 'geometry')
            ET.SubElement(ET.SubElement(geometry, 'box'), 'size').text = '0.002 0.03 0.03'
            material = ET.SubElement(visual, 'material')
            ET.SubElement(material, 'ambient').text = '0 0 0 1'
            ET.SubElement(material, 'diffuse').text = '0 0 0 1'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--template', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    tree = ET.parse(args.template)
    world = tree.find('world')
    if world is None:
        raise ValueError('world element missing')
    for name, y, rows in PATTERNS:
        add_pattern(world, name, y, rows)
    ET.indent(tree, space='  ')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    tree.write(args.output, encoding='utf-8', xml_declaration=True)
    print(f'Generated isolated visual-landmark world: {args.output}')


if __name__ == '__main__':
    main()
