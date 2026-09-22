uint16[] positions          # 所有电机当前位置(mm) [索引0-3对应电机1-4]
uint8 device_id             # 当前设备ID
bool is_moving              # 运动状态
uint8 movement_direction    # 运动方向 (0=停止, 1=向上, 2=向下)