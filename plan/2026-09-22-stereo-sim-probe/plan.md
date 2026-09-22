# 双目仿真依赖探测与最小处理管线

## Goal

在不连接真机、底盘或工控机服务的前提下，为 ROS 2 Humble + Gazebo Classic 建立独立双目仿真验证入口：先确认依赖，再启动左右图像到视差与点云的处理管线。

## Dirty-State Note

起始状态：

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

这三项均不是本 target 的文件，不修改、不暂存、不纳入提交。前两项是用户当前场景；`rec.bak` 的归属不明。

## Owned Files

- `stereo_sim/README.md`
- `stereo_sim/scripts/check_stereo_sim_dependencies.sh`
- `stereo_sim/scripts/run_stereo_sim_test.sh`
- `stereo_sim/scripts/verify_stereo_output.py`
- `stereo_sim/scripts/generate_stereo_test_world.py`
- `stereo_sim/scripts/run_stereo_range_test.sh`
- `stereo_sim/launch/stereo_processing.launch.py`
- `stereo_sim/launch/stereo_test_world.launch.py`
- `stereo_sim/config/stereo_processing.yaml`
- `stereo_sim/models/stereo_rig/model.config`
- `stereo_sim/models/stereo_rig/model.sdf`
- `stereo_sim/worlds/stereo_test.world`
- `.gitignore`
- `plan/2026-09-22-stereo-sim-probe/plan.md`
- `plan/log.md`

## Read-Only Files

- `gazebo_scene/lab_room.world`
- `gazebo_scene/lab_room.launch.py`
- `reference/industrial_pc_2026-09-22/stereo_reference/visual_slam_start.py`
- all industrial-PC source references

## Shared Dependencies

- ROS 2 Humble, Gazebo Classic, `gazebo_ros`
- `image_proc`, `stereo_image_proc`, and their `CameraInfo` contracts
- user's local WSL workspace `~/ros2_ws`

## Expected Work

1. Record the supported 30 Hz / 60 mm baseline test configuration and the separation from real hardware.
2. Add a WSL preflight script that reports required package, executable, and Gazebo plugin availability without installing anything.
3. Add a parameterized processing launch file for rectification, disparity, and point-cloud nodes; it only consumes camera topics and does not command a robot.
4. Add an isolated 30 Hz, 60 mm-baseline multicamera SDF model and a test world containing a known-distance panel target. It must use Humble's `libgazebo_ros_camera.so`, not the ROS 1 multicamera library name.
5. Perform static Python and shell syntax checks locally, then run the isolated verifier inside WSL.
6. Add an isolated, self-cleaning WSL runtime verifier so a later run proves images, disparity, and point-cloud publication without sharing the user's active ROS or Gazebo processes.
7. Verify that central disparity estimates a plausible depth for the 0.60 m test panel, instead of treating topic existence as a depth result.
8. Keep the isolated verifier shorter than the available foreground execution window by using one message-and-depth collection pass rather than redundant per-topic waits.
9. Ensure the central depth target has visual texture; a uniform surface cannot provide stereo correspondence.
10. Parameterize the known-distance test target and validate the 0.40 m, 0.60 m, and 0.80 m planned work range.

The preflight must source `/opt/ros/humble/setup.bash` before enabling Bash's undefined-variable error mode: the Humble setup script reads optional `AMENT_TRACE_SETUP_FILES` state.

The runtime configuration must use Humble's declared parameter types. The first integration run proved `stereo_image_proc.uniqueness_ratio` is a floating-point parameter.

The Humble `image_proc` executable in this environment publishes the mono rectified feed needed by disparity but does not publish `image_rect_color`. The first point-cloud test therefore uses XYZ-only output; coloured points are a later optional enhancement.

## Validation

- `bash -n` on the dependency probe.
- Python syntax compilation of launch and verifier scripts.
- XML parse of model and world files.
- textual checks that the launch file enables simulated time and contains no velocity, action, or hardware driver node.
- WSL preflight check, followed by isolated 0.40 m, 0.60 m, and 0.80 m simulation runs that each prove raw images, rectification, disparity, XYZ point-cloud publication, and plausible central depth.
- confirm the isolated ROS domain and Gazebo server have stopped after the final run.
- `git diff --check` and `git status --short --branch`.

These checks match a non-hardware simulation target. No physical camera, robot, motor command, or industrial-PC service is started.

## Experience Signal (for human review)

- The industrial PC was temporarily unreachable over SSH during this target, so its package state cannot substitute for a WSL dependency check.

## Commit Intent

```text
feat: add stereo simulation preflight pipeline
```
