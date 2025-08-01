import pyvisa
import time
import serial
import pandas as pd
import numpy as np
from datetime import datetime
import matplotlib.pyplot as plt
import os

# 导入配置文件
try:
    from dds_sweep_config import *
    print("已加载配置文件 dds_sweep_config.py")
except ImportError:
    print("警告: 无法加载配置文件，使用默认配置")
    # 默认配置
    OSCILLOSCOPE_USB = 'USB0::0x1AB1::0x04B0::DS2F164350759::INSTR'
    SERIAL_PORT = 'COM7'
    BAUD_RATE = 115200
    AMPLITUDE_START = 0.1
    AMPLITUDE_END = 2.0
    AMPLITUDE_STEPS = 20
    FREQ_START = 100
    FREQ_END = 3000
    FREQ_STEPS = 30
    MEASUREMENT_AVERAGES = 3
    STABILIZATION_TIME = 0.5
    AUTO_SCALE_TIMEOUT = 3
    DDS_TYPE_CMD = 'SET:DDS_TYPE:9954'
    DDS_FREQ_CMD = 'SET:DDS_FREQ:{}'
    DDS_AMP_CMD = 'SET:DDS_AMP:{:.2f}'
    SCOPE_CHANNEL = 2
    SCOPE_COUPLING = 'DC'
    SCOPE_AVERAGES = 1

# Initialize the VISA resource manager
rm = pyvisa.ResourceManager()

def setup_h7_dds(ser, frequency, amplitude_vpp):
    """设置STM32H7 DDS输出参数"""
    try:
        # 清空串口缓冲区
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # 设置DDS类型
        cmd = f'{DDS_TYPE_CMD}\r\n'
        ser.write(cmd.encode())
        time.sleep(0.1)
        
        # 设置频率 (Hz)
        cmd_freq = f'{DDS_FREQ_CMD.format(int(frequency))}\r\n'
        ser.write(cmd_freq.encode())
        time.sleep(0.1)
        
        # 设置幅度 (根据配置文件格式)
        cmd_amp = f'{DDS_AMP_CMD.format(amplitude_vpp)}\r\n'
        ser.write(cmd_amp.encode())
        time.sleep(0.1)
        
        # 等待输出稳定
        time.sleep(STABILIZATION_TIME)
        
        print(f"H7 DDS设置完成 - 频率: {frequency}Hz, 幅度: {amplitude_vpp}Vpp")
        return True
        
    except Exception as e:
        print(f"H7 DDS设置失败: {e}")
        return False

def setup_oscilloscope_for_dds_test(scope, frequency, expected_amplitude):
    """根据DDS参数设置示波器"""
    try:
        # 简化的时基设置
        if frequency <= 200:
            timebase = 0.005  # 5ms/div
        elif frequency <= 500:
            timebase = 0.002  # 2ms/div
        elif frequency <= 1000:
            timebase = 0.001  # 1ms/div
        elif frequency <= 2000:
            timebase = 0.0005  # 500μs/div
        else:
            timebase = 0.0002  # 200μs/div
            
        scope.write(f':TIMebase:SCALE {timebase}')
        print(f"- 时基设置为: {timebase} s/div")
        
        # 设置通道显示
        scope.write(f':CHANnel{SCOPE_CHANNEL}:DISPlay ON')
        if SCOPE_CHANNEL == 1:
            scope.write(':CHANnel2:DISPlay OFF')
        else:
            scope.write(':CHANnel1:DISPlay OFF')
        
        # 根据预期幅度设置垂直刻度
        if expected_amplitude <= 0.5:
            v_scale = 0.1  # 100mV/div
        elif expected_amplitude <= 1.0:
            v_scale = 0.2  # 200mV/div
        elif expected_amplitude <= 2.0:
            v_scale = 0.5  # 500mV/div
        else:
            v_scale = 1.0  # 1V/div
        
        scope.write(f':CHANnel{SCOPE_CHANNEL}:SCALe {v_scale}')
        scope.write(f':CHANnel{SCOPE_CHANNEL}:OFFSet 0')
        scope.write(f':CHANnel{SCOPE_CHANNEL}:COUPling {SCOPE_COUPLING}')
        
        # 设置采样和触发
        scope.write(':ACQuire:TYPE AVERages')
        # scope.write(f':ACQuire:AVERages {SCOPE_AVERAGES}')
        
        scope.write(f':TRIGger:EDGE:SOURce CHANnel{SCOPE_CHANNEL}')
        scope.write(':TRIGger:EDGE:SLOPe POSitive')
        
        # 设置触发电平为0
        trigger_level = 0
        scope.write(f':TRIGger:EDGE:LEVel {trigger_level}')
        scope.write(':TRIGger:SWEep AUTO')
        
        # 设置采样率
        if frequency <= 1000:
            sample_rate = "10e6"  # 10MSa/s
        else:
            sample_rate = "100e6"  # 100MSa/s
            
        scope.write(f':ACQuire:SRATe {sample_rate}')
        
        time.sleep(1.0)  # 等待设置生效
        return True
        
    except Exception as e:
        print(f"示波器设置失败: {e}")
        return False

