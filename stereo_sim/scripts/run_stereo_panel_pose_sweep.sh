#!/usr/bin/env bash
# Measure stereo panel OCR/LED and XYZ across fixed synthetic range/pose cases.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
timestamp="$(date +%Y%m%d_%H%M%S)"
output="${1:-${HOME}/stereo_sim_generated/stereo_panel_pose_sweep_${timestamp}}"
profile="${2:-full}"
repeats="${3:-3}"
case_name='fire_on'
domain=99
export ROS_DOMAIN_ID="$domain"
export GAZEBO_MASTER_URI="http://127.0.0.1:113${domain}"
export GAZEBO_MODEL_PATH="${output}/models:${GAZEBO_MODEL_PATH:-}"
launch_pid=''

cleanup() {
  if [[ -n "$launch_pid" ]]; then
    kill "$launch_pid" 2>/dev/null || true
    wait "$launch_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

source /opt/ros/humble/setup.bash
set -u
if [[ "$profile" != smoke && "$profile" != diagnostic && "$profile" != full ]]; then
  echo 'Usage: run_stereo_panel_pose_sweep.sh [output-dir] [smoke|diagnostic|full] [repeats]' >&2
  exit 2
fi
if ! [[ "$repeats" =~ ^[1-9][0-9]*$ ]]; then
  echo 'repeats must be a positive integer' >&2
  exit 2
fi
mkdir -p "$output"
if [[ -e "${output}/generation.json" || -e "${output}/summary.json" ||
      -e "${output}/trials.jsonl" ]]; then
  echo "Refusing to overwrite existing experiment output: ${output}" >&2
  exit 2
fi
python3 "${script_dir}/generate_gazebo_perception_panel.py" \
  --output "$output" --case "$case_name"
ros2 launch "${root}/launch/stereo_panel_depth.launch.py" \
  gui:=false world:="${root}/worlds/semantic_ocr_panel.world" \
  >"${output}/launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 75); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  services="$(ros2 service list 2>/dev/null || true)"
  if grep -Fxq '/stereo/left/image_rect' <<<"$topics" &&
     grep -Fxq '/stereo/points2' <<<"$topics" &&
     grep -Fxq '/stereo_panel/observation' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics" &&
     grep -Fxq '/set_entity_state' <<<"$services"; then
    ready=true
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if [[ "$ready" != true ]]; then
  echo "FAIL: stereo topics or Gazebo state service not ready; inspect ${output}/launch.log" >&2
  exit 2
fi

verify_rc=0
python3 "${script_dir}/verify_stereo_panel_pose_sweep.py" \
  --output "$output" --profile "$profile" --repeats "$repeats" \
  | tee "${output}/run.log" || verify_rc=$?
echo "Stereo panel sweep summary: ${output}/summary.json"
echo "Raw measurements: ${output}/trials.jsonl and ${output}/trials.csv"
exit "$verify_rc"
