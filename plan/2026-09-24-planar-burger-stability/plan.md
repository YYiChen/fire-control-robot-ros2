# Burger 仿真平面稳定与运动期定位链诊断

**Target owner**：当前 Codex task
**Status**：平面稳定与短程 Nav2 门通过；长程导航、复杂碰撞及旧 TF 原因仍待验证。

## Goal

只在 Gazebo 仿真中，为 TurtleBot3 Burger 的隔离模型验证一种能保留车轮/车体碰撞的平面稳定约束；允许 x、y、yaw 运动，抑制非预期 z、roll、pitch。先确认约束不会关闭接触/雷达/里程计，再做静止、低速直行/转弯和低速接触障碍测试。若稳定门通过，再用同一短程目标采集带时间戳的 scan、odom、TF 与姿态，判断上一轮 Nav2 运动期 TF 过旧/定位分离是否仍复现。不得连接、控制或修改实机；不以强制姿态重置掩盖碰撞、时间同步或地图/世界坐标问题。

## Dirty-State Note

起始 `main` 与 `origin/main` 在 `fc33867` 同步。以下已有用户未跟踪内容由用户拥有，本 target 不读取其内容、不编辑、不暂存：`gazebo_scene/lab_room.launch.py`、`gazebo_scene/lab_room.world`、`log/`、`reference/industrial_pc_2026-09-22/packages/fire_panel_rec/src/ppocr/rec.bak`。

## Owned Files

- `plan/2026-09-24-planar-burger-stability/plan.md`
- `plan/log.md`
- WSL 隔离实现：`~/textmap_evaluation/harness/planar_stability/`
- WSL 本 target 运行记录：`~/textmap_evaluation/output/planar_stability/`
- `docs/open-source-textmap-evaluation.md`（仅在新增运行证据足以更新结论时）

## Read-Only Files

- `/opt/ros/humble`、Gazebo/SDFormat 系统安装及其插件
- `~/ros2_ws`、固定提交的 `~/textmap_evaluation/src/{NavOCR,TextMap,text_nav_bridge,text_nav_sim,TextMap_Examples}`
- `~/textmap_evaluation/output/textmap_sim/` 中保存的地图/地标
- 本项目与用户的 Gazebo world/model 源文件、已有 SLAM 地图、上轮 Nav2 retest 记录
- GitHub 上游与本地 `origin/main`

## Shared Dependencies

- TurtleBot3 Burger 差速驱动、碰撞几何、惯量、摩擦、Gazebo Classic 物理引擎与 SDF 版本
- `/cmd_vel`、`/odom`、`/scan`、`/clock`、`map→odom→base_footprint` TF 的时间戳与频率
- Nav2 map、AMCL 更新门限、controller/costmap 频率及已有安全 stand-off 目标
- 上轮发现：未约束 30 Hz SDF 副本在长程 Nav2 试验中发生明显 roll/pitch；静止时 scan/odom/TF 约 29–30 Hz，但运动时定位链失败原因尚未确认

## Expected Work

1. 检查 WSL 中 Gazebo 进程确已退出；只读确认 Gazebo/SDFormat/物理引擎版本、隔离 Burger SDF、碰撞/惯量、现有插件和上轮 TF/odom 记录。确认当前会话可用接口与实际安装的能力，不凭记忆假定插件存在。
2. 在 `~/textmap_evaluation/harness/planar_stability/` 为候选约束做独立副本或小型模型插件；绝不覆盖原模型。先验证 world-parented yaw/x/y 串联物理关节链能否保留 Burger 轮驱、接触和原 diff-drive 插件。如果与 stock 对照后发现 yaw/平移响应明显变差，则舍弃关节链，改为在 stock SDF 克隆上附加只对 roll/pitch 施加物理恢复力矩的 anti-roll 插件；插件不得改写位姿或 x/y/yaw，也不得关闭碰撞。不得把直接控制模型位姿的 planar-move 控制器与原 wheel diff-drive/odom TF 叠加。若只有位姿重置或禁用碰撞才可稳定，则停止并记录，不把该做法用于 Nav2 验证。
3. 先做静止与平面驱动测试，验证 x/y/yaw 仍可控、底盘和障碍仍报告真实碰撞、`/scan`/`/odom`/TF 时间戳连续；再以最低可控速度做一次轻触障碍测试。记录 roll/pitch 最大值、z 漂移、接触结果、速度、消息间隔和 Gazebo 日志。
4. 只有前述门通过才启动 AMCL/Nav2，以同一已知安全短程目标复测；同步记录 odom、map TF、合成 map 位姿、action 状态和过旧 TF 日志。出现明显倾斜、陈旧 TF或非预期运动立即停止本次仿真。
5. 复验后关闭本 target 启动的仿真进程；整理成功/失败与因果边界，按证据更新本 plan、`plan/log.md` 及必要的评估报告；只提交本 target 文件并推送。

## Validation

