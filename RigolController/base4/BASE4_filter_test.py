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
    from base4_config import *
    print("已加载配置文件 base4_config.py")
except ImportError:
    print("警告: 无法加载配置文件，使用默认配置")
    # 默认配置
    OSCILLOSCOPE_USB = 'USB0::0x1AB1::0x04B0::DS2F172300309::INSTR'
    SERIAL_PORT = 'COM7'
    BAUD_RATE = 115200
    SCOPE_CHANNEL = 2
    TEST_FREQUENCIES =  [100, 200, 500, 1000, 1100, 1800, 2000, 2300, 2900, 3000]
    TEST_AMPLITUDES = [0.1, 0.2, 0.5, 1.0, 1.2, 2.0]
    MEASUREMENT_AVERAGES = 3
    STABILIZATION_TIME = 1.0
    SCOPE_AVERAGES = 2
    TRIGGER_LEVEL = 0.0

# Initialize the VISA resource manager
rm = pyvisa.ResourceManager()

def setup_h7_filter_base4(ser, frequency, voltage):
    """设置STM32H7 BASE4滤波器输出参数"""
    try:
        # 清空串口缓冲区
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # 使用SET:BASE4:voltage&frequency命令
        # 注意：根据命令文档，格式应该是 SET:BASE4:voltage&frequency
        cmd = f'{BASE4_CMD_FORMAT.format(voltage, frequency)}\r\n'
        print(f"发送命令: {cmd.strip()}")  # 调试：显示实际发送的命令
        ser.write(cmd.encode())
        time.sleep(0.2)
        
        # 可选：读取H7的响应以确认命令执行
        response = ser.read_all().decode('utf-8', errors='ignore')
        if response:
            print(f"H7响应: {response.strip()}")
        
        # 等待输出稳定
        time.sleep(STABILIZATION_TIME)
        
        print(f"H7 BASE4设置完成 - 频率: {frequency}Hz, 电压: {voltage}V")
        return True
        
    except Exception as e:
        print(f"H7 BASE4设置失败: {e}")
        return False

