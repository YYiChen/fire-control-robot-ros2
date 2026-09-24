#!/usr/bin/env python3
"""Score real OCR boxes and image-derived LEDs on synthetic panel cases."""

import argparse
import csv
import difflib
import io
import json
import math
from pathlib import Path
import subprocess
import tempfile
import time

import cv2
import numpy as np


LABELS = ('火警', '故障', '主电工作')
MARKER_CANONICAL = np.float32([[760, 24], [844, 24], [844, 108], [760, 108]])
MARKER_CENTER = (802.0, 66.0)


def read_ocr(bgr, tesseract, tessdata, offset=(0, 0)):
    encoded = cv2.imencode('.png', bgr)[1].tobytes()
    command = [str(tesseract), 'stdin', 'stdout',
               '--tessdata-dir', str(tessdata), '-l', 'chi_sim+eng',
               '--psm', '7', '-c', 'tessedit_create_tsv=1']
    result = subprocess.run(command, input=encoded, capture_output=True,
                            check=True, timeout=30)
    groups = {}
    for row in csv.DictReader(io.StringIO(result.stdout.decode('utf-8')),
                              delimiter='\t'):
        try:
            confidence = float(row['conf'])
        except (TypeError, ValueError):
            continue
        if (confidence < 20 or not row['text'].strip() or
                not any('\u4e00' <= char <= '\u9fff' for char in row['text'])):
            continue
        key = (row['block_num'], row['par_num'], row['line_num'])
        groups.setdefault(key, []).append(row)
    lines = []
    for words in groups.values():
        words.sort(key=lambda item: int(item['left']))
        text = ''.join(item['text'].strip().replace(' ', '') for item in words)
        x0 = offset[0] + min(int(item['left']) for item in words)
        y0 = offset[1] + min(int(item['top']) for item in words)
        x1 = offset[0] + max(int(item['left']) + int(item['width']) for item in words)
        y1 = offset[1] + max(int(item['top']) + int(item['height']) for item in words)
        lines.append({'text': text,
                      'box': [round(x0), round(y0), round(x1), round(y1)],
                      'confidence': round(float(np.mean(
                          [float(item['conf']) for item in words])), 1)})
    return lines


def read_ocr_multipage(images, tesseract, tessdata):
    """Run the same PSM 7 OCR separately on multiple TIFF pages in one process."""
    with tempfile.NamedTemporaryFile(suffix='.tiff', delete=False) as stream:
        path = Path(stream.name)
    try:
        if not cv2.imwritemulti(str(path), images):
            raise RuntimeError(f'Cannot encode OCR pages to {path}')
        command = [str(tesseract), str(path), 'stdout',
                   '--tessdata-dir', str(tessdata), '-l', 'chi_sim+eng',
                   '--psm', '7', '-c', 'tessedit_create_tsv=1']
        result = subprocess.run(command, capture_output=True, check=True,
                                timeout=30)
    finally:
        path.unlink(missing_ok=True)

    groups = {}
    for row in csv.DictReader(io.StringIO(result.stdout.decode('utf-8')),
                              delimiter='\t'):
        try:
            confidence = float(row['conf'])
            page_num = int(row['page_num'])
        except (TypeError, ValueError):
            continue
        if (confidence < 20 or not row['text'].strip() or
                not any('\u4e00' <= char <= '\u9fff' for char in row['text'])):
            continue
        key = (page_num, row['block_num'], row['par_num'], row['line_num'])
        groups.setdefault(key, []).append(row)
    lines = []
    for (page_num, _block, _paragraph, _line), words in groups.items():
        words.sort(key=lambda item: int(item['left']))
        text = ''.join(item['text'].strip().replace(' ', '') for item in words)
        x0 = min(int(item['left']) for item in words)
        y0 = min(int(item['top']) for item in words)
        x1 = max(int(item['left']) + int(item['width']) for item in words)
        y1 = max(int(item['top']) + int(item['height']) for item in words)
        lines.append({'page_num': page_num, 'text': text,
                      'box': [round(x0), round(y0), round(x1), round(y1)],
                      'confidence': round(float(np.mean(
                          [float(item['conf']) for item in words])), 1)})
    return lines


def label_from_text(text):
    cleaned = ''.join(char for char in text if '\u4e00' <= char <= '\u9fff')
    for label in LABELS:
        if label in cleaned:
            return label
    best = max(LABELS, key=lambda label: difflib.SequenceMatcher(
        None, cleaned, label).ratio())
    score = difflib.SequenceMatcher(None, cleaned, best).ratio()
    return best if score >= 0.75 else None