def measure_signal_multiple_times(scope, num_measurements=None):
    """多次测量取平均，提高精度"""
    if num_measurements is None:
        num_measurements = MEASUREMENT_AVERAGES
        
    freq_measurements = []
    vamp_measurements = []
    
    for i in range(num_measurements):
        try:
            # 等待信号稳定
            time.sleep(STABILIZATION_TIME)
            
            # 测量频率
            freq_str = scope.query(f':MEASure:FREQuency? CHANnel{SCOPE_CHANNEL}').strip()
            # 测量峰峰值
            vamp_str = scope.query(f':MEASure:VAMP? CHANnel{SCOPE_CHANNEL}').strip()
            
            # 解析结果并验证范围
            try:
                freq_val = float(freq_str)
                if 0 < freq_val < 1e9:  # 合理范围
                    freq_measurements.append(freq_val)
            except (ValueError, TypeError):
                print(f"频率测量 {i+1} 无效: {freq_str}")
                
            try:
                vamp_val = float(vamp_str)
                # 检查是否为异常值
                if vamp_val > 9e36:  # 检测9.9e37等异常值
                    print(f"检测到异常幅度值: {vamp_val}")
                    return float('nan'), float('nan'), float('nan'), float('nan')
                elif MIN_SIGNAL_LEVEL < vamp_val < MAX_SIGNAL_LEVEL:
                    vamp_measurements.append(vamp_val)
            except (ValueError, TypeError):
                print(f"幅度测量 {i+1} 无效: {vamp_str}")
                
        except Exception as e:
            print(f"测量 {i+1} 失败: {e}")
            continue
    
    # 计算平均值和标准差
    if freq_measurements:
        avg_freq = np.mean(freq_measurements)
        std_freq = np.std(freq_measurements) if len(freq_measurements) > 1 else 0
    else:
        avg_freq = float('nan')
        std_freq = float('nan')
        
    if vamp_measurements:
        avg_vamp = np.mean(vamp_measurements)
        std_vamp = np.std(vamp_measurements) if len(vamp_measurements) > 1 else 0
    else:
        avg_vamp = float('nan')
        std_vamp = float('nan')
    
    return avg_freq, avg_vamp, std_freq, std_vamp

def check_measurement_precision(scope):
    """检查当前测量设置的精度是否足够"""
    try:
        # 获取当前垂直刻度
        v_scale_str = scope.query(f':CHANnel{SCOPE_CHANNEL}:SCALe?').strip()
        v_scale = float(v_scale_str)
        
        # 测量当前信号幅度
        vamp_str = scope.query(f':MEASure:VAMP? CHANnel{SCOPE_CHANNEL}').strip()
        vamp = float(vamp_str)
        
        # 检查是否为异常值
        if vamp > 9e36:
            print(f"检测到异常幅度值: {vamp}")
            return False, 0
        
        # 计算信号占用的格数（8格满屏）
        grid_usage = vamp / (v_scale * 2)  # 峰峰值除以每格电压再除以2
        
        # 理想范围：信号占用1-6格（太小精度不够，太大可能削波）
        if 1 <= grid_usage <= 6:
            return True, grid_usage
        else:
            return False, grid_usage
            
    except Exception as e:
        print(f"精度检查失败: {e}")
        return False, 0

def auto_scale_if_needed(scope):
    """仅在精度不够时使用Auto Scale"""
    try:
        precision_ok, grid_usage = check_measurement_precision(scope)
        
        if not precision_ok:
            print(f"信号占用 {grid_usage:.1f} 格，精度不足，使用Auto Scale...")
            scope.write(':AUToscale')
            time.sleep(AUTO_SCALE_TIMEOUT)  # 使用配置的等待时间
            
            # 读取Auto Scale后的设置
            timebase = float(scope.query(':TIMebase:SCALE?').strip())
            v_scale = float(scope.query(f':CHANnel{SCOPE_CHANNEL}:SCALe?').strip())
            print(f"Auto Scale完成 - 时基: {timebase}s/div, 垂直: {v_scale}V/div")
            return True
        else:
            print(f"信号占用 {grid_usage:.1f} 格，精度足够")
            return False
            
    except Exception as e:
        print(f"Auto Scale失败: {e}")
        return False

