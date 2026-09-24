# 消防面板语义像素与双目点云三维关联

## Goal

把中文 OCR 识别到的三种面板状态及其 LED 中心映射到同时间戳的校正双目点云，发布具名相机坐标系三维位置，并与独立 Gazebo 面板几何真值比较。此 target 只验证相机坐标中的 RGB/语义—深度关联；地图/`graph_map` 投影和导航决策另立 target。

## Dirty-State Note

起始状态：

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

上述三个未跟踪文件来自其他工作，与本 target owned 文件不重叠；保持不变、不暂存。

## Owned Files

- `plan/2026-09-24-stereo-semantic-depth/plan.md`
- `stereo_sim/scripts/stereo_panel_depth_node.py`
- `stereo_sim/scripts/run_stereo_panel_depth_test.sh`
- `stereo_sim/scripts/verify_stereo_panel_depth.py`
- `stereo_sim/launch/stereo_panel_depth.launch.py`
- `stereo_sim/README.md` 的本 target 章节
- `plan/log.md` 的本 target 条目

## Read-Only Files

- `stereo_sim/scripts/evaluate_panel_perception.py`（导入 OCR/LED/标记检测）
- `stereo_sim/scripts/generate_panel_perception_cases.py`
- `stereo_sim/launch/stereo_processing.launch.py`
- `stereo_sim/launch/stereo_test_world.launch.py`
- `stereo_sim/worlds/perception_panel.world`
- 现有 VO、graph-SLAM、语义投影代码及所有其他用户未跟踪文件

## Shared Dependencies

- 上一 target 的 `fire_on` / `all_on` / `all_off` 合成面板案例与 ArUco 582 校正器
- Gazebo 左目校正图 `/stereo/left/image_rect` 与 `/stereo/points2` 的图像像素对齐及时间戳
- WSL 用户目录下的 Tesseract 中文数据和 `~/stereo_sim_generated/` 输出；隔离 `ROS_DOMAIN_ID=95` / Gazebo 端口 `11395`

## Expected Work

1. 新增节流 OCR 节点，将校正后的 LED 像素位置反投影回左目图像，再在同时间戳 `PointCloud2` 小邻域内稳健取三维点；输出标签、灯态、采集时间、像素和相机坐标。
2. 增加隔离 launch / runner / verifier，运行 `fire_on`、`all_on`、`all_off`，把预测点与生成面板的已知平面几何对照；真值只由验证器读取，不进入感知节点。
3. 记录数据关联频率、深度有效率和三维误差，并明确其与后续 VO/`graph_map` 变换、真实面板性能的边界。

## Validation

- Python 编译、Shell 语法、launch Python 编译及 `git diff --check`。
- WSL ROS2 Humble / Gazebo 无界面实测：图像与点云时间差门槛、有效点数、三种 OCR/灯态状态、输出 frame/stamp、相机 XYZ 对已知面板真值的误差；三个案例各自独立重启。
- 停机后检查隔离 ROS 域 95 无残留节点、该世界无 Gazebo 进程；确认源码与 WSL 实测副本哈希一致。

手写 `PointCloud2` 字段读取和同帧校验有格式/坐标风险，因此需检查组织点云字段、步长、大小端与非有限点过滤，并用 Gazebo 真值验证完整 XYZ；不跑无关导航、硬件或全量图 SLAM 测试。

## Scope Update 2026-09-24

初版 0.06 m 三维位置验收门槛明显宽于三个案例首测的最大 0.00245 m 误差。将最终通过门槛收紧到 **0.02 m**，并在更新后复跑完整验证，避免以过宽阈值掩盖坐标/点云关联问题。

## Outcome 2026-09-24

- 三个隔离 Gazebo 场景 `fire_on`、`all_on`、`all_off` 分别重启并通过；三行 OCR、红/黄/绿/熄灭分类均与生成真值一致。
- 九个 LED 邻域均从同帧组织点云取得 148–152 个有效 XYZ 点；图像/点云时间差均为 0，输出均与 ROS 位姿话题的图像时间戳一致。
- 9 个相机坐标 XYZ 与独立面板几何真值的误差中位数 0.00225 m、最大值 0.00245 m，低于 0.02 m 门槛。
- Python、Shell、launch 与 SDF/DAE XML 检查通过；Windows 源文件与 WSL 执行副本哈希一致；隔离 ROS 域无残留节点且无面板 Gazebo 服务进程。
- 节点按至少 0.75 秒间隔处理 OCR，但没有测持续吞吐率；测试距离固定 0.888 m，输出仍在相机光学坐标，未转成 `vo_odom` 或 `graph_map`。

## Experience Signal (for human review)

若 LED 点云采样受纹理低纹理、遮挡或 disparity invalid 区影响，记录检测像素、有效点数量和错误分布，供人后续判断是否提取经验。

## Commit Intent

```text
feat: associate panel OCR labels with stereo point cloud
```
