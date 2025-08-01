# BASE4 Filter Output Control Test Configuration

# === 硬件连接配置 ===
OSCILLOSCOPE_USB = 'USB0::0x1AB1::0x04B0::DS2F172300309::INSTR'  # 示波器USB连接
SERIAL_PORT = 'COM10'  # STM32H7串口，请检查设备管理器确认端口号
BAUD_RATE = 115200
SCOPE_CHANNEL = 2           # 使用的示波器通道 (1或2)

# === 测试参数配置 ===
# 指定的测试频率 (Hz) - 按照用户要求的特定频率点
TEST_FREQUENCIES = [1000, 1200, 2000, 5000, 5200, 10000, 20000, 20200, 49800, 50000]

# 指定的测试幅度 (V) - 按照用户要求的特定电压点  
TEST_AMPLITUDES = [0.1, 0.2, 0.5, 1.0, 1.2, 2.0]

# === 测量参数配置 ===
MEASUREMENT_AVERAGES = 3    # 每个点测量次数 (建议3-5次)
STABILIZATION_TIME = 1.0    # 信号稳定等待时间 (秒)
SCOPE_AVERAGES = 2          # 示波器平均次数 (按要求设置为2次)

# === 示波器设置配置 ===
TRIGGER_LEVEL = 0.0         # 触发电平 (V) - 按要求设置为0V
SCOPE_COUPLING = 'DC'       # 耦合方式
AUTO_TIMEBASE = True        # 是否自动设置时基 (确保一个周期内显示)
AUTO_VSCALE = True          # 是否自动设置垂直刻度 (目标电压的1/2)

# === BASE4 API配置 ===
# 命令格式: SET:BASE4:voltage&frequency 
# 第一个{}是voltage，第二个{}是frequency
BASE4_CMD_FORMAT = 'SET:BASE4:{}&{}'  # 格式正确: voltage&frequency
BASE4_RESPONSE_TIMEOUT = 2.0           # 命令响应超时 (秒)

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

print("BASE4滤波器测试配置已加载:")
print(f"- 测试频率: {len(TEST_FREQUENCIES)}个点 ({min(TEST_FREQUENCIES)}Hz - {max(TEST_FREQUENCIES)}Hz)")
print(f"- 测试幅度: {len(TEST_AMPLITUDES)}个点 ({min(TEST_AMPLITUDES)}V - {max(TEST_AMPLITUDES)}V)")
print(f"- 总测试点: {len(TEST_FREQUENCIES) * len(TEST_AMPLITUDES)}")
print(f"- 串口: {SERIAL_PORT}")
print(f"- 示波器通道: CH{SCOPE_CHANNEL}")
print(f"- 触发电平: {TRIGGER_LEVEL}V")
print(f"- 示波器平均: {SCOPE_AVERAGES}次")
