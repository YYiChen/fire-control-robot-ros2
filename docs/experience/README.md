# 经验笔记（docs/experience/）

`docs/experience/` 存放**从 plan / log / commit / 评审 / 验证报告 / 失败尝试中提炼出的、可复用的项目经验**。

**它不是第二份维护日志。**

## 什么算"经验"

一条经验笔记应解释一个**可迁移的判断**：
- 测试了什么假设；
- 发生了什么；
- 什么证据支持该结论；
- 应改变哪条规则 / 模板 / 验证命令 / 习惯；
- 该经验适用于哪些场景、不适用于哪些场景。

## 什么不算（不该放这）

- 改动文件的裸清单；
- 抄来的 commit message；
- 每日状态更新；
- 没有解读的验证记录；
- 没有证据的宽泛原则。

以上属于 `plan/log.md`、git 历史或项目状态报告。

## 人与 Agent 的职责

**经验提取是人的决定，不是 Agent 的结项步骤。**

- **人**决定"某项工作是否产生了可复用经验、何时触发提取"；跨 target 的模式也只有人能看到。
- **Agent 只做辅助，绝不 gate-keep**：
  - 标记候选信号（重复失败、被推翻的规则、危险捷径、验证缺口）供人确认；
  - 被要求时，起草**引用证据**的候选笔记并填模板；
  - accept / edit / reject 由人决定。

原因与 kernel 的"反自评"原则一致："可复用 / 重复失败 / 跨项目适用"没有确定性边界；若让 Agent 自决，要么每个 target 硬凑一条浅经验（退化成第二份 log），要么漏掉只有人能看到的跨 target 模式。

## 建议流程

1. 先让 target plan 与 `plan/log.md` 记录事实；提取**不属于**结项步骤。
2. 人审查近期 log、失败 plan、重复修复，判断是否值得提取。
3. 人提出时，Agent 起草带证据的候选笔记。
4. 人 accept / edit / reject。
5. 引用证据：plan 路径、log 条目、commit、报告、评审。
6. 成熟的经验回灌到 `AGENTS.md`、模板或验证检查里。

## 文件命名

与具体项目事件相关 → 短名 + 日期：
```text
YYYY-MM-DD-short-lesson-title.md
```
长期通用实践 → 稳定名：
```text
plan-log-experience-loop.md
dirty-worktree-ownership.md
validation-before-confidence.md
```

以 `lesson.template.md` 为模板。

> **本项目当前状态**：尚无正式经验笔记。候选信号见 `plan/2026-09-20-slam-mapping/plan.md` 的 "Experience Signal" 字段——**待人类决定是否提取**。
