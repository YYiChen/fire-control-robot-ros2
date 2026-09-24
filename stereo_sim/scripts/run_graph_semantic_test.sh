#!/usr/bin/env bash
# The VO/graph/semantic chain runs in a separate ROS domain and Gazebo server.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${HOME}/stereo_sim_test_logs/graph_semantic"
overlay="${HOME}/stereo_sim_deps/overlay/opt/ros/humble"
mode="${1:-base}"
case "$mode" in
  base) domain=90; panel_x=0.60; panel_y=0.0; panel_yaw=0.0; extra=() ;;
  rotated) domain=91; panel_x=0.68; panel_y=0.05; panel_yaw=0.12; extra=() ;;
  occluded) domain=92; panel_x=0.68; panel_y=0.05; panel_yaw=0.0; extra=(--occlude-panel) ;;
  *) echo 'Usage: run_graph_semantic_test.sh [base|rotated|occluded]' >&2; exit 2 ;;
esac
export ROS_DOMAIN_ID="$domain"
export GAZEBO_MASTER_URI="http://127.0.0.1:113${domain}"
launch_pid=''

cleanup() {
  if [[ -n "$launch_pid" ]]; then
    kill "$launch_pid" 2>/dev/null || true
    wait "$launch_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

mkdir -p "$log_dir" "${HOME}/stereo_sim_generated"
source /opt/ros/humble/setup.bash
if ! ros2 pkg executables rtabmap_slam | grep -q 'rtabmap_slam rtabmap'; then
  if [[ ! -x "${overlay}/lib/rtabmap_slam/rtabmap" ]]; then
    echo 'FAIL: rtabmap_slam overlay absent; run graph SLAM audit first.' >&2
    exit 2
  fi
  export AMENT_PREFIX_PATH="${overlay}:${AMENT_PREFIX_PATH:-}"
  export LD_LIBRARY_PATH="${overlay}/lib:${LD_LIBRARY_PATH:-}"
  export PYTHONPATH="${overlay}/lib/python3.10/site-packages:${PYTHONPATH:-}"
fi
database="${HOME}/stereo_sim_generated/graph_semantic_${mode}_$(date +%Y%m%d_%H%M%S).db"
world="${HOME}/stereo_sim_generated/graph_semantic_${mode}.world"
python3 "${script_dir}/generate_semantic_panel_world.py" \
  --template "${root}/worlds/semantic_panel.world" --output "$world" \
  --panel-x "$panel_x" --panel-y "$panel_y" --panel-yaw "$panel_yaw" "${extra[@]}"
ros2 launch "${root}/launch/semantic_graph_slam.launch.py" \
  gui:=false world:="$world" database_path:="$database" \
  >"${log_dir}/${mode}.launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 65); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/mapGraph' <<<"$topics" &&
     grep -Fxq '/graph_semantic_panel/status' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics"; then
    ready=true
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if [[ "$ready" != true ]]; then
  echo "FAIL: graph semantic topics absent; see ${log_dir}/${mode}.launch.log" >&2
  exit 2
fi
verify_rc=0
python3 "${script_dir}/verify_graph_semantic.py" --mode "$mode" \
  --panel-x "$panel_x" --panel-y "$panel_y" --panel-yaw "$panel_yaw" \
  | tee "${log_dir}/${mode}.result.log" || verify_rc=$?
if [[ "$mode" == occluded ]] && \
   grep -Eq 'TF_NAN_INPUT|TF_DENORMALIZED_QUATERNION' "${log_dir}/${mode}.launch.log"; then
  echo 'FAIL: invalid VO pose reached the TF tree.' >&2
  verify_rc=2
fi
echo "Graph database: ${database}"
echo "Launch log: ${log_dir}/${mode}.launch.log"
exit "$verify_rc"
