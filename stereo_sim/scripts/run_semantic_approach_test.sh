#!/usr/bin/env bash
# Isolated, headless planning-only test. Never publishes /cmd_vel.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${TMPDIR:-/tmp}/semantic_approach_test"
export ROS_DOMAIN_ID=80
export GAZEBO_MASTER_URI=http://127.0.0.1:11380

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
if ! ros2 pkg executables octomap_server | grep -q octomap_server_node; then
  echo 'FAIL: install ros-humble-octomap-server first.' >&2
  exit 2
fi
world_file="${SEMANTIC_WORLD:-${root}/worlds/semantic_panel.world}"
if [[ ! -f "$world_file" ]]; then
  echo "FAIL: world file does not exist: $world_file" >&2
  exit 2
fi
ros2 launch "${root}/launch/semantic_occupancy.launch.py" \
  world:="$world_file" gui:=false >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!

for attempt in $(seq 1 45); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/semantic_panel/approach_status' <<<"$topics" &&
     grep -Fxq '/projected_map' <<<"$topics" &&
     grep -Fxq '/semantic_panel/button/reset/pose' <<<"$topics"; then
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    echo "FAIL: launch exited. See ${log_dir}/launch.log" >&2
    exit 2
  fi
  sleep 1
done

python3 "${script_dir}/verify_semantic_approach.py"
echo "Launch log: ${log_dir}/launch.log"
