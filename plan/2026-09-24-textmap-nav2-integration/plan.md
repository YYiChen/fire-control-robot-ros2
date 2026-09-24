# TextMap 文字地标到 Nav2 动作的独立复验

## Goal

在只用仿真的前提下，查清上一轮 TextMap/Nav2 启动时 lifecycle manager 等待服务、`text_nav_bridge` 找不到 action server 的问题；验证 Nav2 action、运动、TF 时间新鲜度与定位误差，并审计文字地标到可安全接近目标的映射。只有地图能证明地标附近存在足够机器人足迹净空时才发送地标导航目标；否则保留安全阻断证据，不以直接抵达地标代替安全接近。至少做 3 次独立进程重启，保留逐次证据；不得把已有自定义 A* 跟踪器结果说成 Nav2 结果。若底盘发生明显倾斜或 TF 陈旧，立即停止本次仿真，不再向该实例发目标。

## Dirty-State Note

开始时 `main` 与 `origin/main` 在 `ab8305c` 同步。保留未跟踪的 `gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world`、`log/` 和 `reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak`，不检查入提交。WSL 的 `~/textmap_evaluation` 有已固定版本的上游 checkout、已保存的 TextMap 地图/地标以及隔离构建；本次只在专属 ROS 域和该隔离目录中诊断，不改 `/opt/ros`、`~/ros2_ws` 或上游 checkout。

## Owned Files

- `plan/2026-09-24-textmap-nav2-integration/plan.md`
- `docs/open-source-textmap-evaluation.md`
- `stereo_sim/README.md`（仅在有新的可复验 Nav2 结果时更新）
- `plan/log.md`
- WSL 临时复验器仅放在 `~/textmap_evaluation/harness/`；运行数据仅放在 `~/textmap_evaluation/output/nav2_retest/`

## Read-Only Files

- 固定提交的 `~/textmap_evaluation/src/{NavOCR,TextMap,text_nav_bridge,text_nav_sim,TextMap_Examples}`
- `~/textmap_evaluation/output/textmap_sim/{map.yaml,map.pgm,landmarks.yaml}`
- `/opt/ros/humble`、系统 Nav2/Gazebo 安装、原项目自定义 A* 跟踪器
- 用户的 Gazebo 场景、备份文件与现有未跟踪 `log/`

## Shared Dependencies

- Nav2 地图、AMCL、`map→odom→base_footprint` TF、有效 `/scan` 和 `/clock`
- `NavigateToPose` action，以及 `text_nav_bridge` 订阅的文本命令、地图与地标文件
- 固定的 `Exit` 地标和 `map.yaml`；如地图范围、机器人初始位置或传感器输入不满足 Nav2 前置条件，先记录阻塞证据，不人为伪造成功状态
- `text_nav_bridge` 的实际实现沿机器人至文字地标直线逐格取最后一个静态 `/map` 空闲格；此逻辑不检查机器人足迹、未知区，也不保证直线路段可通行。当前 Exit 点在地图里为空闲时，仍须核对真实墙面/障碍和安全停靠距离，才可执行。
- 当前 WSL 的 ROS 2 Humble/Nav2 版本与上游 `nav2_sim_params.yaml` 的 BT Navigator 插件配置兼容性；Gazebo RGB-D 模型对 WSL 计算负载的影响
- AMCL 在 harness 参数中使用 `update_min_d=0.25 m`、`update_min_a=0.2 rad`、`transform_tolerance=1.0 s`；控制器从 `map` 取 TF 时必须核验变换时间与传感器/里程计数据时间，不能只看 `/scan` 频率或生命周期状态。

## Expected Work

1. 读取上轮 Nav2 原始 launch 日志、启动参数、服务发现和保存地图几何；确定原失败是在节点启动、lifecycle、时间/TF、地图、传感器还是 ROS 图查询层。
2. 先在隔离域启动 Gazebo 传感器源和 Nav2。用轻量底盘保持同一场景/速度/安全墙前目标，对照官方 5 Hz 与 harness-only 30 Hz 雷达的 `/scan`、带时间戳的 `map→odom` 与 `odom→base_footprint` TF、`/odom` 和 `/clock`；同时记录 TF 超龄报错、AMCL 更新门限、生命周期 bond 心跳及 WSL 资源。不可将时间戳陈旧的 `/amcl_pose` 与当前里程计直接相减。若运动试验触发安全停止，先做不发送任何目标的 30 Hz 静止时间戳审计，确认激光/里程计持续更新时 `map→odom` 与 `/amcl_pose` 的时间新鲜度。确认 Humble BT Navigator 使用本机版本支持的参数/插件格式。单独的 localization bringup 必须显式提供 map server 的 `yaml_filename`，不将缺少该参数的失败误判为 Nav2 本身不可用。
3. 用实际 ROS action client 做无位移 NavigateToPose 响应和已知空闲格的规划验收；同时核对地图中的 Exit 像素、周围机器人足迹净空及 world 中对应墙面。可另用传感器全局 costmap 选定一个经验证有足迹余量的墙前 stand-off 目标，直接测试 Nav2 的安全接近路线，但不要把这个结果记成 bridge 端到端成功。只有 action 有确定响应、地图/TF/scan 持续新鲜且地标目标具有安全停靠空间后才接入 bridge。出现 TF 明显过期或机器人倾斜时先停仿真，不尝试恢复驾驶。
4. 先离线复现 bridge 的 ray-march 目标选择并计算目标净空；若桥接器把目标放到墙面/障碍附近，或地图没表达真实结构，则不发布 `Exit` 命令，记录风险与待补模块。满足条件时才执行真实 bridge 目标，并记录 action 状态、轨迹/速度、真值位姿、VO 位姿、到目标误差和停车后漂移。
5. 完成至少 3 次相互独立的仿真进程重启；失败场景保留，禁止只挑成功结果。若动作层仍无响应或心跳中断，复验最小 Nav2 例程并缩小故障边界。
6. 更新开源评估报告、README 与日志；不修改或冒充上游源码修复，必要的参数覆盖放在隔离 harness。

