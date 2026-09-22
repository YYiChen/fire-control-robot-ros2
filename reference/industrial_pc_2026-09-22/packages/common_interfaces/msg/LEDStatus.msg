# LEDStatus.msg

# 状态常量定义
uint8 OFF     = 0
uint8 RED     = 1
uint8 GREEN   = 2
uint8 YELLOW  = 3
uint8 UNKNOWN = 4

string name          # 指示灯名称，如"火警"、"消音"等
uint8 state          # 指示灯状态

