# 维护日志（plan/log.md）

> 项目级、被接受的维护历史。事实记录，不替代 git。
> 字段：date / target / changed areas / validation performed / commit status。
> 本项目当前**未启用 git** → commit status 记 `N/A (no git)`。

---

## 2026-09-18 ~ 09-19 —— 搭建 ROS2 开发环境

- **target**：在 Windows 上搭建 WSL2 + Ubuntu 22.04 + ROS2 Humble 环境（直连安装）。
- **changed areas**：
  - 安装 WSL2 + Ubuntu-22.04 发行版；OOBE 创建用户 `yyyyyc001`。
  - 安装 ROS2 Humble（`/opt/ros/humble`）、colcon、rosdep。
  - 配置 VS Code Remote-WSL、环境自动 source、`.vscode` 配置。
  - 生成脚本：`install_wsl2_direct.ps1`、`install_ros2_humble.sh`、`check_ros2_env.sh`、`setup_ros2_dev_env.sh`、`Open-ROS2-VSCode.bat` 等。
  - 产出文档：`ROS2开发环境_完整记录与现状.md`（+ `scripts/` 归档）。
- **validation performed**：`check_ros2_env.sh` 体检 24/24 通过；`ros2 doctor` 正常；`echo $DISPLAY`=`:0`（WSLg）；`xeyes` 出窗；talker/listener。。。以 `ros2` CLI 与 `ros2 run` 验证。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— 论文获取与整理

- **target**：获取并整理学长论文，作为复现/学习依据。
- **changed areas**：
  - 通读论文 docx（139,843 字符，6 章）。
  - 生成 `论文_md版/`（`00_总览与索引.md` + `01`~`06` 分章 md）。
  - 生成 `论文解读与复现路线.md`（技术栈 + 复现路线）。
- **validation performed**：md 文件生成并抽查内容；确认公式(OMML)/图片未提取（已知局限）。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— 学习路线、依赖安装、文档体系

- **target**：制定"学会整套技术栈"的路线并安装依赖；建立项目文档体系。
- **changed areas**：
  - 生成 `ROS2技术栈_学习路线.md`（A~H 分阶段）、`install_learning_stack.sh`（依赖安装）。
  - 安装 Gazebo、turtlebot3、nav2、slam-toolbox、moveit 等（部分包可能未装全）。
  - 生成 `AGENT.md`（项目上下文）、`我要做的操作.md`（操作清单）、本 kernel 结构（`AGENTS.md` / `plan/` / `docs/experience/`）。
- **validation performed**：`diag_gazebo.sh` 自检；`ros2 topic list`；Gazebo 进程检查。
- **commit status**：`N/A (no git)`

## 2026-09-20 —— M1 完成（小车正常显示）+ 启用 git

- **target**：完成 SLAM 建图（`plan/2026-09-20-slam-mapping/`）。
- **changed areas**：
  - 修复 `TURTLEBOT3_MODEL`（写入 `~/.bashrc`）→ TurtleBot3 在 Gazebo **正常显示**。
  - **启用 git**：`git init` + 首次提交（纳入文档 / 论文 / 计划 / 经验模板）。
- **validation performed**：
  - `diag_gazebo.sh` 初检 → `TURTLEBOT3_MODEL=''`；无 `/scan` `/odom`；仅 `gzserver`、无 spawn 进程。
  - 修复后：**用户确认小车正常显示**（M1 ✅）。
  - **M2（建图）命令尚未运行**。
- **commit status**：首次提交（`chore: init repo ...`）
- **待办**：跑建图 4 条命令（`turtlebot3_world` → `cartographer` → `teleop` → `map_saver`），产出并验证 `~/my_map.yaml` + `~/my_map.pgm`。

## 2026-09-20 —— 多 Agent 协同发现 + git 启用（复核）

