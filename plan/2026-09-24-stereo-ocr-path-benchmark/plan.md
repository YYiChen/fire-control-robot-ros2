# 面板 OCR 单次调用路径基准

## Goal

在已有合成相机验收图和多距离/姿态失败帧上，比较现有三行分别调用 Tesseract 与将三张原始裁剪保存为单个多页 TIFF 后一次调用的文字检出、LED 中心/状态/面板坐标误差和墙钟耗时；只有完整链路不退化且耗时有明确改善时才集成候选实现。并审阅归档中的 PP-OCRv3/OpenVINO 工程能否作为后续本地 OCR 评估路径。仅做离线/仿真，不接实机，不安装或升级系统依赖。

## Dirty-State Note

开始时 `main` 位于 `040ae62`，已与 `origin/main` 同步。用户已有未跟踪的 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world` 与 `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak`；均不属于本 target，保持原样。

## Owned Files

- `plan/2026-09-24-stereo-ocr-path-benchmark/plan.md`
- `plan/log.md`
- `stereo_sim/README.md`
- `stereo_sim/scripts/evaluate_panel_perception.py`
- `stereo_sim/scripts/benchmark_panel_ocr_batch.py`
- `stereo_sim/scripts/stereo_panel_depth_node.py`

## Read-Only Files

- `stereo_sim/scripts/verify_stereo_panel_pose_sweep.py`
- `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/`（工控机归档源码和模型）
- WSL 输出 `~/stereo_sim_generated/panel_perception_baseline/` 与 `panel_pose_sweep_diagnostic2_20260924/`
- ROS、Gazebo、Tesseract overlay 与其他用户文件

## Shared Dependencies

- WSL Ubuntu 22.04 + ROS2 Humble 已有 Python/OpenCV/NumPy 与用户目录下的 Tesseract 4 overlay。
- 比较图像为 11 张带标签真值的合成场景图，以及姿态实验的唯一失败图像（按 SHA-256 去重）；连续重复帧不作为独立图片计数。
- 基线执行当前生产推理函数；候选将三条既有文字 ROI 分别写入 TIFF 页，由 Tesseract 以原有 PSM 7 单次处理多页，再把页内框映射回原校正图并复用当前 LED/位置逻辑。

## Expected Work

1. 审阅当前 Tesseract 推理链和归档 PP-OCRv3 C++/OpenVINO 依赖，区分已具备模型、源码和运行时，不猜测其可直接运行。
2. 新增离线基准脚本；比较现有三行调用、堆叠 ROI 单次调用、保持三行独立版面但合并为单个多页 TIFF/PSM 7 调用、保留行间空间的整块文字 ROI（PSM 4/6/11）和全图稀疏文字模式（PSM 11）；保留逐图原始识别、LED、坐标和计时，按有真值合成集与固定 `fire_on` 姿态失败集分别统计。
3. 以至少 5 次热运行测量每图耗时；报告中位数/P95、全文检出率、LED 中心检出率、灯态准确率和位置误差，并附样本数及数据限制。
4. 验证基准里的现有 Tesseract 路径与已有验收报告一致；任何合成全局指标或已知困难姿态的文字/LED/位置指标下降时，不改生产节点，仅留下评估证据。
5. 通过门槛的多页 TIFF/PSM 7 路径在 17/17 张唯一图像上得到与当前实现逐图完全相同的文字框、LED 状态和坐标；据此在共享推理函数中集成该路径，并将双目节点默认切到新实现。复跑离线基准、三灯状态 depth 回归和 30 帧姿态诊断，确认节点侧延迟下降且逐条件识别结果一致后更新 README/log。
6. 执行 Python/Shell 语法检查、`git diff --check`，只暂存 owned files，提交本 target 并推送远端。

## Validation

- 基准脚本的 Python 编译；生产节点若改动则重复运行 `run_stereo_panel_depth_test.sh` 固定三色案例。
- 对同一输入图像计算候选和基线识别；核实失败/遮挡样本没有被移除，重复的同一姿态图按唯一图像报告。
- 热运行至少 5 次，分别报告 OCR 阶段及全推理耗时；单次调用从 ROI 编码到进程返回均用墙钟测量，不称作 CPU 利用率。
- 基线对照之前保存的生成真值与在线失败记录；对外推位置只在输出真实可用坐标时计误差，不把漏检从分母排除。
- 仓库 `git diff --check`、目标 dirty-state 与 stage 路径审计；保留三项用户未跟踪文件。

## Commit Intent

只提交本 target owned files。报告和临时图继续放在 WSL 的 `~/stereo_sim_generated/`，不提交测量输出或用户数据。

## Open Risks

- 测试图片是受控纹理，不代表真实消防主机文字、反光、污损、屏幕像素或工控机性能。
- OpenCV 4.5.4 提供多页 TIFF 写入，Tesseract TSV 能保留页号；当前离线原型在 17 张唯一图像（11 张真值图 + 6 张失败姿态图）上与三次调用的文字、文本框、LED、坐标结果均逐图完全一致。
- 堆叠 ROI 虽在 11 张带真值图上 100% 文字/灯/中心检出，但在去重姿态失败图文字召回为 50.0%，低于三次调用的 61.1%；生产集成改用多页路径，不使用堆叠方案。
- 工控机归档含 PP-OCRv3 检测/识别模型和 C++ OpenVINO 包装；当前 WSL Python 未安装 Paddle/PaddleOCR，OpenVINO 动态库也未出现在系统 linker cache 中，因此尚未运行归档 C++ 路径。
- 一次 all_on 仿真回归首次运行在 Gazebo `gzserver` 退出码 255 时报告了错误 LED 状态；用新的输出目录单独重跑通过。首次失败没有保存相机帧，具体启动瞬态原因未证实。

## Outcome

离线基准 11 张有真值的合成图中，三次调用和多页 TIFF 都得到 29/29 可见标签、28/28 LED 中心/灯态及相同坐标，标记遮挡负例均拒绝输出。另对姿态失败目录按 SHA-256 去重得到 6 张困难图，18 个目标中两种方式的文字检出均为 11/18、LED 中心/灯态均为 10/18，坐标结果逐图一致。17 张唯一图像两种方式所有 OCR 行、文本框、LED 状态和面板坐标均逐图完全相同。各图 5 次热运行中，合成集全流程中位数约从 0.994 s 降至 0.335 s。

集成后单场景 `fire_on`、`all_on`、`all_off` 三种 Gazebo 双目深度回归均通过；图像/点云时间差为 0，最大 XYZ 误差分别为 2.45、2.35、2.45 mm。30 条新姿态诊断 30/30 采集，失败条件及标签状态与旧实现逐项相同；90 个必需标签中正文识别 69 个（76.67%）、LED 中心/有效位姿 66 个（73.33%），成功位置误差中位/P95/最大值 2.426/5.121/5.575 mm。节点 OCR 中位/P95 从 1.041/1.126 s 降至 0.407/0.470 s；外部端到端中位/P95 从 1.196/1.820 s 降至 0.513/0.580 s。输入样本间隔仍为 0.75 s；本 target 没有改处理频率或 SLAM/雷达设置。

归档源码采用 C++ OCR 检测、识别和 OpenVINO CPU 推理，识别器按最多 6 个文本框批处理；归档模型是 PP-OCRv3，不是论文的 PP-OCRv5。上游 PaddleOCR 另有 Linux C++ 本地部署文档；目前 WSL 缺少运行这些归档资产的 OpenVINO/Paddle 环境，所以后续应独立建立用户目录下的可复现评测环境，不应把归档当成已经能运行的仿真节点。
