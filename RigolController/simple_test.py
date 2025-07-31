import pyvisa
import time
import serial
import pandas as pd
import numpy as np
from datetime import datetime

# --- Connection Configuration ---
# Replace with your instrument's actual VISA resource string.
# You can find this using software like NI MAX or RIGOL's Ultra Sigma.
# The example string is from the programming manual[cite: 2919].
RESOURCE_STRING = 'TCPIP::192.168.1.2::INSTR'

# Initialize the VISA resource manager
rm = pyvisa.ResourceManager()

# 初始化stm32H7串口
SERIAL_PORT = 'COM10'  # Replace with your actual COM port
BAUD_RATE = 115200

def setup_h7_dds(ser, frequency, amplitude):
    """设置STM32H7 DDS输出参数"""
    try:
        # 设置DDS类型为AD9833
        ser.write(b'SET:DDS_TYPE:9833\n\r')
        time.sleep(0.1)
        
        # 设置频率 (Hz)
        cmd_freq = f'SET:DDS_FREQ:{int(frequency)}\n\r'
        ser.write(cmd_freq.encode())
        time.sleep(0.1)
        
        # 设置幅度 (V)
        cmd_amp = f'SET:DDS_AMP:{amplitude}\n\r'
        ser.write(cmd_amp.encode())
        time.sleep(0.1)
        
        # 等待输出稳定
        time.sleep(0.5)
        
        print(f"H7 DDS设置完成 - 频率: {frequency}Hz, 幅度: {amplitude}V")
        return True
        
    except Exception as e:
        print(f"H7 DDS设置失败: {e}")
        return False

def setup_oscilloscope_for_frequency(scope, frequency, expected_amplitude=1.0):
    """根据频率和预期幅度智能设置示波器参数"""
    try:
        # 根据频率自动设置合适的时基（显示3-5个周期）
        if frequency <= 100:  # 100Hz以下
            timebase = 0.01  # 10ms/div
        elif frequency <= 500:  # 500Hz以下
            timebase = 0.002  # 2ms/div
        elif frequency <= 1000:  # 1kHz以下
            timebase = 0.001  # 1ms/div
        elif frequency <= 5000:  # 5kHz以下
            timebase = 0.0002  # 200us/div
        elif frequency <= 10000:  # 10kHz以下
            timebase = 0.0001  # 100us/div
        elif frequency <= 50000:  # 50kHz以下
            timebase = 0.00002  # 20us/div
        elif frequency <= 100000:  # 100kHz以下
            timebase = 0.00001  # 10us/div
        elif frequency <= 500000:  # 500kHz以下
            timebase = 0.000002  # 2us/div
        else:  # 500kHz以上
            timebase = 0.000001  # 1us/div
            
        scope.write(f':TIMebase:SCALE {timebase}')
        print(f"- 时基设置为: {timebase} s/div (频率: {frequency}Hz)")
        
        # 根据预期幅度智能设置垂直刻度（使信号占用屏幕的60-80%）
        if expected_amplitude <= 0.5:
            v_scale = 0.1  # 100mV/div
        elif expected_amplitude <= 1.0:
            v_scale = 0.2  # 200mV/div
        elif expected_amplitude <= 2.0:
            v_scale = 0.5  # 500mV/div
        elif expected_amplitude <= 5.0:
            v_scale = 1.0  # 1V/div
        else:
            v_scale = 2.0  # 2V/div
        
        # 设置通道1参数
        scope.write(':CHANnel1:DISPlay ON')
        scope.write(f':CHANnel1:SCALe {v_scale}')
        scope.write(':CHANnel1:OFFSet 0')
        print(f"- 垂直刻度设置为: {v_scale} V/div (预期幅度: {expected_amplitude}V)")
        
        # 设置2次平均采样
        scope.write(':ACQuire:TYPE AVERages')
        scope.write(':ACQuire:AVERages 2')
        
        # 设置触发参数
        scope.write(':TRIGger:EDGE:SOURce CHANnel1')
        scope.write(':TRIGger:EDGE:SLOPe POSitive')
        
        # 设置触发电平为预期信号的20%
        trigger_level = expected_amplitude * 0.2
        scope.write(f':TRIGger:EDGE:LEVel {trigger_level}')
        scope.write(':TRIGger:SWEep AUTO')
        print(f"- 触发电平设置为: {trigger_level} V")
        
        # 设置采样率和内存深度
        if frequency <= 1000:
            sample_rate = "1e6"  # 1MSa/s for low frequency
        elif frequency <= 100000:
            sample_rate = "100e6"  # 100MSa/s for medium frequency
        else:
            sample_rate = "1e9"  # 1GSa/s for high frequency
            
        scope.write(f':ACQuire:SRATe {sample_rate}')
        print(f"- 采样率设置为: {sample_rate} Sa/s")
        
        time.sleep(1.0)  # 等待设置生效
        return True
        
    except Exception as e:
        print(f"示波器设置失败: {e}")
        return False