def marker_homography(bgr):
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_ARUCO_ORIGINAL)
    corners, ids, _ = cv2.aruco.detectMarkers(bgr, dictionary)
    if ids is None:
        return None
    matches = np.flatnonzero(ids.ravel() == 582)
    if len(matches) != 1:
        return None
    observed = corners[int(matches[0])].reshape(4, 2).astype(np.float32)
    refined = observed.reshape(4, 1, 2).copy()
    try:
        cv2.cornerSubPix(
            cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY), refined, (5, 5), (-1, -1),
            (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 40, 0.001))
    except cv2.error:
        return None
    refined = refined.reshape(4, 2)
    if (not np.isfinite(refined).all() or
            np.max(np.linalg.norm(refined - observed, axis=1)) > 2.0):
        return None
    return cv2.getPerspectiveTransform(refined.astype(np.float32),
                                       MARKER_CANONICAL)


def classify_led(bgr, text_box):
    x0, y0, _x1, y1 = text_box
    height = y1 - y0
    middle_y = (y0 + y1) / 2
    left = max(0, x0 - int(2.6 * height))
    right = max(left, x0 - max(8, int(0.25 * height)))
    top = max(0, int(middle_y - 0.85 * height))
    bottom = min(bgr.shape[0], int(middle_y + 0.85 * height))
    roi = bgr[top:bottom, left:right]
    if roi.size == 0:
        return 'unknown', None, 'empty_roi'
    gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
    dark = cv2.threshold(gray, 88, 255, cv2.THRESH_BINARY_INV)[1]
    contours, _ = cv2.findContours(dark, cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_SIMPLE)
    choices = []
    for contour in contours:
        area = cv2.contourArea(contour)
        if not 180 <= area <= 6000:
            continue
        bx, by, width, height = cv2.boundingRect(contour)
        if not 0.65 <= width / max(height, 1) <= 1.5:
            continue
        (cx, cy), radius = cv2.minEnclosingCircle(contour)
        circularity = area / max(math.pi * radius * radius, 1)
        if circularity < 0.48 or radius < 10:
            continue
        alignment = abs((top + cy) - middle_y)
        choices.append((alignment, -area, cx, cy, radius))
    if not choices:
        return 'unknown', None, 'no_led_outline'
    _alignment, _area, cx, cy, radius = min(choices)
    radius = max(2, int(radius * 0.60))
    yy, xx = np.ogrid[:roi.shape[0], :roi.shape[1]]
    mask = (xx - cx) ** 2 + (yy - cy) ** 2 <= radius * radius
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    pixels = hsv[mask]
    if len(pixels) < 25:
        return 'unknown', None, 'too_few_led_pixels'
    sat = float(np.median(pixels[:, 1]))
    value = float(np.median(pixels[:, 2]))
    centre = [round(left + float(cx), 2), round(top + float(cy), 2)]
    if value < 83 or sat < 48:
        return 'off', centre, 'dark_or_desaturated'
    hue = float(np.median(pixels[:, 0]))
    if hue <= 12 or hue >= 170:
        state = 'red'
    elif 16 <= hue <= 38:
        state = 'yellow'
    elif 39 <= hue <= 92:
        state = 'green'
    else:
        state = 'unknown'
    return state, centre, f'hsv_median={hue:.1f},{sat:.1f},{value:.1f}'


def associate_leds(rectified, ocr):
    predictions = {}
    for item in ocr:
        label = label_from_text(item['text'])
        if label is None or label in predictions:
            continue
        state, centre, reason = classify_led(rectified, item['box'])
        local = None
        if centre is not None:
            local = [round((centre[0] - MARKER_CENTER[0]) / 1000, 4),
                     round((MARKER_CENTER[1] - centre[1]) / 1000, 4), 0.0]
        predictions[label] = {'ocr_text': item['text'], 'ocr_box': item['box'],
                              'ocr_confidence': item['confidence'],
                              'led_state': state, 'led_center_px': centre,
                              'marker_local_xyz_m': local, 'reason': reason}
    return predictions


def infer(image_path, tesseract, tessdata, timing=None):
    started = time.perf_counter()
    bgr = cv2.imread(str(image_path))
    if bgr is None:
        raise ValueError(f'Cannot read {image_path}')
    homography = marker_homography(bgr)
    if homography is None:
        if timing is not None:
            timing.update({'ocr_sec': 0.0, 'total_sec': time.perf_counter() - started})
        return [], False, {}
    rectified = cv2.warpPerspective(bgr, homography, (960, 540),
                                    borderValue=(150, 150, 150))
    # Only fixed panel geometry is used here. The recognized label still comes
    # from OCR, and the LED search is driven by its detected text box.
    ocr = []
    ocr_started = time.perf_counter()
    for row_y in (145, 275, 405):
        x0, y0, x1, y1 = 290, row_y - 53, 700, row_y + 56
        ocr.extend(read_ocr(rectified[y0:y1, x0:x1], tesseract,
                            tessdata, offset=(x0, y0)))
    if timing is not None:
        timing['ocr_sec'] = time.perf_counter() - ocr_started
        timing['total_sec'] = time.perf_counter() - started
    predictions = associate_leds(rectified, ocr)
    return ocr, homography is not None, predictions


