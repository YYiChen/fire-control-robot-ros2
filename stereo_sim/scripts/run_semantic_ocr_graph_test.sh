#!/usr/bin/env bash
# Exercise synthetic OCR/depth landmarks through VO and graph-map coordinates.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
mode="${1:-base}"
case "$mode" in
  base) domain=96; case_name=fire_on ;;
  occluded) domain=97; case_name=marker_occluded ;;
  tf_unavailable) domain=98; case_name='' ;;
  *) echo 'Usage: run_semantic_ocr_graph_test.sh [base|occluded|tf_unavailable]' >&2; exit 2 ;;
esac
output="${2:-${HOME}/stereo_sim_generated/semantic_ocr_graph_${mode}}"
log_dir="${output}/logs"
database="${output}/graph_$(date +%Y%m%d_%H%M%S).db"
world="${root}/worlds/semantic_ocr_panel.world"
overlay="${HOME}/stereo_sim_deps/overlay/opt/ros/humble"
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

mkdir -p "$output" "$log_dir"
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
if [[ "$mode" == tf_unavailable ]]; then
  python3 "${script_dir}/semantic_ocr_graph_projector.py" \
    >"${log_dir}/projector.log" 2>&1 &
  launch_pid=$!
  ready=false
  for attempt in $(seq 1 35); do
    topics="$(ros2 topic list 2>/dev/null || true)"
    if grep -Fxq '/stereo_panel/fire/pose' <<<"$topics" &&
       grep -Fxq '/graph_semantic_panel/ocr/status' <<<"$topics"; then
      ready=true
      break
    fi
    if ! kill -0 "$launch_pid" 2>/dev/null; then
      break
    fi
    sleep 1
  done
  if [[ "$ready" != true ]]; then
    echo "FAIL: standalone projector topics absent; inspect ${log_dir}/projector.log" >&2
    exit 2
  fi
  verify_rc=0
  python3 "${script_dir}/verify_semantic_ocr_graph.py" \
    --mode "$mode" --output "$output" \
    | tee "${log_dir}/result.log" || verify_rc=$?
  echo "TF failure report: ${output}/semantic_ocr_graph_report.json"
  exit "$verify_rc"
fi
python3 "${script_dir}/generate_gazebo_perception_panel.py" \
  --output "$output" --case "$case_name"
ros2 launch "${root}/launch/semantic_ocr_graph_slam.launch.py" \
  gui:=false world:="$world" database_path:="$database" \
  >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 75); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/mapGraph' <<<"$topics" &&
     grep -Fxq '/stereo_panel/observation' <<<"$topics" &&
     grep -Fxq '/graph_semantic_panel/ocr/status' <<<"$topics" &&
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
  echo "FAIL: OCR graph topics absent; inspect ${log_dir}/launch.log" >&2
  exit 2
fi
verify_rc=0
python3 "${script_dir}/verify_semantic_ocr_graph.py" \
  --mode "$mode" --output "$output" \
  | tee "${log_dir}/result.log" || verify_rc=$?
echo "Graph OCR report: ${output}/semantic_ocr_graph_report.json"
echo "Launch log: ${log_dir}/launch.log"
exit "$verify_rc"
