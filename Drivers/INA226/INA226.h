#ifndef INA226_H_
#define INA226_H_

#include "stm32h7xx_hal.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

void INA226_init(void);
//仅使用了获取电压电流功率3个功能
float INA226_GetBusV(void);
float INA226_GetCurrent(void);
float INA226_GetPower(void);

#endif /* INA226_H_ */