def infer_multipage(image_path, tesseract, tessdata, timing=None):
    """Infer panel labels with one PSM 7 process over three separate TIFF pages."""
    started = time.perf_counter()
    bgr = cv2.imread(str(image_path))
    if bgr is None:
        raise ValueError(f'Cannot read {image_path}')
    homography = marker_homography(bgr)
    if homography is None:
        if timing is not None:
            timing.update({'ocr_sec': 0.0, 'total_sec': time.perf_counter() - started})
        return [], False, {}
    rectified = cv2.warpPerspective(bgr, homography, (960, 540),
                                    borderValue=(150, 150, 150))
    x0, x1 = 290, 700
    pages = [rectified[row_y - 53:row_y + 56, x0:x1]
             for row_y in (145, 275, 405)]
    ocr_started = time.perf_counter()
    page_lines = read_ocr_multipage(pages, tesseract, tessdata)
    ocr = []
    for item in page_lines:
        page_index = item['page_num'] - 1
        if not 0 <= page_index < 3:
            continue
        row_y = (145, 275, 405)[page_index]
        y0 = row_y - 53
        bx0, by0, bx1, by1 = item['box']
        ocr.append({'text': item['text'],
                    'box': [bx0 + x0, by0 + y0, bx1 + x0, by1 + y0],
                    'confidence': item['confidence']})
    if timing is not None:
        timing['ocr_sec'] = time.perf_counter() - ocr_started
        timing['total_sec'] = time.perf_counter() - started
    return ocr, True, associate_leds(rectified, ocr)


def evaluate(manifest, root, tesseract, tessdata):
    cases = []
    counts = {'visible_text': 0, 'recognized_text': 0,
              'hidden_text_false_positive': 0, 'visible_led': 0,
              'correct_led_state': 0, 'hidden_led_false_certainty': 0,
              'position_samples': 0, 'marker_absence_rejected': 0}
    position_errors = []
    for case in manifest['cases']:
        ocr, marker_found, predictions = infer(root / case['image'],
                                                tesseract, tessdata)
        errors = []
        if not case.get('marker_visible', True):
            if marker_found or predictions:
                errors.append('marker_absent_but_predictions_published')
            else:
                counts['marker_absence_rejected'] += 1
            cases.append({'id': case['id'], 'variant': case['variant'],
                          'marker_found': marker_found, 'ocr_raw': ocr,
                          'truth': case['truth'], 'predictions': predictions,
                          'errors': errors})
            continue
        for truth in case['truth']:
            name = truth['label']
            predicted = predictions.get(name)
            if truth['visible_text']:
                counts['visible_text'] += 1
                if predicted:
                    counts['recognized_text'] += 1
                else:
                    errors.append(f'{name}:text_missing')
            elif predicted:
                counts['hidden_text_false_positive'] += 1
                errors.append(f'{name}:hidden_text_false_positive')
            if truth['visible_text'] and truth['visible_led']:
                counts['visible_led'] += 1
                if predicted and predicted['led_state'] == truth['led_state']:
                    counts['correct_led_state'] += 1
                else:
                    errors.append(f'{name}:led_state')
                if predicted and predicted['marker_local_xyz_m'] is not None:
                    distance = math.dist(predicted['marker_local_xyz_m'],
                                         truth['marker_local_xyz_m'])
                    position_errors.append(distance)
                    counts['position_samples'] += 1
            elif truth['visible_text'] and not truth['visible_led']:
                if predicted and predicted['led_state'] not in ('unknown',):
                    counts['hidden_led_false_certainty'] += 1
                    errors.append(f'{name}:hidden_led_false_certainty')
        cases.append({'id': case['id'], 'variant': case['variant'],
                      'marker_found': marker_found, 'ocr_raw': ocr,
                      'truth': case['truth'], 'predictions': predictions,
                      'errors': errors})
    summary = {'domain': 'synthetic_only', 'ocr_backend': 'tesseract_4_chi_sim',
               'prediction_pixel_frame': 'rectified_panel_960x540',
               'truth_pixel_frame': 'generated_camera_image_960x540',
               'cases': len(cases),
               'perception_scored_cases': sum(
                   case.get('marker_visible', True) for case in manifest['cases']),
               **counts,
               'text_recall': round(counts['recognized_text'] /
                                    max(counts['visible_text'], 1), 4),
               'led_state_accuracy': round(counts['correct_led_state'] /
                                           max(counts['visible_led'], 1), 4),
               'median_marker_local_error_m': (round(float(np.median(position_errors)), 4)
                                               if position_errors else None),
               'failure_case_ids': [case['id'] for case in cases if case['errors']]}
    return {'summary': summary, 'cases': cases}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dataset', type=Path, required=True)
    parser.add_argument('--tesseract', type=Path, required=True)
    parser.add_argument('--tessdata', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.dataset / 'truth.json').read_text(encoding='utf-8'))
    report = evaluate(manifest, args.dataset, args.tesseract, args.tessdata)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                           encoding='utf-8')
    print(json.dumps(report['summary'], ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