def perform_dds_filter_sweep():
    """执行DDS滤波器幅频扫描测试"""
    print("=== DDS滤波器幅频响应测试 ===")
    print(f"幅度范围: {AMPLITUDE_START}Vpp - {AMPLITUDE_END}Vpp ({AMPLITUDE_STEPS}步)")
    print(f"频率范围: {FREQ_START}Hz - {FREQ_END}Hz ({FREQ_STEPS}步)")
    print()
    
    # 连接设备
    try:
        # 连接示波器
        scope = rm.open_resource(OSCILLOSCOPE_USB, timeout=10000)
        scope_id = scope.query('*IDN?')
        print(f"示波器连接成功: {scope_id.strip()}")
        
        # 连接STM32H7
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
        print(f"STM32H7连接成功: {SERIAL_PORT}")
        
    except Exception as e:
        print(f"设备连接失败: {e}")
        return
    
    # 准备数据存储
    results = []
    
    # 生成测试点
    amplitudes = np.linspace(AMPLITUDE_START, AMPLITUDE_END, AMPLITUDE_STEPS)
    frequencies = np.linspace(FREQ_START, FREQ_END, FREQ_STEPS)
    
    total_points = len(amplitudes) * len(frequencies)
    current_point = 0
    
    print(f"开始测试，共 {total_points} 个测试点...")
    print()
    
    try:
        for amp_idx, target_amplitude in enumerate(amplitudes):
            print(f"\n=== 幅度 {amp_idx+1}/{len(amplitudes)}: {target_amplitude:.2f}Vpp ===")
            
            for freq_idx, target_frequency in enumerate(frequencies):
                current_point += 1
                print(f"\n[{current_point}/{total_points}] 测试: {target_frequency:.0f}Hz, {target_amplitude:.2f}Vpp")
                
                # 设置H7 DDS输出
                if not setup_h7_dds(ser, target_frequency, target_amplitude):
                    print("H7设置失败，跳过此点")
                    continue
                
                # 设置示波器
                if not setup_oscilloscope_for_dds_test(scope, target_frequency, target_amplitude):
                    print("示波器设置失败，跳过此点")
                    continue
                
                # 检查并调整精度
                auto_scale_used = auto_scale_if_needed(scope)
                
                # 多次测量
                measured_freq, measured_vamp, freq_std, vamp_std = measure_signal_multiple_times(scope, 3)
                
                # 计算误差
                freq_error = ((measured_freq - target_frequency) / target_frequency * 100) if not np.isnan(measured_freq) else float('nan')
                amp_error = ((measured_vamp - target_amplitude) / target_amplitude * 100) if not np.isnan(measured_vamp) else float('nan')
                
                # 记录结果
                result = {
                    'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    'target_frequency_hz': target_frequency,
                    'target_amplitude_vpp': target_amplitude,
                    'measured_frequency_hz': measured_freq,
                    'measured_amplitude_vpp': measured_vamp,
                    'frequency_error_percent': freq_error,
                    'amplitude_error_percent': amp_error,
                    'frequency_std': freq_std,
                    'amplitude_std': vamp_std,
                    'auto_scale_used': auto_scale_used
                }
                results.append(result)
                
                # 显示结果
                if not np.isnan(measured_freq) and not np.isnan(measured_vamp):
                    print(f"测量结果: {measured_freq:.1f}Hz ({freq_error:+.2f}%), {measured_vamp:.3f}Vpp ({amp_error:+.2f}%)")
                else:
                    print("测量失败")
                
                # 进度显示
                progress = (current_point / total_points) * 100
                print(f"进度: {progress:.1f}%")
    
    except KeyboardInterrupt:
        print("\n测试被用户中断")
    except Exception as e:
        print(f"\n测试过程中发生错误: {e}")
    
    finally:
        # 关闭连接
        try:
            scope.close()
            ser.close()
            print("\n设备连接已关闭")
        except:
            pass
    
    # 保存结果
    if results:
        print(f"\n保存 {len(results)} 个测试结果...")
        
        # 创建DataFrame
        df = pd.DataFrame(results)
        
        # 生成文件名
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        excel_file = f'DDS_Filter_Sweep_{timestamp}.xlsx'
        
        # 保存到Excel
        with pd.ExcelWriter(excel_file, engine='openpyxl') as writer:
            # 原始数据
            df.to_excel(writer, sheet_name='Raw_Data', index=False)
            
            # 统计摘要
            summary_data = {
                'Parameter': ['Total Points', 'Valid Measurements', 'Frequency Range (Hz)', 
                             'Amplitude Range (Vpp)', 'Avg Freq Error (%)', 'Avg Amp Error (%)',
                             'Max Freq Error (%)', 'Max Amp Error (%)'],
                'Value': [
                    len(results),
                    len(df.dropna(subset=['measured_frequency_hz', 'measured_amplitude_vpp'])),
                    f"{FREQ_START} - {FREQ_END}",
                    f"{AMPLITUDE_START} - {AMPLITUDE_END}",
                    f"{df['frequency_error_percent'].mean():.2f}" if not df['frequency_error_percent'].isna().all() else 'N/A',
                    f"{df['amplitude_error_percent'].mean():.2f}" if not df['amplitude_error_percent'].isna().all() else 'N/A',
                    f"{df['frequency_error_percent'].abs().max():.2f}" if not df['frequency_error_percent'].isna().all() else 'N/A',
                    f"{df['amplitude_error_percent'].abs().max():.2f}" if not df['amplitude_error_percent'].isna().all() else 'N/A'
                ]
            }
            summary_df = pd.DataFrame(summary_data)
            summary_df.to_excel(writer, sheet_name='Summary', index=False)
        
        print(f"结果已保存到: {excel_file}")
        
        # 生成可视化图表
        plot_results(df, timestamp)
    
    else:
        print("没有有效的测试结果")

