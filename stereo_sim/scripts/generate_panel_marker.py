#!/usr/bin/env python3
"""Embed ArUco Original ID 582 in the SDF as ordinary black/white geometry."""

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

import cv2


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('model_sdf', type=Path)
    args = parser.parse_args()
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_ARUCO_ORIGINAL)
    cells_per_side = dictionary.markerSize + 2
    pixels = cells_per_side * 40
    if hasattr(cv2.aruco, 'generateImageMarker'):
        image = cv2.aruco.generateImageMarker(dictionary, 582, pixels, borderBits=1)
    else:
        image = cv2.aruco.drawMarker(dictionary, 582, pixels, borderBits=1)

    tree = ET.parse(args.model_sdf)
    link = tree.find('.//model/link')
    for visual in list(link.findall('visual')):
        if visual.get('name', '').startswith('aruco_'):
            link.remove(visual)

    cell = 0.012
    side = cells_per_side * cell
    plate = ET.SubElement(link, 'visual', name='aruco_white_plate')
    ET.SubElement(plate, 'pose').text = f'-0.019 0 -0.075 0 0 0'
    ET.SubElement(ET.SubElement(ET.SubElement(plate, 'geometry'), 'box'), 'size').text = f'0.002 {side} {side}'
    material = ET.SubElement(plate, 'material')
    ET.SubElement(material, 'ambient').text = '1 1 1 1'
    ET.SubElement(material, 'diffuse').text = '1 1 1 1'

    for row in range(cells_per_side):
        for col in range(cells_per_side):
            if int(image[row * 40 + 20, col * 40 + 20]) > 127:
                continue
            visual = ET.SubElement(link, 'visual', name=f'aruco_black_{row}_{col}')
            y = (cells_per_side / 2 - col - 0.5) * cell
            z = (cells_per_side / 2 - row - 0.5) * cell - 0.075
            ET.SubElement(visual, 'pose').text = f'-0.021 {y:.6f} {z:.6f} 0 0 0'
            ET.SubElement(ET.SubElement(ET.SubElement(visual, 'geometry'), 'box'), 'size').text = f'0.002 {cell} {cell}'
            material = ET.SubElement(visual, 'material')
            ET.SubElement(material, 'ambient').text = '0 0 0 1'
            ET.SubElement(material, 'diffuse').text = '0 0 0 1'

    ET.indent(tree, space='  ')
    tree.write(args.model_sdf, encoding='utf-8', xml_declaration=True)
    print(f'Embedded ArUco Original 582 as {cells_per_side}x{cells_per_side} cells.')


if __name__ == '__main__':
    main()
