#!/usr/bin/env bash
# Offline synthetic visual benchmark; no ROS domain, Gazebo, or hardware access.
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
output="${1:-${HOME}/stereo_sim_generated/panel_perception_baseline}"
overlay="${HOME}/stereo_sim_deps/tesseract_overlay/usr"
tesseract="${overlay}/bin/tesseract"
tessdata="${overlay}/share/tesseract-ocr/4.00/tessdata"
if [[ ! -x "$tesseract" ]] || [[ ! -s "${tessdata}/chi_sim.traineddata" ]]; then
  echo "Missing user-local Tesseract binary or chi_sim data in ${overlay}" >&2
  echo 'See stereo_sim/README.md for the no-sudo setup.' >&2
  exit 2
fi
python3 "${script_dir}/generate_panel_perception_cases.py" --output "$output"
python3 "${script_dir}/evaluate_panel_perception.py" \
  --dataset "$output" --tesseract "$tesseract" \
  --tessdata "$tessdata" --output "${output}/report.json"
echo "Case images and truth: ${output}/truth.json"
echo "Predictions and failures: ${output}/report.json"