- **target**：启用 git；复核已有的多 Agent 产物。
- **changed areas**：
  - **启用 git**：`git init`（`main`），首次提交 `1a7b505`（32 文件，含文档 / 论文 / plan / experience）。
  - 发现本目录**已有另一 Agent 的产物**：`docs/project-data-index.md`、`plan/2026-09-20-project-data-index/`（**非本 Agent 创建**；随之入库，但其 dirty 变化归该 Agent，本 Agent 不编辑）。
- **validation / 核查差异**（来源：`docs/project-data-index.md`）：
  - `~/ros2_ws` **只有 `.vscode`、无 `src`** → 旧记录"已创建第一个包"**不成立**（`create_first_ros2_pkg.sh` 未真正跑过）。
  - **另一 Agent 可直连运行 WSL 命令**（`wsl --list --verbose`、`ros2 pkg list`、`ls -la ~/ros2_ws`），而**本 Agent 沙箱拦截 `wsl.exe`** → 能力因会话而异（已在 `AGENT.md` / `AGENTS.md` 加限定）。
- **commit status**：`1a7b505`（首次）；另有 `plan/2026-09-20-project-data-index/` 的 dirty 归另一 Agent。
- **备注**：另一 Agent 索引称"本目录不是 Git 仓库"——**已被本次 `git init` 取代（过时）**。
- **多 Agent 说明（用户澄清）**：本项目可能**多 Agent 参与**，**能力因会话而异**（**本 Agent 被拦 `wsl.exe`**，其他 Agent 可能可直连 WSL）；**用户不预设分区、按需使用** → **不擅自划分职责**。已按中性描述更新 `AGENT.md` / `AGENTS.md`。（先前一版曾误写为"执行 / 文档分工"，**已更正**。）

## 2026-09-20 —— 低速键盘遥控包

- **target**：在 TurtleBot3 仿真建图阶段提供易停车的低速遥控。
- **changed areas**：在 WSL `~/ros2_ws/src/slow_teleop/` 新建独立 ROS2 Python 包；WASD 控制，默认直线 0.05 m/s、原地转向 0.35 rad/s，最后一次按键 0.35 秒后自动发布零速度；空格急停、Q 退出。包内 `README.md` 记录用法。
- **validation performed**：`python3 -m py_compile`、`colcon build --packages-select slow_teleop --symlink-install` 成功；`ros2 pkg executables slow_teleop` 返回 `slow_teleop slow_teleop`；交互终端启动并以 Q 退出，未发送移动按键；`/cmd_vel` 运行前无其他发布者。实际驾驶效果待用户试用。
- **commit status**：WSL 包独立 Git 提交 `53f96be`；本项目计划与日志单独提交。

## 2026-09-20 —— 建图保存、遥控速度与刷新调优

- **target**：保存当前扫图，并提高后续遥控与地图刷新体验。
- **changed areas**：保存 `~/maps/turtlebot3_world_2026-09-20_01` 的 YAML/PGM 导航地图，以及 `.posegraph`/`.data` 位姿图；WSL `slow_teleop` 默认直线速度 0.05→0.15 m/s、保留 0.35 秒自动停车，并加入参数服务处理；新增 `config/slam_toolbox_fast.yaml`，下次启动 SLAM 可使用 1.0 秒地图更新、0.1 米位移阈值、0.2 弧度转向阈值和 0.2 秒最小扫描间隔。
- **validation performed**：当前 `/map` 由 `slam_toolbox` 发布；`map_saver_cli` 报告 112×103、0.05 m/像素并成功写文件；PGM 文件检查一致；位姿图服务结果 `0` 且两文件存在；当前 `/scan` 约 5 Hz、Gazebo 截图实时倍率约 1.0，原参数实际为 5 秒/0.5 米/0.5 弧度/0.5 秒；新 YAML 解析及参数值检查成功；遥控包重新编译成功，隔离 ROS 域内节点显示 0.15 m/s 且参数查询成功。实际提速驾驶与新 SLAM 配置的实时效果待用户试用。
- **commit status**：WSL 包独立 Git 提交 `e041ad0`；本项目计划与日志单独提交。