def adaptive_measure_signal(scope, expected_freq, expected_amp, max_retries=3):
    """自适应测量信号，动态调整设置以获得最佳结果"""
    for attempt in range(max_retries):
        try:
            print(f"测量尝试 {attempt + 1}/{max_retries}")
            
            # 等待信号稳定
            time.sleep(1.5)
            
            # 测量频率
            freq_str = scope.query(':MEASure:FREQuency? CHANnel1').strip()
            print(f"频率测量原始值: {freq_str}")
            
            # 测量峰峰值
            vamp_str = scope.query(':MEASure:VAMP? CHANnel1').strip()
            print(f"幅度测量原始值: {vamp_str}")
            
            # 解析测量结果
            try:
                measured_freq = float(freq_str)
                if measured_freq <= 0 or measured_freq > 1e9:
                    measured_freq = float('nan')
            except (ValueError, TypeError):
                measured_freq = float('nan')
                
            try:
                measured_vamp = float(vamp_str)
                if measured_vamp <= 0 or measured_vamp > 10:
                    measured_vamp = float('nan')
            except (ValueError, TypeError):
                measured_vamp = float('nan')
            
            # 检查测量结果的合理性
            freq_reasonable = not np.isnan(measured_freq) and abs(measured_freq - expected_freq) / expected_freq < 0.5
            amp_reasonable = not np.isnan(measured_vamp) and abs(measured_vamp - expected_amp) / expected_amp < 2.0
            
            if freq_reasonable and amp_reasonable:
                print(f"测量成功 - 频率: {measured_freq}Hz, 幅度: {measured_vamp}V")
                return measured_freq, measured_vamp
            
            # 如果测量不合理，尝试调整设置
            if not freq_reasonable and attempt < max_retries - 1:
                print("频率测量异常，调整时基...")
                # 调整时基
                current_timebase = float(scope.query(':TIMebase:SCALE?'))
                if measured_freq > expected_freq * 2:
                    new_timebase = current_timebase * 0.5  # 减小时基
                else:
                    new_timebase = current_timebase * 2    # 增大时基
                scope.write(f':TIMebase:SCALE {new_timebase}')
                time.sleep(1)
                
            if not amp_reasonable and attempt < max_retries - 1:
                print("幅度测量异常，调整垂直刻度...")
                # 调整垂直刻度
                current_vscale = float(scope.query(':CHANnel1:SCALe?'))
                if measured_vamp > expected_amp * 2:
                    new_vscale = current_vscale * 2    # 增大刻度
                else:
                    new_vscale = current_vscale * 0.5  # 减小刻度
                scope.write(f':CHANnel1:SCALe {new_vscale}')
                time.sleep(1)
            
        except Exception as e:
            print(f"测量失败: {e}")
            if attempt == max_retries - 1:
                return float('nan'), float('nan')
    
    print("多次尝试后仍无法获得合理测量结果")
    return measured_freq if 'measured_freq' in locals() else float('nan'), \
           measured_vamp if 'measured_vamp' in locals() else float('nan')

