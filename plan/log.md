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
## 2026-09-22 —— 工控机论文复现材料补全复核

- **target**：确认首轮视觉归档能否覆盖论文复现及未来双目升级，并补齐直接缺失的工程材料。
- **changed areas**：补充归档 Piper/MoveIt、升降与状态发布、WheelTec 底盘/传感器、N10P 雷达、Astra 深度相机、Nav2 参数/多点巡航、ArUco 库、OCR 模型与推理代码、海康 MVS 头文件和 Linux 库；新增经过密码脱敏的任务流程 YAML，以及来自另一 WheelTec 工程的通用双目处理启动参考。
- **结论**：现在的归档足以作为论文工程的源码与运行材料参考，覆盖完整业务链路；它不构成当前 WSL 中可直接启动的真机系统。重新标定与双目仍依赖实际硬件、双目内外参/同步、系统依赖和设备权限。工控机环境中已观察到 MoveIt2/Nav2/SLAM Toolbox/RTAB-Map，未发现 easy_handeye2、aruco_ros、image_proc、stereo_image_proc；双目示例未与消防面板链路集成，保持“未验证参考”状态。
- **validation performed**：25 个远端/本地关键锚点 SHA-256 一致；全量 `SHA256SUMS.txt` 包含 2,633 个内容文件；检索未发现私钥、未脱敏内网 RTSP 地址、历史复件或临时目录。任务 YAML 仅保留 `<redacted>` 密码字段。暂存的上游源文件在 `git diff --cached --check` 中出现既有尾随空白，为保持 SHA-256 一致性未改写；提交后工作树 `git diff --check` 通过。
- **commit status**：已提交到根仓库 `main`（本条记录随提交一并保存）。

## 2026-09-22 —— 私有 GitHub 共享与论文文档排除

- **target**：创建可与同学共享的私有 GitHub 仓库，按用户最新要求不共享 Word 论文。
- **changed areas**：创建私有仓库 `YYiChen/fire-control-robot-ros2` 并配置为 `origin`；将两份 `.docx` 从全部共享 Git 历史移除，同时保留电脑上的原文件；新增 `*.docx` 忽略规则以防后续误提交；新增本 target plan。
- **validation performed**：首次推送前发现中文论文文件为 103.44 MiB，超过 GitHub 普通 Git 的 100 MiB 单文件限制；曾短暂改为 LFS 指针，但在推送前收到用户“不用推送”的明确指令，随即停止推送并改为完全排除。历史过滤后工作区中的 LFS 指针已从本机 LFS 缓存恢复为真实文件（108,465,283 与 36,665,927 bytes）。后续以 `main` 历史路径检索为空、本地文件仍在、GitHub 远端提交记录和 `git ls-remote` 作为确认。
- **commit status**：已推送至私有远端 `origin/main`，远端 `HEAD=ad230cd7115613eb6ef2724ad67ab5fed059214e`；本条最终确认记录待随下一次小型文档提交推送。

## 2026-09-22 —— 双目仿真依赖探测与最小处理管线

- **target**：在不连接真机、底盘或工控机服务的条件下，完成 Gazebo Classic 双目仿真第一阶段：由左右相机图像生成校正图、视差和 XYZ 点云，并以已知距离验证深度。
- **changed areas**：`stereo_sim/` 新增 30 Hz、640 × 480、60 mm 基线的静态 Gazebo 多相机模型和高对比度目标场景；提供参数化的图像校正、`disparity_node`、XYZ `point_cloud_node` 处理链；提供 0.40/0.60/0.80 m 世界生成器、单次与批量隔离测试脚本及深度验证器。预检脚本改为先 source ROS，再开启 Bash 未定义变量检查，修复用户遇到的 `AMENT_TRACE_SETUP_FILES` 错误。
- **validation performed**：实际 WSL 预检确认 `gazebo_ros`、`gazebo_plugins`、`image_proc`、`stereo_image_proc`、`image_view`、三个处理可执行文件和 `libgazebo_ros_camera.so` 全部可用。隔离 `ROS_DOMAIN_ID=77` / Gazebo 端口 `11377` 下，三次无界面运行均发现左右原图、校正图、视差和点云：0.40 m 得中央深度 0.371 m（视差 234、点云 225）；0.60 m 得 0.573 m（234、209）；0.80 m 得 0.769 m（231、197），均通过相对深度范围。首轮集成中发现并修复 `uniqueness_ratio` 应为浮点数、`image_proc` 只提供单色校正图故先输出 XYZ 点云、均匀面板缺少对应纹理等问题。最终确认无隔离 ROS 节点或 `gzserver` 残留；未启动真实硬件或发送底盘命令。静态 Bash/Python/XML 检查与 Git 检查见本 target 最终验证。
- **commit status**：初始脚手架为 `a67fe2a`；完整仿真实现待本次根仓库提交并推送。

