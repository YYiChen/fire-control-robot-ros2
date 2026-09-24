# 开源文字地标导航项目复核

日期：2026-09-24
范围：只在 WSL 用户目录运行 Gazebo 仿真；未连接实机，未修改系统 ROS、现有 `~/ros2_ws` 或上游仓库。

## 结论

这组开源项目适合作为“文字识别结果如何进入地图、再被转换成 Nav2 目标”的架构样例。独立 WSL 工作区中的 5 个 ROS 包均已编译；Gazebo RGB-D 场景也实际产出了 `Exit` 文字地标和二维栅格地图。

它还不能直接解决本项目的消防按钮问题。NavOCR 在本次 11 张合成面板图上只找到了重复出现的“主电工作”，32 个可见文字实例中正确识别 11 个；TextMap 接收深度图而不是本项目的双目点云。Nav2 生命周期通过延迟激活可以启动，短程目标也能运行；但同一安全观察位的长程导航未通过，过程中出现陈旧 `map→odom` TF、无有效轨迹和车体倾斜。本项目暂不应把该静态文字地图用于自主接近。建议复用 TextMap 的“检测结果—深度—位姿—语义地标”接口思路，保留本项目的按钮/LED检测、双目 XYZ 和图地图投影实现，再补时间同步门控、地图/世界坐标验收和可重复的导航闭环测试。

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

### Nav2 生命周期、动作与安全观察位复验

复验仍在 WSL 用户目录和独立 ROS 域运行，没有连接实机或改动上游 checkout。原先生命周期 manager 等待 action server 的问题可用一个启动顺序绕过：`autostart:=false`，等 map server、AMCL 和 Nav2 服务出现后，再分别调用 localization/navigation lifecycle manager 的 `manage_nodes`。多个独立启动中两个服务均返回 `success=True`，`/controller_server` 为 active，`/navigate_to_pose` action server 可用。这证明 Nav2 可在该隔离环境内激活；它没有证明自动启动 race 的根因已查清。

短程目标 `(-6.5,-1.8)` 在 5 Hz 下 4 次独立冷启动均返回 action `SUCCEEDED`。30 Hz 下另一次短程服务器日志有 `Reached the goal!`、`Goal succeeded`；当次 action 客户端被 8 秒外层 timeout 先结束，因此服务器成功与客户端超时分别记录，不把它当作完整客户端验收。

随后用同一个已检查净空的墙前安全观察位 `(-5.5,-3.0)`（距离南墙 0.85 m）比较 5 Hz 与隔离 harness 中的 30 Hz 雷达：

| 雷达 | `/scan` | NavigateToPose | 终点/运行证据 |
|---|---:|---|---|
| 原版 5 Hz | 4.997 Hz | `SUCCEEDED` | 同为 317.442 s 的里程计与 map TF 分别为 `(-5.489,-3.245)` 与 `(-5.583,-2.951)`，两者相差 0.309 m；map 下距目标 0.097 m，里程计坐标距目标约 0.245 m。速度已近零，距南墙约 0.605 m。Action 成功状态没有证明达到配置的 0.05 m 定位精度。 |
| 30 Hz 第一次 | 约 29.39 Hz | `ABORTED` | Nav2 记录 819 条轨迹均无效，spin 恢复报告前方碰撞。最终 odom 与 map TF 快照时间不同，不用它们计算定位差。 |
| 30 Hz 第二次冷启动 | 约 30 Hz 输入配置 | 目标已接受；客户端 60 秒无结果，之后停止 Nav2/Gazebo | 1444 个 TF/里程计近同步样本的时间差中位数 0.034 s、最大 0.102 s。起点 `map→odom` 平移接近零；运动后段它变为约 `(-1.393,-3.991,-0.683)`，而 odom 位置距目标约 0.300 m、组合后的 map 位置距目标约 2.167 m，车仍以约 0.348 m/s 运动。Nav2 另记录 TF 数据时间约 194.1 s、变换时间约 59.494 s，并持续报变换过旧。车体随后出现明显倾斜和非零速度，立即停止本次仿真。 |

这组结果证明 30 Hz 将激光话题提高到约 29.4 Hz，但没有证据说明它能修复长程导航。长程路线在 30 Hz 下两次都未通过；第二次不算 action 完成，也不作为性能比较的成功试次。AMCL 隔离参数为 `update_min_d=0.25 m`、`update_min_a=0.2 rad`、`transform_tolerance=1.0 s`；controller frequency 为 10 Hz，local/global costmap 更新频率为 5/1 Hz。运动失败试次出现 TF 超龄报错，但根因尚未确定，也不能排除碰撞后车体倾斜使激光扫描失配。

