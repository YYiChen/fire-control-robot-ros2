#!/usr/bin/env bash
# Run one synthetic panel case through Gazebo stereo depth and OCR association.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
output="${1:-${HOME}/stereo_sim_generated/perception_depth}"
case_name="${2:-fire_on}"
export ROS_DOMAIN_ID=95
export GAZEBO_MASTER_URI=http://127.0.0.1:11395
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
mkdir -p "$output"
python3 "${script_dir}/generate_gazebo_perception_panel.py" \
  --output "$output" --case "$case_name"
ros2 launch "${root}/launch/stereo_panel_depth.launch.py" gui:=false \
  world:="${root}/worlds/perception_panel.world" \
  >"${output}/depth_launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 50); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/stereo/left/image_rect' <<<"$topics" &&
     grep -Fxq '/stereo/points2' <<<"$topics" &&
     grep -Fxq '/stereo_panel/observation' <<<"$topics"; then
    ready=true
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if [[ "$ready" != true ]]; then
  echo "FAIL: stereo image/cloud topics not ready; inspect ${output}/depth_launch.log" >&2
  exit 2
fi
python3 "${script_dir}/verify_stereo_panel_depth.py" --output "$output"
echo "Stereo panel depth report: ${output}/stereo_panel_depth_report.json"
