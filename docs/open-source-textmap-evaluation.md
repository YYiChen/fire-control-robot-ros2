# 开源文字地标导航项目复核

日期：2026-09-24
范围：只在 WSL 用户目录运行 Gazebo 仿真；未连接实机，未修改系统 ROS、现有 `~/ros2_ws` 或上游仓库。

## 结论

这组开源项目适合作为“文字识别结果如何进入地图、再被转换成 Nav2 目标”的架构样例。独立 WSL 工作区中的 5 个 ROS 包均已编译；Gazebo RGB-D 场景也实际产出了 `Exit` 文字地标和二维栅格地图。

它还不能直接解决本项目的消防按钮问题。NavOCR 在本次 11 张合成面板图上只找到了重复出现的“主电工作”，32 个可见文字实例中正确识别 11 个；TextMap 接收深度图而不是本项目的双目点云；`text_nav_bridge` 的 Nav2 生命周期未能激活，所以没有验证它能否把地标导航跑完。建议复用 TextMap 的“检测结果—深度—位姿—语义地标”接口思路，保留本项目的按钮/LED检测、双目 XYZ 和图地图投影实现，再补时间同步门控和可重复的导航闭环测试。

## 固定源码与许可检查

源码保存在 WSL `~/textmap_evaluation/src/`，用固定提交复核，避免结果随上游 `main` 漂移。

