#!/usr/bin/env bash
# Isolated graph-SLAM experiment; all dependency extraction stays in $HOME.
set -eo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/.." && pwd)"
log_dir="${HOME}/stereo_sim_test_logs/semantic_graph_slam"
dep_root="${HOME}/stereo_sim_deps"
overlay="${dep_root}/overlay/opt/ros/humble"
export ROS_DOMAIN_ID=89
export GAZEBO_MASTER_URI=http://127.0.0.1:11389
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
if ! ros2 pkg executables rtabmap_slam >/dev/null 2>&1; then
  if [[ ! -x "${overlay}/lib/rtabmap_slam/rtabmap" ]]; then
    mkdir -p "${dep_root}/debs" "${dep_root}/overlay"
    (cd "${dep_root}/debs" && apt download \
      ros-humble-apriltag-msgs ros-humble-aruco-opencv-msgs ros-humble-rtabmap-slam)
    for package in "${dep_root}"/debs/*.deb; do
      dpkg-deb -x "$package" "${dep_root}/overlay"
    done
  fi
  export AMENT_PREFIX_PATH="${overlay}:${AMENT_PREFIX_PATH:-}"
  export LD_LIBRARY_PATH="${overlay}/lib:${LD_LIBRARY_PATH:-}"
  export PYTHONPATH="${overlay}/lib/python3.10/site-packages:${PYTHONPATH:-}"
fi
if ! ros2 pkg executables rtabmap_slam | grep -q 'rtabmap_slam rtabmap'; then
  echo 'FAIL: rtabmap_slam unavailable, including in the user overlay.' >&2
  exit 2
fi
"${script_dir}/check_stereo_sim_dependencies.sh"
database="${HOME}/stereo_sim_generated/graph_slam_$(date +%Y%m%d_%H%M%S).db"
world="${HOME}/stereo_sim_generated/graph_room_$(date +%Y%m%d_%H%M%S).world"
python3 "${script_dir}/generate_graph_room.py" \
  --template "${root}/worlds/semantic_panel.world" --output "$world"
ros2 launch "${root}/launch/semantic_graph_slam.launch.py" \
  gui:=false world:="$world" database_path:="$database" \
  >"${log_dir}/launch.log" 2>&1 &
launch_pid=$!
ready=false
for attempt in $(seq 1 60); do
  topics="$(ros2 topic list 2>/dev/null || true)"
  if grep -Fxq '/vo/odom' <<<"$topics" &&
     grep -Fxq '/model_states' <<<"$topics" &&
     grep -Fxq '/info' <<<"$topics"; then
    ready=true
    break
  fi
  if ! kill -0 "$launch_pid" 2>/dev/null ||
     grep -Fq '[ERROR] [graph_slam' "${log_dir}/launch.log"; then
    break
  fi
  sleep 1
done
if [[ "$ready" != true ]]; then
  echo "FAIL: graph SLAM topics absent; see ${log_dir}/launch.log" >&2
  exit 2
fi
verify_rc=0
python3 "${script_dir}/verify_semantic_graph_slam.py" | tee "${log_dir}/result.log" || verify_rc=$?
echo "Graph database: ${database}"
echo "World: ${world}"
echo "Launch log: ${log_dir}/launch.log"
exit "$verify_rc"
