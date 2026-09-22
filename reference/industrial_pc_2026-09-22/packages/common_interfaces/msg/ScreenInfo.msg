uint8 NORMAL_PAGE=0             # 初始页面
uint8 ALERT_PAGE=1              # 报警信息页面
uint8 PASSWORD_INPUT_PAGE=2     # 密码输入页面
uint8 CONFIRM_CLICK_PAGE=3      # 确认点击页面

std_msgs/Header header
string model              # 消防主机型号 e.g. "JB-TG-HJ9000"
uint8 current_page        # 消防主机页面，指示主机当前处于什么页面


# 报警详情(当status为alert时有效)
string first_alarm_id     # 首次火警ID e.g. "A001"
string first_alarm_time   # 首次火警时间 e.g. "11:00"
string first_alarm_type   # 首次火警类型 e.g. "光电感烟"
string active_alarm_id    # 当前激活火警ID
string active_alarm_time  # 当前激活火警时间
string active_alarm_type  # 当前激活火警类型
bool has_supervision      # 监管状态存在标志(true=有监管报警）
bool has_activation       # 设备启动状态标志(true=有设备启动）
bool has_feedback         # 设备反馈状态标志(true=有反馈信号）
bool has_delay            # 延时状态标志(true=存在延时操作）
bool has_fault            # 系统故障标志(true=存在故障）
bool has_shield           # 设备屏蔽标志(true=有屏蔽设备）
uint32 total_alarms       # 火警总数 e.g. 1
uint32 total_starts       # 总启动设备数量 e.g. 0
