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

我现在需要在一个对简单信号输入ADC采样得到的数组，对他进行时域上的操作，根据：Reference\时域参考\index.md中的指引检测输入信号，注意其中提到的，计算有效值时，待测波形的位置必须关于时间轴对称，所以在计算前要把所有波平移到关于时间轴对称的位置。所以需要你滤除直流分量，待测波形的离散点必须形成整数个完整的周期，也就是说，需要从ADC采集的点中取出一个或整数个完整周期的点，并且我还在my_adc_task.c做好了如何使用这个模块的示例，设计好了一部分接口，之后你按照我的规范工作流：模块实现的流程规范，第一遍是根据我的需求，查阅系统相关接口，明确一个设计文档（文档包括模块的基本原理、拟定设定的数据结构和函数接口，怎么从目前系统中获取数据、数据处理的流动）第二遍是生成完上述步骤之后，实现具体的模块代码，等待用户编译检测通过之后，第三遍是阅读系统中该模块需要的数据是怎么获得的，根据系统中的数据流动，设计一个兼容原先数据流动的条件分支，来在这个分支进行模块测试，如果模块效果不理想，可以在代码中通过简单的标志位弃用模块，同时不改变系统中原有的数据流动和所有功能。

dds

正弦波的峰峰值以及正弦波的边沿（上升沿+下降沿），我调研了主流方案：Reference\正弦波时域峰值+边沿检测\test.md，以及时域处理的基本方法：Reference\正弦波时域峰值+边沿检测\时域参考，请你在我的模块文件夹Module\TimeDetect\my_time_detect.h实现，

我在测试中发现串口输出显示是：Time Detect Module Initialized
Time domain detection failed! 我觉得就是 if (my_time_detect_start(g_adc1_data_8bit, &result) == 0)  失败了


我想要在工程中，使用uint32_t g_dds_frequency = 1000; // 默认DDS频率为1kHz
uint32_t g_desired_dds_frequency = 1000; // 用户期望设置的DDS频率（相关变量在 Module/Button/my_button_task.c 中定义），然后在 my_uart_task.c 中实现一个解析功能，能够通过串口接收用户输入的字符串，解析出是设置ADC采样率还是DDS频率，并且设置对应的变量。

这两个变量来通过串口接收用户想要设置的DDS频率，并且进行配置，类似于我已经有的g_desired_ADC_sample_rate_Hz，但是在系统中，我已经有了ADC设置频率的功能，为了不把用户频率设定的是ADC还是DDS搞混淆，我想要到可以让用户发送A100000来表示设置ADC采样率为100000Hz，发送D100000来表示设置DDS频率为100000Hz，这样就可以通过串口接收用户的输入，解析出是设置ADC还是DDS的频率，并且设置对应的变量。请你在 my_uart_task.c 中实现这个解析功能。


先用 FFT 算一个输入信号的频率
然后根据这个频率动态调整一下采样率
调整完之后再精细测量



我现在想要在 my_uart_task 实现一个利用上位机按照指定协议发送数据给 MCU，然后 MCU 解析对应数据并调整程序中参数的一套解析系统，我现在需要控制系统中如下的全局变量：
- g_desired_dds_frequency：设置 DDS 的期望频率
- g_desired_ADC_sample_rate_Hz ：设置 ADC 的期望采样率
- g_desired_dds_type：设置 DDS 的期望类型（例如 AD9833、AD9954等）
- g_desired_dds_phase：设置 DDS 的期望相位
- g_desired_dds_amplitude：设置 DDS 的期望幅度
- g_desired_switch2which_relay：设置目前的按键切换到控制哪个继电器
- g_desired_function_state (该值为 LCR_state )
    - LCR_state -> 此时 ADC_mode 为 Normal （Sweep模式还在测试中，不考虑使用串口使能）是目前系统中的 LCR 表测量功能
    - Spectrum_state -> 此时 ADC_mode 为 Normal，不过串口打印各个ADC通道的频谱，目前系统迭代成 LCR 表测量，但是该部分的功能实现在 my_frequency_config 中的 my_armcfft32_apply 中有相应功能注释，不过当时为了节约资源，没有把中间的缓冲数组单独保存，所以ADC1的数据算完，ADC2的会覆盖ADC1的频谱数据，需要你重新开几个对应全局变量，保存每个通道的频谱数据
    - Time_state -> 此时 ADC_mode 为 Normal，不过串口打印各个ADC通道的时域数据，目前系统中的状态好像有对应通道的全局数组，不过加窗和经过FIR的处理也是在 my_frequency_config 中的 my_armcfft32_apply 中实现的，注意该部分的功能实现在 my_frequency_config 中的 my_armcfft32_apply 中有相应功能注释，不过当时为了节约资源，没有把中间的缓冲数组单独保存
    - diy_state -> 此时 ADC_mode 为 Normal，测试开发中的其他功能（保留和其他功能的一致性，在功能开发测试完成后，用户只用把该值从 diy_state 改为新功能即可，再自行开辟一个 diy_state）




Done
- PC1 按键触发定时器驱动 ADC
- 运行时修改 ADC 采样率
    - 我现在想要每次按下按键之后切换ADC采样率，ADC是使用定时器触发的，简而言之就是要修改htim3的各种参数，我觉得可以通过htim3的句柄实现，但是这种运行时修改是否需要重新init timer还是什么，请你查阅工程中的hal timer api 进行解答

定时器修改 DAC 输出波形的频率