| 仓库 | 固定提交 | 作用 | 本次检查到的许可信息 |
|---|---|---|---|
| [NavOCR](https://github.com/kc-ml2/NavOCR/tree/02e360140fd7aa82eee9b58b80357fc3cb920766) | `02e360140fd7aa82eee9b58b80357fc3cb920766` | ROS 文字检测与识别 | 仓库含 Apache-2.0 `LICENSE` |
| [TextMap](https://github.com/kc-ml2/TextMap/tree/ccf62fdc3f3c2e1e62b1464d8e50cd9c3daf0199) | `ccf62fdc3f3c2e1e62b1464d8e50cd9c3daf0199` | 将文字框投影成 SLAM 地图中的 3D 地标 | 仓库含 Apache-2.0 `LICENSE` |
| [text_nav_bridge](https://github.com/kc-ml2/text_nav_bridge/tree/29d4667e4fa3cf00dcc607bf617e4685f23eb756) | `29d4667e4fa3cf00dcc607bf617e4685f23eb756` | 文本指令匹配地标并请求 Nav2 | README 声明 Apache-2.0；固定源码中未找到根目录 `LICENSE` 文件 |
| [text_nav_sim](https://github.com/kc-ml2/text_nav_sim/tree/ebf63cbf346c6501f6525cd63bb0ce13fb2c02c7) | `ebf63cbf346c6501f6525cd63bb0ce13fb2c02c7` | TurtleBot3 RGB-D Gazebo 场景 | 固定源码中未找到根目录 `LICENSE` 文件 |
| [TextMap_Examples](https://github.com/kc-ml2/TextMap_Examples/tree/be0981cea587f524c7479605e5d285862438db26) | `be0981cea587f524c7479605e5d285862438db26` | 组合启动与教程 | 仓库含 Apache-2.0 `LICENSE` |

NavOCR、TextMap 和示例仓库的项目许可不能自动代表其中所有模型权重、训练数据和第三方模型的再分发许可；把模型或代码放进同学共享仓库前，应按具体文件再核对。本次以提交 SHA 固定源码，不跟随会变化的 `main` 分支。

## 隔离环境与构建

- ROS 2 Humble、Gazebo Classic 11；包、虚拟环境、模型和结果都在 `~/textmap_evaluation/`。
- Humble 环境缺少 `vision_msgs`。为不动系统安装，使用官方 Humble 源码固定提交 `5f35e557cfb5e085ac5c6dc0210c5736c2c362f7`，只编入本隔离工作区。
- 使用本地 OpenVINO 2026.4.0 运行 NavOCR；Python 依赖装在 `~/textmap_evaluation/venv`，未安装到系统 Python。
- `colcon build` 结果：`vision_msgs`、`text_nav_bridge`、`text_nav_sim`、`navocr`、`textmap` 五个包全部完成。
- 上游 `textmap_slamtoolbox.openvino.launch.py` 引用了不存在的 `TextMap/config/textmap_sim.yaml`。本次改用存在的 `textmap_slamtoolbox.launch.py`，通过参数传入 OpenVINO 配置；没有改写上游 checkout。

## 实测结果

### NavOCR 对中文面板图

在本项目生成的消防面板合成图上，用上游 OpenVINO 模型独立推理：

- 英文 `Kitchen` 标牌单图检出并识别为 `Kitchen`，检测置信度约 0.93；只是一张图的冒烟检查。
- 11 张带真值面板图中，32 个可见文字实例仅正确检出/识别 11 个（34.4%），而且只命中同一个“主电工作”标签，漏掉“火警”和“故障”。18 张额外诊断图也只检出“主电工作”。因此，这个导航标牌检测器不能直接作为消防控制面板检测器。
- 热身后的单图流程在 18 张诊断图上平均约 15.18 FPS；11 张真值图含冷启动的平均约 14.17 FPS。输入是重复播放的静态合成图，不是相机持续流或完整 ROS 节点吞吐率。

逐图和汇总数据位于 WSL `~/stereo_sim_generated/panel_perception_baseline/`。

### TextMap 静态仿真冒烟检查

将上游启动流程改成无界面 `gzserver` 并关闭 RViz 后，在独立 ROS 域 109 中观察到 TurtleBot3 `waffle_rgbd`、相机 RGB-D、`/odom`、`/scan`、TF 和 `/clock`。NavOCR 连续识别到 `Exit`；TextMap 导出：

```text
text: Exit
position: (-5.5053, -3.7209, 0.6558) m
confidence: 1.0
observation_count: 5759
```

同时保存了 `map.pgm` / `map.yaml`：分辨率 `0.05 m/cell`，尺寸 `45 × 97`。产物保存在 WSL `~/textmap_evaluation/output/textmap_sim/`。机器人没有被遥控行驶；地标计数来自静止单视角的重复帧，不等于 5759 次独立定位观测，也没有验证绕行、回环或地标稳定性。

另有一个需要后续查清的坐标差异：场景文件把 `text_sign_exit` 放在 `(-5.5, -3.85, 1.8)`，导出的地标 Z 是 `0.656 m`。XY 接近，但在独立核准 `map` 与 Gazebo world 的变换之前，这既不能直接判为算法错误，也不能把 3D 高度当作通过验证。

### Nav2 导航未通过激活门

使用保存的地图和地标启动无界面 Nav2 与 `text_nav_bridge` 时，bridge 在等待期后报告 NavigateToPose action server 不可用；Nav2 lifecycle manager 持续等待 `amcl/get_state`、`controller_server/get_state`。同一轮 ROS 图查询又出现服务列表可见、生命周期查询报节点不存在或挂起的矛盾结果，根因尚未查明。没有发送导航目标，也没有依据这次启动判断路径规划器好坏。

停止本次启动的 Nav2、Gazebo 和挂起的诊断查询后，检查未发现本轮仿真节点或 `gzserver` 留存。此次结论是“TextMap 建图输出可产生；Nav2 集成仍未验证”，不是完整文字导航闭环成功。

## 实现边界与对本项目的适配

1. **TextMap 不负责 SLAM。** 它订阅 OCR `Detection2DArray`、深度 `Image`、`CameraInfo`、`/odom`，并可读 RTAB-Map `/mapGraph`；它把检测附着到外部 SLAM/定位结果。示例仓库也明确说明 RTAB-Map 实机示例当前是文字建图，尚未稳定支持 2D 路径规划。见 [TextMap 接口](https://github.com/kc-ml2/TextMap) 与 [TextMap_Examples 范围说明](https://github.com/kc-ml2/TextMap_Examples)。
2. **深度配对缺少时差上限。** `TextMap/include/textmap/textmap_node.hpp` 的 `findClosestDepth()` 选择最近时间戳，但不拒绝超出某个阈值的帧；`textmap_node.cpp` 只记录检测和深度的时间差用于调试。运动时可能把旧深度套到新文字框。本次静止仿真不能检验这个风险。
3. **取的是文本框中心深度。** 当前代码在 OCR 框中心取约 `10 × 10` 像素区域深度。它标注的是文字，不是控制按钮或 LED。按钮定位应使用按钮/灯的像素中心或轮廓，并以同一校正左目像素坐标查询同步视差或点云。
4. **当前双目输出需做适配。** 上游消费对齐深度图；本项目主要输出 `/stereo/points2`。适配节点应明确图像/点云像素对齐、相机内外参、非零且足够新鲜的时间戳、无效深度剔除，以及按图像采集时刻查 TF，再经过 `vo_odom → graph_map`。对时差、缺 TF 或无效视差应拒绝发布地标。
5. **地标合并面向房间标牌。** 上游按文本相似度和空间距离合并重复观测；相邻 13 cm 左右的“火警/故障/主电”控件可能被混并。消防面板应保留稳定的面板 ID、控件 ID、状态、二维框、深度/XYZ、时间戳、置信度和观测来源，不能只用 OCR 字符串当唯一地标键。重复静止帧要降权，跨机器人位姿的独立观测才用于稳定性判断。
6. **bridge 不实现论文里的 A* 或 B 样条。** 它沿机器人到文字地标的直线采样空闲栅格来选一个附近目标，找不到空闲格时会退回机器人当前位置，再把 `NavigateToPose` 交给 Nav2。是否使用 A*、DWB 或其他规划/控制器取决于 Nav2 配置；不能把这个文本桥接器说成论文的 A* + B 样条实现。
7. **对性能的日志影响。** NavOCR 节点在测试时对识别结果持续打印，日志量较大。需要确认实际 ROS 流输入速率、推理速率与丢帧策略，不能用 standalone FPS 代替 ROS 端到端表现。

## 建议的技术路线

1. 先在新的隔离 ROS 域把现有 Nav2 的 map server、AMCL、controller/planner lifecycle 和 `NavigateToPose` action 单独启动并验收，再接 `text_nav_bridge`。当前这一关未过。
2. 在项目双目语义节点上增加可测的图像—点云最大时间差门限与诊断字段；保留同一帧、同一像素坐标和同一图像时间戳的逐样本证据。
3. 用本项目已有的合成面板多距离、多姿态场景评估按钮/LED中心 XYZ 与 `graph_map` 坐标；对照独立 Gazebo 真值，同时报告检出率、漏检、错误地标、误差分位数和延迟。
4. 只在静态位置和运动期间的地标稳定性通过后，增加“靠近目标控件”的 Nav2 目标。目标点应落在按钮前安全接近位；不要让导航目标落在面板表面或机器人当前位姿。
5. 把 NavOCR 作为可对照的预训练模型，而不是消防面板主检测器；优先使用当前面板专用识别/LED链路，后续若有足够标注数据，再训练或微调面板专用目标检测器。

## 复核边界

本轮完成了固定提交源码审阅、隔离构建、OpenVINO 单图/合成面板推理和静态 RGB-D TextMap/Gazebo 建图冒烟测试。没有完成 Nav2 单点目标、动态机器人运动中的语义地图一致性、真实相机、真实消防主机或按键操作测试。上游代码、模型与 WSL 测试产物未复制进本项目仓库；只会提交本评估文档、README 摘要、plan 和 log。
