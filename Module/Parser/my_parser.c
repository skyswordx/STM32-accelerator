#include "my_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "AD9833.h"
#include "AD9954.h"
#include "my_dac_config.h"
#include "my_timer_config.h" 


extern uint16_t g_dac_sine[256];
extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;

#define FREQ_MAX 1000000.0f


void my_parser_task(void const * argument)
{
    // Initialize the parser
    AD9833_Init_GPIO();
    AD9954_Init(); // Initialize AD9954
    // Set amplitude to 2V for AD9833 using the voltage conversion macro
    AD9833_AmpSet(255);
    
    // Set amplitude to maximum for AD9954 (max value is 16383)
    AD9954_Set_Amp(16383);
    AD9954_Set_Phase(0);//写相位

    // DAC
    // HAL_TIM_Base_Start(&htim4);
    // HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_sine, 256, DAC_ALIGN_12B_R);
    

    // Frequency scanning from 1kHz to 50kHz with 200Hz step in a continuous loop
    double frequency = 1000.0;  // Start frequency
    int direction = 1;  // 1 for increasing, -1 for decreasing
    
    while (1) {
        // 停止timer4
        // HAL_TIM_Base_Stop(&htim4);
        
        // 设置timer4的新频率
        // 会联动改动 ADC 采样率
        // switch_timer_sampleRate_Auto(&htim4, (uint32_t)frequency, (uint32_t)frequency);
   
        // 启动timer4
        // HAL_TIM_Base_Start(&htim4);
        
        // Set frequency for AD9833
        AD9833_WaveSeting(frequency, 0, SIN_WAVE, 0); // Set a sine wave with no phase shift
        // Set frequency for AD9954
        AD9954_Set_Fre(frequency);

        printf("desired freq: %f\n", frequency);
        
        // Update frequency for next iteration
        if (direction == 1) {
            // Increasing frequency
            frequency += 200.0;
            if (frequency > FREQ_MAX) {
                frequency = FREQ_MAX;  // Cap at 50kHz
                direction = -1;       // Change direction to decreasing
            }
        } else {
            // Decreasing frequency
            frequency -= 200.0;
            if (frequency < 1000.0) {
                frequency = 1000.0;   // Cap at 1kHz
                direction = 1;        // Change direction to increasing
            }
        }
        
        osDelay(5000); // Delay for 2 seconds between steps
    }
}