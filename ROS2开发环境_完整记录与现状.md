# ROS2 开发环境搭建 —— 完整记录与现状

- **文档生成时间**：2026-09-20
- **宿主机**：Windows 11（Windows 用户名：32126）
- **WSL 发行版**：Ubuntu 22.04.5 LTS（WSL2）
- **WSL 用户 / 主机名**：yyyyc001 ｜ DESKTOP-PQF11PP
- **用途**：搭建 ROS2 Humble 开发环境，用于「仿真平台」相关开发

---

## 一、目标与结论

**目标**：在 Windows 电脑上搭建 ROS2（Humble）开发环境；安装过程尽量**直连、不走代理**；最终实现「一键打开 VS Code 即进入可开发的 ROS 环境」。

**结论**：✅ 环境已全线打通。
组成 = Windows 11 + WSL2(Ubuntu 22.04) + ROS2 Humble + VS Code(Remote-WSL) + WSLg 图形。

**为什么选这套方案（不用 Docker）**：
- 仿真平台基线 = **Ubuntu 22.04 + ROS2 Humble**（学长给的平台环境）。
- WSL2 提供接近原生的 Linux 体验，比 Docker 更轻、文件持久、与 VS Code 集成好。
- WSLg（Win11 自带）让 rviz2 / Gazebo 等图形工具**直接弹窗到 Windows**，无需额外的 X Server（VcXsrv 等）。

---

## 二、当前环境现状（清单）

| 组件 | 版本 / 状态 | 说明 |
|---|---|---|
| 操作系统（宿主） | Windows 11 | WSLg 可用（Win11 原生特性） |
| WSL | WSL2 | 内核 `6.6.114.1-microsoft-standard-WSL2` |
| Linux 发行版 | Ubuntu 22.04.5 LTS (Jammy) | WSL 名称 `Ubuntu-22.04` |
| ROS2 | **Humble**（LTS） | 安装于 `/opt/ros/humble` |
| ROS2 包数量 | 282 个 `ros-humble-*` | |
| 构建工具 | `colcon` + `python3-colcon-common-extensions` | |
| 依赖工具 | `rosdep` 0.27.0-1 | 已 `init` + `update` |
| 编译链 | gcc / g++ / cmake / make | |
| 图形工具 | `rviz2`、`rqt` | `DISPLAY=:0`（WSLg 就绪） |
| 编辑器 | VS Code（Windows 侧）+ **Remote - WSL** 扩展 | |
| ROS 扩展 | **Robotics Developer Environment**（`Ranch-Hand-Robotics.rde-pack`） | 装在 **WSL 侧** |
| 工作区 | `/home/yyyyyc001/ros2_ws` | |
| WSL 内网 IP | `172.31.40.20` | 网卡 eth0 |
| 环境自动加载 | ✅ 已配置 | `~/.bashrc` 自动 source ROS2 |

**依赖体检结果（`check_ros2_env.sh`）**：PASS 24 / FAIL 0；`ros2 doctor` 正常
（distribution=humble(active)、RMW=rmw_fastrtps_cpp、平台 WSL2 / glibc2.35）。

---

## 三、系统架构

```
Windows 11（宿主）
├─ VS Code 窗口（界面层）
│    └─ Remote - WSL 扩展        ← 负责建立连接
└─ WSL2
     └─ Ubuntu 22.04.5 LTS（执行层）
          ├─ VS Code Server + 开发扩展（ROS / C++ / Python）
          ├─ 集成终端（bash）
          ├─ ROS2 Humble（/opt/ros/humble）
          ├─ 你的代码：~/ros2_ws
          └─ WSLg  →  rviz2 / Gazebo 窗口弹到 Windows 桌面
```

**关键概念**：VS Code 的「界面」在 Windows，而「执行（扩展 / 终端 / ROS 环境）」在 WSL 的 Ubuntu 里。
这与「直接在 Ubuntu 里装 VS Code」**效果等价**——所以教程里说「装某插件、配某环境」，在 WSL 侧照做即可。

---

## 四、完整操作记录（时间线）

> 时间：2026-09-18 ~ 2026-09-20

