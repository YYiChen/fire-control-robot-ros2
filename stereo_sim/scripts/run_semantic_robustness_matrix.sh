#!/usr/bin/env bash
# Two independent pose variations of the isolated mobile stereo experiment.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${HOME}/stereo_sim_test_logs/semantic_robustness"
world_dir="${HOME}/stereo_sim_generated/worlds"
launch_pid=''

cleanup() {
  if [[ -n "$launch_pid" ]]; then
    kill "$launch_pid" 2>/dev/null || true
    wait "$launch_pid" 2>/dev/null || true
    launch_pid=''
  fi
}
trap cleanup EXIT INT TERM

mkdir -p "$log_dir" "$world_dir"
source /opt/ros/humble/setup.bash
"${script_dir}/check_stereo_sim_dependencies.sh"
failures=0
selected="${1:-all}"
if [[ "$selected" != all && "$selected" != offset && "$selected" != rotated && "$selected" != occluded ]]; then
  echo 'Usage: run_semantic_robustness_matrix.sh [all|offset|rotated|occluded]' >&2
  exit 2
fi
tested=0
for spec in 'offset 0.68 0.05 0.00 86' 'rotated 0.68 0.05 0.12 87' 'occluded 0.68 0.05 0.00 88'; do
  read -r name x y yaw domain <<<"$spec"
  if [[ "$selected" != all && "$selected" != "$name" ]]; then
    continue
  fi
  tested=$((tested + 1))
  export ROS_DOMAIN_ID="$domain"
  export GAZEBO_MASTER_URI="http://127.0.0.1:113${domain}"
  export SEMANTIC_PANEL_X="$x" SEMANTIC_PANEL_Y="$y" SEMANTIC_PANEL_YAW="$yaw"
  world="${world_dir}/semantic_panel_mobile_${name}.world"
  extra=()
  [[ "$name" == occluded ]] && extra=(--occlude-panel)
  python3 "${script_dir}/generate_semantic_panel_world.py" \
    --template "${root}/worlds/semantic_panel_mobile.world" \
    --output "$world" --panel-x "$x" --panel-y "$y" --panel-yaw "$yaw" "${extra[@]}"
  ros2 launch "${root}/launch/semantic_autonomous_mobile.launch.py" \
    gui:=false world:="$world" >"${log_dir}/${name}.launch.log" 2>&1 &
  launch_pid=$!
  ready=false
  for attempt in $(seq 1 65); do
    topics="$(ros2 topic list 2>/dev/null || true)"
    if grep -Fxq '/semantic_panel/control_status' <<<"$topics" &&
       grep -Fxq '/model_states' <<<"$topics" &&
       grep -Fxq '/vo/odom' <<<"$topics"; then
      ready=true
      break
    fi
    if ! kill -0 "$launch_pid" 2>/dev/null ||
       grep -Fq '[ERROR] [spawn_entity.py' "${log_dir}/${name}.launch.log"; then
      break
    fi
    sleep 1
  done
  verifier="${script_dir}/verify_semantic_robustness.py"
  [[ "$name" == occluded ]] && verifier="${script_dir}/verify_semantic_occlusion.py"
  if [[ "$ready" == true ]] && python3 "$verifier" \
      >"${log_dir}/${name}.result.log" 2>&1; then
    cat "${log_dir}/${name}.result.log"
  else
    echo "FAIL: ${name} scenario; see ${log_dir}/${name}.launch.log" >&2
    [[ -f "${log_dir}/${name}.result.log" ]] && cat "${log_dir}/${name}.result.log"
    failures=$((failures + 1))
  fi
  cleanup
done
echo "Matrix: $((tested - failures))/${tested} passed. Logs: ${log_dir}"
[[ "$failures" -eq 0 ]]
