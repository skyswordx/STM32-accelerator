
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