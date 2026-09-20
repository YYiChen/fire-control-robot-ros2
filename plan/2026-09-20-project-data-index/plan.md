# 项目资料索引与多 Agent 交接边界

## Goal

把当前 ROS 文件夹中的论文、说明、脚本和实时核查结果整理成可供多 Agent 共用的资料索引，明确证据等级与待补资料。

## Dirty-State Note

开始本目标时本目录不是 Git 仓库。工作期间，另一项工作创建了 `main` 上的初始提交 `1a7b505`，其中包含本目标当时已经写入的索引与计划。`AGENTS.md`、`plan/README.md` 与 `plan/target-plan.template.md` 也在本轮阅读期间出现，视为其他工作者拥有；本目标不编辑这些文件。当前其他文件的 dirty 状态不影响本目标 owned 文件。

## Owned Files

- `plan/2026-09-20-project-data-index/plan.md`
- `plan/2026-09-20-project-data-index/log.md`
- `docs/project-data-index.md`

## Read-Only Files

- 论文原件、`论文_md版/`、现有路线与环境记录、`scripts/`
- `AGENT.md`、`AGENTS.md`、`我要做的操作.md`
- `plan/README.md`、`plan/target-plan.template.md`、`plan/log.md`

## Shared Dependencies

- 用户目标：先学习 ROS 基础，再沿论文框架逐步搭建近似系统。
- 参考工作流：`https://github.com/chenzc24/agent-workflow-kernel`。
- 现有 `AGENTS.md` 对目标计划、文件归属和记录的要求。

## Expected Work

1. 建立资料来源、当前状态与证据等级索引。
2. 标明各模块的并行边界、共享接口及交接材料。
3. 保留未验证事项，不把资料记录直接写成已完成的运行结果。

## Validation

- 核对索引中列出的每个本地文件路径是否存在。
- 核对“已核查”结论对应本轮只读命令输出。
- 检查本目标只修改 owned 文件；Git 启用后运行 `git diff --check`、`git status --short --branch` 并复审目标文件差异。
- 在本目标目录记录事实结果；共享的 `plan/log.md` 已被另一目标声明为 owned，本轮不并发编辑。

这些检查覆盖本次纯文档整理的直接风险：路径错误、状态误标和越权改动。

## Experience Signal (for human review)

- 旧环境文档与当前工作区目录不一致；是否形成可复用经验由用户决定。

## Commit Intent

本目标不初始化仓库；Git 已由另一项工作启用。将本目标剩余改动单独提交为 `docs: align ROS project data index with current Git state`，不包含其他目标的 dirty 文件。
