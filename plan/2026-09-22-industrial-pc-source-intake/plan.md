# 工控机 ROS 源码择优归档

## Goal

从 `ubuntu@192.168.1.165` 只读调查后，择优归档与当前消防控制室机器人学习项目直接相关的 ROS2 源码、接口、启动配置和标定参考，并写明其可复用边界。

## Dirty-State Note

起始状态：

```text
## main
?? gazebo_scene/lab_room.launch.py
?? gazebo_scene/lab_room.world
```

这两个未跟踪场景文件为用户现有成果，不属于本 target，保持只读且不纳入提交。

## Owned Files

- `reference/industrial_pc_2026-09-22/`
- `docs/industrial-pc-source-audit.md`
- `plan/2026-09-22-industrial-pc-source-intake/plan.md`
- `plan/log.md`

## Read-Only Files

- `gazebo_scene/lab_room.launch.py`
- `gazebo_scene/lab_room.world`
- 工控机上的所有远程文件

## Shared Dependencies

- 工控机 ROS2 工作区 `/home/ubuntu/arm_ws/src`
- 当前项目的单目相机、ArUco 标记定位、OCR、任务编排和后续双目升级路线

## Expected Work

1. 确认单目采集、ArUco 手眼定位和识别链路的源码与配置。
2. 只复制自定义源码、消息/服务定义、启动参考、相机内参与标定参考；排除构建产物、第三方 SDK、模型权重、图像样本、私钥及含实际密码的任务配置。
3. 从本地副本移除历史复件和临时目录，并将写死的内网 RTSP 地址替换为占位符；远程原件保持不变。
4. 生成来源、文件清单、校验和与双目升级边界说明。

## Validation

- 比对未脱敏文件与远程所选文件的 SHA-256；列明经过脱敏的少数文件。
- 扫描本地归档，确认不含私钥、模型权重、第三方 SDK 或任务密码。
- `git diff --check`。
- `git status --short --branch`。

上述检查覆盖本 target 的复制完整性和敏感信息排除；归档只作参考，不在当前 Windows 项目编译，因此不跑全量 ROS 构建。

## Experience Signal (for human review)

远程工程中的 `hk_ost.yaml` 名为 `narrow_stereo`，但当前消防链路源码只请求单张 RGB 图像；相机文件名不能单独作为双目接入证据。

## Commit Intent

```text
docs: archive industrial pc vision source references
```
