# 工控机论文复现依赖补全

## Goal

补足首轮工控机参考归档中缺失、但论文闭环复现或后续双目设计直接需要的工程文件，并记录源码层面与真实运行层面的剩余缺口。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

两份用户场景文件无关且保持只读。

## Owned Files

- `reference/industrial_pc_2026-09-22/`
- `docs/industrial-pc-source-audit.md`
- `plan/2026-09-22-industrial-pc-completeness/plan.md`
- `plan/log.md`

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- 工控机上的所有远程文件

## Shared Dependencies

- 首轮工控机参考归档与 `SHA256SUMS.txt`
- 论文的导航、机械臂、升降、视觉/OCR 与任务编排模块
- 未来双目改造所需的同步、矫正、视差/深度接口

## Expected Work

1. 归档实体底盘与传感器启动、Nav2、N10P 雷达、Astra 深度相机、速度仲裁、多点巡航、Piper/MoveIt、升降与机器人状态发布源码。
2. 归档单目 OCR 与海康相机包装器缺失的模型、PaddleOCR 代码及 MVS SDK，使源码参考具备完整构建材料；保留图像样本和私钥的排除规则。
3. 提供经过本地脱敏的任务流程配置和一份通用双目处理启动参考；记录其未与消防链路集成、不可直接运行的边界。
4. 更新清单、校验和、审计报告和计划日志。

## Validation

- 对新增项目关键入口文件与远端比较 SHA-256；对本地经过脱敏的配置逐项列明例外。
- 检查不含私钥、未脱敏内网 RTSP 地址或实际任务密码。
- `git diff --check` 与 `git status --short --branch`。

原始上游文件若自带尾随空白，将保留字节以保持校验一致，并在日志中记录该格式检查的已知结果。

## Experience Signal (for human review)

论文的“深度相机用于下层避障”与后续“自建双目”不是同一件事；通用 `stereo_image_proc` 示例只能作为接口参考，不能代表已集成到消防识别链路。

## Commit Intent

```text
docs: complete industrial pc reproduction references
```
