# 合成面板 OCR 与 LED 关联基线

## Goal

建立只在 WSL 用户目录运行的可重复离线基线：合成中文消防面板图像、光照/视角/遮挡和亮灭变化，从实际 OCR 输出框关联灯位并统计错误及拒识。结果须保留逐样本真值和预测，作为以后接入归档 Paddle 模型与实时双目图的同一评价契约。

## Dirty-State Note

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

这些文件属其他工作，不编辑、不暂存。

## Owned Files

- `plan/2026-09-24-synthetic-panel-baseline/plan.md`
- `stereo_sim/scripts/generate_panel_perception_cases.py`
- `stereo_sim/scripts/evaluate_panel_perception.py`
- `stereo_sim/scripts/run_panel_perception_baseline.sh`
- `stereo_sim/README.md` 中本基线使用说明
- `plan/log.md` 中本 target 条目

## Read-Only Files

- `reference/industrial_pc_2026-09-22/**`
- `论文_md版/**`
- 其他 `stereo_sim` 源码和用户未跟踪文件

## Shared Dependencies

- OpenCV、Pillow、WSL 系统中文字体；用户目录中的 Tesseract 命令及 `chi_sim` 数据（若本轮能无 sudo 下载）
- 面板标签/LED 语义与论文第 4 章 OCR→灯态关联流程
- 现有 graph_map 投影链的坐标约定；本 target 只做离线视觉评测，不修改在线 ROS 运行链

## Expected Work

1. 生成带真值的不同视角、光照、反光、遮挡和灯态面板图。
2. 对 OCR 实际输出框关联其左侧 LED，输出标签、颜色、亮灭、未知及位置；没有 OCR/可见灯时不得填入真值。
3. 运行基线，保存逐样本结果、摘要和失败样本索引，记录精度与明确局限。

## Validation

- Python 编译、Shell 语法检查。
- WSL 实际运行全组合，核对样本数量、真值/预测分离、遮挡拒识和灯态变化；评估必须从 OCR 输出而非标签真值框获得文本位置。
- `git diff --check`、`git status --short --branch`；只暂存本 target 文件。

增加生成和评估脚本是因为没有归档实拍数据，而识别/关联逻辑的回归风险只能通过保留真值和失败样本的确定性评测发现。全量 ROS/Gazebo 回归不属于本离线 target。

## Experience Signal (for human review)

原归档训练样本和 v5 权重缺失，离线合成成绩不可外推真实场景。

## Commit Intent

```text
feat: add reproducible synthetic panel perception baseline
```