1. **环境诊断**：查明双层代理（进程环境变量 `127.0.0.1:9494`；Windows 系统代理 `127.0.0.1:7897`）；确认 WorkBuddy 会话为非管理员。
2. **安装 WSL2 + Ubuntu 22.04（直连）**：以管理员 PowerShell 运行 `install_wsl2_direct.ps1`（临时关系统代理 → `wsl --install -d Ubuntu-22.04` → 恢复代理）。下载期间直连。
3. **OOBE 初始化**：Ubuntu 22.04.5 首次启动，创建 UNIX 用户 `yyyyc001`。
4. **验证图形**：`echo $DISPLAY` 返回 `:0` → WSLg 就绪；`xeyes` 成功出窗。
5. **安装 ROS2 Humble**：在 Ubuntu 内运行 `install_ros2_humble.sh`（官方二进制源 `ros-humble-desktop`）。
6. **依赖体检**：`check_ros2_env.sh` → PASS 23 / FAIL 1（仅缺 `rosdep`）。
7. **补齐 rosdep**：`sudo apt install -y python3-rosdep` + `sudo rosdep init` + `rosdep update`（安装输出 `0 upgraded`，未改动任何已装 ROS 包）。
8. **建工作区与第一个包**：`create_first_ros2_pkg.sh` 生成 `~/ros2_ws` + `my_first_pkg`（publisher / subscriber）。
9. **配置环境自动加载**：`setup_ros2_dev_env.sh` 写入 `~/.bashrc`（自动 source ROS2 + 工作区）、添加别名、生成 `~/ros2_ws/.vscode/{settings,tasks,launch}.json`。
10. **安装 VS Code 扩展**：`install_vscode_extensions.sh` / `Ranch-Hand-Robotics.rde-pack`（ROS 扩展套件）。
11. **打通 Remote-WSL**：装好 `Remote - WSL` 扩展后，成功以 Remote-WSL 模式连入 WSL。
12. **最终验证（01:14）**：左下角 `WSL: Ubuntu-22.04`、`echo $ROS_DISTRO` = `humble`，环境全线打通。

---

## 五、遇到的问题与解决方案

| # | 问题 | 原因 | 解决 |
|---|---|---|---|
| 1 | `.ps1` 脚本报 `ParserError`（数组索引表达式丢失） | PowerShell 按 GBK 读无 BOM 的 UTF-8 脚本，中文乱码 | 脚本改为纯 ASCII（英文）提示 |
| 2 | `wsl: 正在等待 OOBE 命令完成分发` | 正常：等 Ubuntu 首次启动完成用户初始化 | 打开 Ubuntu 完成用户名/密码设置 |
| 3 | apt 报 `Could not get lock /var/lib/dpkg/lock-frontend` | 后台 `unattended-upgrades` 自动更新占锁 | 等其释放；一键等待：`while sudo fuser /var/lib/dpkg/lock-frontend >/dev/null 2>&1; do sleep 5; done; sudo apt install -y <pkg>` |
| 4 | PowerShell 里跑 `source`/`ros2` 报「术语不被识别」 | 在 Windows PowerShell 里执行了 bash 命令（当前目录虽是 UNC 的 WSL 路径，但执行环境是 Windows） | 进 Ubuntu bash，或用 `wsl -d Ubuntu-22.04 -- bash -lc "<cmd>"` |
| 5 | `ros2 --version` 报 `unrecognized arguments` | ROS2 CLI 没有 `--version` 参数 | 用 `ros2 --help` 或 `echo $ROS_DISTRO` 验证 |
| 6 | `code --install-extension` 报 `certificate has expired` | 代理（TUN/全局模式）做 TLS 中间人（MITM），证书 SAN 不匹配 | 临时退出代理 / 关 TUN → WSL 直连后重试；或走 Windows HTTP 代理端口 |
| 7 | VS Code 打开 `ros2_ws` 弹「不允许的主机 `wsl.localhost`」 | 用 UNC 路径 `\\wsl.localhost\...` 打开了工作区（Windows 侧，非 Remote-WSL） | 改用 Remote-WSL 方式打开（`WSL: Connect to WSL` 或 `code ~/ros2_ws`） |
| 8 | 左下角「受限模式」、无法信任工作区 | VS Code 的 Workspace Trust 安全机制 | 设置 `"security.workspace.trust.enabled": false` |
| 9 | 信任时报「文件系统提供程序不可用」 | Remote-WSL 连接尚未就绪 | 关掉 trust 检查；或先连好 WSL 再操作 |
| 10 | 左下角「已与 vscode-remote 断开连接」 | Remote-WSL 连接丢失 / Server 半装 | `Remote-WSL: Rebuild Window`；或删 `~/.vscode-server` 后重连 |
| 11 | 命令面板只有 `Connect to SSH`，没有 WSL | **`Remote - WSL` 扩展未安装/未启用** | 在**Windows 侧** VS Code 扩展面板搜 `WSL`，安装/启用 `Remote - WSL` |

