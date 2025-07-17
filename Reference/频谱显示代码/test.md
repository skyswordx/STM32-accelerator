```c
// 示例代码，实际的采样长度和ADC数组长度可能会有所不同

/* 全局变量定义 */
#define FFT_LENGTH 1024//采样长度
arm_cfft_radix4_instance_f32 scfft;//定义scfft结构体
float FFT_InputBuf[FFT_LENGTH*2];  //FFT输入数组
float FFT_OutputBuf[FFT_LENGTH];  //FFT输出数组
uint16_t AD_Value[1024] = {0};//存放ADC的值


//   HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);        //用来触发adc采样 
//   HAL_ADC_Start_DMA(&hadc1, (uint32_t *)AD_Value, FFT_LENGTH);//开启ADC
/* 假设AD_Value数组已经被填充了ADC采样数据 */

arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH,0,1);
HAL_Delay(1000);//稍微等待一会ADC一轮转换结束

    for(int i=0; i < FFT_LENGTH; i++)
    {
      FFT_InputBuf[2*i]=AD_Value[i]*3.3/4096; //实部
      FFT_InputBuf[2*i+1]=0;          //虚部
    }
    arm_cfft_radix4_f32(&scfft,FFT_InputBuf);  
    //arm_cmplx_mag_f32(FFT_InputBuf,FFT_OutputBuf,FFT_LENGTH);  //取模得幅值
    for(int i = 0;i<FFT_LENGTH;i++)
    {
      float32_t real = FFT_InputBuf[2 * i];
       float32_t imag = FFT_InputBuf[2 * i + 1];
        float32_t magnitude = sqrtf(real * real + imag * imag);
        
        // 打印每个频率分量的模值
        printf("Fre: %f \r\n", magnitude);
    }

/* 滤除直流分量 */
  for(int i = 0;i<FFT_LENGTH;i++)
  {
    sum+=AD_Value[i];
  }
  sum/=1024;
    for(int i=0; i < FFT_LENGTH; i++)
    {
      FFT_InputBuf[2*i]=(AD_Value[i]-sum)*3.3/4096; //实部
      FFT_InputBuf[2*i+1]=0;          //虚部
    }
```