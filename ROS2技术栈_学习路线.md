# ROS2 技术栈 —— 学习与搭建路线

> **目标**：学会搭论文用到的整套技术栈（ROS2 + Gazebo + 建图/导航 + MoveIt2 + ArUco + OCR），**结果不追求与论文完全一致**。
> **每阶段格式**：对应论文哪一章 → 命令 → 产出（做到什么算过关）。
> 全部基于官方 / 开源资源，**不需要学长的源码**。

---

## 阶段 0：装依赖（一次）
在 WSL 里跑（保持直连，关 TUN 代理）：
```bash
bash /mnt/c/Users/32126/WorkBuddy/2026-09-18-19-50-39/install_learning_stack.sh
```
并在 `~/.bashrc` 加一行：
```bash
export TURTLEBOT3_MODEL=burger
```
然后 `source ~/.bashrc`。

---

## 阶段 A：Gazebo + 机器人模型　【对应论文第2章】
```bash
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py
```
- **产出**：Gazebo 窗口里出现一台 TurtleBot3 小车（有激光雷达）。
- 提示：`ros2 launch turtlebot3_gazebo turtlebot3_empty_world.launch.py` 是空世界。

## 阶段 B：遥控 + 话题观察　【对应论文第2章·通信】
```bash
# 终端2：键盘遥控（方向键移动小车）
ros2 run turtlebot3_teleop teleop_keyboard
# 终端3：看话题
ros2 topic list
ros2 topic echo /scan       # 激光雷达数据
ros2 topic echo /odom       # 里程计数据
ros2 topic hz /scan         # 频率
```
- **产出**：能遥控小车，并在终端看到 `/scan`、`/odom` 的数据流。
- **学到的**：话题（Topic）发布/订阅、节点如何用话题通信。

## 阶段 C：SLAM 建图　【对应论文第3章·建图】
```bash
# 终端1：启动建图（Cartographer，TurtleBot3 官方教程用的；也可换 SLAM Toolbox）
ros2 launch turtlebot3_cartographer cartographer.launch.py
# 用 SLAM Toolbox 的话：
#   ros2 launch slam_toolbox online_async_launch.py
# 终端2：遥控走一圈，把房间扫完
ros2 run turtlebot3_teleop teleop_keyboard
# 终端3：保存地图
ros2 run nav2_map_server map_saver_cli -f ~/map
```
- **产出**：RViz 里逐渐长出一张 2D 栅格地图，并保存成 `~/map.pgm + ~/map.yaml`。
- **学到的**：SLAM 原理、`/map` 与 TF、栅格地图格式。

## 阶段 D：Nav2 自主导航　【对应论文第3章·导航】
```bash
ros2 launch turtlebot3_navigation2 navigation2.launch.py map:=$HOME/map.yaml
```
- 在 RViz 里：先点 **"2D Pose Estimate"** 给初始位姿，再点 **"Nav2 Goal"** 给目标点。
- **产出**：小车自动规划路径、避障、走到目标点。
- **学到的**：Nav2 架构、全局/局部规划、AMCL 定位。
- 论文里还用了 **A\*+B样条** 和 **DWB**——可以在 Nav2 参数里切换 planner/controller 体会。

## 阶段 E：MoveIt2 机械臂　【对应论文第4章·机械臂】
```bash
ros2 launch moveit_resources_panda_moveit_config demo.launch.py
```
- 在 RViz 的 MotionPlanning 面板里，拖动目标位姿 → Plan → Execute。
- **产出**：机械臂在可视化里完成运动规划与执行。
- **学到的**：MoveIt2 规划组、运动规划、轨迹执行（论文第4章后半的基础）。

## 阶段 F：ArUco 位姿估计　【对应论文第4章·位姿】
```bash
# 用 usb_cam 或图片发布图像，再用 aruco_ros 检测
ros2 run usb_cam usb_cam_node_exe        # 有摄像头时
# 或直接用 aruco 的示例/自己写节点喂图片
ros2 run aruco_ros single                 # 单标识检测（参考包内 launch）
```
- **产出**：从图像里检测出 ArUco 标识并输出其位姿（RViz 可视化）。
- **学到的**：相机模型、PnP 位姿估计——论文第4章"主辅 ArUco"的基础。
- 若 `aruco_ros` apt 包缺失 → 从源码装：`git clone` 后 `colcon build`。

## 阶段 G：OCR 文本识别　【对应论文第4章·OCR】
```bash
python3 -c "from paddleocr import PaddleOCR; o=PaddleOCR(lang='ch'); print(o.ocr('test.png'))"
```
- 先拿一张**面板/仪表照片**测识别效果。
- **产出**：能识别中文文本。
- **学到的**：PaddleOCR 使用——论文"文本关联 LED / 屏幕语义解析"的基础。

## 阶段 H：整合（自己的小项目）　【对应论文第5章雏形】
自己写一个 launch，把「机器人 + 导航」或「相机 + ArUco/OCR」串起来；再考虑：
- 用 `colcon` 建自己的 package；
- 写 launch 一键启动；
- 逐步向"导航到点 → 识别 → 动作"的闭环靠拢。

---

## 学习顺序 & 过关标准
| 阶段 | 过关标准 |
|---|---|
| A | Gazebo 里能看到小车 |
| B | 能遥控 + 终端看到 `/scan`、`/odom` |
| C | 扫出一张能用的栅格地图并保存 |
| D | RViz 设点后小车自主到达 |
| E | MoveIt2 里机械臂能规划+执行 |
| F | 图像里识别出 ArUco 并输出位姿 |
| G | PaddleOCR 识别出一段中文 |
| H | 能把自己的节点串成 launch 一键启动 |

**建议节奏**：A→B→C→D 先打通"移动机器人"这条线（对应论文第 2、3 章）；再 E→F→G 打通"感知+操作"这条线（对应第 4 章）；最后 H 整合（第 5 章）。**每一步都动手跑，别只看**。

---

## 和论文的对照
| 论文 | 本路线对应 |
|---|---|
| 第2章 平台/通信 | A、B（Gazebo + 话题） |
| 第3章 建图/导航 | C、D（SLAM + Nav2） |
| 第4章 视觉/位姿/OCR/机械臂 | E、F、G |
| 第5章 全流程实验 | H（整合） |

> 差别：论文用的是**自建消防机器人 + 消防主机场景**；本路线用**标准 TurtleBot3 + Panda**。**技术栈相同，具体对象不同**——这正是"学会技术栈但不完全复刻"的定位。等学长源码到手，再把"标准对象"换成"他的对象"即可。

---

*配套脚本：`install_learning_stack.sh`（一键装依赖）。*