### 后续修复

- **问题**：用户首次运行预检时，`/opt/ros/humble/setup.bash` 在 `set -u` 环境下读取未定义的 `AMENT_TRACE_SETUP_FILES`，导致预检未开始。
- **修复与验证**：将 `set -u` 移到 ROS setup 完成之后；实际 WSL 预检已完整通过。

## 2026-09-23 —— 双目语义面板三维定位仿真

- **target**：在无实机的 Gazebo 仿真中，把左目画面的具名按钮、双目视差测距、ArUco 面板基准和 `map` 三维坐标接起来，验证相机/面板位置变化时的稳定性及失效保护。
- **changed areas**：新增静态三按钮面板和 ArUco Original 582 模型、语义场景/启动、逐帧按钮检测与视差融合节点、位置真值验证和失效测试、README 实验步骤；`.gitattributes` 固定双目实验 Bash 脚本为 LF 换行，避免 Windows Git 自动转换后无法在 WSL 运行。仿真节点使用 Gazebo `/model_states` 取得相机真值位姿，仅用于本阶段验证；未连接底盘、机械臂或实机。
- **validation performed**：WSL2 ROS Humble 下的独立 `ROS_DOMAIN_ID=78` 与 Gazebo 端口 `11378` 无界面测试，面板 0.60 m 正向和 0.75 m/偏航 -0.06 rad 两个场景均通过。相机移动前后，三个按钮各有连续位姿更新，图像位移约 4–17 px；相对 Gazebo 几何真值的三维误差中位数在两场景中为 1–3 mm。另以合成 ROS 消息验证二维码缺失及视差无效时均不发布按钮位置。Python 编译、Bash 语法、SDF/XML 解析、`git diff --check` 通过；实验结束后隔离 ROS 域无节点，未见实验进程残留。
- **boundary**：色彩与二维码属于受控仿真目标；当前没有真实相机标定、机器人自身定位/手眼 TF、语义三维占据地图、主动避障和机械臂按压闭环，不能把理想场景的毫米数值作为实机精度结论。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-23 —— 双目点云三维占据地图与语义按钮同框

- **target**：让已有双目点云持续形成三维地图，并在相同的 `map` 坐标系里核对具名按钮的位置，不接入真实设备或底盘控制。
- **changed areas**：WSL 的 ROS Humble 补装 `ros-humble-octomap-server` 及两个新依赖（apt 报告 0 升级、3 新装）；新增 OctoMap 启动、隔离测试和地图/按钮关联验证器；README 补充使用方式与边界。
- **validation performed**：独立 `ROS_DOMAIN_ID=79` / Gazebo 端口 `11379` 两个无界面场景均通过。默认面板相机移动前/后 114/168 个已占据体素，三个按钮距最近占据体素 0.010/0.015/0.009 m；偏移旋转面板分别 184/192 个体素，距离 0.019/0.012/0.011 m。两次实验都收到超过 300 帧点云/地图消息，且语义定位的各按钮仍通过前后两观察位姿真值检查。静态与 Git 检查见本 target 最终验证。
- **boundary**：地图坐标使用 Gazebo 真值相机 TF；OctoMap 是占据层，不自带按钮名称或自主移动决策。尚未在真实相机/有漂移的 SLAM/机械臂按压场景验证。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-23 —— 语义目标接近位与安全路径建议

