/**********************************************************
                       康威电子
功能：AD9954 HAL驱动测试版本
接口：测试用完整驱动
时间：2023/06/xx
版本：4.3 - 重构为HAL库版本
作者：康威电子
**********************************************************/

#include "main.h"
#include "AD9954.h"

// 如果系统中没有定义这些，则提供简单的实现
#ifndef __NOP
#define __NOP() __asm volatile ("nop")
#endif

extern uint32_t SystemCoreClock;

/**
 * @brief 基于循环的微秒延时函数
 * @param us 延时时间（微秒）
 */
void delay_us(uint32_t us)
{
    uint32_t count = us * (SystemCoreClock / 1000000 / 4);
    while(count--) {
        __NOP();
    }
}

/**
 * @brief 基于SysTick的高精度微秒延时函数
 * @param us 延时时间（微秒）
 */
void delay_us_systick(uint32_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t told = SysTick->VAL;
    uint32_t tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    
    while (1) {
        uint32_t tnow = SysTick->VAL;
        
        if (tnow != told) {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            
            if (tcnt >= ticks) {
                break;
            }
        }
    }
}