30 Hz 测试模型是 TurtleBot3 Burger stock SDF 的用户目录副本，仅把雷达 `update_rate` 从 5 改为 30 Hz；没有施加平面约束，所以车体可发生 roll/pitch。观察到的倾斜来自该未约束 Gazebo 动力学试次。后续移动导航前应先做独立的平面稳定性与碰撞检查，再重新测试定位链。

为区分运行时延与 AMCL 静止更新行为，另做了 20.02 秒、未发送任何运动目标的 30 Hz 仿真审计：`/scan` 29.27 Hz、`/odom` 29.32 Hz、AMCL 的 `map→odom` TF 约 30.07 Hz；这些时间戳最大间隔均为 34 ms。最后的 `map→odom` 时间领先最后的 scan 约 1.0 s，正好对应配置的 `transform_tolerance`。这段静止窗口没有 `/amcl_pose` 周期消息，但 TF 持续发布，所以 `/amcl_pose` 的静止期无新消息不能作为 AMCL 停摆证据。静止时序正常不能证明运动过程定位正常；运动失败中的旧 TF 更可能与移动/碰撞阶段有关，具体触发条件仍待独立定位。

逐次结构化记录保存在 WSL `~/textmap_evaluation/output/nav2_retest/nav2_retest_20260924.json`；时间审计脚本只在 WSL `~/textmap_evaluation/harness/` 新增，未修改固定上游源码。Gazebo harness 未提供可用的模型真值话题/服务，因此里程计不冒充真值。完整文字导航仍未通过。

### 平面稳定候选与短程 Nav2 复验（2026-09-24）

在独立 `planar_stability` harness 中，保留 stock Burger 的轮驱、碰撞与传感器，仅加载一个对 `base_link` roll/pitch 施加恢复力矩的 Gazebo 测试插件。它不是物理防倾倒结构，也没有接入项目正式启动流程。此前的 22 秒低速运动/轻触静态墙序列中，候选最大 roll/pitch 约 0.823°/2.345°；同序列无约束 stock 对照约 1.432°/13.653°。Gazebo 接触流仍报告底盘撞墙及车轮/脚轮接地。相同 0.25 rad/s 原地转向 4 秒时，候选 `/odom` yaw 约 0.985 rad，stock 约 0.991 rad。

候选模型随后两次冷启动运行 Nav2，从 map `(-6.5,-1.0)` 导航到 `(-6.5,-1.8)`，两次 NavigateToPose 均返回 `SUCCEEDED`，约 7.8 秒完成。第二次运行 `/scan` 与 `/odom` 约 29.4 Hz、最大间隔约 34 ms；IMU 约 166.7 Hz、最大间隔约 6 ms；`map→odom` 和 `odom→base_footprint` TF 约 29.4 Hz、最大间隔约 34 ms。按 TF 消息到达时刻配对的中位差约 1 ms，AMCL 的 `map→odom` header stamp 则固定领先 odom TF 1 秒，与 `transform_tolerance=1.0 s` 配置相符。短程移动期间未观察到此前的 `map→odom` 数据过旧报错；启动 global costmap 时出现过一次短暂的向过去外推等待，随后 costmap 正常启动。最终 Nav2 feedback 剩余距离约 0.04 m，候选 IMU 最大绝对 roll/pitch 约 0.003°/0.090°。

该结果只通过短程空旷路线的仿真门，不解释 stock 长程失败的根因，也没有复验长程路线、复杂碰撞、窄通道、导航过程中的接触或地图/世界全局对齐。短程成功不代表长程旧 TF 问题已修复；候选插件不能用于真实机器人。结构化数据和 probe 保存在 WSL `~/textmap_evaluation/output/planar_stability/nav2_short_route_stabilized.json` 与 `~/textmap_evaluation/harness/planar_stability/nav2_short_route_probe.py`。

`Exit` bridge 命令没有发送。固定上游 bridge 的 ray-march 仍把目标选在离南墙约 0.126 m 处；这个距离小于 Burger `robot_radius=0.22 m`，也没有足迹净空保证。不能用 3D 地标表面点代替墙前停靠目标。

## 实现边界与对本项目的适配

