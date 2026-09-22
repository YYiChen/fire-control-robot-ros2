
# 任务类型枚举（主任务+子任务）
# 主任类型枚举，对应参数 task_type 
uint8 NAVIGATION=10                  # 导航
uint8 ROUTINE_GUARD=20               # 日常值守
uint8 ALARM_HANDLING=30              # 报警处理


# 子任务类型枚举，对应参数 subtask_type   
uint8 NAVIGATION_TO_CHARGING=11      # 回到充电点位
uint8 NAVIGATION_TO_TASK=12          # 去往任务点位

uint8 ROUTINE_GUARD_VISION=21        # 视觉检查
uint8 ROUTINE_GUARD_SELF_CHECK=22    # 操作自检

uint8 ALARM_HANDLING_SILENCING=31  # 消音
uint8 ALARM_HANDLING_RESETTING=32  # 重置
# ...可扩展其他任务类型


# 当前任务状态枚举, 对应参数 status 
uint8 PENDING=0     # 待处理
uint8 ACTIVE=1      # 处理中
uint8 SUCCEEDED=2   # 成功
uint8 FAILED=3      # 失败
uint8 NORMAL=4      # 正常
uint8 ALERT=5       # 警告


builtin_interfaces/Time stamp
uint8 task_type        # 主任务类型
uint8 subtask_type     # 子任务类型（无子任务时为0）
string task_id         # 任务唯一标识
uint8 status           # 当前任务状态
string location        # 对导航任务
string error_message   # 失败时传递原因