def run_frequency_sweep_test(scope, ser):
    """运行频率扫描测试"""
    print("\n=== 频率扫描测试 ===")
    
    # 测试参数
    frequencies = range(100, 1000001, 100)  # 100~1MHz(100Hz步进)
    amplitude = 1.0  # V
    
    results = []
    
    for i, frequency in enumerate(frequencies):
        print(f"\n测试进度: {i+1}/{len(frequencies)} - 频率: {frequency}Hz")
        
        # 设置DDS输出
        if not setup_h7_dds(ser, frequency, amplitude):
            continue
            
        # 设置示波器（传入预期幅度）
        if not setup_oscilloscope_for_frequency(scope, frequency, amplitude):
            continue
            
        # 自适应测量信号（不再使用自动设置）
        print("开始自适应测量...")
        measured_freq, measured_vamp = adaptive_measure_signal(scope, frequency, amplitude)
        
        # 计算误差
        freq_error = ((measured_freq - frequency) / frequency * 100) if not np.isnan(measured_freq) else float('nan')
        amp_error = ((measured_vamp - amplitude) / amplitude * 100) if not np.isnan(measured_vamp) else float('nan')
        
        # 记录结果
        result = {
            '设定频率(Hz)': frequency,
            '设定幅度(V)': amplitude,
            '测量频率(Hz)': measured_freq,
            '测量幅度(V)': measured_vamp,
            '频率误差(%)': freq_error,
            '幅度误差(%)': amp_error,
            '测量时间': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        }
        
        results.append(result)
        
        # 打印当前结果
        if not np.isnan(measured_freq) and not np.isnan(measured_vamp):
            print(f"✓ 频率误差: {freq_error:.3f}%, 幅度误差: {amp_error:.3f}%")
        else:
            print("✗ 测量失败，数据无效")
    
    return results

def run_amplitude_sweep_test(scope, ser):
    """运行幅度扫描测试"""
    print("\n=== 幅度扫描测试 ===")
    
    # 测试参数
    frequency = 1000  # Hz
    amplitudes = [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]  # V
    
    results = []
    
    for i, amplitude in enumerate(amplitudes):
        print(f"\n测试进度: {i+1}/{len(amplitudes)} - 幅度: {amplitude}V")
        
        # 设置DDS输出
        if not setup_h7_dds(ser, frequency, amplitude):
            continue
            
        # 设置示波器（传入预期幅度）
        if not setup_oscilloscope_for_frequency(scope, frequency, amplitude):
            continue
            
        # 自适应测量信号（不再使用自动设置）
        print("开始自适应测量...")
        measured_freq, measured_vamp = adaptive_measure_signal(scope, frequency, amplitude)
        
        # 计算误差
        freq_error = ((measured_freq - frequency) / frequency * 100) if not np.isnan(measured_freq) else float('nan')
        amp_error = ((measured_vamp - amplitude) / amplitude * 100) if not np.isnan(measured_vamp) else float('nan')
        
        # 记录结果
        result = {
            '设定频率(Hz)': frequency,
            '设定幅度(V)': amplitude,
            '测量频率(Hz)': measured_freq,
            '测量幅度(V)': measured_vamp,
            '频率误差(%)': freq_error,
            '幅度误差(%)': amp_error,
            '测量时间': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        }
        
        results.append(result)
        
        # 打印当前结果
        if not np.isnan(measured_freq) and not np.isnan(measured_vamp):
            print(f"✓ 频率误差: {freq_error:.3f}%, 幅度误差: {amp_error:.3f}%")
        else:
            print("✗ 测量失败，数据无效")
    
    return results

def save_results_to_excel(results, test_type):
    """保存测试结果到Excel文件"""
    if not results:
        print("没有测试结果可保存")
        return None
        
    filename = f"STM32H7_DDS_{test_type}_Test_{datetime.now().strftime('%Y%m%d_%H%M%S')}.xlsx"
    
    try:
        df = pd.DataFrame(results)
        
        # 创建Excel写入器
        with pd.ExcelWriter(filename, engine='openpyxl') as writer:
            # 写入原始数据
            df.to_excel(writer, sheet_name='测试结果', index=False)
            
            # 创建统计汇总
            valid_data = df.dropna()
            if not valid_data.empty:
                summary = {
                    '项目': ['总测试点数', '有效测试点数', '数据有效率(%)', 
                           '频率误差均值(%)', '频率误差标准差(%)', '最大频率误差(%)',
                           '幅度误差均值(%)', '幅度误差标准差(%)', '最大幅度误差(%)'],
                    '数值': [
                        len(df),
                        len(valid_data),
                        f"{len(valid_data)/len(df)*100:.1f}",
                        f"{valid_data['频率误差(%)'].mean():.3f}",
                        f"{valid_data['频率误差(%)'].std():.3f}",
                        f"{valid_data['频率误差(%)'].abs().max():.3f}",
                        f"{valid_data['幅度误差(%)'].mean():.3f}",
                        f"{valid_data['幅度误差(%)'].std():.3f}",
                        f"{valid_data['幅度误差(%)'].abs().max():.3f}"
                    ]
                }
                summary_df = pd.DataFrame(summary)
                summary_df.to_excel(writer, sheet_name='统计汇总', index=False)
        
        print(f"\n测试结果已保存到: {filename}")
        return filename
        
    except Exception as e:
        print(f"保存结果失败: {e}")
        return None