- **target**：从具名按钮、面板朝向和 OctoMap 的 2D 投影计算 0.25 m 接近位及 A* 路径，只发布规划信息，不控制底盘。
- **changed areas**：新增 `semantic_approach_planner.py` 与受控/实际场景验证器；在三维地图启动中接入建议节点。已知占据格和未知格按 7 cm 足迹膨胀，任何缺失/过期输入或不可通行路径都发布空路径与原因状态。
- **validation performed**：受控地图中 A* 给出 16 个路点，绕行障碍的最大横向偏移 0.138 m；堵死通道后状态 `no_known_free_path`，无路径。在当前单视角双目 Gazebo 场景，语义按钮定位持续通过，但 OctoMap 投影的已知空闲扇区太窄，规划器返回 `start_or_goal_not_free`，未发布非空路径；此为安全拒绝，不以放松未知区约束换测试通过。静态和 Git 检查见本 target 最终验证。
- **boundary**：尚无 Nav2 底盘跟踪、双目环绕扫描、手眼定位、机械臂按压；需要增加环境覆盖后才能对实际场景给出通行路径。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-23 —— 多视角扫掠打通实际占据地图上的路径建议

- **target**：通过仿真相机的多视角扫掠增加双目地图覆盖，验证规划器能否从安全拒绝转为可检验的接近路径。
- **changed areas**：新增独立的多视角实验脚本与验证器；README 记录八视角程序、地图覆盖变化和路径结果。不发送 `/cmd_vel`，不碰实机或用户日常 Gazebo 实例。
- **validation performed**：隔离 `ROS_DOMAIN_ID=81` / Gazebo 端口 `11381` 无界面运行通过。初始 OctoMap 投影已知空闲格 214；前七个视角维持 `start_or_goal_not_free`；最后相机在 `(0.12, 0)` 后空闲格 513，状态 `ready`，输出 8 个路点，终点距 reset 按钮 0.249 m，路径 0 个路点落入占据或未知格。静态及 Git 检查见本 target 最终验证。
- **boundary**：这是 Gazebo 服务移动相机且姿态使用真值 TF 的受控实验，未证明实际车体能跟踪路径、自动探索，也未验证机械臂按压或实机感知。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-24 —— RTAB-Map 双目视觉里程计与真值审计

- **target**：独立评价 RTAB-Map 双目视觉里程计是否能取代现有语义/地图链中的 Gazebo 真值位姿。
- **changed areas**：WSL Humble 仅安装 `ros-humble-rtabmap-odom` 和 13 个新依赖，0 旧包升级；新增隔离启动、VO 专用 CameraInfo 归一化、运动真值对照脚本；README 记录接口问题和实测误差。Gazebo 左右原始 `P[3]` 均约 -30，VO 支路修正左目为 0 后保留右目 -30；基座到相机 TF 与 `Reg/Force3DoF=true` 让平面运动约束作用在 z-up 基座。
- **validation performed**：`ROS_DOMAIN_ID=82` / Gazebo 端口 `11382` 无界面两次独立运行，平滑执行 x=+0.10 m、y=+0.05 m、yaw=+0.08 rad；最终位置向量误差分别 0.0051/0.0021 m，偏航误差 0.0096/0.0047 rad，收到 643/617 个 `/vo/odom` 消息。真值仅供验证器比较，VO 节点不订阅 `/model_states`。静态及 Git 检查见本 target 最终验证。
- **boundary**：短程受控结果支持继续集成，尚未替换语义地图链的真值 TF；中途位姿有约厘米级波动，未验证长程、回环、遮挡或实际相机标定。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-24 —— 视觉里程计接管语义按钮、三维地图与路径建议