---

## 六、脚本清单（本目录 `scripts/`）

| 脚本 | 运行位置 | 用途 |
|---|---|---|
| `install_wsl2_direct.ps1` | Windows 管理员 PowerShell | 直连安装 WSL2 + Ubuntu 22.04（临时关系统代理） |
| `install_ros2_humble.sh` | WSL Ubuntu | 安装 ROS2 Humble（官方二进制源） |
| `check_ros2_env.sh` | WSL Ubuntu | ROS2 依赖体检 + `ros2 doctor` 自检 |
| `create_first_ros2_pkg.sh` | WSL Ubuntu | 建工作区 `~/ros2_ws` + 示例包 `my_first_pkg` |
| `setup_ros2_dev_env.sh` | WSL Ubuntu | 一次性配置：`.bashrc` 自动 source、别名、`.vscode` 配置 |
| `Open-ROS2-VSCode.bat` | Windows（双击） | 一键以 Remote-WSL 打开 `~/ros2_ws` |
| `run_ros2.sh` | WSL Ubuntu | 一键「colcon build + 运行指定命令」 |
| `install_vscode_extensions.sh` | WSL Ubuntu | 安装 ROS2 开发所需 VS Code 扩展 |

---

## 七、常用命令速查

**打开环境**
- 双击 `scripts/Open-ROS2-VSCode.bat`
- 或 VS Code → `File → Open Recent` → `ros2_ws [WSL: Ubuntu-22.04]`
- 或 WSL 终端：`code ~/ros2_ws`

**ROS2 日常**
```bash
echo $ROS_DISTRO                 # 应输出 humble（环境已自动加载）
cd ~/ros2_ws && colcon build     # 编译工作区
source install/setup.bash        # 加载本工作区
ros2 run <包名> <节点名>          # 运行节点
ros2 launch <包名> <launch文件>   # 启动整套（含仿真）
ros2 node list                   # 查看节点
ros2 topic list                  # 查看话题
ros2 topic echo /<话题>          # 实时查看话题数据
rqt                              # 图形化工具台
rviz2                            # 3D 可视化
ros2 doctor --report             # 官方环境自检
```

**入门体验（小乌龟）**
```bash
ros2 run turtlesim turtlesim_node      # 终端1：弹窗
ros2 run turtlesim turtle_teleop_key   # 终端2：方向键控制
```

**VS Code 终端分屏**：命令面板 `Terminal: Split Terminal`（或 `Ctrl+Shift+5`）

---

## 八、重要注意事项 / 环境约束

1. **不要运行 `sudo apt upgrade`** —— 会升级所有包（含 ROS 源），导致 Humble **版本漂移**，破坏基线环境。
   - 如需保险可锁定：`sudo apt-mark hold 'ros-humble-*'`（需要装新 ROS 包前再 `unhold`）。
   - `sudo apt install -y <新包>` 只装新包、不动已装 ROS 包，**安全**。
2. **代理 TUN / 全局模式会挡 HTTPS** —— 做 TLS MITM 替换证书，导致 WSL 里 `pip` / `git clone` / 扩展下载报证书错。
   - 规律：**WSL 里 HTTPS 出问题 → 先关 TUN 代理、走直连。**
3. **代码放 WSL 内**（`~/ros2_ws`），**不要**放 `/mnt/c` —— 跨文件系统编译（colcon）会极慢。
4. **VS Code 开发类扩展要装到 WSL 侧**（不是 Windows 侧），才与「Ubuntu 原生开发」等价。
5. **打开工作区务必用 Remote-WSL 方式**，不要用 `\\wsl.localhost\...` UNC 路径。
6. Ubuntu 首次启动后台会跑自动更新（`unattended-upgrades`），**只升 Ubuntu 安全补丁，不动 ROS 包**，安全，无需干预。

---

## 九、下一步待办

- [ ] 跑 `turtlesim` 或学长的 `launch` 文件，验证整条链路
- [ ] 拿到学长**仿真工程的目录 + 启动方式**（README / launch 文件），做成「双击即启动」的一键脚本
- [ ] （可选）用 **Foxglove**（Windows 桌面版 + `rosbridge_server`）复刻以前 Docker 里的 web 3D 可视化
- [ ] （可选）dev container 方式：若以后要回到 Docker 方案，VS Code 的 `Remote - Containers` 与本套 WSL 流程可并存

---

*本文档由环境搭建过程整理生成，记录了完整操作、现状、脚本与注意事项。*
