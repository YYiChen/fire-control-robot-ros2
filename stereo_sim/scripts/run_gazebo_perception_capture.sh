#!/usr/bin/env bash
# Launch a separate Gazebo/ROS domain and capture a physically rendered frame.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
output="${1:-${HOME}/stereo_sim_generated/perception_camera}"
case_name="${2:-fire_on}"
overlay="${HOME}/stereo_sim_deps/tesseract_overlay/usr"
export ROS_DOMAIN_ID=94
export GAZEBO_MASTER_URI=http://127.0.0.1:11394
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
ros2 launch "${root}/launch/stereo_test_world.launch.py" gui:=false \
  world:="${root}/worlds/perception_panel.world" \
  >"${output}/gazebo_launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 45); do
  if ros2 topic list 2>/dev/null | grep -Fxq '/stereo/stereo_rig/left/image_raw'; then
    ready=true
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if [[ "$ready" != true ]]; then
  echo "FAIL: no left camera topic; inspect ${output}/gazebo_launch.log" >&2
  exit 2
fi
python3 "${script_dir}/verify_gazebo_perception_capture.py" \
  --output "$output" \
  --tesseract "${overlay}/bin/tesseract" \
  --tessdata "${overlay}/share/tesseract-ocr/4.00/tessdata"
echo "Gazebo capture: ${output}/gazebo_left_raw.png"
echo "Capture report: ${output}/gazebo_capture_report.json"