# 主程序
ser = None
scope = None

try:
    # 连接串口
    print(f"连接STM32H7串口: {SERIAL_PORT}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    time.sleep(1)
    print("STM32H7串口连接成功")
    
    # --- Connect to the Oscilloscope ---
    print(f"Connecting to instrument: {RESOURCE_STRING}...")
    scope = rm.open_resource(RESOURCE_STRING)
    scope.timeout = 5000  # Set a 5-second timeout for communication
    print("Connection successful.")

    # --- Instrument Identification ---
    # Query the instrument's identification string (*IDN?) [cite: 309]
    identity = scope.query('*IDN?')
    print(f"Instrument identified as: {identity.strip()}")

    # 选择测试类型
    print("\n=== STM32H7 DDS扫频幅度准确性测试 ===")
    print("1. 频率扫描测试 (固定幅度，扫描频率)")
    print("2. 幅度扫描测试 (固定频率，扫描幅度)")
    print("3. 单点测试")
    
    while True:
        try:
            choice = input("\n请选择测试类型 (1-3): ").strip()
            if choice in ['1', '2', '3']:
                break
            else:
                print("无效选择，请输入1、2或3")
        except KeyboardInterrupt:
            print("\n用户取消操作")
            exit()
    
    results = []
    
    if choice == '1':
        results = run_frequency_sweep_test(scope, ser)
        save_results_to_excel(results, "Frequency_Sweep")
        
    elif choice == '2':
        results = run_amplitude_sweep_test(scope, ser)
        save_results_to_excel(results, "Amplitude_Sweep")
        
    elif choice == '3':
        try:
            frequency = int(input("请输入测试频率 (Hz): "))
            amplitude = float(input("请输入测试幅度 (V): "))
            
            print(f"\n=== 单点测试: {frequency}Hz, {amplitude}V ===")
            
            # 设置DDS输出
            setup_h7_dds(ser, frequency, amplitude)
            
            # 设置示波器（传入预期幅度）
            setup_oscilloscope_for_frequency(scope, frequency, amplitude)
            
            # 自适应测量信号（不再使用自动设置）
            print("开始自适应测量...")
            measured_freq, measured_vamp = adaptive_measure_signal(scope, frequency, amplitude)
            
            # 计算误差
            freq_error = ((measured_freq - frequency) / frequency * 100) if not np.isnan(measured_freq) else float('nan')
            amp_error = ((measured_vamp - amplitude) / amplitude * 100) if not np.isnan(measured_vamp) else float('nan')
            
            print(f"\n=== 测试结果 ===")
            print(f"设定频率: {frequency}Hz, 测量频率: {measured_freq}Hz")
            print(f"设定幅度: {amplitude}V, 测量幅度: {measured_vamp}V")
            print(f"频率误差: {freq_error:.3f}%")
            print(f"幅度误差: {amp_error:.3f}%")
            
        except ValueError:
            print("输入格式错误")
        except KeyboardInterrupt:
            print("\n用户取消操作")
    
    # 显示测试统计
    if results:
        df = pd.DataFrame(results)
        valid_data = df.dropna()
        if not valid_data.empty:
            print(f"\n=== 测试统计 ===")
            print(f"总测试点数: {len(results)}")
            print(f"有效测量点: {len(valid_data)}")
            print(f"数据有效率: {len(valid_data)/len(results)*100:.1f}%")
            print(f"频率误差均值: {valid_data['频率误差(%)'].mean():.3f}%")
            print(f"幅度误差均值: {valid_data['幅度误差(%)'].mean():.3f}%")
            print(f"最大频率误差: {valid_data['频率误差(%)'].abs().max():.3f}%")
            print(f"最大幅度误差: {valid_data['幅度误差(%)'].abs().max():.3f}%")

except pyvisa.errors.VisaIOError as e:
    print(f"\nVISA连接错误: {e}")
    print("请检查示波器IP地址和网络连接")
except serial.SerialException as e:
    print(f"\n串口连接错误: {e}")
    print("请检查串口号和STM32H7连接")
except Exception as e:
    print(f"\n程序运行错误: {e}")
finally:
    # --- Close the Connection ---
    if scope:
        scope.close()
        print("\n示波器连接已关闭")
    if ser:
        ser.close()
        print("串口连接已关闭")
    if 'rm' in locals():
        rm.close()
        print("VISA资源管理器已关闭")
    print("\n程序结束")