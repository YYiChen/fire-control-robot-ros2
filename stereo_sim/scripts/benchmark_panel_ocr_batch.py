#!/usr/bin/env python3
"""Compare three per-row Tesseract calls with one stacked-image call."""

import argparse
import csv
import hashlib
import io
import json
import math
import statistics
import subprocess
import time
from pathlib import Path

import cv2
import numpy as np

import evaluate_panel_perception as panel


ROWS = (145, 275, 405)
ROI = (290, 700)
ROI_HALF_HEIGHT = 53
STACK_GAP = 20
FULL_ROI_Y = (ROWS[0] - ROI_HALF_HEIGHT, ROWS[-1] + ROI_HALF_HEIGHT + 3)


def ocr_once(bgr, tesseract, tessdata, psm):
    encoded = cv2.imencode('.png', bgr)[1].tobytes()
    command = [str(tesseract), 'stdin', 'stdout', '--tessdata-dir',
               str(tessdata), '-l', 'chi_sim+eng', '--psm', str(psm),
               '-c', 'tessedit_create_tsv=1']
    result = subprocess.run(command, input=encoded, capture_output=True,
                            check=True, timeout=30)
    return parse_tsv(result.stdout)


def parse_tsv(payload):
    groups = {}
    for row in csv.DictReader(io.StringIO(payload.decode('utf-8')),
                              delimiter='\t'):
        try:
            confidence = float(row['conf'])
        except (TypeError, ValueError):
            continue
        if (confidence < 20 or not row['text'].strip() or
                not any('\u4e00' <= char <= '\u9fff' for char in row['text'])):
            continue
        key = (row.get('page_num', '1'), row['block_num'],
               row['par_num'], row['line_num'])
        groups.setdefault(key, []).append(row)
    lines = []
    for words in groups.values():
        words.sort(key=lambda item: int(item['left']))
        text = ''.join(item['text'].strip().replace(' ', '') for item in words)
        x0 = min(int(item['left']) for item in words)
        y0 = min(int(item['top']) for item in words)
        x1 = max(int(item['left']) + int(item['width']) for item in words)
        y1 = max(int(item['top']) + int(item['height']) for item in words)
        lines.append({'page_num': int(words[0].get('page_num', '1')),
                      'text': text, 'box': [x0, y0, x1, y1],
                      'confidence': round(float(np.mean(
                          [float(item['conf']) for item in words])), 1)})
    return lines


def stacked_rows(rectified):
    x0, x1 = ROI
    height = 2 * ROI_HALF_HEIGHT + 3
    width = x1 - x0
    result = np.full((3 * height + 2 * STACK_GAP, width, 3), 150,
                     dtype=np.uint8)
    for index, row_y in enumerate(ROWS):
        y0, y1 = row_y - ROI_HALF_HEIGHT, row_y + ROI_HALF_HEIGHT + 3
        offset = index * (height + STACK_GAP)
        result[offset:offset + height] = rectified[y0:y1, x0:x1]
    return result


