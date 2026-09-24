# TextMap 开源文字地标导航基线复核

## Goal

在独立 WSL 工作区审阅并尝试复现 `kc-ml2` 的 TextMap / NavOCR / `text_nav_bridge` / `text_nav_sim` 仿真流程，验证“文字检测 → 深度投影 → SLAM 地标 → Nav2 目标”的公开实现能否工作，并据实判断可复用模块与当前双目消防面板链路之间的接口差距。只运行仿真，不连接实机，不修改现有 ROS 工作区或系统 ROS 安装。

## Dirty-State Note

开始时仓库 `main` 与 `origin/main` 同步在 `f3d74e6`。三个用户已有未跟踪文件 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world`、`reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak` 保持不变；状态检查还发现未跟踪的 `log/`，也不属于本 target，保持不变。本 target 只拥有本计划、`docs/open-source-textmap-evaluation.md`、`stereo_sim/README.md`、`plan/log.md`；上游代码、权重与运行产物放在 WSL 用户目录，不纳入 Git。

## Owned Files

- `plan/2026-09-24-textmap-open-source-baseline/plan.md`
- `docs/open-source-textmap-evaluation.md`
- `stereo_sim/README.md`
- `plan/log.md`

## Read-Only Files

- `AGENT.md`、`AGENTS.md`、当前项目已有双目语义节点 / launch / 验证器
- `reference/industrial_pc_2026-09-22/`
- WSL 用户目录中的只读上游 checkout：`~/textmap_evaluation/src/{NavOCR,TextMap,text_nav_bridge,text_nav_sim,TextMap_Examples}`
- 系统 ROS/Humble、Gazebo、Nav2、SLAM Toolbox 与 WSL 图形服务

## Shared Dependencies

- 上游 README 描述 4 包管线：NavOCR、TextMap、文字目标 Nav2 bridge 和 TurtleBot3 Gazebo 场景。
- 上游接口依赖 `vision_msgs/Detection2DArray`、相机深度图/CameraInfo、`/odom`、RTAB-Map `/mapGraph`；当前项目以双目 disparity/PointCloud2、同步 VO/TF 和 RTAB-Map 图地图输出。WSL 未安装 `vision_msgs`；为避免更改系统 ROS，计划在评估 workspace 内构建 Humble 官方 `vision_msgs` 源码（固定 `humble` 分支 SHA `5f35e557cfb5e085ac5c6dc0210c5736c2c362f7`），只给隔离 workspace 使用。
- README 明确上游 RTAB-Map 真实相机例子只做文字地图，尚未宣称 2D 导航稳定；此限制须保留在结论中。
- WSL 无 pip；若 NavOCR 默认 ONNX 后端缺 Python 运行包，只允许在 `~/textmap_evaluation/venv` 安装，禁止改 ROS 系统 Python/apt 包。

## Expected Work

1. 在独立 WSL 目录克隆固定 commit 的五个公开仓库；审阅 package.xml、模型配置、启动文件、脚本副作用和许可证，不运行其广域清理命令。
2. 检查现有 Humble/Gazebo 依赖与 Python 运行环境；如有缺失，优先判断能否用用户目录隔离环境补齐，不进行系统升级或系统 ROS 安装。构建上游依赖时也只写入 `~/textmap_evaluation`。
3. 编译和运行上游 Gazebo + OCR + TextMap + Nav2 文字目标流程；用 headless 模式与专属 ROS_DOMAIN_ID。若 Nav2 未激活，记录 lifecycle/action 的实际状态和失败边界，不在 action server 未就绪时发送导航目标。
4. 对比上游 RGB-D/深度图接口与本项目双目点云接口，给出可复用模块、必须改造的消息/坐标/同步契约及消防面板专属感知缺口；不把上游低星项目称为成熟工业方案。
5. 更新本 target 文档、README 与日志，执行 Python/launch/Shell 检查和 `git diff --check`，只提交 owned 文件并推送。

## Validation

- 上游仓库固定 commit SHA、许可证与文件树可复查。
- `colcon build` 在隔离 workspace 通过，或记录阻塞的精确缺失项及失败日志。
- 运行期检查真实 ROS 节点/话题/Action，不以 README 宣称或 GUI 截图代替；记录生成的 map、landmarks YAML 及内容摘要。
- 只在自己的 ROS_DOMAIN_ID 和进程组启动/清理 Gazebo；结束后确认该域节点与本次 Gazebo 进程无残留。
- `git diff --check` 已通过；提交前将再次核对 staged diff 并确认只含 owned files。

## Execution Outcome (2026-09-24)

- 固定提交：NavOCR `02e360140fd7aa82eee9b58b80357fc3cb920766`、TextMap `ccf62fdc3f3c2e1e62b1464d8e50cd9c3daf0199`、text_nav_bridge `29d4667e4fa3cf00dcc607bf617e4685f23eb756`、text_nav_sim `ebf63cbf346c6501f6525cd63bb0ce13fb2c02c7`、TextMap_Examples `be0981cea587f524c7479605e5d285862438db26`。Humble `vision_msgs` 源码固定 SHA `5f35e557cfb5e085ac5c6dc0210c5736c2c362f7`，只编入本地隔离环境。
- 五个 ROS 包 `vision_msgs`、`text_nav_bridge`、`text_nav_sim`、`navocr`、`textmap` 均构建通过。NavOCR OpenVINO standalone 在 11 张面板真值图上只命中同一个中文标签 11/32；额外 18 张也只认出“主电工作”。
- 独立 ROS 域 109 的 headless RGB-D Gazebo 流程发布了相机、`/odom`、`/scan`、TF 和 `/clock`。TextMap 输出 `Exit`，静止单视角累计 5759 次重复观察；地图保存为 45×97、0.05 m/cell 的 PGM/YAML，文件留在 WSL `~/textmap_evaluation/output/textmap_sim/`。
- Nav2 启动未能越过 lifecycle 门：bridge 未发现 NavigateToPose action server，lifecycle manager 持续等待 `amcl/get_state` 与 `controller_server/get_state`；ROS graph CLI 结果相互矛盾。未发送 goal，根因待隔离复测。
- 源码检查确认 TextMap 最近邻深度匹配没有最大时间差拒绝条件；`text_nav_bridge` 沿机器人至地标的直线搜索自由栅格，找不到时回退到机器人当前位置。TextMap 的 OpenVINO launch 引用了缺失配置 `TextMap/config/textmap_sim.yaml`，本次使用已有通用 launch 和显式参数成功跑图。
- 本 target 的完整结果、对双目消防面板的接口差异和坐标高度待核实项记入 `docs/open-source-textmap-evaluation.md`。未改系统 ROS、现有 `~/ros2_ws`、上游源码或真实硬件；测试进程组已停止。
- **Target status**：源码/模型/静态建图评估完成；Nav2 lifecycle/action 集成未验证，保留为后续可见的未决项。

## Experience Signal (for human review)

> 候选信号：公开 TextMap 示例把 RTAB-Map 实机支持范围标为“文字地图”，并说明其输出尚不稳定支持 2D 导航；需区分可复用文本投影架构和可直接依赖的完整导航产品。是否沉淀为经验由人审决定。

## Commit Intent

提交信息：

```text
docs: verify open-source text landmark mapping baseline
```
