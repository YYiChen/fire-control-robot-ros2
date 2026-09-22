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
- `stereo_sim/launch/stereo_processing.launch.py`
- `stereo_sim/config/stereo_processing.yaml`
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
4. Perform static Python and shell syntax checks locally. Run-time ROS verification remains pending because this agent cannot execute WSL commands in this session.

## Validation

- `bash -n` on the dependency probe.
- Python syntax compilation of the launch file.
- textual checks that the launch file enables simulated time and contains no velocity, action, or hardware driver node.
- `git diff --check` and `git status --short --branch`.

These checks match a non-hardware scaffold. The preflight command has to run inside WSL before any package installation or Gazebo launch is claimed as verified.

## Experience Signal (for human review)

- The industrial PC was temporarily unreachable over SSH during this target, so its package state cannot substitute for a WSL dependency check.

## Commit Intent

```text
feat: add stereo simulation preflight pipeline
```
