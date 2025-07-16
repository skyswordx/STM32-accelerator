#ifndef __AD9954_H
#define __AD9954_H	 
#include "main.h"
#include <stdint.h>

/* 
 * CubeMX GPIO 配置要求：
 * 
 * 输出引脚 (Push-Pull Output, GPIO_SPEED_FREQ_HIGH):
 * - PA12 -> PS1
 * - PA11 -> S_CS
 * - PA10 -> S_SCLK
 * - PA9  -> S_DIO
 * - PA8  -> AD9954_RES
 * - PC9  -> IOUPDATE
 * - PC8  -> AD9954_PWR
 * - PC7  -> AD9954_IOSY
 * - PC6  -> PS0
 * - PD13 -> AD9954_OSK
 * 
 * 输入引脚 (Input Pull-Up):
 * - PD14 -> S_SDO
 * 
 * 在CubeMX中配置时，请给每个引脚设置对应的User Label
 */

// 使用软件SPI驱动AD9954
#define AD9954_SOFTWARE_SPI

#ifdef AD9954_SOFTWARE_SPI
// GPIO端口定义
#define AD9954_CS_PORT      GPIOA
#define AD9954_CS_PIN       GPIO_PIN_11
#define AD9954_SCLK_PORT    GPIOA
#define AD9954_SCLK_PIN     GPIO_PIN_10
#define AD9954_SDIO_PORT    GPIOA
#define AD9954_SDIO_PIN     GPIO_PIN_9
#define AD9954_OSK_PORT     GPIOD
#define AD9954_OSK_PIN      GPIO_PIN_13
#define PS1_PORT            GPIOA
#define PS1_PIN             GPIO_PIN_12
#define PS0_PORT            GPIOC
#define PS0_PIN             GPIO_PIN_6
#define IOUPDATE_PORT       GPIOC
#define IOUPDATE_PIN        GPIO_PIN_9
#define AD9954_SDO_PORT     GPIOD
#define AD9954_SDO_PIN      GPIO_PIN_14
#define AD9954_IOSY_PORT    GPIOC
#define AD9954_IOSY_PIN     GPIO_PIN_7
#define AD9954_RES_PORT     GPIOA
#define AD9954_RES_PIN      GPIO_PIN_8
#define AD9954_PWR_PORT     GPIOC
#define AD9954_PWR_PIN      GPIO_PIN_8

// HAL API 宏定义
#define AD9954_CS(x)        HAL_GPIO_WritePin(AD9954_CS_PORT, AD9954_CS_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_SCLK(x)      HAL_GPIO_WritePin(AD9954_SCLK_PORT, AD9954_SCLK_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_SDIO(x)      HAL_GPIO_WritePin(AD9954_SDIO_PORT, AD9954_SDIO_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_OSK(x)       HAL_GPIO_WritePin(AD9954_OSK_PORT, AD9954_OSK_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define PS1(x)              HAL_GPIO_WritePin(PS1_PORT, PS1_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define PS0(x)              HAL_GPIO_WritePin(PS0_PORT, PS0_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define IOUPDATE(x)         HAL_GPIO_WritePin(IOUPDATE_PORT, IOUPDATE_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_IOSY(x)      HAL_GPIO_WritePin(AD9954_IOSY_PORT, AD9954_IOSY_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_RES(x)       HAL_GPIO_WritePin(AD9954_RES_PORT, AD9954_RES_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define AD9954_PWR(x)       HAL_GPIO_WritePin(AD9954_PWR_PORT, AD9954_PWR_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

#define AD9954_SDO          HAL_GPIO_ReadPin(AD9954_SDO_PORT, AD9954_SDO_PIN)

// 精确延时函数声明
void delay_us(uint32_t us);
void delay_us_systick(uint32_t us);
#endif


#define CFR1		0X00	//控制功能寄存器1
#define CFR2    0X01	//控制功能寄存器2
#define ASF     0X02	//振幅比例因子寄存器
#define ARR     0X03	//振幅斜坡速率寄存器

#define FTW0    0X04	//频率调谐字寄存器0
#define POW0    0X05	//相位偏移字寄存器
#define FTW1    0X06	//频率调谐字寄存器1

#define NLSCW   0X07	//下降扫描控制字寄存器
#define PLSCW   0X08	//上升扫描控制字寄存器

#define RSCW0   0X07	//
#define RSCW1   0X08	//
#define RSCW2   0X09	//
#define RSCW3   0X0A	//RAM段控制字寄存器
#define RAM     0X0B	//读取指令写入RAM签名寄存器数据

#define No_Dwell	0x04	//No_Dwell不停留，输出扫频到终止频率回到起始频率。
#define Dwell 		0x00	//Dwell停留，输出扫频到终止频率后保持在终止频率。


void AD9954_GPIO_Init(void);//初始化控制AD9954需要用到的IO口
void AD9954_RESET(void);		//复位AD9954
void UPDATE(void);					//产生一个更新信号，更新AD9954内部寄存器

void AD9954_Send_Byte(uint8_t dat);//向AD9954发送一个字节的内容
uint8_t AD9954_Read_Byte(void);			//读AD9954一个字节的内容
void AD9954_Write_nByte(uint8_t RegAddr,uint8_t *Data,uint8_t Len);//向AD9954指定的寄存器写数据
uint32_t AD9954_Read_nByte(uint8_t ReadAddr,uint8_t Len);					//读AD9954寄存器数据
uint32_t Get_FTW(double Real_fH);	//频率数据转换


void AD9954_Init(void);//初始化AD9954的管脚和内部寄存器的配置，
void AD9954_Set_Fre(double fre);//设置AD9954输出频率，点频
void AD9954_Set_Amp(uint16_t Ampli);//写幅度
void AD9954_Set_Phase(uint16_t Phase);//写相位

void AD9954_SetFSK(double f1,double f2,double f3,double f4,uint16_t Ampli);//AD9954的FSK参数设置
void AD9954_SetPSK(uint16_t Phase1,uint16_t Phase2,uint16_t Phase3,uint16_t Phase4,double fre,uint16_t Ampli);//AD9954的PSK参数设置
void AD9954_Set_LinearSweep(double Freq_Low,double Freq_High,double  UpStepFreq, uint8_t UpStepTime,double	DownStepFreq, uint8_t DownStepTime,uint8_t mode);//扫频参数设置

		

#endif

