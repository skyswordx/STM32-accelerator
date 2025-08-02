# DDS Filter Sweep Test Configuration
# 请根据您的实际硬件配置修改以下参数

# === 硬件连接配置 ===
OSCILLOSCOPE_USB = 'USB0::0x1AB1::0x04B0::DS2F164350759::INSTR'  # 示波器USB连接
SERIAL_PORT = 'COM7'  # STM32H7串口，请检查设备管理器确认端口号
BAUD_RATE = 115200

# === 测试参数配置 ===
# 幅度扫描范围 (峰峰值电压)
AMPLITUDE_START = 0.5  # 起始幅度 (Vpp)
AMPLITUDE_END = 0.5    # 结束幅度 (Vpp)
AMPLITUDE_STEPS = 1   # 幅度测试点数 (建议10-50)

# 频率扫描范围
FREQ_START = 100       # 起始频率 (Hz)
FREQ_END = 3000        # 结束频率 (Hz)  
FREQ_STEPS = 30        # 频率测试点数 (建议20-100)

# === 测量参数配置 ===
MEASUREMENT_AVERAGES = 10    # 每个点测量次数 (建议3-5次)
STABILIZATION_TIME = 0.5    # 信号稳定等待时间 (秒)
AUTO_SCALE_TIMEOUT = 2      # Auto Scale等待时间 (秒)

# === STM32H7 DDS命令格式 ===
# 根据您的固件调整命令格式
DDS_TYPE_CMD = 'SET:DDS_TYPE:9954'        # DDS类型设置命令
DDS_FREQ_CMD = 'SET:DDS_FREQ:{}'          # 频率设置命令模板 ({}会被替换为频率值)
DDS_AMP_CMD = 'SET:DDS_AMP:{:.2f}'        # 幅度设置命令模板 ({}会被替换为幅度值)

# === 示波器通道配置 ===
SCOPE_CHANNEL = 2           # 使用的示波器通道 (1或2)
SCOPE_COUPLING = 'DC'       # 耦合方式 ('DC' 或 'AC')
SCOPE_AVERAGES = 1          # 示波器平均次数

# === 数据验证范围 ===
MAX_FREQ_ERROR = 50         # 最大允许频率误差 (%)
MAX_AMP_ERROR = 100          # 最大允许幅度误差 (%)
MIN_SIGNAL_LEVEL = 0.01     # 最小信号电平 (V)
MAX_SIGNAL_LEVEL = 10       # 最大信号电平 (V)

# === 文件输出配置 ===
OUTPUT_EXCEL = True         # 是否输出Excel文件
OUTPUT_PLOTS = True         # 是否生成图表
SAVE_RAW_DATA = True        # 是否保存原始数据

print("配置文件已加载:")
print(f"- 幅度范围: {AMPLITUDE_START}V - {AMPLITUDE_END}V ({AMPLITUDE_STEPS}步)")
print(f"- 频率范围: {FREQ_START}Hz - {FREQ_END}Hz ({FREQ_STEPS}步)")
print(f"- 总测试点: {AMPLITUDE_STEPS * FREQ_STEPS}")
print(f"- 串口: {SERIAL_PORT}")
print(f"- 示波器通道: CH{SCOPE_CHANNEL}")
