# AGENTS.md —— Agent 工作规则（本项目）

> 本项目采用 **plan → 实现 → 验证 → 记录 → 提交** 的 Agent 工作流；可复用的「经验」由**人**决定是否提取。
> 工作流内核：`chenzc24/agent-workflow-kernel`（本文件按其规范落地，并追加项目特定条款）。
> 项目背景 / 现状：`AGENT.md`　｜　用户操作清单：`我要做的操作.md`
> 最后更新：2026-09-20

---

## 一、项目特定信息

- **项目**：复现 / 学习《面向消防控制室主机状态信息识别与自主处置的机器人关键技术研究》论文的技术栈（**不强求 1:1 复刻**）。详见 `AGENT.md`。
- **技术栈**：Ubuntu 22.04（WSL2）+ **ROS2 Humble** + Gazebo + Nav2 / SLAM Toolbox / MoveIt2 / ArUco / PaddleOCR。
- **运行环境**：Windows 11 + WSL2（Ubuntu 22.04，用户 `yyyyyc001`）；工作区 `~/ros2_ws`。
- **Git 状态**：本目录**尚未纳入 git**（见第七节）。kernel 的 `git status / diff / commit` 环节在启用 git 前以「人工记录」代替。

## 二、本项目硬规则（不可违反）

1. **禁止 `sudo apt upgrade`** —— 会升级 ROS 包、破坏 Humble 基线版本。（`apt install` 安全）
2. **代理 TUN / 全局模式会挡 HTTPS**（TLS 中间人 → 证书错）→ **联网安装前先关代理直连**。
3. 代码放 **WSL 内 `~/`**，**不要放 `/mnt/c`**（跨文件系统编译极慢）。
4. VS Code 开发类扩展装 **WSL 侧**；工作区用 **Remote-WSL** 打开（**勿用** `\\wsl.localhost\` UNC）。
5. **AI 无法直接执行 WSL 命令**：AI 沙箱拦截 `wsl.exe` → 必须**让用户回贴命令输出 / 截图**再判断。
6. 运行 `turtlebot3*` 相关命令前，确保该终端有 `TURTLEBOT3_MODEL=burger`（否则世界加载但机器人不 spawn）。

## 三、本项目验证命令（validation 用）

| 目的 | 命令 |
|---|---|
| 环境自检 | `bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/diag_gazebo.sh` |
| ROS 环境 | `echo $ROS_DISTRO`（= humble）、`ros2 doctor --report` |
| 仿真就绪 | `ros2 topic list \| grep -E 'scan\|odom'` |
| 编译工作区 | `cd ~/ros2_ws && colcon build --symlink-install` |
| 地图产出 | `ls -l ~/my_map.yaml ~/my_map.pgm` |

## 四、Agent 工作纪律（kernel 通用）

### 4.1 开始任一 target 前
1. 明确 **target owner / goal / 预期改动文件 / 验证面**；
2. 读项目入口文档（`AGENT.md`、本文件、`我要做的操作.md`）；
3. 若项目已启用 git：跑 `git status --short --branch` 做 **dirty-state 审计**（dirty 且与本 target 无关可继续；若与 owned 文件重叠或归属不明 → 停止并协调）。

### 4.2 编辑前
**先建 target plan**，默认路径 `plan/<date-goal-slug>/plan.md`（模板：`plan/target-plan.template.md`）。plan 必须包含：
- goal
- dirty-state note
- owned files / read-only files
- shared dependencies
- expected work（步骤）
- validation（与风险匹配的最小确定性检查）
- commit intent

**不要编辑 owned 集合之外的文件**（除非先更新 plan）。

### 4.3 工作中（保持 target 有界）
- 优先小而可审的改动；不把无关清理混进来。
- 共享契约 / 生成物 / 他人工作视为受保护。
- 出现新风险或依赖 → **先更新 plan 再改范围**。
- 验证力度与「改动行为 / 影响面 / 风险」匹配；**不默认加测试、不默认跑全量**。
- 未决问题记进 plan / review note，别藏在对话里。

### 4.4 完成后
1. 按 plan 执行验证（全量仅在有充分理由或项目策略要求时）；
2. 至少 `git diff --check` + `git status --short --branch`（启用 git 后）；
3. **更新 `plan/log.md`**：记录 target / changed areas / validation / commit status；
4. 复审 diff → 只 stage 目标文件 → 提交。

### 4.5 经验层（**人工触发，非结项步骤**）
- **Agent 不得自行判定"存在可复用经验"**。
- 人审查 plan / log / 失败后决定是否提取；被要求时，**Agent 起草候选 note**（引用证据）交人 accept / edit / reject。
- Agent 可**标记候选信号**（重复失败、现实推翻的规则、危险捷径、验证缺口、可复用协作模式、被证实/推翻的假设），但不做 gate-keep。

## 五、边界规则

- 不在一个 plan / log / commit 里混多个无关 target。
- 不把「可复用工作流改动」与「项目产物改动」混提（除非 plan 说明必须同提）。
- 有确定性检查或人工评审路径时，**不以模型评审作为唯一质量门**。
- 不为了让仓库"看起来干净"而删除未解决的 plan / 未决评审 note。

## 六、Plan 卫生

- 已完成的详细 plan 可汇总进 `plan/log.md` 后归档；
- 失败 / 阻塞 / 未解决的 plan **保持可见**，直到完成、被取代或明确放弃；
- 归档前若可能含可复用经验，**先标记给人审**，未决定前不归档。

## 七、Git 说明

kernel 假设项目在 git 下工作。**本目录当前不是 git 仓库**。建议：
```bash
cd "C:\Users\32126\Desktop\Leeds Homework\Semester 5\科研\ROS"
git init
```
启用后，`plan/log.md` 的 `commit status` 字段才可填真实提交；在此之前记 `N/A (no git)`。

---

*本文件融合 `chenzc24/agent-workflow-kernel` 的通用规则与本项目特定条款。*
