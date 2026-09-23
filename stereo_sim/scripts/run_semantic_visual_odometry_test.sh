#!/usr/bin/env bash
# Isolated Gazebo test; only this script's launch tree is stopped on exit.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${HOME}/stereo_sim_test_logs/semantic_visual_odometry_test"
export ROS_DOMAIN_ID=83
export GAZEBO_MASTER_URI=http://127.0.0.1:11383

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
ros2 launch "${root}/launch/semantic_visual_odometry.launch.py" gui:=false \
  >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!
for attempt in $(seq 1 50); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/semantic_panel/status' <<<"$topics" &&
     grep -Fxq '/semantic_panel/approach_status' <<<"$topics" &&
     grep -Fxq '/projected_map' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics"; then
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    echo "FAIL: launch exited. See ${log_dir}/launch.log" >&2
    exit 2
  fi
  sleep 1
done
for node in /semantic_panel /semantic_approach_planner; do
  info="$(ros2 node info "$node")"
  if grep -Fq '/model_states:' <<<"$info" ||
     ! grep -Fq '/vo/odom:' <<<"$info"; then
    echo "FAIL: ${node} has an unexpected pose input." >&2
    exit 2
  fi
done
python3 "${script_dir}/verify_semantic_visual_odometry.py"
echo "Launch log: ${log_dir}/launch.log"
