# STM32-accelerator

[FPGA_MCU_SPI_COM: 基于SPI的FPGA与MCU简易通讯，以EP4CE15与STM32F407为例 (gitee.com)](https://gitee.com/themql/FPGA_MCU_SPI_COM)

[STM32H7 ADC交替采样实现8M采样率(14bit) 与高速USB（VCP）传输 - STM32/STM8单片机论坛 - ST MCU意法半导体官方技术支持论坛 (21ic.com)](https://bbs.21ic.com/icview-3409296-1-1.html)

TODO:
- 信号失真度测量
- 信号测量（基波、谐波的幅度、相位等）
- 阻抗测量


在我reset开启系统后（其中系统一直接入信号发生器，发生器一直有输出），output频谱函数在一开始会输出好多0，但是之后的频谱没有输出那么多0，是否是因为DMA搬运了很多0导致的呢，但是我的初始化代码在 adc_processing_task.c中已经设置了先开定时器，再开dma，应该在初始几个缓存区不会搬运0，是否是FFT还没计算完呢，可以检查一下是否是当时FFT在计算中导致的。


在 adc_processing_task 任务中实现
- 先采样一个缓存区（16k）的ADCdata
- 然后停止采集
- 输出缓存区中的 ADCdata 作为时域波形
- 对这个缓存区分 4 段，每段 4k 进行FFT处理
- 计算每段的幅度谱依次输出

可能需要修改一下相关的接口，比如 ProcessCompleteBuffer 以及 buffer_fill_count++ 等，实现更加灵活的 ADC 采样和