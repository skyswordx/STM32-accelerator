#include "my_ina22x_task.h"
#include "INA226.h"
#include "stm32h7xx_hal.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "my_button_task.h" // 引入按键任务头文件
#include "main.h"

void StartINA22XProcessingTask(void *argument) {
    // 初始化INA226设备
    
    for (;;) {
        // 读取电流和电压数据
        // 延时一段时间
        osDelay(1000); // 每秒读取一次
    }
}