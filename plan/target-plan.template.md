# <Target 标题>

## Goal

写明具体目标。

## Dirty-State Note

起始状态（启用 git 后由 `git status --short --branch` 得到）：

```text
<paste status>
```

说明工作区是否干净；若不干净，说明无关 dirty 文件为何可安全保留。(无 git 时记 `N/A (no git)`)

## Owned Files

- `<本 target 可编辑的路径>`

## Read-Only Files

- `<本 target 只读、不可编辑的路径>`

## Shared Dependencies

- `<本工作依赖的共享契约 / 生成物 / API / 文档 / 决策>`

## Expected Work

1. `<步骤>`
2. `<步骤>`
3. `<步骤>`

## Validation

- `git diff --check`（启用 git 后）
- `git status --short --branch`（启用 git 后）
- `<覆盖本次改动行为与直接依赖的最小确定性检查>`

说明这些检查为何匹配本 target 的影响面与风险。若要加测试代码或跑全量，必须说明其依据（行为 / 回归风险 / 共享契约 / 项目策略）；否则默认不加、不跑。

## Experience Signal (for human review)

> 本字段**不是 Agent 自检**，而是供人日后判断是否要从中提取经验时阅读的**候选信号**。
> 记录本次工作中出现的候选信号：重复失败、被现实推翻的规则、危险捷径、验证缺口等。日常改动留空。
> Agent 可在此标记疑似信号，但**是否提取经验由人决定**。

## Commit Intent

提交信息：

```text
<commit message>
```