1. **TextMap 不负责 SLAM。** 它订阅 OCR `Detection2DArray`、深度 `Image`、`CameraInfo`、`/odom`，并可读 RTAB-Map `/mapGraph`；它把检测附着到外部 SLAM/定位结果。示例仓库也明确说明 RTAB-Map 实机示例当前是文字建图，尚未稳定支持 2D 路径规划。见 [TextMap 接口](https://github.com/kc-ml2/TextMap) 与 [TextMap_Examples 范围说明](https://github.com/kc-ml2/TextMap_Examples)。
2. **深度配对缺少时差上限。** `TextMap/include/textmap/textmap_node.hpp` 的 `findClosestDepth()` 选择最近时间戳，但不拒绝超出某个阈值的帧；`textmap_node.cpp` 只记录检测和深度的时间差用于调试。运动时可能把旧深度套到新文字框。本次静止仿真不能检验这个风险。
3. **取的是文本框中心深度。** 当前代码在 OCR 框中心取约 `10 × 10` 像素区域深度。它标注的是文字，不是控制按钮或 LED。按钮定位应使用按钮/灯的像素中心或轮廓，并以同一校正左目像素坐标查询同步视差或点云。
4. **当前双目输出需做适配。** 上游消费对齐深度图；本项目主要输出 `/stereo/points2`。适配节点应明确图像/点云像素对齐、相机内外参、非零且足够新鲜的时间戳、无效深度剔除，以及按图像采集时刻查 TF，再经过 `vo_odom → graph_map`。对时差、缺 TF 或无效视差应拒绝发布地标。
5. **地标合并面向房间标牌。** 上游按文本相似度和空间距离合并重复观测；相邻 13 cm 左右的“火警/故障/主电”控件可能被混并。消防面板应保留稳定的面板 ID、控件 ID、状态、二维框、深度/XYZ、时间戳、置信度和观测来源，不能只用 OCR 字符串当唯一地标键。重复静止帧要降权，跨机器人位姿的独立观测才用于稳定性判断。
6. **bridge 不实现论文里的 A* 或 B 样条。** 它沿机器人到文字地标的直线采样空闲栅格来选一个附近目标，找不到空闲格时会退回机器人当前位置，再把 `NavigateToPose` 交给 Nav2。是否使用 A*、DWB 或其他规划/控制器取决于 Nav2 配置；不能把这个文本桥接器说成论文的 A* + B 样条实现。
7. **对性能的日志影响。** NavOCR 节点在测试时对识别结果持续打印，日志量较大。需要确认实际 ROS 流输入速率、推理速率与丢帧策略，不能用 standalone FPS 代替 ROS 端到端表现。

## 建议的技术路线

1. 先独立确认 static map 与 Gazebo world 的旋转/平移、LaserScan/TF/clock 时间戳及 AMCL 的 map→odom 广播是否持续新鲜；用仍保持平面的低风险路线复现后再做长程导航。不要只通过增大 transform tolerance 掩盖旧 TF。
2. 建立一张由当前场景雷达实际扫出的、与 Gazebo world 对齐的导航占据图，再比较它与 TextMap 的单视角静态地图；报告未知区、墙/障碍位置差、足迹膨胀余量。
3. 在上述两个门通过前，不接入 `text_nav_bridge` 的 Exit 命令。之后应先修正 bridge 的目标选择逻辑，让它在墙/按钮前选有足迹净空的 stand-off 点，再做 Nav2 端到端验收。
4. 在项目双目语义节点上增加可测的图像—点云最大时间差门限与诊断字段；保留同一帧、同一像素坐标和同一图像时间戳的逐样本证据。
5. 用本项目已有的合成面板多距离、多姿态场景评估按钮/LED中心 XYZ 与 `graph_map` 坐标；对照独立 Gazebo 真值，同时报告检出率、漏检、错误地标、误差分位数和延迟。
6. 只在静态位置和运动期间的地标稳定性通过后，增加“靠近目标控件”的 Nav2 目标。目标点应落在按钮前安全接近位；不要让导航目标落在面板表面或机器人当前位姿。
7. 把 NavOCR 作为可对照的预训练模型，而不是消防面板主检测器；优先使用当前面板专用识别/LED链路，后续若有足够标注数据，再训练或微调面板专用目标检测器。

## 复核边界

本轮完成了固定提交源码审阅、隔离构建、OpenVINO 单图/合成面板推理、静态 RGB-D TextMap/Gazebo 建图冒烟，以及 Nav2 生命周期、短程动作和长程安全观察位的失败复验。没有完成成功的长程目标定位、TextMap bridge 端到端导航、双目输入接入、真实相机、真实消防主机或按键操作测试。上游代码、模型与 WSL 测试产物未复制进本项目仓库；项目仓库只记录评估结论和 WSL 复验数据位置。
