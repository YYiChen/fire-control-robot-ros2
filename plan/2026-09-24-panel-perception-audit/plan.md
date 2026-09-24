# 消防面板视觉数据与源码可复现性审计

## Goal

核对论文、归档的面板识别代码/模型、现有双目仿真链之间的差距，形成下一轮仅仿真的可执行实验门槛；不把彩色按钮检出视为真实 OCR/灯态识别。

## Dirty-State Note

起始 `git status --short --branch`：

```text
## main...origin/main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
?? reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak
```

上述文件来自其他工作，不属于本 target，不编辑、不暂存。

## Owned Files

- `plan/2026-09-24-panel-perception-audit/plan.md`
- `docs/panel-perception-reproducibility-audit.md`
- `plan/log.md` 中本 target 的条目

## Read-Only Files

- `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/**`
- `reference/industrial_pc_2026-09-22/packages/fia_launch/**`
- `论文_md版/04_第4章_视觉感知与自主交互.md`
- `stereo_sim/**`
- 上述三个未跟踪文件

## Shared Dependencies

- 论文的 PP-OCRv5 微调与 LED 文本关联叙述
- 原始 `fire_panel_rec` 的 PP-OCRv3 推理模型及 ROS 消息接口
- 当前双目按钮/图地图隔离仿真结果

## Expected Work

1. 清点图像、标注、模型及推理依赖，并逐条标明证据来源。
2. 审核 OCR/LED 数据流的可复现性与明确代码风险。
3. 定义可验证的仿真基准、数据门槛和不能宣称的结论。

## Validation

- 逐项用源文件、模型路径和本机环境输出复核报告中的事实。
- `git diff --check`、`git status --short --branch`，并仅暂存本 target 文件。

这是文档审计，不改变运行行为，因此不运行机器人、图形界面或全量测试。

## Experience Signal (for human review)

论文声称的模型版本与归档模型版本不一致；档案中的图像/标注缺失；原始 LED 代码的颜色空间前后不一致。是否提取经验由人决定。

## Commit Intent

```text
docs: audit panel perception reproducibility gaps
```
