# 双目面板测距位置姿态实验

## Goal

在隔离 Gazebo 中运行现有中文面板 OCR/LED 与双目点云关联链，测量固定合成面板在多距离、偏航角和横向偏移下的检测率、灯态正确率、三维定位误差及端到端观测延迟；保留每次运行原始记录和失败图像，并分解节点排队与处理时间，明确仿真适用范围。仅仿真，不连接实机。

## Dirty-State Note

开始时 `main` 与 `origin/main` 同步在 `994e1b5`。用户已有未跟踪 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world` 与 `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak`；均不属于本 target，不修改、不暂存。

## Owned Files

- `plan/2026-09-24-stereo-panel-pose-sweep/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/run_stereo_panel_pose_sweep.sh`
- `stereo_sim/scripts/verify_stereo_panel_pose_sweep.py`
- `stereo_sim/scripts/stereo_panel_depth_node.py`

## Read-Only Files

- `stereo_sim/scripts/generate_gazebo_perception_panel.py`
- `stereo_sim/scripts/generate_panel_perception_cases.py`
- `stereo_sim/launch/stereo_panel_depth.launch.py`
- `stereo_sim/worlds/perception_panel.world`
- 用户自建 Gazebo 场景、实机与其他 agent 文件

## Shared Dependencies

- ROS2 Humble、Gazebo Classic、现有 stereo rig 与系统相机点云。
- 当前 OCR/灯态与深度关联节点；固定使用 `fire_on` 合成面板。
- 使用 `semantic_ocr_panel.world` 中的 `gazebo_ros_state` 插件，通过隔离 Gazebo `/set_entity_state` 服务改变静态面板位置，并用 `/model_states` 核验。

## Expected Work

1. 探测现有 Gazebo 场景能否在同一仿真实例中可靠移动静态面板；若不能，更新本计划并采用每条件独立启动方案。
2. 实现可复现实验矩阵，覆盖多个面深距离、正负偏航和横向偏移，并为每条件做重复运行。
3. 按 DAE 纹理平面和模型真值变换计算相机光学坐标真值；记录成功、部分识别、标记缺失和超时，不丢弃失败条件；失败帧另存为图像。
4. 为当前节点增加纯观测计时：输入帧到结果总耗时、配对后处理耗时、标记检测、OCR 和点云采样耗时；不改变节点调度或识别行为。
5. 输出逐次 JSON/CSV 与汇总指标：目标检测/位姿有效率、面板灯态混淆/准确率、三维误差中位数/P95/最大值、有效深度采样数、图像-点云时间差、外部观测端到端延迟及节点内部各阶段耗时。
6. 运行最小正视回归、完整矩阵和针对临界距离/不对称偏航/横移的重复诊断组，检查原有单场景 depth 测试未被破坏。
7. 更新 README 和 `plan/log.md`，复核 diff，目标文件单独提交并同步远端。

## Validation

- SDF/DAE 几何的纯数学坐标断言与已知正视基线。
- Python 编译、Shell 语法、`git diff --check`、目标 dirty-state/status。
- WSL ROS2 Humble 隔离无界面 Gazebo 正视 smoke 与完整矩阵；检查每个条件都有结果，汇总由逐次原始记录可重算。
- 对相同距离/姿态重复采样，报告观测延迟和定位误差分布，不把成功样本均值代替失败率。
- 正视、远距、正负偏航和正负横移的失败图可逐帧复看；有/无失败图、定位缺失、标签漏检状态一致。
- 节点新增计时字段非负且各阶段耗时之和不超过输入到输出总耗时（允许小量计时采样误差）。

## Commit Intent

只提交本 target owned files。测试报告保存在 WSL 用户目录，不提交生成图、仿真日志或临时数据库。既有用户未跟踪文件保持原状。

## Open Risks

- 当前节点以至少 0.75 秒为处理间隔；计时只用于量化限频等待与计算耗时，不调整算法/处理频率。
- 字体、灯颜色、面板布局与 Gazebo 光照均为合成且受控；此结果不能代替真实消防主机图像上的 OCR、LED 或立体测距评估。
- Gazebo/WSLg 的可视化刷新不是处理性能的证据；本测试以 ROS 话题、源时间戳、墙钟延迟和报告为准。

## Outcome

完整矩阵 51/51 条采集完成；所有 Gazebo 位姿都经模型状态核验，图像/点云时间差为 0。153 个预期标签中 OCR 正文识别 120 个（78.43%），LED 中心/有效位姿/2 cm 内定位均为 117 个（76.47%）；成功位姿误差中位数 2.6 mm、P95 6.696 mm、最大 7.263 mm。正视 0.65–0.888 m、负偏航 −15°至−35°及横向 −0.15 至 +0.25 m 的测试组完整通过；1.2/1.5 m 正视、正偏航 +15°至+35°和横向 −0.25 m 有重复标签漏检；1.8/2.1 m 所有标签漏检。

临界诊断组 30/30 条采集完成，18 条未满足完整三标签通过标准；90 个预期标签中正文识别 69 个（76.67%），LED 中心/有效位姿为 66 个（73.33%）。失败帧均保存并与相同 ROS 时间戳的推理报告关联。视觉复核与离线 OCR 显示漏检随距离、正偏航、负向横移而出现；点云时间同步维持为 0 ms。30 条诊断的外部延迟中位数/P95 为 1.196/1.820 s；感知节点输入到输出为 1.097/1.241 s，配对后处理为 1.047/1.131 s，其中 Tesseract 三行串行推理及图像编码/子进程调用为 1.041/1.126 s，点云关联约 2.3 ms、标记检测约 2.5 ms。外部延迟尾部抖动高于节点内部阶段；未采集主机 CPU 利用率，因此不能据墙钟延迟判断 CPU 饱和。节点墙钟耗时主要集中在 OCR 路径，提高相机或雷达帧率不会缩短这部分处理时间。原单场景回归在加入计时字段后通过，最大 XYZ 误差 2.45 mm。

此结果是受控合成数据的可重复性/失效边界试验，不是统计独立样本或实机科研结论：同条件重复来自单个 Gazebo 进程，固定照明/字体/纹理，且只有 `fire_on` 灯态。真实设备效果还需真实面板采集集、独立运行/光照随机化、与论文 PP-OCRv5 的模型/训练设置对齐及动态机器人验证。建议下个独立 target 比较一次调用的 OCR 和归档 PP-OCRv3/可获取的 PP-OCRv5 路径，先用已保存失败帧与 11 类离线图集量化精度/延迟，再决定模型替换。