## 2026-09-20 —— 修复消防控制室近似场景

- **target**：修复另一 Agent 创建的未跟踪 Gazebo 场景，确保 Burger 可在各分区通行，并提供与当前建图流程一致的启动说明。
- **changed areas**：在 `gazebo_scene/fire_control_room.world` 增加前侧 1 米入口、移开堵门的 `equip_a` 与内墙重叠的 `equip_b`；在 launch 中使用同目录 world、默认 Burger、120 秒生成等待及可选 `gui:=false`；README 改为 SLAM Toolbox + RViz + `slow_teleop` + 独立地图文件名。world 和 launch 已复制到 WSL `~/my_worlds/`。
- **validation performed**：`gz sdf -k` 通过；ROS2 launch 参数解析通过；按 0.12 米车体余量的平面栅格检查显示从 `(0.5,0.5)` 可达左上、右上、右下与前侧入口，设备与墙无重叠；隔离 `ROS_DOMAIN_ID=98` 和 Gazebo master 端口 11356 下完整运行 launch，`SpawnEntity` 成功、模型列表含所有场景物体与 `burger`、`/scan` 与 `/odom` 各有 1 个发布者、里程计 `(0.50007,0.499995)` 且姿态近直立。隔离测试已停止，原有 Gazebo 进程仍运行。真人驾驶/实际建图尚待用户试用。
- **commit status**：本 target 提交见 Git 历史。

## 2026-09-21 —— 遥控速度再调（线 ×2.5 / 角 ×1.5）

- **target**：按用户要求，在**当前默认速度**基础上提高遥控速度——**移动（线）速度 ×2.5、转向（角）速度 ×1.5**。
- **原有基础（依据既有记录 / 包 README）**：slow_teleop 当前默认 = 线速度 **0.15 m/s**、角速度 **0.35 rad/s**。
- **计算结果**：
  - 线速度 `0.15 × 2.5 = ` **0.375 m/s**
  - 角速度 `0.35 × 1.5 = ` **0.525 rad/s**
- **用法（启动指令）**：
  `ros2 run slow_teleop slow_teleop --ros-args -p linear_speed:=0.375 -p angular_speed:=0.525`
- **changed areas**：本项目 `plan/log.md`；WSL `slow_teleop` 默认参数与 README 同步更新为线速度 0.375 m/s、角速度 0.525 rad/s。
- **validation performed**：数值由既有默认值（0.15 / 0.35）按倍率计算得出；`python3 -m py_compile` 和 `colcon build --packages-select slow_teleop --symlink-install` 成功；隔离 ROS 域内启动节点显示 0.38 m/s、0.53 rad/s，参数服务实际返回 0.375、0.525、0.35。实际驾驶手感待用户试用。
- **commit status**：WSL 包与本项目记录均已提交。

## 2026-09-21 —— 遥控速度再翻倍与激光量程修正

- **target**：按用户要求将当前 slow_teleop 默认线速度、角速度各再翻倍，并排查自建 lab_room 扫图时的报错。
- **changed areas**：WSL slow_teleop 默认线速度 0.375 → 0.75 m/s、默认角速度 0.525 → 1.05 rad/s；同步放宽参数合法范围、README；slam_toolbox_fast.yaml 的激光量程从不符合 Burger 雷达的 0.0–20.0 m 改为实际 /scan 发布范围 0.12–3.5 m。
- **diagnosis evidence**：SLAM 日志无运行时异常，只有旧量程配置触发的两条警告；RViz 日志出现 WSL OpenGL/GLSL sampler 纹理兼容性错误，但地图仍连续接收尺寸更新。碰撞后地图畸变与轮式里程计的位姿突变相符；该判断基于运行日志和当时的 /odom，未在受控碰撞试验中复现。
- **validation performed**：python3 -m py_compile 通过；colcon build --packages-select slow_teleop --symlink-install 成功；隔离 ROS_DOMAIN_ID=98 启动节点并查询到 linear_speed=0.75、angular_speed=1.05、hold_seconds=0.35；新量程配置文本断言通过；两个 Git 仓库均执行 git diff --check。
- **commit status**：WSL 遥控包提交 `2938956`；本项目计划和记录随本次根仓库提交保存。

