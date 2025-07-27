# STM32-accelerator

[FPGA_MCU_SPI_COM: 基于SPI的FPGA与MCU简易通讯，以EP4CE15与STM32F407为例 (gitee.com)](https://gitee.com/themql/FPGA_MCU_SPI_COM)

[STM32H7 ADC交替采样实现8M采样率(14bit) 与高速USB（VCP）传输 - STM32/STM8单片机论坛 - ST MCU意法半导体官方技术支持论坛 (21ic.com)](https://bbs.21ic.com/icview-3409296-1-1.html)

DOne:
- 信号失真度测量
- DA 输出
    - [x] DAC 输出波形
    - [没做] DAC 输出波形的频率可调
    - [x] DDS 驱动（AD9833、AD9954等）
- 频域测量（基波、谐波的幅度、相位等）
    - [x] FFT 得到幅度谱和相位谱
    - [x] 打表幅度拟合
    - [x] 相位测量
    - [x] 频谱插值
    - [待验证] DILA
    - [待验证] Quinn
    - [待验证] CZT
- 阻抗测量
    - [x] LCR 计算、ESR、D、Q 值 
    - [待验证] 驱动 DDS 扫频
- 时域测量
    - [] 正弦波过零检测
    - [] 正弦波峰值检测
    
Todo:
- 上电通过基准源校准
- 校准温漂


dds

先用 FFT 算一个输入信号的频率
然后根据这个频率动态调整一下采样率
调整完之后再精细测量


Done
- PC1 按键触发定时器驱动 ADC
- 运行时修改 ADC 采样率
    - 我现在想要每次按下按键之后切换ADC采样率，ADC是使用定时器触发的，简而言之就是要修改htim3的各种参数，我觉得可以通过htim3的句柄实现，但是这种运行时修改是否需要重新init timer还是什么，请你查阅工程中的hal timer api 进行解答

定时器修改 DAC 输出波形的频率
