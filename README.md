# STM32-accelerator

[FPGA_MCU_SPI_COM: 基于SPI的FPGA与MCU简易通讯，以EP4CE15与STM32F407为例 (gitee.com)](https://gitee.com/themql/FPGA_MCU_SPI_COM)

[STM32H7 ADC交替采样实现8M采样率(14bit) 与高速USB（VCP）传输 - STM32/STM8单片机论坛 - ST MCU意法半导体官方技术支持论坛 (21ic.com)](https://bbs.21ic.com/icview-3409296-1-1.html)

TODO:
- 信号失真度测量
- 信号测量（基波、谐波的幅度、相位等）
    - [x] FFT 得到幅度谱和相位谱
    - [ ] 打表幅度拟合
    - [ ] 相位拟合
- 阻抗测量
- 上电通过基准源校准
- 校准温漂


先用 FFT 算一个输入信号的频率
然后根据这个频率动态调整一下采样率
调整完之后再精细测量


Done
- PC1 按键触发定时器驱动 ADC
- 运行时修改 ADC 采样率
    - 我现在想要每次按下按键之后切换ADC采样率，ADC是使用定时器触发的，简而言之就是要修改htim3的各种参数，我觉得可以通过htim3的句柄实现，但是这种运行时修改是否需要重新init timer还是什么，请你查阅工程中的hal timer api 进行解答

定时器修改 DAC 输出波形的频率


目前系统的初始采样率 200k 
adc 采样和进行 fft 的点数都是 4096 个点
因此目前 FFT 的 LSB 为 200000 / 4096 = 48.828125 Hz

我需要满足如下几个条件
- 4096个点内，至少有2个周期
- 每一个周期内至少有 16 个采样点
- 信号的带宽给定是 1k 到 100k

我目前的想法是先根据初始采样率测得一个粗略的信号频率，再根据这个