## 2026-09-21 —— Burger 仿真雷达提升至 15 Hz

- **target**：保持用户要求的遥控速度（线速度 0.75 m/s、转向 1.05 rad/s），将自建 `lab_room` 场景可用 Burger 的仿真激光扫描频率从 5 Hz 提升为 15 Hz。
- **changed areas**：在 WSL 用户目录创建 `~/my_models/turtlebot3_burger_15hz/model.sdf`，仅将 `hls_lfcd_lds` 的 `<update_rate>` 改为 `15`；创建 `~/my_worlds/lab_room_15hz.launch.py`，从该用户模型文件生成机器人；WSL `slow_teleop/config/slam_toolbox_fast.yaml` 的 `minimum_time_interval` 从 `0.2` 改为 `0.06` 秒，使 SLAM 不再把 15 Hz 扫描限速回约 5 Hz。未改动 `/opt/ros` 的系统安装包，也未改动项目中用户未跟踪的 `gazebo_scene/lab_room.*`。
- **validation performed**：系统 Burger SDF 已确认原值仍为 5 Hz；新 SDF 的 15 Hz 标记恰有一处，`gz sdf -k` 通过；新 launch 的 `python3 -m py_compile` 通过，`ros2 launch ~/my_worlds/lab_room_15hz.launch.py --show-args` 成功解析；新 SLAM 间隔的文本断言与 WSL Git 检查通过。运行中的 Gazebo 仍加载旧模型，实际 `/scan`=15 Hz 需要切换到新 launch 后验证。
- **commit status**：WSL `slow_teleop` 配置提交 `a7dfaae`；本项目计划与记录随本 target 根仓库提交保存；WSL 用户目录下的模型和启动文件不在 Git 工作区。

## 2026-09-21 —— 保存障碍物地图、30 Hz 平面约束建图配置

- **target**：保存用户补充障碍物后的当前地图；保持 0.75 m/s、1.05 rad/s 遥控速度，提供 30 Hz 仿真扫描、滚转/俯仰锁定和后期地图异常的诊断配置。
- **map artifact**：已保存 `~/maps/lab_room_obstacles_2026-09-21.yaml` 与 `.pgm`。map_saver 报告图像为 198 × 139、0.05 m/像素；两文件存在并且 YAML 指向对应 PGM。
- **changed areas**：WSL 新增独立 `planar_lock_plugin`，每个 Gazebo 更新周期锁定机器人高度、roll、pitch，保留 X/Y/yaw；新增 `~/my_models/turtlebot3_burger_30hz/model.sdf`，激光 `update_rate=30` 并关闭仅用于显示的蓝色射线；新增 `~/my_worlds/lab_room_30hz.launch.py`，通过 `GAZEBO_PLUGIN_PATH` 加载该插件；WSL `slow_teleop` 新增 `config/slam_toolbox_high_rate.yaml`，使用 0.03 秒扫描间隔、0.02 m / 0.03 rad 阈值、0.5 秒地图刷新，并在诊断配置中关闭回环优化。
- **diagnosis evidence**：原运行实测 `/scan`=14.9 Hz、`/odom`=29.3 Hz、RTF=0.985；15 Hz 配置已未被 SLAM 节流。后期才发生的地图整体畸变与 `do_loop_closing=true` 的全局回环优化时机一致，是高优先级逻辑嫌疑；地图刷新间隔只影响发布/显示，不是扫描匹配频率。15 Hz 模型中蓝色激光可视化开启，30 Hz 模型已关闭以避免 GUI 负担。传感器高频测试中的 RTF=0.978，未见实时性积压。
- **validation performed**：新模型 `gz sdf -k` 通过，launch 通过 `py_compile` 和 `--show-args`，姿态锁插件 `colcon build` 成功且链接库无缺失依赖；隔离 ROS 域 98 / Gazebo 端口 11356 完整启动，Burger 成功生成、`/scan` 实测 29.35 Hz、静止 `/odom` 的 z≈0.01 且 roll/pitch 接近 0；高频 SLAM 节点实际返回 0.03 秒、0.02 m、0.03 rad、loop closing=false、0.5 秒。隔离实例已正常停止。碰撞过程中的横向接触与长期地图稳定性仍须在用户场景驾驶中复验。
- **commit status**：WSL `slow_teleop` 提交 `639639a`；WSL 平面插件独立仓库提交 `be0dab6`；本项目计划和记录随本 target 根仓库提交保存。