- **target**：让三个运行节点不读取 Gazebo 模型真值，直接将双目 VO 位姿用于按钮三维坐标、OctoMap 和接近路径；真值只供隔离验证器比较。
- **changed areas**：语义节点按图像时间戳配对相机参数与 VO 位姿，发布 `map → stereo_base_link`；规划器从 `/vo/odom` 取起点并按输入种类报告过期；新建 `/vo/points2` 桥接，只转发具有同帧 VO 的点云并限到约 5 Hz，避免 30 Hz 原始点云先于 TF 到达导致地图中断。新增启动和验证脚本、README。
- **validation performed**：桥接前复现了点云持续到达而地图停止/完全不发布的间歇故障；抽查原始点云时间戳时 TF 可用比例为 0/55。桥接后三次独立 `ROS_DOMAIN_ID=83` 无界面实验，平滑移动相机 x=+0.10 m、y=+0.05 m，VO 位移向量误差 0.0048/0.0015/0.0017 m；按钮移动阶段中位误差约 1.4–4.5 mm；地图已知空闲格 216→331/343/336；均发布 7 路点、终点距 reset 0.249 m、路点均在已知空闲格。脚本核验两个运行节点订阅 `/vo/odom` 且未订阅 `/model_states`。旧模式的无 ArUco 与无视差失效保护、Python 编译、Bash 语法通过。隔离域实验后无遗留节点。
- **boundary**：仿真相机由验证器平滑移动，路径尚未驱动车体；VO 地图是无回环局部坐标系，识别依赖理想颜色/ArUco。三次运行不足以证明长期可靠性，真实面板、遮挡、光照、车体运动和机械臂按压仍未验证。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-24 —— 双目装上仿真车体并用车轮驱动验证

- **target**：在隔离 Gazebo 环境用真实差速轮运动替代测试器对相机模型的瞬移，核验已有 VO、按钮定位、OctoMap 与规划输出。
- **changed areas**：从 ROS 安装的 TurtleBot3 Burger 模型生成用户目录中的临时 SDF，将已有双目传感器装到 `base_link`；新增仅含面板的世界、独立启动与低速 `/cmd_vel` 验证脚本。原系统模型、旧静态世界和用户场景不修改。
- **validation performed**：首轮发现 Humble `spawn_entity.py` 不接受文本 XML 编码声明，修正生成器后两次独立 `ROS_DOMAIN_ID=84` 无界面测试通过。Gazebo 真值车体前进约 0.14939 m、转向约 0.122 rad；VO 位移向量误差 0.0025/0.0049 m；`reset` 运动后位置误差中位数 0.0061/0.0065 m；地图已知空闲格 351→539、361→509；地图新鲜且路径状态 `ready`、7 路点。静态、旧启动回归和 Git 检查见 target 结项。
- **boundary**：验证器给出短程固定速度命令，尚未让机器人自动跟踪 A* 路径；只覆盖理想面板和短程平地运动，未验证回环、障碍避让或机械臂动作。
- **commit status**：本 target 文件随本次根仓库提交保存；远端推送结果在提交后确认。

## 2026-09-24 —— 激光守护的自动接近与停车

- **target**：让双目仿真车依据视觉里程计、具名按钮和已知空闲 A* 路径自动驶向 reset 前观察位，输入失效则停车。
- **changed areas**：新增状态门控低速路径跟踪器、独立 `ROS_DOMAIN_ID=85` 启动与验收；当起点地图未知时，在前方激光净空有效下最多低速前进 0.12 m 主动扫图。自动实验将观察距离设为 0.28 m，通用默认 0.25 m 不变。ArUco 姿态改用 PnP 三维旋转，避免旧的正视近似在车体转向时把相机朝向当面板法向。
- **validation performed**：无主动扫图时实验安全地原地停车；加入后两次独立实测自动前进约 0.272/0.252 m，停车时 VO 距采用目标 0.023/0.024 m，停车后 2 秒 Gazebo 真值无可测位移；最新重规划目标距 VO 0.024/0.045 m，地图持续更新。独立失效注入检查表明路径不可达、激光过期、近障碍和扫图受阻时均为零速。旧静态面板定位及无 ArUco/无视差失效回归通过。Python/Shell 与 Git 检查见结项。
- **boundary**：仅在理想面板、平地短距离仿真中验证；VO 和目标仍有厘米级波动，局部地图无回环，未做真实控制室、动态行人避障、Nav2 长程行为或机械臂动作。不能把本次到位与自动按压画等号。
- **commit status**：本 target 文件随本次根仓库提交保存；GitHub 连接恢复后同步。