## Interim Evidence (2026-09-24)

- Nav2 启动可用性：将 `autostart:=false` 并在 Gazebo、map server、AMCL 与 Nav2 服务出现后分别调用两个 lifecycle manager，多个独立启动均返回 `success=True`；控制器 active，`/navigate_to_pose` action server 可见。原自动启动等待服务问题有可复验的绕行办法；启动 race 根因仍未被源码级证明。
- 短程已知目标：5 Hz 4 次冷启动的 `(-6.5,-1.8)` 目标均返回 `SUCCEEDED`。30 Hz 一次相同短程 action 在 Nav2 日志中明确 `Reached the goal!/Goal succeeded`；当时 action CLI 被 8 秒外层 timeout 截断，故将服务器结果与客户端退出分开记录。
- 同一安全观察位 `(-5.5,-3.0)`：5 Hz 一次 `SUCCEEDED`，时间戳同为 317.442 s 时 `/odom=(-5.4893,-3.2451)`、`map→base_footprint=(-5.5831,-2.9508)`；差约 0.309 m，里程计坐标距目标约 0.245 m，虽离南墙约 0.605 m但未达到设定目标精度。
- 30 Hz 安全观察位第一次 action `ABORTED`；`/scan` 稳态约 29.39 Hz，Nav2 报告“819 条轨迹均无效”及旋转恢复“前方碰撞”。该试次终点的 odom 与 map TF 时间不一致，不据此计算精确定位差。
- 30 Hz 安全观察位第二次：连续采集 1444 个近同步 TF/里程计样本，中位时间差 0.034 s、最大 0.102 s。起点 `map→odom` 的 x/y/yaw 近零；运动中后段该变换变化至约 `(-1.393,-3.991,-0.683)`，组成的 map 位姿与里程计对应位姿差异显著。Nav2 控制器日志另报告数据时间约 194.1 s、`map→odom` 变换约 59.494 s 的“too old”；action 等待 60 s 无结果。停止前观测到 Burger 有明显 roll/pitch 与速度，随后立即停止本试次 Nav2/Gazebo。
- 30 Hz SDF 是 stock Burger 模型副本，只把雷达更新率改为 30 Hz；没有限制 roll/pitch。倾斜观测发生于未约束的 3D Gazebo 试次，后续运动试验需先单独验证平面稳定约束与碰撞设置。
- 不发送任何运动目标的 30 Hz 静止审计运行了 20.02 s：`/scan` 29.27 Hz、`/odom` 29.32 Hz、`map→odom` 约 30.07 Hz；三者时间戳最大间隔均为 0.034 s。最新 `map→odom` 时间比最新 scan 超前 1.0 s，符合 AMCL `transform_tolerance=1.0`。静止观察窗口内 `/amcl_pose` 为 0 条周期消息，但 TF 持续广播；因此不能用静止时旧/无 `/amcl_pose` 判定定位失活。
- 因此目前只能说 30 Hz 把 `/scan` 提高到约 29.4 Hz；静止时传感器/TF 时间链健康，没有证据证明它改善长程导航。长程安全观察位在 30 Hz 下两次均未通过，第二次在明显倾斜期间被安全停止。运动期间的陈旧 TF 可能与碰撞/倾斜、激光几何或地图/世界定位差异有关，因果尚未分离。复验原始结构化记录位于 WSL `~/textmap_evaluation/output/nav2_retest/nav2_retest_20260924.json`。
- `Exit` bridge 命令仍未发送：上游 ray-march 选择点约距南墙 0.126 m，墙前机器人足迹净空不满足当前 `robot_radius=0.22 m` 与安全裕量。
- **Gate status: incomplete.** 生命周期/短程 action 门已验证；安全 stand-off 长程门未通过，自动 bridge 命令继续禁用。后续若要恢复运动试验，先开独立 plan 处理 Burger 平面稳定约束、map/world 标定与碰撞/TF 同步；从低速短段起逐级验证，不直接重复当前长路线。

## Validation

- 逐次记录并检查 `map_server`、`amcl`、`controller_server`、`planner_server`、两个 lifecycle manager 的状态与 bond 心跳，持续采样 `/clock`、`/scan`、`/odom` 和 TF；使用 action client 的接受/结果回包作为 `/navigate_to_pose` 可用证据。
- 成功试次必须有 Nav2 action accepted 且 result=SUCCEEDED；机器人在 Gazebo 中实际位移，最终位姿在预设接近误差内，速度归零且停车后漂移有界。
- 三次完整冷启动运行按同一条件验收，输出逐次数据及成功率，不以 RViz 画面或单次 action server 可见替代。
- 不满足任何运行前置条件时，不发目标；保存节点日志和诊断结果并准确标为未验证。
- 实验后核实本 target 的 ROS 域、Gazebo 进程全部退出；执行静态检查、`git diff --check`、精确暂存和最终状态检查。

## Commit Intent

若产生仓库文档改动，使用独立 target-only 提交并推送 `main`；不暂存用户已有未跟踪文件。