- 起始及结束均核实隔离 ROS domain、Gazebo 服务和进程状态；不影响其它用户进程。
- 无目标静止至少 10 秒：scan、odom、clock 与 TF 连续，时间戳间隔无异常大洞。
- 平面运动：前进、后退、原地转向均可用；机器人姿态 roll/pitch 保持在预设限值内，地面与障碍碰撞保持启用。
- 低速轻触：模型不能穿过障碍；记录碰撞响应和是否仍出现异常倾斜。强制位姿重置或关闭碰撞视为验证失败。
- 若进入 Nav2 复测，必须用同时间/近同步数据比较 map 与 odom；不能把 odom 当绝对真值，不能仅以 RViz 图像判定定位正确。
- `git diff --check`、逐项复核 target-only stage、`git status --short --branch`；保留用户已有未跟踪文件。

## Findings and Validation (2026-09-24)

- **运行范围**：WSL2 中 ROS 2 Humble + Gazebo Classic 11.10.2。两轮验证都使用隔离 ROS domain 197、独立候选 SDF 与既有 `turtlebot3_house_signs_light.world` / `map.yaml`；没有改原始模型、world、ROS 安装、`~/ros2_ws` 或固定上游 checkout，也没有连接实机。
- **约束方案选择**：world-parented 串联 x/y/yaw 关节链被 stock 对照否决。相同 0.25 rad/s、4 s 原地转向下 stock `/odom` yaw 约 0.991 rad，串联链约 0.204 rad，虽轮速积分相近，说明串联约束显著改变了轮地运动。最终候选保留 stock wheel-joint 差速驱动和原始碰撞几何，只加载 `planar_attitude_stabilizer`，在 `base_link` 上按 roll/pitch 误差施加 PD 恢复力矩；不重置模型姿态、不禁用碰撞、不接管 x/y/yaw。
- **平面与接触验证**：候选 22 s 序列含静止、靠近静态墙、后退、前进、原地转向和停稳；最大绝对 roll/pitch 分别约 0.823°/2.345°，odom z 约 0.0085–0.0104 m。IMU 4393、odom/scan/odom-TF 各约 646 个样本，消息最大间隔约 0.034–0.036 s。Gazebo contact stream 观察到 `burger::base_link::base_collision` 对 `contact_wall::wall::wall_collision`（132 条接触样本），以及车轮/脚轮对地面接触，故碰撞几何仍参与物理计算。相同序列的无约束 stock 对照最大 roll/pitch 约 1.432°/13.653°，odom z 达约 0.0249 m。该比较支持候选减少这次轻触序列的倾斜，不证明任何速度或任意碰撞下都不会翻倒。
- **差速响应对照**：同样 0.25 rad/s 转向 4 s，stock `/odom`/IMU yaw 约 0.9913/0.9927 rad，恢复力矩候选约 0.9845/0.988 rad；本测试中候选保留了原地转向响应。
- **Nav2 短程复验**：候选模型冷启动两次，手动启动 localization/navigation lifecycle manager 均返回 `success=True`。从 map `(-6.5,-1.0)` 到 `(-6.5,-1.8)` 的 NavigateToPose 两次均 `SUCCEEDED`，每次约 7.8 s；第二次 action feedback 最终距目标约 0.040 m，按同接收时刻近似配对的 `map→odom` 与 `odom→base_footprint` 组合位姿距目标约 0.0486 m。第二次运行最大绝对 roll/pitch 约 0.0032°/0.0896°。/scan 与 /odom 各约 29.4 Hz、最大间隔约 0.034 s；IMU 约 166.7 Hz、最大间隔约 0.006 s；两段 TF 约 29.4 Hz、最大间隔约 0.034 s。202 对 TF 在到达时刻的中位接收间隔约 0.001 s，而 `map→odom` 消息时间戳固定比 `odom→base_footprint` 超前 1.0 s，与 AMCL `transform_tolerance=1.0 s` 配置相符；不能把这个预期时间戳偏移单独当成消息延迟或旧 TF 证据。Nav2 在短程移动期间没有再报 `map→odom` 数据过旧；启动 global costmap 时曾有一次约 0.15–0.29 s 的“向过去外推”等待，随后 costmap 启动并完成导航。
- **结论边界**：证据只证明该候选在当前 Gazebo、当前模型和一条空旷 0.8 m 目标上可保持平面并完成 Nav2 短程导航。此前 stock 模型的长程 30 Hz 目标仍有失败记录；本 target 没有用候选复测长程路线、狭窄通道、复杂障碍/硬碰撞、长时间持续导航或 Nav2 期间的接触。旧 TF 触发因果、地图/世界全局对齐及仿真插件的物理真实性仍未解决，不能宣称长程问题已经修好或把该插件用于实机。
- **结束状态**：Nav2 和 Gazebo launch 已停止；结束检查没有发现本 target 的 Gazebo、Nav2 或 probe 进程。保留 ROS CLI 后台发现服务，不影响其他域或用户进程。

## Experience Signal (for human review)

候选信号：上一轮静止时间链正常而运动期出现 TF 过旧与车体倾斜，提示“提高扫描频率”不能代替运动、碰撞和姿态链路诊断。是否形成经验由人审决定。

## Commit Intent

提交信息：`docs: record planar stability and Nav2 retest`；仿真候选代码与原始记录留在 WSL 隔离 harness，不宣称其为项目正式模型或实机解决方案。
