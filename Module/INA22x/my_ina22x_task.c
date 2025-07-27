#include "my_ina22x_task.h"
#include "INA226.h"

void StartINA22XProcessingTask(void *argument) {
    // 初始化INA226设备
    INA226_init();
    
    for (;;) {
        // 读取电流和电压数据
        float current = INA226_GetBusV();
        float voltage = INA226_GetCurrent();
        float power = INA226_GetPower();
        
        // 处理数据（例如发送到UART或存储）
        printf("Current: %.2f A, Voltage: %.2f V, Power: %.2f W\n", current, voltage, power);
        
        // 延时一段时间
        osDelay(1000); // 每秒读取一次
    }
}