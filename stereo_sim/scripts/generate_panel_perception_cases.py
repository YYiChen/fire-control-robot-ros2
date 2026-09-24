#!/usr/bin/env python3
"""Deterministic synthetic Chinese panel images with separate scoring truth."""

import argparse
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont


WIDTH, HEIGHT = 960, 540
MARKER_LEFT, MARKER_TOP, MARKER_SIZE = 760, 24, 84
MARKER_CENTER = (MARKER_LEFT + MARKER_SIZE / 2, MARKER_TOP + MARKER_SIZE / 2)
ROWS = [('火警', 145, 'red'), ('故障', 275, 'yellow'), ('主电工作', 405, 'green')]
LED_X, TEXT_X = 240, 300
CASES = [
    ('all_off', (False, False, False), 'normal'),
    ('fire_on', (True, False, False), 'normal'),
    ('fault_on', (False, True, False), 'normal'),
    ('power_on', (False, False, True), 'normal'),
    ('all_on', (True, True, True), 'normal'),
    ('dim_fire', (True, False, True), 'dim'),
    ('tilted_fire', (True, False, True), 'tilt'),
    ('glare_fire', (True, False, True), 'glare'),
    ('led_occluded', (True, False, True), 'led_occluded'),
    ('text_occluded', (True, False, True), 'text_occluded'),
    ('marker_occluded', (True, False, True), 'marker_occluded'),
]
COLORS = {'red': (235, 30, 30), 'yellow': (250, 205, 25),
          'green': (30, 210, 55)}


def marker_image():
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_ARUCO_ORIGINAL)
    if hasattr(cv2.aruco, 'generateImageMarker'):
        return cv2.aruco.generateImageMarker(dictionary, 582, MARKER_SIZE)
    return cv2.aruco.drawMarker(dictionary, 582, MARKER_SIZE)


def make_case(states, variant, font, marker):
    canvas = Image.new('RGB', (WIDTH, HEIGHT), (150, 150, 150))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((75, 12, 885, 520), radius=10,
                           fill=(211, 213, 213), outline=(35, 35, 35), width=5)
    draw.rectangle((MARKER_LEFT - 12, MARKER_TOP - 12,
                    MARKER_LEFT + MARKER_SIZE + 12, MARKER_TOP + MARKER_SIZE + 12),
                   fill=(255, 255, 255))
    canvas.paste(Image.fromarray(marker).convert('RGB'), (MARKER_LEFT, MARKER_TOP))
    for (label, y, color), on in zip(ROWS, states):
        draw.ellipse((LED_X - 25, y - 25, LED_X + 25, y + 25), fill=(20, 20, 20))
        draw.ellipse((LED_X - 19, y - 19, LED_X + 19, y + 19),
                     fill=COLORS[color] if on else (43, 43, 43))
        draw.text((TEXT_X, y - 38), label, font=font, fill=(12, 12, 12))
    image = np.asarray(canvas).copy()
    homography = np.eye(3, dtype=np.float32)
    if variant == 'dim':
        image = np.clip(image.astype(np.float32) * 0.62, 0, 255).astype(np.uint8)
    elif variant == 'tilt':
        source = np.float32([[75, 12], [885, 12], [885, 520], [75, 520]])
        target = np.float32([[95, 55], [834, 22], [880, 486], [105, 510]])
        homography = cv2.getPerspectiveTransform(source, target)
        image = cv2.warpPerspective(image, homography, (WIDTH, HEIGHT),
                                    borderValue=(150, 150, 150))
    elif variant == 'glare':
        glare = image.copy()
        cv2.ellipse(glare, (610, 245), (165, 65), -25, 0, 360,
                    (245, 245, 245), -1)
        image = cv2.addWeighted(image, 0.60, glare, 0.40, 0)
    elif variant == 'led_occluded':
        cv2.rectangle(image, (205, 110), (272, 180), (135, 135, 135), -1)
    elif variant == 'text_occluded':
        cv2.rectangle(image, (295, 105), (450, 185), (135, 135, 135), -1)
    elif variant == 'marker_occluded':
        cv2.rectangle(image, (738, 10), (868, 124), (135, 135, 135), -1)
    truth = []
    for (label, y, color), on in zip(ROWS, states):
        point = cv2.perspectiveTransform(
            np.float32([[[LED_X, y]]]), homography)[0, 0]
        visible_text = not (variant == 'text_occluded' and label == '火警')
        visible_led = not (variant == 'led_occluded' and label == '火警')
        truth.append({'label': label, 'led_state': color if on else 'off',
                      'led_center_px': [round(float(point[0]), 2),
                                        round(float(point[1]), 2)],
                      'visible_text': visible_text, 'visible_led': visible_led,
                      'marker_local_xyz_m': [round((LED_X - MARKER_CENTER[0]) / 1000, 4),
                                             round((MARKER_CENTER[1] - y) / 1000, 4), 0.0]})
    return image, truth


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--font', type=Path, default=Path(
        '/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype(str(args.font), 58)
    marker = marker_image()
    manifest = {'schema': 'synthetic_panel_v1', 'domain': 'synthetic_only',
                'marker_id': 582, 'marker_side_m': 0.084, 'cases': []}
    for name, states, variant in CASES:
        image, truth = make_case(states, variant, font, marker)
        path = args.output / f'{name}.png'
        cv2.imwrite(str(path), cv2.cvtColor(image, cv2.COLOR_RGB2BGR))
        manifest['cases'].append({'id': name, 'image': path.name,
                                  'variant': variant,
                                  'marker_visible': variant != 'marker_occluded',
                                  'truth': truth})
    (args.output / 'truth.json').write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Generated {len(CASES)} synthetic cases in {args.output}')


if __name__ == '__main__':
    main()
