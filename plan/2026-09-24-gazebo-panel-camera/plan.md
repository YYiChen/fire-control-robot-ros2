# Gazebo 相机真实渲染的中文面板试验

## Goal

把上一 target 的合成中文面板作为 Gazebo Classic 纹理模型放进独立世界，证明左目 ROS 相机话题收到的是 Gazebo 渲染结果而不是离线原图，并对这一实际相机帧试跑现有 OCR/灯态分析。为后续与 VO、graph_map 联动提供可复现画面和失败记录。

## Dirty-State Note

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

三项未跟踪文件属于其他工作，保持原状。

## Owned Files

- `plan/2026-09-24-gazebo-panel-camera/plan.md`
- `stereo_sim/worlds/perception_panel.world`
- `stereo_sim/scripts/generate_gazebo_perception_panel.py`
- `stereo_sim/scripts/run_gazebo_perception_capture.sh`
- `stereo_sim/scripts/verify_gazebo_perception_capture.py`
- `stereo_sim/scripts/evaluate_panel_perception.py` marker-corner refinement only
- `stereo_sim/README.md` 的本实验说明
- `plan/log.md` 的本 target 条目

## Read-Only Files

- `reference/**`、`论文_md版/**`
- `stereo_sim/scripts/generate_panel_perception_cases.py`
- 其他 `stereo_sim` 文件和用户未跟踪文件

## Shared Dependencies

- Gazebo Classic 官方材质/网格资源目录约定
- 既有 `stereo_rig` 的左右相机话题，`stereo_test_world.launch.py` 的隔离启动
- 上一 target 的确定性合成面板和 OCR/LED 基线；Gazebo 生成文件仅放 WSL 用户目录

## Expected Work

1. 由已定义案例生成带纹理的独立面板模型和世界，不修改旧场景。
2. 在独立 ROS 域启动 Gazebo，采集左目 `Image`，保存原始时间戳、图像与模型生成参数。
3. 从捕获帧检验标记、文字/LED 可见性与现有基线能否识别；如达不到目标，留下实际渲染图和诊断，而不伪造成功。

## Scope Update 2026-09-24

Gazebo 实拍中 ArUco 约 47×47 像素，整数角点含约 1 px 偏差；用这四点将整板外推导致投影偏移，固定 ROI 漏掉首行文字。只扩展 `evaluate_panel_perception.py` 的 owned 范围来加入亚像素角点细化；不改 OCR、灯态或位置推理规则。Gazebo 验证器读取独立生成的真值，报告文字召回、灯态准确率和标记局部位置误差；实拍样本最大位置误差门槛为 0.02 m。验证同时覆盖 Gazebo 原始帧和离线基线回归。

## Scope Update 2 2026-09-24

单一 `fire_on` 状态不足以验证颜色分类。将 capture runner 扩展为可选接收生成案例名，以独立世界重启方式额外测试 `all_on` 与 `all_off`，覆盖红/黄/绿和熄灭状态；其余文件范围不变。

## Validation

- SDF/DAE XML 解析、Python 编译、Shell 语法。
- WSL 无界面隔离 Gazebo 实测：相机消息非空且非离线原图，记录捕获帧与识别输出；检查停止后无残留该隔离域节点。
- `git diff --check` 和 `git status --short --branch`；仅暂存 owned files。

运行时渲染与图像方向/纹理坐标是此 target 的主要风险，静态 XML 检查不足以替代实际相机帧验证；不跑无关机器人全量测试。

## Outcome 2026-09-24

- 以亚像素 ArUco 角点校正后，五次独立 Gazebo 启动均从左目 ROS 话题采到 640×480、非零时间戳帧：`fire_on` 三次、`all_on` 和 `all_off` 各一次。每次三行文字和三个灯态全部正确，覆盖红/黄/绿及熄灭；标记局部位置误差为 0.00036/0.00076/0.00149 m。
- 离线 11 样本回归得到 29/29 可见文字、28/28 灯态、28 个位置样本中位误差 0.001 m，所有遮挡门控通过、失败列表为空。
- Python、Shell、SDF/DAE XML 检查通过；隔离 ROS 域无残留节点，未发现该世界的 Gazebo 服务进程。
- 结果仅覆盖理想合成面板与固定视角；尚无真实相机、视角/距离矩阵、双目深度关联或 `graph_map` 投影验证。

## Experience Signal (for human review)

若 Gazebo Classic 纹理和相机帧的方向/尺寸与离线假设不一致，留实际截图和参数作为后续调整证据。是否提取经验由人决定。

## Commit Intent

```text
feat: render synthetic fire panel in isolated Gazebo camera test
```
