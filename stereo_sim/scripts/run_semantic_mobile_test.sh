#!/usr/bin/env bash
# Drive only the isolated Gazebo TurtleBot3 created by this launch.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${HOME}/stereo_sim_test_logs/semantic_mobile_test"
export ROS_DOMAIN_ID=84
export GAZEBO_MASTER_URI=http://127.0.0.1:11384

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
ros2 launch "${root}/launch/semantic_mobile_stereo.launch.py" gui:=false \
  >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!
for attempt in $(seq 1 65); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/semantic_panel/status' <<<"$topics" &&
     grep -Fxq '/cmd_vel' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics"; then
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    echo "FAIL: launch exited. See ${log_dir}/launch.log" >&2
    exit 2
  fi
  if grep -Fq '[ERROR] [spawn_entity.py' "${log_dir}/launch.log"; then
    echo "FAIL: mobile robot spawn failed. See ${log_dir}/launch.log" >&2
    exit 2
  fi
  sleep 1
done
python3 "${script_dir}/verify_semantic_mobile.py"
echo "Launch log: ${log_dir}/launch.log"
