
请你结合一下目前的系统，优化输出一下我的系统架构设计需求文档

目前系统主要的 ADC 数据流是在 my_adc_task.c 中，见其详细注释，请你认识当前系统的数据流，所有获取 ADC 数据的数据流要严格遵守：【ADC 数据流】所有的 ADC 数据处理都得等待 DMA 传输完成标志位（DMA 传输完成的中断会停止 ADC 并设置标志位）

我现在想要在 my_uart_task 实现一个利用上位机按照指定协议发送数据给 MCU，然后 MCU 解析对应数据并调整程序中参数的一套解析系统，我现在需要控制系统中如下的全局变量（有一些全局变量还未创建）：
- g_desired_dds_frequency：设置 DDS 的期望频率
- g_desired_ADC_sample_rate_Hz ：设置 ADC 的期望采样率
- g_desired_dds_type：设置 DDS 的期望类型（例如 AD9833、AD9954等）
- g_desired_dds_phase：设置 DDS 的期望相位
- g_desired_dds_amplitude：设置 DDS 的期望幅度
- g_desired_DAC_output_waveform：设置 DAC 输出的期望波形（例如正弦波、方波、三角波等）
- g_desired_DAC_output_frequency：设置 DAC 输出的期望频率
- g_desired_DAC_single_output_amplitude：设置 DAC 输出的期望值
- g_desired_switch2which_relay：设置目前的按键切换到控制哪个继电器
- g_desired_function_state (该值为 LCR_state )
    - LCR_state -> 此时 ADC_mode 为 Normal （Sweep模式还在测试中，不考虑使用串口使能）是目前系统中的 LCR 表测量功能
    - Spectrum_state -> 此时 ADC_mode 为 Normal，不过串口打印各个ADC通道的频谱，目前系统迭代成 LCR 表测量，但是该部分的功能实现在 my_frequency_config 中的 my_armcfft32_apply 中有相应功能注释，不过当时为了节约资源，没有把中间的缓冲数组单独保存，所以ADC1的数据算完，ADC2的会覆盖ADC1的频谱数据，需要你重新开几个对应全局变量，保存每个通道的频谱数据
    - Time_state -> 此时 ADC_mode 为 Normal，不过串口打印各个ADC通道的时域数据，目前系统中的状态好像有对应通道的全局数组，不过加窗和经过FIR的处理也是在 my_frequency_config 中的 my_armcfft32_apply 中实现的，注意该部分的功能实现在 my_frequency_config 中的 my_armcfft32_apply 中有相应功能注释，不过当时为了节约资源，没有把中间的缓冲数组单独保存
    - diy_state -> 此时 ADC_mode 为 Normal，测试开发中的其他功能（保留和其他功能的一致性，在功能开发测试完成后，用户只用把该值从 diy_state 改为新功能即可，再自行开辟一个 diy_state）
