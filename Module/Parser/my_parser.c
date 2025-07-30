#include "my_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "AD9833.h"
#include "AD9954.h"
#include "my_dac_config.h"

extern uint16_t g_dac_sine[256];
extern TIM_HandleTypeDef htim4;
extern DAC_HandleTypeDef hdac1;

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
    HAL_TIM_Base_Start(&htim4);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)g_dac_sine, 256, DAC_ALIGN_12B_R);
    

    // Frequency scanning from 1kHz to 50kHz with 200Hz step
    double frequency;
    for (frequency = 1000.0; frequency <= 50000.0; frequency += 200.0) {
        // Set frequency for AD9833
        AD9833_WaveSeting(frequency, 0, SIN_WAVE, 0); // Set a sine wave with no phase shift
        // Set frequency for AD9954
        AD9954_Set_Fre(frequency);

        printf("desired freq: %f", frequency);
        
        osDelay(2000); // Delay for 2 seconds between steps
    }
    
    // After scanning, keep the last frequency
    while (1)
    {
        osDelay(1000); // Delay for 1 second
    }
}