def setup_oscilloscope_for_filter_output(scope, frequency, expected_voltage):
    """根据滤波器输出参数设置示波器"""
    try:
        # 根据频率设置时基，确保显示一个完整周期
        period = 1.0 / frequency  # 周期 (秒)
        # 修正：时基应该设置为显示多个周期，而不是1/10周期
        # 一个周期在10格显示需要: timebase = period / 10
        # 但实际应该显示2-4个周期比较好看
        timebase = period / 4.0  # 时基设置为周期的1/4，显示约2.5个周期 (10格屏幕)
        
        # 选择合适的时基档位
        closest_timebase = min(STANDARD_TIMEBASES, key=lambda x: abs(x - timebase))
        
        scope.write(f':TIMebase:SCALE {closest_timebase}')
        print(f"- 时基设置为: {closest_timebase} s/div (频率: {frequency}Hz, 周期: {period:.6f}s)")
        
        # 设置通道显示
        scope.write(f':CHANnel{SCOPE_CHANNEL}:DISPlay ON')
        if SCOPE_CHANNEL == 1:
            scope.write(':CHANnel2:DISPlay OFF')
        else:
            scope.write(':CHANnel1:DISPlay OFF')
        
        # 设置垂直刻度 - 确保信号占用合适的屏幕空间
        # 假设expected_voltage是峰峰值，那么信号应该占用4-6格（8格满屏）
        # 垂直刻度设置为峰峰值的1/4到1/3比较合适
        v_scale = expected_voltage / 4.0  # 信号占用约4格，留有余量
        
        # 但是要确保刻度不会太小导致噪声放大
        if v_scale < 0.01:  # 小于10mV时使用10mV
            v_scale = 0.01
        elif v_scale < 0.02:  # 小于20mV时使用20mV  
            v_scale = 0.02
        
        # 选择合适的垂直刻度档位
        closest_vscale = min(STANDARD_VSCALES, key=lambda x: abs(x - v_scale))
        
        scope.write(f':CHANnel{SCOPE_CHANNEL}:SCALe {closest_vscale}')
        scope.write(f':CHANnel{SCOPE_CHANNEL}:OFFSet 0')
        scope.write(f':CHANnel{SCOPE_CHANNEL}:COUPling {SCOPE_COUPLING}')
        print(f"- 垂直刻度设置为: {closest_vscale} V/div (期望电压: {expected_voltage}V, 计算刻度: {v_scale:.4f}V/div)")
        
        # 设置采样和平均
        scope.write(':ACQuire:TYPE AVERages')
        scope.write(f':ACQuire:AVERages {SCOPE_AVERAGES}')
        
        # 设置触发为0V (按要求)
        scope.write(f':TRIGger:EDGE:SOURce CHANnel{SCOPE_CHANNEL}')
        scope.write(':TRIGger:EDGE:SLOPe POSitive')
        scope.write(f':TRIGger:EDGE:LEVel {TRIGGER_LEVEL}')
        scope.write(':TRIGger:SWEep AUTO')
        print(f"- 触发电平设置为: {TRIGGER_LEVEL}V")
        
        # 设置采样率 - 确保足够的采样点
        # 根据奈奎斯特定理，采样率应该至少是信号频率的2倍，实际建议10倍以上
        if frequency <= 1000:
            sample_rate = "10e6"   # 10MSa/s (对于1kHz信号过采样10,000倍)
        elif frequency <= 10000:
            sample_rate = "100e6"  # 100MSa/s (对于10kHz信号过采样10,000倍)
        elif frequency <= 50000:
            sample_rate = "500e6"  # 500MSa/s (对于50kHz信号过采样10,000倍)
        else:
            sample_rate = "1e9"    # 1GSa/s (最高采样率)
            
        scope.write(f':ACQuire:SRATe {sample_rate}')
        print(f"- 采样率设置为: {sample_rate} Sa/s (频率: {frequency}Hz)")
        
        time.sleep(1.0)  # 等待设置生效
        return True
        
    except Exception as e:
        print(f"示波器设置失败: {e}")
        return False

def measure_output_amplitude_multiple_times(scope, num_measurements=None):
    """多次测量输出幅度，取平均值"""
    if num_measurements is None:
        num_measurements = MEASUREMENT_AVERAGES
        
    vamp_measurements = []  # 峰峰值 (示波器直接测量)
    vrms_measurements = []  # 有效值
    freq_measurements = []  # 频率 (用于验证)
    
    for i in range(num_measurements):
        try:
            # 等待信号稳定
            time.sleep(0.5)
            
            # 测量峰峰值 (主要参数)
            vamp_str = scope.query(f':MEASure:VAMP? CHANnel{SCOPE_CHANNEL}').strip()
            # 测量有效值
            vrms_str = scope.query(f':MEASure:VRMS? CHANnel{SCOPE_CHANNEL}').strip()
            # 测量频率 (用于验证设置正确)
            freq_str = scope.query(f':MEASure:FREQuency? CHANnel{SCOPE_CHANNEL}').strip()
            
            # 解析峰峰值
            try:
                vamp_val = float(vamp_str)
                # 检查是否是异常值 (如9.9e37)
                if vamp_val > 1e30:
                    print(f"检测到异常峰峰值: {vamp_val}")
                    continue
                if MIN_SIGNAL_LEVEL < vamp_val < MAX_SIGNAL_LEVEL:
                    vamp_measurements.append(vamp_val)
                else:
                    print(f"峰峰值超出范围: {vamp_val}V")
            except (ValueError, TypeError):
                print(f"峰峰值测量 {i+1} 无效: {vamp_str}")
                
            # 解析有效值
            try:
                vrms_val = float(vrms_str)
                if vrms_val > 1e30:
                    print(f"检测到异常有效值: {vrms_val}")
                    continue
                if MIN_SIGNAL_LEVEL < vrms_val < MAX_SIGNAL_LEVEL:
                    vrms_measurements.append(vrms_val)
                else:
                    print(f"有效值超出范围: {vrms_val}V")
            except (ValueError, TypeError):
                print(f"有效值测量 {i+1} 无效: {vrms_str}")
                
            # 解析频率
            try:
                freq_val = float(freq_str)
                if 10 < freq_val < 1e8:  # 合理的频率范围
                    freq_measurements.append(freq_val)
            except (ValueError, TypeError):
                print(f"频率测量 {i+1} 无效: {freq_str}")
                
        except Exception as e:
            print(f"测量 {i+1} 失败: {e}")
            continue
    
    # 计算平均值和标准差
    if vamp_measurements:
        avg_vamp = np.mean(vamp_measurements)
        std_vamp = np.std(vamp_measurements) if len(vamp_measurements) > 1 else 0
    else:
        avg_vamp = float('nan')
        std_vamp = float('nan')
        
    if vrms_measurements:
        avg_vrms = np.mean(vrms_measurements)
        std_vrms = np.std(vrms_measurements) if len(vrms_measurements) > 1 else 0
    else:
        avg_vrms = float('nan')
        std_vrms = float('nan')
        
    if freq_measurements:
        avg_freq = np.mean(freq_measurements)
        std_freq = np.std(freq_measurements) if len(freq_measurements) > 1 else 0
    else:
        avg_freq = float('nan')
        std_freq = float('nan')
    
    # 返回测量结果
    return avg_vamp, avg_vrms, avg_freq, std_vamp, std_vrms, std_freq

