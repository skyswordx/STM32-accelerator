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


请你阅读一下Drivers\AD5933\zhengdian-AD5933的工程，这个是使用stm32标准库的iic-ad5933驱动，请你结合 stm32 hal 库的 api ，把其中的标准库iic-api都替换成hal的，以便我在hal工程中使用

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

长按逻辑

下面是我们目前的按键映射表和按键检测变量
```
// 按键映射表
static const uint8_t key_map[KEYPAD_NUM_ROWS][KEYPAD_NUM_COLS] = {
    {3, 2(所有DDS幅度增加), 1},
    {6(所有DDS频率增加), 5, 4(所有DDS频率减小)},
    {9, 8(所有DDS幅度减小), 7}
};

// 按键检测变量
uint8_t g_short_pressed_key = NO_KEY_PRESSED;
uint8_t g_long_pressed_key = NO_KEY_PRESSED;
```
为了应对测评中，不能使用电脑和串口助手的情况下，我想要首先利用矩阵键盘进行仪器的操作

要求第一次按下按键1，就进入base2_function处理base2_function的逻辑
第一次按下按键2，就进入base3_function处理base3_function的逻辑
第一次按下按键3，就进入base4_function处理base4_function的逻辑

第二次按下的意义则根据进入不同的函数会有不一样的逻辑，请你在 my_parser实现这个框架


Done
- PC1 按键触发定时器驱动 ADC
- 运行时修改 ADC 采样率
    - 我现在想要每次按下按键之后切换ADC采样率，ADC是使用定时器触发的，简而言之就是要修改htim3的各种参数，我觉得可以通过htim3的句柄实现，但是这种运行时修改是否需要重新init timer还是什么，请你查阅工程中的hal timer api 进行解答

定时器修改 DAC 输出波形的频率


复现通过串口发送数据让 MCU 解析的功能
我是这么设想的：
- 第一次按下前的编号，就代表了串口解析格式`xx:yy:cc`中的 xx 字段（cmd_type），
    - 例如第一次按下的 1 键值代表设置参数命令，就是类似于发送串口解析中的 SET 字段，
    - 例如第一次按下的 2 键值代表读取参数命令，就是类似于发送串口解析中的 GET 字段，
    - 1~9 键都是合法的（不过等待后续自己diy，全部归类到switch的default中）
- 第二次按下前的编号，就代表了串口解析格式`xx:yy:cc`中的 yy 字段（param），
    - 例如第二次按下的 1 键值代表设置 ADC 采样率，就是类似于发送串口解析中的 ADC_RATE 字段，
    - 例如第二次按下的 2 键值代表设置 DDS 频率，就是类似于发送串口解析中的 DDS_FREQ 字段，
    - 1~9 键都是合法的（不过剩余的参数等待后续自己diy，全部归类到switch的default中）
- 第三次按下前的值，就代表了串口解析格式`xx:yy:cc`中的 cc 字段（value）
    - 例如第三次按下的1 ~ 9 可以对应程序解析中的对应 value 字段
    - 涉及到要为程序中变量赋值数字时，此时按需要长按按键 1，识别成功长按后，后续的按键输入被视为数字，并且按照用户输入的逻辑解析数字大小
        - 例如长按 1 键后，后续先后按下 2、3、4键，被视为数字 234，再次长按 1 键进行确认输入结束
- 这一套过程如果有任何按键错误，可以选择通过长按按键 9 进程取消输入，重新开始检测第一次按下

请你参考 my_uart_task.c 中的串口解析函数修改 my_button_task 按键解析系统，这个串口解析经过修改可以应对get:all命令整个命令只有两个字段的情形，而按键部分却没办法应对，请你修改并完善一下按键处理get:all等两个字段长度的命令

串口屏上电之后，默认在基础功能 2 页面，按下屏幕上的开始测试之后，串口屏会发送“S2”命令，表示开始测试，MCU 接收到该命令后会进入 base2_function 模式

在点击下一页之后，串口屏进入基础功能 3 页面，此时会发送“I3”，按下屏幕上的开始测试之后，串口屏会发送“S3”命令，表示开始测试，MCU 接收到该命令后会进入 base3_function 模式

在点击下一页之后，串口屏进入基础功能 4 页面，此时会发送“I4”，按下屏幕上的开始测试之后，串口屏会发送“S4”命令，表示开始测试，MCU 接收到该命令后会进入 base4_function 模式

从基础功能 4 页面点击上一页，串口屏会进入基础功能 3 页面，此时会发送“I3”，按下屏幕上的开始测试之后，串口屏会发送“S3”命令，表示开始测试，MCU 接收到该命令后会进入 base3_function 模式

请你在 my_uart_task.c 中的void parse_serial_lcd_command(char* cmd)实现这个串口屏的命令解析功能，能够根据串口屏发送的命令进入对应的 base2_function、base3_function 或 base4_function 模式



我在cubemx中设置了一个合适的timer4触发频率，我只需要在代码中使用 `HAL_TIM_Base_Start(&htim4);` 就可以启动定时器

如果我需要使用DMA去把一共数组搬运到DAC让DAC输出波形，我需要使用 `HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_out_array, length_of_array, DAC_ALIGN_12B_R);` 来启动DMA传输

如果我想要停止定时器触发和DAC的DMA搬运，我需要使用 `HAL_TIM_Base_Stop(&htim4);` 来停止定时器，然后使用 `HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);` 来停止DAC的DMA传输

再次启动定时器和DAC的DMA传输，我只需要再次调用 `HAL_TIM_Base_Start(&htim4);` 和 `HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_out_array, length_of_array, DAC_ALIGN_12B_R);` 即可。