def unstack_boxes(lines):
    x0, _x1 = ROI
    height = 2 * ROI_HALF_HEIGHT + 3
    mapped = []
    for item in lines:
        bx0, by0, bx1, by1 = item['box']
        center_y = (by0 + by1) / 2
        index = int(center_y // (height + STACK_GAP))
        if not 0 <= index < len(ROWS):
            continue
        segment_offset = index * (height + STACK_GAP)
        if center_y - segment_offset >= height:
            continue
        y0 = ROWS[index] - ROI_HALF_HEIGHT
        mapped.append({**item, 'box': [bx0 + x0, by0 - segment_offset + y0,
                                       bx1 + x0, by1 - segment_offset + y0]})
    return mapped


def make_predictions(rectified, lines):
    predictions = {}
    for item in lines:
        label = panel.label_from_text(item['text'])
        if label is None or label in predictions:
            continue
        state, center, reason = panel.classify_led(rectified, item['box'])
        local = None
        if center is not None:
            local = [round((center[0] - panel.MARKER_CENTER[0]) / 1000, 4),
                     round((panel.MARKER_CENTER[1] - center[1]) / 1000, 4),
                     0.0]
        predictions[label] = {
            'ocr_text': item['text'], 'ocr_box': item['box'],
            'ocr_confidence': item['confidence'], 'led_state': state,
            'led_center_px': center, 'marker_local_xyz_m': local,
            'reason': reason}
    return predictions


def infer(image_path, variant, tesseract, tessdata):
    started = time.perf_counter()
    if variant in ('three_calls', 'one_call_multipage_psm7'):
        timing = {}
        operation = (panel.infer_multipage if variant == 'one_call_multipage_psm7'
                     else panel.infer)
        lines, marker_found, predictions = operation(
            Path(image_path), tesseract, tessdata, timing=timing)
        return {'marker_found': marker_found, 'ocr': lines,
                'predictions': predictions, 'ocr_sec': timing['ocr_sec'],
                'total_sec': time.perf_counter() - started}
    bgr = cv2.imread(str(image_path))
    if bgr is None:
        raise ValueError(f'Cannot read {image_path}')
    homography = panel.marker_homography(bgr)
    if homography is None:
        return {'marker_found': False, 'ocr': [], 'predictions': {},
                'ocr_sec': 0.0, 'total_sec': time.perf_counter() - started}
    rectified = cv2.warpPerspective(bgr, homography, (960, 540),
                                    borderValue=(150, 150, 150))
    ocr_started = time.perf_counter()
    if variant == 'one_call_stacked':
        lines = unstack_boxes(ocr_once(stacked_rows(rectified), tesseract,
                                       tessdata, psm=6))
    elif variant.startswith('full_roi_psm'):
        psm = int(variant.removeprefix('full_roi_psm'))
        x0, x1 = ROI
        y0, y1 = FULL_ROI_Y
        lines = ocr_once(rectified[y0:y1, x0:x1], tesseract, tessdata, psm=psm)
        for item in lines:
            bx0, by0, bx1, by1 = item['box']
            item['box'] = [bx0 + x0, by0 + y0, bx1 + x0, by1 + y0]
    elif variant == 'full_image_psm11':
        lines = ocr_once(rectified, tesseract, tessdata, psm=11)
    else:
        raise ValueError(f'Unknown variant: {variant}')
    ocr_sec = time.perf_counter() - ocr_started
    predictions = make_predictions(rectified, lines)
    return {'marker_found': True, 'ocr': lines, 'predictions': predictions,
            'ocr_sec': ocr_sec, 'total_sec': time.perf_counter() - started}


def percentile(values, percentile_value):
    if not values:
        return None
    return float(np.percentile(np.asarray(values, dtype=float), percentile_value))


def score(items, output_by_key):
    counts = {'visible_text': 0, 'recognized_text': 0,
              'visible_led': 0, 'led_center_detected': 0,
              'led_state_correct': 0, 'position_samples': 0,
              'marker_absence_rejected': 0, 'marker_absence_false_accept': 0}
    errors = []
    per_case = []
    for case in items:
        result = output_by_key[case['key']]
        predictions = result['predictions']
        case_errors = []
        if not case.get('marker_visible', True):
            if not result['marker_found'] and not predictions:
                counts['marker_absence_rejected'] += 1
            else:
                counts['marker_absence_false_accept'] += 1
                case_errors.append('marker_absence_false_accept')
            per_case.append({'key': case['key'], 'errors': case_errors,
                             'predictions': predictions})
            continue
        for truth in case['truth']:
            label = truth['label']
            predicted = predictions.get(label)
            if truth.get('visible_text', True):
                counts['visible_text'] += 1
                if predicted:
                    counts['recognized_text'] += 1
                else:
                    case_errors.append(f'{label}:text_missing')
            if truth.get('visible_text', True) and truth.get('visible_led', True):
                counts['visible_led'] += 1
                if predicted and predicted['led_center_px'] is not None:
                    counts['led_center_detected'] += 1
                    local = predicted['marker_local_xyz_m']
                    if truth.get('marker_local_xyz_m') is not None:
                        errors.append(math.dist(local,
                                                truth['marker_local_xyz_m']))
                        counts['position_samples'] += 1
                else:
                    case_errors.append(f'{label}:led_center_missing')
                if predicted and predicted['led_state'] == truth['led_state']:
                    counts['led_state_correct'] += 1
                else:
                    case_errors.append(f'{label}:led_state_wrong')
        per_case.append({'key': case['key'], 'errors': case_errors,
                         'predictions': predictions})
    return {
        'cases': len(items), **counts,
        'text_recall': counts['recognized_text'] / max(counts['visible_text'], 1),
        'led_center_detection_rate': (counts['led_center_detected'] /
                                      max(counts['visible_led'], 1)),
        'led_state_accuracy': counts['led_state_correct'] / max(counts['visible_led'], 1),
        'median_position_error_m': statistics.median(errors) if errors else None,
        'p95_position_error_m': percentile(errors, 95),
        'max_position_error_m': max(errors) if errors else None,
        'failure_case_count': sum(bool(row['errors']) for row in per_case),
        'per_case': per_case,
    }


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def comparable_result(result):
    ocr = sorted((item['text'], item['box'], item['confidence'])
                 for item in result['ocr'])
    return {'marker_found': result['marker_found'], 'ocr': ocr,
            'predictions': result['predictions']}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dataset', type=Path, required=True,
                        help='Directory containing truth.json and its images')
    parser.add_argument('--sweep-failure-images', type=Path, required=True)
    parser.add_argument('--tesseract', type=Path, required=True)
    parser.add_argument('--tessdata', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--variants', nargs='+', choices=(
        'three_calls', 'one_call_multipage_psm7', 'one_call_stacked',
        'full_roi_psm4', 'full_roi_psm6', 'full_roi_psm11',
        'full_image_psm11'))
    args = parser.parse_args()
    if args.repeats < 5:
        parser.error('--repeats must be at least 5')

    manifest = json.loads((args.dataset / 'truth.json').read_text(encoding='utf-8'))
    truth_by_name = {case['image']: case['truth'] for case in manifest['cases']}
    sources = {}
    for path in sorted(args.dataset.glob('*.png')):
        matching_case = next((case for case in manifest['cases']
                              if case['image'] == path.name), None)
        digest = sha256(path)
        sources.setdefault(digest, {'path': path, 'ids': [], 'truth': None,
                                    'marker_visible': True,
                                    'source_group': 'synthetic_truth_set'})
        sources[digest]['ids'].append(path.stem)
        if matching_case is not None:
            sources[digest]['truth'] = matching_case['truth']
            sources[digest]['marker_visible'] = matching_case.get(
                'marker_visible', True)

    sweep_sources = {}
    for path in sorted(args.sweep_failure_images.glob('*.png')):
        digest = sha256(path)
        sweep_sources.setdefault(digest, {'path': path, 'ids': [],
                                          'source_group': 'pose_sweep_failure_only'})
        sweep_sources[digest]['ids'].append(path.stem)
    fire_truth = truth_by_name['fire_on.png']
    for digest, item in sweep_sources.items():
        if digest not in sources:
            item['truth'] = fire_truth
            item['marker_visible'] = True
            sources[digest] = item

    images = []
    for digest, source in sources.items():
        images.append({'key': digest[:16], 'sha256': digest,
                       'path': str(source['path']), 'source_group': source['source_group'],
                       'ids': sorted(source['ids']), 'truth': source['truth'],
                       'marker_visible': source['marker_visible']})

    variants = tuple(dict.fromkeys(args.variants or (
        'three_calls', 'one_call_multipage_psm7', 'one_call_stacked',
        'full_roi_psm4', 'full_roi_psm6', 'full_roi_psm11',
        'full_image_psm11')))
    if 'three_calls' not in variants:
        variants = ('three_calls',) + variants
    baseline_outputs = {}
    for item in images:
        expected = panel.infer(Path(item['path']), args.tesseract, args.tessdata)
        measured = infer(item['path'], 'three_calls', args.tesseract, args.tessdata)
        if (expected[0] != measured['ocr'] or expected[1] != measured['marker_found'] or
                expected[2] != measured['predictions']):
            raise RuntimeError(f'benchmark baseline diverged from production evaluator: '
                               f"{item['key']} ({item['path']})")
        baseline_outputs[item['key']] = measured
    raw_runs = {variant: [] for variant in variants}
    outputs = {variant: {} for variant in variants}
    baseline_equivalence = {}
    for variant in variants:
        infer(images[0]['path'], variant, args.tesseract, args.tessdata)
        for item in images:
            samples = []
            for repeat_index in range(args.repeats):
                result = infer(item['path'], variant, args.tesseract, args.tessdata)
                samples.append(result)
                raw_runs[variant].append({
                    'image_key': item['key'], 'sha256': item['sha256'],
                    'source_group': item['source_group'], 'repeat': repeat_index + 1,
                    'marker_found': result['marker_found'],
                    'ocr_sec': result['ocr_sec'], 'total_sec': result['total_sec'],
                    'recognized': {label: result['predictions'].get(label, {}).get('ocr_text')
                                   for label in panel.LABELS},
                    'led_state': {label: result['predictions'].get(label, {}).get('led_state')
                                  for label in panel.LABELS},
                    'led_center_px': {label: result['predictions'].get(label, {}).get(
                        'led_center_px') for label in panel.LABELS},
                    'marker_local_xyz_m': {label: result['predictions'].get(label, {}).get(
                        'marker_local_xyz_m') for label in panel.LABELS}})
            outputs[variant][item['key']] = samples[-1]
        if variant == 'one_call_multipage_psm7':
            matches = [key for key, result in outputs[variant].items()
                       if comparable_result(result) ==
                       comparable_result(baseline_outputs[key])]
            baseline_equivalence = {
                'exact_image_matches': len(matches),
                'images_compared': len(images),
                'matching_image_keys': matches,
            }

    groups = {
        group: [item for item in images if item['source_group'] == group]
        for group in ('synthetic_truth_set', 'pose_sweep_failure_only')}
    summary = {}
    for variant in variants:
        run_records = raw_runs[variant]
        duration_records = [record for record in run_records
                            if record['source_group'] == 'synthetic_truth_set']
        all_duration = [record['total_sec'] for record in run_records]
        all_ocr = [record['ocr_sec'] for record in run_records]
        group_scores = {}
        for group, group_items in groups.items():
            group_scores[group] = score(group_items, outputs[variant])
        summary[variant] = {
            'unique_images': len(images),
            'runs': len(run_records),
            'synthetic_truth_set': group_scores['synthetic_truth_set'],
            'pose_sweep_failure_only': group_scores['pose_sweep_failure_only'],
            'all_image_total_latency_sec': {
                'median': statistics.median(all_duration),
                'p95': percentile(all_duration, 95), 'max': max(all_duration)},
            'all_image_ocr_stage_latency_sec': {
                'median': statistics.median(all_ocr),
                'p95': percentile(all_ocr, 95), 'max': max(all_ocr)},
            'synthetic_total_latency_sec': {
                'median': statistics.median(record['total_sec']
                    for record in duration_records),
                'p95': percentile([record['total_sec']
                    for record in duration_records], 95)},
        }

    output = {'schema': 'panel_ocr_batch_benchmark_v1',
              'domain': 'synthetic_only',
              'repeats_per_unique_image': args.repeats,
              'dataset_case_count': len(manifest['cases']),
              'sweep_failure_file_count': len(list(args.sweep_failure_images.glob('*.png'))),
              'unique_images_after_sha256_deduplication': len(images),
              'baseline_equivalence': baseline_equivalence,
              'unique_image_index': images,
              'variants': summary,
              'raw_runs': raw_runs}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n',
                           encoding='utf-8')
    print(json.dumps({'dataset_case_count': output['dataset_case_count'],
                      'unique_image_count': len(images), 'repeats': args.repeats,
                      'variants': {name: {
                          'text_recall': info['synthetic_truth_set']['text_recall'],
                          'led_center_detection_rate': info['synthetic_truth_set'][
                              'led_center_detection_rate'],
                          'led_state_accuracy': info['synthetic_truth_set'][
                              'led_state_accuracy'],
                          'synthetic_total_latency_sec': info[
                              'synthetic_total_latency_sec']}
                          for name, info in summary.items()}},
                     ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
