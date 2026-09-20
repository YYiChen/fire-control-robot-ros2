# plan/ 目录说明

`plan/` 存放**实现意图**与**事实维护历史**，让 Agent 不做无边界、无归属、不可验证的改动。
（本目录按 `chenzc24/agent-workflow-kernel` 规范落地。）

## Target Plans（target 计划）

**编辑受跟踪文件前**先建 target plan。默认路径：

```text
plan/<date-goal-slug>/plan.md
```

示例：

```text
plan/2026-09-20-slam-mapping/plan.md
```

每份 target plan 应包含：
- goal（目标）
- dirty-state note（起始状态说明）
- owned files / directories（可改）
- read-only files / directories（只读）
- expected work（步骤）
- validation（与改动行为、影响面、风险匹配的验证）
- commit intent（提交意图）

以 `plan/target-plan.template.md` 为模板。

## Dirty Worktree 处理

dirty 工作区**不必然**阻塞无关工作。满足以下条件可继续：
- dirty 文件与当前 target 无关；
- 不与 target 的 owned 文件重叠；
- 不改变 target 依赖的共享契约；
- target plan 记录了该决定。

以下情况**停止并协调**：
- dirty 文件与 target owned 文件重叠；
- dirty 文件归属不明；
- dirty 共享契约可能影响本 target；
- 需要编辑/暂存他人的 dirty 文件。

## 维护日志（plan/log.md）

`plan/log.md` 记录项目级、被接受的维护历史。每条含：**date / target / changed areas / validation performed / commit status**。日志要**事实化**，不强行声称每个任务都产生了"经验"。

## 经验提取（体验层）

**经验提取是人的决定，不是 Agent 的结项步骤。** 人审查 plan / log / 失败后，决定是否值得提取进 `docs/experience/`。人可关注的候选信号：
- 同一失败发生多次；
- 现实推翻了原计划 → 规则改变；
- 某验证命令抓到了模型评审漏掉的问题；
- 某协作规则防止（或没能防止）冲突编辑；
- 某捷径省了时间且仍安全。

当人提出时，Agent 可**起草候选 note** 并引用相关 plan / log / commit / 报告；由人 accept / edit / reject。

## 与 Git 的关系

plan 与 log **不替代 git**：
- plan = 工作前的意图；log = 工作后的事实；git = 实际的仓库状态。

一个完整 target 通常以「验证 → 更新 log → stage 目标文件 → 提交 → push」结束。

> **本项目当前未启用 git**（见 `AGENTS.md` 第七节），故 commit 环节暂以记录代替。

## 本项目已有 target

| 目录 | 目标 |
|---|---|
| `plan/2026-09-20-slam-mapping/` | 完成 SLAM 建图（当前进行中） |
