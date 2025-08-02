# BASE4 Filter Output Control Test Configuration

# === 硬件连接配置 ===
OSCILLOSCOPE_USB = 'USB0::0x1AB1::0x04B0::DS2F164350759::INSTR'  # 示波器USB连接
SERIAL_PORT = 'COM7'  # STM32H7串口，请检查设备管理器确认端口号
BAUD_RATE = 115200
SCOPE_CHANNEL = 2           # 使用的示波器通道 (1或2)

# === 测试参数配置 ===
# 校准频率范围: 2300Hz 到 3000Hz，步长100Hz (8个频率点)
TEST_FREQUENCIES = [2300 + i*100 for i in range(8)]  # [2300, 2400, 2500, ..., 3000]

# 校准电压范围: 1.0V 到 2.0V，步长0.1V (11个电压点)
TEST_AMPLITUDES = [1.0 + i*0.1 for i in range(11)]  # [1.0, 1.1, 1.2, ..., 2.0]

# === 测量参数配置 ===
MEASUREMENT_AVERAGES = 3        # 每个点测量次数
STABILIZATION_TIME = 1.5        # 信号稳定等待时间 (秒)
# SCOPE_AVERAGES = 2              # 示波器平均次数
VOLTAGE_TOLERANCE_PERCENT = 3.0 # 电压误差容忍度 (%)
QUICK_EXIT_THRESHOLD = 5.0      # 快速跳出阈值 (%)，当误差小于此值时立即记录并进入下一轮

# === DDS幅度搜索配置 ===
DDS_AMP_MIN = 0.2               # DDS幅度搜索最小值
DDS_AMP_MAX = 2.5               # DDS幅度搜索最大值
DDS_AMP_STEP_INITIAL = 0.1      # DDS幅度初始步长
DDS_AMP_STEP_FINE = 0.02        # DDS幅度精细步长
MAX_SEARCH_ITERATIONS = 100     # 最大搜索迭代次数

# === 示波器设置配置 ===
TRIGGER_LEVEL = 0.12         # 触发电平 (V) - 按要求设置为0V
SCOPE_COUPLING = 'DC'       # 耦合方式
AUTO_TIMEBASE = True        # 是否自动设置时基 (确保一个周期内显示)
AUTO_VSCALE = True          # 是否自动设置垂直刻度 (目标电压的1/2)

# === BASE4 API配置 ===
# 校准命令格式: SET:DDS_AMP:value (设置DDS幅度)
DDS_AMP_CMD_FORMAT = 'SET:DDS_AMP:{}'     # DDS幅度设置命令
BASE4_FREQ_CMD_FORMAT = 'SET:BASE4:FREQ:{}' # 频率设置命令 
BASE4_RESPONSE_TIMEOUT = 2.0               # 命令响应超时 (秒)

# === 数据验证范围 ===
MAX_VOLTAGE_ERROR = 100     # 最大允许电压误差 (%)
MIN_SIGNAL_LEVEL = 0.01     # 最小信号电平 (V)
MAX_SIGNAL_LEVEL = 50       # 最大信号电平 (V)

# === 时基和垂直刻度档位 ===
STANDARD_TIMEBASES = [
    1e-9, 2e-9, 5e-9,          # 1ns, 2ns, 5ns
    1e-8, 2e-8, 5e-8,          # 10ns, 20ns, 50ns  
    1e-7, 2e-7, 5e-7,          # 100ns, 200ns, 500ns
    1e-6, 2e-6, 5e-6,          # 1μs, 2μs, 5μs
    1e-5, 2e-5, 5e-5,          # 10μs, 20μs, 50μs
    1e-4, 2e-4, 5e-4,          # 100μs, 200μs, 500μs
    1e-3, 2e-3, 5e-3,          # 1ms, 2ms, 5ms
    1e-2, 2e-2, 5e-2,          # 10ms, 20ms, 50ms
    1e-1, 2e-1, 5e-1,          # 100ms, 200ms, 500ms
    1.0, 2.0, 5.0              # 1s, 2s, 5s (低频信号)
]

STANDARD_VSCALES = [
    1e-3, 2e-3, 5e-3,          # 1mV, 2mV, 5mV
    1e-2, 2e-2, 5e-2,          # 10mV, 20mV, 50mV
    1e-1, 2e-1, 5e-1,          # 100mV, 200mV, 500mV
    1.0, 2.0, 5.0,             # 1V, 2V, 5V
    10.0                       # 10V
]

# === 文件输出配置 ===
OUTPUT_EXCEL = True         # 是否输出Excel文件
OUTPUT_PLOTS = True         # 是否生成图表
SAVE_RAW_DATA = True        # 是否保存原始数据
INCLUDE_STATISTICS = True   # 是否包含统计分析

print("BASE4滤波器校准配置已加载:")
print(f"- 校准频率: {len(TEST_FREQUENCIES)}个点 ({min(TEST_FREQUENCIES)}Hz - {max(TEST_FREQUENCIES)}Hz)")
print(f"- 校准电压: {len(TEST_AMPLITUDES)}个点 ({min(TEST_AMPLITUDES)}V - {max(TEST_AMPLITUDES)}V)")
print(f"- 总校准点: {len(TEST_FREQUENCIES) * len(TEST_AMPLITUDES)}")
print(f"- 串口: {SERIAL_PORT}")
print(f"- 示波器通道: CH{SCOPE_CHANNEL}")
print(f"- 电压误差容忍度: {VOLTAGE_TOLERANCE_PERCENT}%")
print(f"- 快速跳出阈值: {QUICK_EXIT_THRESHOLD}%")
print(f"- DDS幅度搜索范围: {DDS_AMP_MIN}V - {DDS_AMP_MAX}V")