def perform_base4_filter_test():
    """执行BASE4滤波器输出控制精度测试"""
    print("=== STM32H7 BASE4滤波器输出控制精度测试 ===")
    print(f"测试频率: {TEST_FREQUENCIES}")
    print(f"测试幅度: {TEST_AMPLITUDES}")
    print(f"总测试点: {len(TEST_FREQUENCIES) * len(TEST_AMPLITUDES)}")
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
    
    total_points = len(TEST_FREQUENCIES) * len(TEST_AMPLITUDES)
    current_point = 0
    
    print(f"开始测试，共 {total_points} 个测试点...")
    print()
    
    try:
        for freq_idx, target_frequency in enumerate(TEST_FREQUENCIES):
            print(f"\n=== 频率 {freq_idx+1}/{len(TEST_FREQUENCIES)}: {target_frequency}Hz ===")
            
            for amp_idx, target_voltage in enumerate(TEST_AMPLITUDES):
                current_point += 1
                print(f"\n[{current_point}/{total_points}] 测试: {target_frequency}Hz, {target_voltage}V")
                
                # 设置H7 BASE4滤波器输出
                if not setup_h7_filter_base4(ser, target_frequency, target_voltage):
                    print("H7设置失败，跳过此点")
                    continue
                
                # 设置示波器
                if not setup_oscilloscope_for_filter_output(scope, target_frequency, target_voltage):
                    print("示波器设置失败，跳过此点")
                    continue
                
                # 多次测量
                measured_vamp, measured_vrms, measured_freq, vamp_std, vrms_std, freq_std = measure_output_amplitude_multiple_times(scope)
                
                # 计算误差
                if not np.isnan(measured_vamp):
                    amp_error = ((measured_vamp - target_voltage) / target_voltage * 100)
                else:
                    amp_error = float('nan')
                    
                if not np.isnan(measured_freq):
                    freq_error = ((measured_freq - target_frequency) / target_frequency * 100)
                else:
                    freq_error = float('nan')
                
                # 记录结果
                result = {
                    'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    'target_frequency_hz': target_frequency,
                    'target_voltage_v': target_voltage,
                    'measured_vamp_v': measured_vamp,      # 峰峰值
                    'measured_vrms_v': measured_vrms,      # 有效值
                    'measured_freq_hz': measured_freq,     # 实际频率
                    'voltage_error_percent': amp_error,    # 基于峰峰值的误差
                    'frequency_error_percent': freq_error, # 频率误差
                    'vamp_std': vamp_std,
                    'vrms_std': vrms_std,
                    'freq_std': freq_std
                }
                results.append(result)
                
                # 显示结果
                if not np.isnan(measured_vamp):
                    print(f"测量结果: Vpp={measured_vamp:.3f}V ({amp_error:+.2f}%), Vrms={measured_vrms:.3f}V, 频率={measured_freq:.1f}Hz ({freq_error:+.2f}%)")
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
        excel_file = f'BASE4_Filter_Test_{timestamp}.xlsx'
        
        # 保存到Excel
        with pd.ExcelWriter(excel_file, engine='openpyxl') as writer:
            # 原始数据
            df.to_excel(writer, sheet_name='Raw_Data', index=False)
            
            # 按频率分组的统计
            freq_stats = df.groupby('target_frequency_hz').agg({
                'measured_vamp_v': ['mean', 'std', 'count'],
                'voltage_error_percent': ['mean', 'std', 'min', 'max']
            }).round(4)
            freq_stats.columns = ['_'.join(col).strip() for col in freq_stats.columns.values]
            freq_stats.to_excel(writer, sheet_name='Frequency_Stats')
            
            # 按幅度分组的统计
            amp_stats = df.groupby('target_voltage_v').agg({
                'measured_vamp_v': ['mean', 'std', 'count'],
                'voltage_error_percent': ['mean', 'std', 'min', 'max']
            }).round(4)
            amp_stats.columns = ['_'.join(col).strip() for col in amp_stats.columns.values]
            amp_stats.to_excel(writer, sheet_name='Amplitude_Stats')
            
            # 总体统计摘要
            summary_data = {
                'Parameter': [
                    'Total Test Points', 
                    'Valid Measurements', 
                    'Frequency Range (Hz)', 
                    'Voltage Range (V)', 
                    'Avg Voltage Error (%)', 
                    'Max Voltage Error (%)',
                    'Min Voltage Error (%)',
                    'Voltage Error Std (%)'
                ],
                'Value': [
                    len(results),
                    len(df.dropna(subset=['measured_vamp_v'])),
                    f"{min(TEST_FREQUENCIES)} - {max(TEST_FREQUENCIES)}",
                    f"{min(TEST_AMPLITUDES)} - {max(TEST_AMPLITUDES)}",
                    f"{df['voltage_error_percent'].mean():.2f}" if not df['voltage_error_percent'].isna().all() else 'N/A',
                    f"{df['voltage_error_percent'].max():.2f}" if not df['voltage_error_percent'].isna().all() else 'N/A',
                    f"{df['voltage_error_percent'].min():.2f}" if not df['voltage_error_percent'].isna().all() else 'N/A',
                    f"{df['voltage_error_percent'].std():.2f}" if not df['voltage_error_percent'].isna().all() else 'N/A'
                ]
            }
            summary_df = pd.DataFrame(summary_data)
            summary_df.to_excel(writer, sheet_name='Summary', index=False)
        
        print(f"结果已保存到: {excel_file}")
        
        # 生成可视化图表
        plot_base4_results(df, timestamp)
    
    else:
        print("没有有效的测试结果")