这是我目前stm32h750系统中使用到DAC任务的接口
```c
void StartDACProcessingTask(void *argument) {
    // 启动定时器
    HAL_TIM_Base_Start(&htim4);
    
    // 首次生成波形数据
    update_dac_waveform_by_parameters();
    
    // 启动DAC DMA传输
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_square_64, 50, DAC_ALIGN_12B_R);

    // 设置DAC通道2的固定电压输出
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, VOLTAGE_TO_DAC_VALUE(g_desired_DAC_single_output_amplitude));
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2); // 启动DAC通道2
    
    for(;;)
    {
        
        osDelay(100); // 延时100毫秒
    }
}
```

我在cubemx中设置了一个合适的timer3触发频率，默认以2MHz的频率去触发双ADC同步采样（ADC1、ADC2），我只需要在代码中使用 `HAL_TIM_Base_Start(&htim3);` 就可以启动定时器

在第一次使用ADC之前，我需要初始化adc_dmabuffer内存空间（由于 Dcache存在，所以需要对齐）
```c
memset(g_adc_dma_buffer, 0, ADC_SAMPLE_SIZE * sizeof(uint16_t));
SCB_CleanDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));
```

并且我还需要校准ADC
```c
    /* 【ADC 数据流】校准ADC 勿动 */
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
```

并且是双ADC同步采样，在第一次使用ADC前，我需要按照如下的接口和顺序启动 ADC
```c
 /* 【ADC 数据流】初始化同步采样的 ADC 模式 */
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE);

```

然后我想要获取数据时，我就用`HAL_TIM_Base_Start(&htim3);`启动定时器，触发ADC工作，如果ADC DMA 传输完成，我就可以通过传输完成的中断，关闭定时器防止当前数据没处理完就被下一次数据冲刷，并且设置 `g_adc_dma_buffer` 获取采集完成的标记
```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance == ADC1)
  {
    /* ADC DMA 传输完成之后会进入这里 */
    HAL_TIM_Base_Stop(&htim3); // 停止定时器，停止ADC触发
    g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_COMPLETED;
  }
}

// 【ADC 数据流】所有的 ADC 数据处理都得等待 DMA 传输完成标志位（DMA 传输完成的中断会停止 ADC 并设置标志位）
        if (g_adc_dma_transfer_flag == ADC_DMA_TRANSFER_COMPLETED) {
            g_adc_dma_transfer_flag = ADC_DMA_TRANSFER_NOT_COMPLETED; // 重置标志
            
            /* Dcache 缓存一致性处理 */
            SCB_InvalidateDCache_by_Addr((uint32_t*)g_adc_dma_buffer, ADC_SAMPLE_SIZE * sizeof(uint16_t));
            
            /* 提取 ADC 数据 */
            for (uint32_t i = 0; i < ADC_SAMPLE_SIZE; i++) {
                // debug1[i] = (uint16_t)(g_adc_dma_buffer[i] & g_and_mask); // ADC1数据
                // debug2[i] = (uint16_t)((g_adc_dma_buffer[i] >> g_right_shift) & g_and_mask); // ADC2数据

                g_adc1_data_8bit[i] = (float32_t)((g_adc_dma_buffer[i] & g_and_mask) * ADC_REF_VOLTAGE / ADC_RESOLUTION_FACTOR); // ADC1数据
                g_adc2_data_8bit[i] = (float32_t)(((g_adc_dma_buffer[i] >> g_right_shift) & g_and_mask) * ADC_REF_VOLTAGE / ADC_RESOLUTION_FACTOR); // ADC2数据
                // printf("ADC1/2:%.3f, %.3f, %lu\n", g_adc1_data_8bit[i], g_adc2_data_8bit[i], g_ADC_SAMPLE_RATE_Hz);
            }
```



如果我想要停止定时器触发和DAC的DMA搬运，我需要使用 `HAL_TIM_Base_Stop(&htim4);` 来停止定时器，然后使用 `HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);` 来停止DAC的DMA传输

再次启动定时器和DAC的DMA传输，我只需要再次调用 `HAL_TIM_Base_Start(&htim4);` 和 `HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_out_array, length_of_array, DAC_ALIGN_12B_R);` 即可。

这是我目前stm32h750系统中使用到ADC任务的接口
```c
void StartADCProcessingTask(void *argument) {
    
    
    for(;;)
    {
        
        osDelay(100); // 延时100毫秒
    }
}


发挥2流程整理：
1. 探究装置的输出接入未知模型电路的输入，未知模型输出接探究装置输入，按下“一键学习”后，开始学习建模。以下为学习建模流程：
1.1 继电器切换到AD9833/AD9954，输出偏置为0，向未知模型电路输出不同频率，固定幅度的正弦波激励，ADC1对激励信号（探究装置输出）进行采样，同时ADC2对响应信号（探究装置输入）进行同步采样，计算并记录两个信号的幅相关系。
1.2 切换激励频率，重复上述步骤，记录各个频率下位置模型电路的幅、相特性表。
1.3 通过得到的特性表拟合二阶电路的传递函数。至此，学习建模完成。

2. 建模完成后，切换至推理生成模式，继电器切换至片内DAC输出。向探究装置输入正弦波、矩形波（占空比 10%~50%、步长 5%）和其他周期信号，探究装置能够输出与此前记录的传递函数对应的响应信号。
2.1 有两种可能方案：
方案一：系统先确认激励信号的周期，确认DDS缓冲区的相关配置，每隔一段时间重新开启ADC采样，并通过激励与传递函数计算单个周期的响应并刷新缓冲区，需要保证刷新缓冲区的信号相位一致。（软件DDS）
方案二：采用与传递函数等价的差分方程实时进行计算与输出，ADC需要实时进行采样（每输出一个DAC数据，ADC至少需要采样一次）。
2.2 DAC输出削去了全部直流量，需要通过输出偏置进行补偿。


我现在要测试一下

目前我