def plot_results(df, timestamp):
    """生成测试结果的可视化图表"""
    try:
        # 过滤有效数据
        valid_df = df.dropna(subset=['measured_frequency_hz', 'measured_amplitude_vpp'])
        
        if len(valid_df) == 0:
            print("没有有效数据用于绘图")
            return
        
        # 创建图表
        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle(f'DDS Filter Sweep Test Results - {timestamp}', fontsize=16)
        
        # 频率误差散点图
        axes[0,0].scatter(valid_df['target_frequency_hz'], valid_df['frequency_error_percent'], 
                         c=valid_df['target_amplitude_vpp'], cmap='viridis', alpha=0.6)
        axes[0,0].set_xlabel('Target Frequency (Hz)')
        axes[0,0].set_ylabel('Frequency Error (%)')
        axes[0,0].set_title('Frequency Error vs Target Frequency')
        axes[0,0].grid(True, alpha=0.3)
        
        # 幅度误差散点图
        im = axes[0,1].scatter(valid_df['target_amplitude_vpp'], valid_df['amplitude_error_percent'], 
                              c=valid_df['target_frequency_hz'], cmap='plasma', alpha=0.6)
        axes[0,1].set_xlabel('Target Amplitude (Vpp)')
        axes[0,1].set_ylabel('Amplitude Error (%)')
        axes[0,1].set_title('Amplitude Error vs Target Amplitude')
        axes[0,1].grid(True, alpha=0.3)
        
        # 频率响应热力图（如果数据足够）
        try:
            pivot_freq = valid_df.pivot_table(values='measured_frequency_hz', 
                                            index='target_amplitude_vpp', 
                                            columns='target_frequency_hz')
            im1 = axes[1,0].imshow(pivot_freq.values, aspect='auto', origin='lower')
            axes[1,0].set_title('Measured Frequency Heatmap')
            axes[1,0].set_xlabel('Target Frequency Index')
            axes[1,0].set_ylabel('Target Amplitude Index')
        except:
            axes[1,0].plot(valid_df['target_frequency_hz'], valid_df['measured_frequency_hz'], 'bo', alpha=0.5)
            axes[1,0].plot([valid_df['target_frequency_hz'].min(), valid_df['target_frequency_hz'].max()], 
                          [valid_df['target_frequency_hz'].min(), valid_df['target_frequency_hz'].max()], 'r--')
            axes[1,0].set_xlabel('Target Frequency (Hz)')
            axes[1,0].set_ylabel('Measured Frequency (Hz)')
            axes[1,0].set_title('Measured vs Target Frequency')
            axes[1,0].grid(True, alpha=0.3)
        
        # 幅度响应
        axes[1,1].plot(valid_df['target_amplitude_vpp'], valid_df['measured_amplitude_vpp'], 'ro', alpha=0.5)
        axes[1,1].plot([valid_df['target_amplitude_vpp'].min(), valid_df['target_amplitude_vpp'].max()], 
                      [valid_df['target_amplitude_vpp'].min(), valid_df['target_amplitude_vpp'].max()], 'b--')
        axes[1,1].set_xlabel('Target Amplitude (Vpp)')
        axes[1,1].set_ylabel('Measured Amplitude (Vpp)')
        axes[1,1].set_title('Measured vs Target Amplitude')
        axes[1,1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # 保存图表
        plot_file = f'DDS_Filter_Sweep_Plot_{timestamp}.png'
        plt.savefig(plot_file, dpi=300, bbox_inches='tight')
        print(f"图表已保存到: {plot_file}")
        
        # 显示图表
        plt.show()
        
    except Exception as e:
        print(f"绘图失败: {e}")

if __name__ == "__main__":
    perform_dds_filter_sweep()
