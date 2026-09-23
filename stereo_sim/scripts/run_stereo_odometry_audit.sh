#!/usr/bin/env bash
# Compare independent stereo visual odometry against Gazebo truth in isolation.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${TMPDIR:-/tmp}/stereo_odometry_audit"
export ROS_DOMAIN_ID=82
export GAZEBO_MASTER_URI=http://127.0.0.1:11382

cleanup() {
  local code=$?
  [[ -n "${launch_pid:-}" ]] && kill "$launch_pid" 2>/dev/null || true
  wait "${launch_pid:-}" 2>/dev/null || true
  exit "$code"
}
trap cleanup EXIT INT TERM

mkdir -p "$log_dir"
source /opt/ros/humble/setup.bash
"${script_dir}/check_stereo_sim_dependencies.sh"
if ! ros2 pkg executables rtabmap_odom | grep -q stereo_odometry; then
  echo 'FAIL: install ros-humble-rtabmap-odom first.' >&2
  exit 2
fi
ros2 launch "${root}/launch/stereo_odometry_audit.launch.py" gui:=false \
  >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!
for attempt in $(seq 1 45); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/stereo/left/image_rect' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics"; then
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    echo "FAIL: launch exited. See ${log_dir}/launch.log" >&2
    exit 2
  fi
  sleep 1
done
python3 "${script_dir}/verify_stereo_odometry_audit.py"
echo "Launch log: ${log_dir}/launch.log"
