#!/usr/bin/env bash
# Start and verify the isolated Gazebo stereo test. No robot or hardware nodes.

set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
stereo_root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${TMPDIR:-/tmp}/stereo_sim_test"
target_distance="${1:-0.60}"

case "$target_distance" in
  0.40|0.60|0.80) ;;
  *) echo 'Usage: run_stereo_sim_test.sh [0.40|0.60|0.80]' >&2; exit 2 ;;
esac

export ROS_DOMAIN_ID=77
export GAZEBO_MASTER_URI=http://127.0.0.1:11377

cleanup() {
  local code=$?
  [[ -n "${processing_pid:-}" ]] && kill "${processing_pid}" 2>/dev/null || true
  [[ -n "${world_pid:-}" ]] && kill "${world_pid}" 2>/dev/null || true
  wait "${processing_pid:-}" 2>/dev/null || true
  wait "${world_pid:-}" 2>/dev/null || true
  exit "$code"
}
trap cleanup EXIT INT TERM

mkdir -p "$log_dir"
source /opt/ros/humble/setup.bash
python3 "${script_dir}/generate_stereo_test_world.py" \
  --template "${stereo_root}/worlds/stereo_test.world" \
  --output "${log_dir}/stereo_test_${target_distance}.world" \
  --distance-m "$target_distance"
export STEREO_EXPECTED_DEPTH_M="$target_distance"

"${script_dir}/check_stereo_sim_dependencies.sh"

ros2 launch "${stereo_root}/launch/stereo_test_world.launch.py" \
  gui:=false world:="${log_dir}/stereo_test_${target_distance}.world" >"${log_dir}/world.log" 2>&1 &
world_pid=$!

wait_for_topic() {
  local topic="$1"
  local attempts=0
  while (( attempts < 30 )); do
    if ros2 topic list 2>/dev/null | grep -Fxq "$topic"; then
      return 0
    fi
    sleep 1
    ((attempts += 1))
  done
  echo "FAIL missing topic after 30 seconds: $topic"
  return 1
}

wait_for_topic /clock

wait_for_matching_topic() {
  local expression="$1"
  local attempts=0
  local match=''
  while (( attempts < 30 )); do
    match="$(ros2 topic list 2>/dev/null | grep -E "$expression" | head -n 1 || true)"
    if [[ -n "$match" ]]; then
      printf '%s\n' "$match"
      return 0
    fi
    sleep 1
    ((attempts += 1))
  done
  echo "FAIL missing stereo topic matching: $expression" >&2
  return 1
}

left_image="$(wait_for_matching_topic '^/stereo/.*/left/image_raw$')"
left_info="$(wait_for_matching_topic '^/stereo/.*/left/camera_info$')"
right_image="$(wait_for_matching_topic '^/stereo/.*/right/image_raw$')"
right_info="$(wait_for_matching_topic '^/stereo/.*/right/camera_info$')"

echo "Detected topics: $left_image | $left_info | $right_image | $right_info"

ros2 launch "${stereo_root}/launch/stereo_processing.launch.py" \
  use_sim_time:=true \
  left_image_topic:="$left_image" \
  left_camera_info_topic:="$left_info" \
  right_image_topic:="$right_image" \
  right_camera_info_topic:="$right_info" >"${log_dir}/processing.log" 2>&1 &
processing_pid=$!

wait_for_topic /stereo/disparity
wait_for_topic /stereo/points2

echo '--- stereo output connections ---'
ros2 topic info /stereo/left/image_rect
ros2 topic info /stereo/disparity
ros2 topic info /stereo/points2

python3 "${script_dir}/verify_stereo_output.py"

echo "PASS: images, CameraInfo, disparity, points2, and ${target_distance} m depth verification succeeded."
echo "Logs: ${log_dir}/world.log and ${log_dir}/processing.log"