## 2026-09-24 —— 双目语义与自动接近的多场景稳健性矩阵

- **target**：在面板平移、偏航及完全遮挡下检查按钮三维坐标、法向、地图、自动停车与无目标时的安全停止。
- **changed areas**：启动 world 可覆盖；新增隔离场景生成与矩阵验证；静态 ArUco 法向及按钮地图坐标做有界多帧中位数融合；最后一个已确认安全目标的到达阈值从 0.025 m 调整至 0.035 m，与 0.025 m 栅格尺度相适应，其他激光/路径/输入新鲜度门控保留。
- **validation performed**：完整隔离矩阵 3/3 通过。偏移/偏航末段按钮真值误差约 6–9 mm，面板法向末段偏航误差 0.008/0.015 rad，停车时 VO 距记录目标 0.032/0.031 m，停车后 Gazebo 真值位移 0。遮挡场景 0 按钮位姿、0 标记观测、车位移 0，控制保持停止；此轮停因是输入过期。失效注入检查确认不可达、激光过期、近障碍、受阻扫图均零速。Python 编译、Shell 语法和 Git diff/status 见结项。
- **boundary**：曾有一次偏航运行在停车后验证器未收到投影地图，单场景复测与完整矩阵未复现；需长时间重复与数据录制定位。只涵盖理想颜色/ArUco 的静态平面短距离仿真，不是完整科研级真实场景验证；无 OCR 状态识别、双目回环 SLAM、动态障碍或机械臂按压。
- **commit status**：本 target 目标文件随本次根仓库提交；远端同步在提交后核对。

## 2026-09-24 —— 双目 RTAB-Map 图优化及回环审计

- **target**：检验现有双目 VO 能否驱动图优化建图，区分图/占据图输出、全局回环接受和真实误差改善，并核对语义地标坐标接口。
- **changed areas**：新增隔离图 SLAM 启动、同时间戳 VO TF 桥、闭合轨迹验证器及带两块视觉标志的独立世界生成器；运行依赖只解压到 WSL 用户目录。README 记录实测与下一阶段坐标契约。
- **validation performed**：ROS2 Humble 无界面隔离测试中，原稀疏世界默认一致性门槛 3 的三轮均建立 25–30 个图节点/占据图，2/3 接受全局回环；VO 首尾闭合距离约 0.0060/0.0045/0.0061 m，图校正后约 0.0019/0.0047/0.0142 m。门槛 4 的探索未改善接受率或闭合距离，已恢复默认 3。增加视觉标志后的两轮建立 23/22 个节点、38/39 条约束及 23/22 张占据图，均无全局回环，验证器如实返回 `INCOMPLETE`。Python 编译、XML 解析、Shell 语法和 Git 检查见结项。
- **boundary**：这只是相机经 Gazebo 服务运动的小范围仿真图实验，回环接受不稳定，也未证明图优化提高精度。现有 VO 局部 `map` 下的按钮与 OctoMap 尚未随 `graph_map` 修正，直接合并会出现双 TF 父节点；图 SLAM 不能作为已完成的实时语义地图交付。
- **commit status**：仅提交本 target 所有文件；远端同步在提交后核对。

## 2026-09-24 —— 双目按钮地标投影到图地图