## 2026-09-21 —— A* 导航配置与 B 样条路径诊断

- **target**：为已保存的 lab room 地图建立论文导航结构的下一步学习材料：实际 Nav2 导航明确使用 A*；B 样条先作为不控制底盘的路径对比输出。
- **changed areas**：新增 `navigation_profiles/install_lab_room_nav2_profile.py`，在 WSL 中复制当前安装的 TurtleBot3 Burger 参数文件，并只将 `planner_server → GridBased → use_astar` 改为 `true`，不修改 `/opt/ros`；新增 `bspline_path_diagnostic.py`，订阅 `/plan` 并发布三次均匀 B 样条的 `/plan_bspline`；新增 `navigation_profiles/README.md`，记录安装、启动、RViz 对比和接入安全门槛；新增本 target plan。
- **validation performed**：两个 Python 文件均通过 `python -m py_compile`；A* 安装器的 `--help` 参数解析通过；`git diff --check` 通过。脚本特意基于 WSL 实际安装的 `burger.yaml` 生成配置，运行时参数加载、`use_astar=True` 和 `/plan_bspline` 发布仍须在用户的 WSL 内验证。
- **commit status**：待本 target 根仓库提交。
## 2026-09-22 —— 工控机视觉与任务源码择优归档

- **target**：通过 SSH 只读调查工控机，保留对当前消防控制室机器人学习项目有直接价值的单目采集、ArUco 定位、OCR 与任务编排源码参考。
- **changed areas**：新增 `reference/industrial_pc_2026-09-22/` 与 `docs/industrial-pc-source-audit.md`。归档了 `common_interfaces`、`fia_utils`、`task_manager`、启动脚本、面板识别前端、相机服务、单相机标定和 ArUco 手眼定位；加入全量本地 SHA-256 清单。
- **范围与排除**：未改动远程主机；未复制构建产物、模型权重、第三方 SDK/PaddleOCR、图像样本、私钥或任务密码配置。删除本地的历史复件/临时文件；两处硬编码内网 RTSP 地址已本地脱敏。
- **发现**：当前消防链路明确为“单目 RGB 图像服务 + 单相机内参 + ArUco 方形码 + 手眼标定”。可在其前添加双目同步、矫正和视差/深度层，但双目标定、基线、同步及精度不可从单目源码推断。另一工作区虽有深度/双目相关文件，尚无其已接入消防识别链路的证据。
- **validation performed**：对 13 个关键锚点在远程与本地分别计算 SHA-256，全部一致；生成本地 `SHA256SUMS.txt`；检查本地副本不含私钥、模型权重、历史复件、临时目录或未脱敏内网 RTSP 地址。暂存的上游原始源码在 `git diff --cached --check` 中报告既有尾随空白；为保持 SHA-256 一致性未改写，提交后工作树的 `git diff --check` 通过。
- **commit status**：已提交到根仓库 `main`（本条记录随提交一并保存）。