def plot_base4_results(df, timestamp):
    """生成BASE4测试结果的可视化图表"""
    try:
        # 过滤有效数据
        valid_df = df.dropna(subset=['measured_vamp_v'])
        
        if len(valid_df) == 0:
            print("没有有效数据用于绘图")
            return
        
        # 创建图表
        fig, axes = plt.subplots(2, 3, figsize=(18, 12))
        fig.suptitle(f'STM32H7 BASE4 Filter Output Control Test - {timestamp}', fontsize=16)
        
        # 1. 误差 vs 频率
        axes[0,0].scatter(valid_df['target_frequency_hz'], valid_df['voltage_error_percent'], 
                         c=valid_df['target_voltage_v'], cmap='viridis', alpha=0.7)
        axes[0,0].set_xlabel('Target Frequency (Hz)')
        axes[0,0].set_ylabel('Voltage Error (%)')
        axes[0,0].set_title('Voltage Error vs Frequency')
        axes[0,0].grid(True, alpha=0.3)
        axes[0,0].set_xscale('log')
        
        # 2. 误差 vs 目标电压
        scatter = axes[0,1].scatter(valid_df['target_voltage_v'], valid_df['voltage_error_percent'], 
                                   c=valid_df['target_frequency_hz'], cmap='plasma', alpha=0.7)
        axes[0,1].set_xlabel('Target Voltage (V)')
        axes[0,1].set_ylabel('Voltage Error (%)')
        axes[0,1].set_title('Voltage Error vs Target Voltage')
        axes[0,1].grid(True, alpha=0.3)
        plt.colorbar(scatter, ax=axes[0,1], label='Frequency (Hz)')
        
        # 3. 测量值 vs 目标值
        axes[0,2].scatter(valid_df['target_voltage_v'], valid_df['measured_vamp_v'], alpha=0.6)
        # 理想线
        min_v = min(valid_df['target_voltage_v'].min(), valid_df['measured_vamp_v'].min())
        max_v = max(valid_df['target_voltage_v'].max(), valid_df['measured_vamp_v'].max())
        axes[0,2].plot([min_v, max_v], [min_v, max_v], 'r--', label='Ideal')
        axes[0,2].set_xlabel('Target Voltage (V)')
        axes[0,2].set_ylabel('Measured Voltage (V)')
        axes[0,2].set_title('Measured vs Target Voltage')
        axes[0,2].legend()
        axes[0,2].grid(True, alpha=0.3)
        
        # 4. 误差分布直方图
        axes[1,0].hist(valid_df['voltage_error_percent'].dropna(), bins=20, alpha=0.7, edgecolor='black')
        axes[1,0].set_xlabel('Voltage Error (%)')
        axes[1,0].set_ylabel('Count')
        axes[1,0].set_title('Voltage Error Distribution')
        axes[1,0].grid(True, alpha=0.3)
        
        # 5. 频率响应 (按频率分组的平均误差)
        freq_group = valid_df.groupby('target_frequency_hz')['voltage_error_percent'].agg(['mean', 'std'])
        axes[1,1].errorbar(freq_group.index, freq_group['mean'], yerr=freq_group['std'], 
                          marker='o', capsize=5, capthick=2)
        axes[1,1].set_xlabel('Frequency (Hz)')
        axes[1,1].set_ylabel('Average Voltage Error (%)')
        axes[1,1].set_title('Average Error vs Frequency')
        axes[1,1].set_xscale('log')
        axes[1,1].grid(True, alpha=0.3)
        
        # 6. 幅度线性度 (按电压分组的平均误差)
        volt_group = valid_df.groupby('target_voltage_v')['voltage_error_percent'].agg(['mean', 'std'])
        axes[1,2].errorbar(volt_group.index, volt_group['mean'], yerr=volt_group['std'], 
                          marker='s', capsize=5, capthick=2, color='orange')
        axes[1,2].set_xlabel('Target Voltage (V)')
        axes[1,2].set_ylabel('Average Voltage Error (%)')
        axes[1,2].set_title('Average Error vs Target Voltage')
        axes[1,2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # 保存图表
        plot_file = f'BASE4_Filter_Test_Plot_{timestamp}.png'
        plt.savefig(plot_file, dpi=300, bbox_inches='tight')
        print(f"图表已保存到: {plot_file}")
        
        # 显示图表
        plt.show()
        
    except Exception as e:
        print(f"绘图失败: {e}")

if __name__ == "__main__":
    perform_base4_filter_test()