- **target**：让双目识别的三个按钮和面板标记明确处于 VO 坐标，再随 RTAB-Map 图坐标修正重投影，验证按钮与占据地图的空间关系及遮挡失效。
- **changed areas**：语义节点新增坐标系/TF 开关且默认保持旧行为；图启动使用唯一 TF 链，VO TF 桥只转发有限、归一化的里程计；新增图语义投影、时间戳保留、图处理心跳及基础/偏航/遮挡三场景验证。投影输出避免每次单按钮更新重复发送其余按钮。
- **validation performed**：Windows/WSL 代码同步后，Python 编译、Shell 语法、90° 旋转/时间戳数学断言通过。WSL Humble 隔离无界面三场景通过：基础与偏航场景运动后均有 8 个以上图节点、二维占据图和三按钮图坐标；最终按钮真值误差从约 1 mm 到 33 mm（不同运行），到最近地图占据格约 1–3.4 cm。遮挡场景原始/图按钮输出均为 0；过滤前有 VO 丢失 NaN TF 警告，过滤后无该警告。旧 VO→按钮→OctoMap→接近路径回归仍通过，地图已知空闲格 216→350，路径 7 路点。
- **boundary**：测试器经 Gazebo 服务移动相机，目标颜色/ArUco 为理想数据。图地标仅应用最新全局修正，未作为 RTAB 图约束；实际全局回环、长轨迹局部图变形、图校正的三维占据图、真实 OCR/灯态与机器人按压尚未验证。短程厘米内误差不是机械按压精度保证。
- **commit status**：本 target 文件经最终 Git 检查后提交并同步。

## 2026-09-24 —— 图支路三维占据图与按钮对齐

- **target**：验证双目 RTAB-Map 图支路可输出实际三维 OctoMap，并让相同 `graph_map` 中的按钮三维位置与占据体素比较。
- **changed areas**：隔离图节点启用 `Grid/3D=true`、`Grid/RayTracing=true`、2.5 cm 栅格和 1.2 m 深度；三场景验证器订阅非空 `/octomap_occupied_space` 与 `/octomap_binary`、检查坐标系和按钮 XYZ 最近点距离。
- **validation performed**：启动日志确认 Grid 参数被采纳。WSL Humble 基础/偏航场景分别有 8/9 次三维点云更新、末次 173/267 个占据点、8/9 条非空 OctoMap；按钮到最近占据点分别约 0.010–0.012 m 与 0.007–0.010 m。遮挡负例按钮和三维地图消息均为 0。Python 编译、Shell/Git 检查见结项。验证器首轮因 Humble 结构化点云元素不可按元组切片而退出，按 x/y/z 字段读取后完整三场景通过。
- **boundary**：图建图约 2 Hz 处理，三维地图随关键位姿发布；只验证短程理想面板，没有全局回环后多局部地图重组、长程漂移、真实 OCR/LED 或机器人动作。厘米级体素距离不是按钮按压精度。
- **commit status**：本 target 文件经目标复审与检查后提交；GitHub 推送使用本次系统代理的临时 Git 参数。

## 2026-09-24 —— 消防面板视觉复现证据审计

- **target**：核对论文 OCR/LED 方案、工控机归档源码模型和当前双目仿真感知链，定义真实面板识别进入三维语义地图前的实验门槛。
- **changed areas**：新增 `docs/panel-perception-reproducibility-audit.md` 与本 target plan；未修改源码、模型、实机或未跟踪的其他工作文件。
- **validation performed**：清点归档中的图像/标注/模型路径，逐项核对论文第 4 章、OCR launch、LED 解析和双目语义节点；WSL 只读检查确认 OpenCV 可用、Python PaddleOCR/OpenVINO/pip 当前缺失；Git diff/status 在提交前复核。
- **boundary**：论文描述的是 500 张有效真实图像及 PP-OCRv5 微调，当前归档仅有 v3 推理模型且未检出图像/标注；源码风险尚为静态审查，不是运行时缺陷率。现阶段不能宣称论文面板识别已复现。
- **commit status**：本 target 的 plan、报告与 log 条目单独提交；是否推送远端在提交后核对。
