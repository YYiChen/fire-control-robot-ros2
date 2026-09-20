# AGENT.md —— 项目上下文与交接说明

> **给任何接手本项目的 AI 读**。读完你应能回答：这是什么项目、现在到哪一步、下一步做什么、有哪些坑绝不能踩。
> 最后更新：2026-09-20

---

## 1. 项目是什么

- **一句话**：复现 + 学习一篇硕士论文的机器人技术栈。
- **论文**：《面向消防控制室主机状态信息识别与自主处置的机器人关键技术研究》——黄柏岚，西南交通大学机械工程学院硕士，导师宋兴国副教授，2026.05。（原 docx 在本目录）
- **用户**：王天琪（本科在读）；下文的"学长" = 论文作者。
- **项目性质**：用户要**学会搭这套技术栈**，**不强求 1:1 复刻**论文。学长源码在**工控机**里，**用户还没拿到**。

## 2. 当前环境（现状）

| 项 | 状态 |
|---|---|
| 宿主 | Windows 11（用户名 32126） |
| Linux | WSL2 + Ubuntu 22.04.5 LTS（用户 `yyyyyc001`，主机 `DESKTOP-PQF11PP`） |
| ROS | **ROS2 Humble**（`/opt/ros/humble`），colcon + rosdep 就绪，依赖体检 24/24 |
| 编辑器 | VS Code（Windows 侧）+ **Remote - WSL** 扩展 + Robotics Developer Environment（rde-pack）|
| 图形 | WSLg（`DISPLAY=:0`）→ Gazebo / RViz 可直接弹窗 |
| 仿真 | **Gazebo 已装**；`turtlebot3*` / nav2 / slam-toolbox / moveit 等学习依赖已装（PaddleOCR 未装，缺 pip） |
| 目录 | 工作区 `~/ros2_ws`；项目目录 `C:\Users\32126\Desktop\Leeds Homework\Semester 5\科研\ROS\` |
| 环境变量 | `TURTLEBOT3_MODEL=burger`（已写入 `~/.bashrc`） |

## 3. 当前状态 / 下一步

- **当前里程碑**：完成 **SLAM 建图**（用户明确：**先搞建图，导航以后再说**）。
- **当前卡点**：跑 `ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py` 后 **Gazebo 里没看到小车**（疑似 `TURTLEBOT3_MODEL` 在该终端未生效 → 世界开了但机器人 spawn 失败）。**待用户回贴自检结果确认**。
- **下一步**：
  1. 跑通标准世界的**建图**（turtlebot3_world + cartographer → 存地图）。
  2. 学**自建模型/世界**（Gazebo Building Editor / 手写 SDF）。
  3. 之后：**导航**（Nav2）→ **感知/操作**（MoveIt2 / ArUco / OCR）→ **整合**。
  4. 并行：去**工控机拷学长源码**。

## 4. 关键约定与"坑"（务必遵守）

1. **禁止 `sudo apt upgrade`** —— 会升级 ROS 包、破坏基线版本。（`apt install` 安全）
2. **代理 TUN / 全局模式会挡 HTTPS**（做 TLS 中间人 → 证书错）→ **联网装东西前先关代理直连**。
3. 代码放 **WSL 内 `~/`**，**别放 `/mnt/c`**（跨文件系统编译极慢）。
4. VS Code 开发类扩展装 **WSL 侧**；工作区用 **Remote-WSL** 打开（**勿用** `\\wsl.localhost\` UNC 路径）。
5. **（本会话 / 本 Agent）沙箱拦截 `wsl.exe`** → 本 Agent **无法直接执行 WSL 命令**，需**让用户回贴输出或截图**判断。⚠️ 已发现**其他 Agent 可直连 WSL**（见 `docs/project-data-index.md`）→ **能力因会话而异，使用前先复验**。
6. Ubuntu 后台 `unattended-upgrades` 会占 apt 锁 → **等锁释放**（`while sudo fuser /var/lib/dpkg/lock-frontend ...`）。
7. 用户环境**有代理**；`ping` 通不代表 HTTPS 通（MITM 会拦）。

## 5. 文件索引（本目录）

| 文件 | 内容 |
|---|---|
| `2023210500+...docx` | 论文原文 |
| `论文_md版/` | 论文 md 化（`00_总览与索引.md` + `01`~`06` 分章） |
| `论文解读与复现路线.md` | 论文提炼：技术栈 + 复现路线 |
| `ROS2技术栈_学习路线.md` | A~H 分阶段实操路线 |
| `AGENT.md` | 本文件（AI 交接） |
| `我要做的操作.md` | 用户当前操作清单 |

**脚本**（在 WorkBuddy 工作区 `C:\Users\32126\WorkBuddy\2026-09-18-19-50-39\`，WSL 经 `/mnt/c/...` 调用）：
`install_wsl2_direct.ps1`、`install_ros2_humble.sh`、`check_ros2_env.sh`、`setup_ros2_dev_env.sh`、`Open-ROS2-VSCode.bat`、`install_learning_stack.sh`、`diag_gazebo.sh`。

## 6. 论文技术栈速查

- **系统**：Ubuntu 22.04 + ROS2 Humble
- **导航**：Nav2、SLAM Toolbox、AMCL、A\*（+B样条）、DWB
- **视觉/位姿**：OpenCV、`aruco_ros`、`easy_handeye2`、camera_calibration
- **OCR**：PaddleOCR（PP-OCRv5）+ PaddleX（微调）
- **机械臂**：MoveIt2、`ros2_control`、OMPL
- **仿真**：Gazebo、Rviz2、Xacro/URDF
- **硬件**：i7-13700H 工控机、轮趣 S100 底盘、松灵 Piper 六轴、升降立柱、海康 MV-CU013-A0UC 相机、镭神 N10P 雷达；被控对象 JB-TG-HJ9000。

## 7. 给 AI 的协助指引

- 用户是 **ROS 新手**：**先概念后操作**，多配例子 / 图，结论先行。
- 用户环境**有代理**，联网类操作常遇证书错 → 先建议**关代理直连**。
- 用户偏好**简洁、结构化**（表格 / 清单），不接受过早的"完成了"结论。
- 给的命令要**可直接复制**，并说明"**应该看到什么**"（过关标准）。
- 遇到**报错**：先让用户**贴输出 / 截图**再判断（AI 不能直接连 WSL）。
- 当前**别跑 `apt upgrade`**；当前里程碑是**建图**。

---

*本文件供 AI 交接使用；用户操作清单见 `我要做的操作.md`。*
