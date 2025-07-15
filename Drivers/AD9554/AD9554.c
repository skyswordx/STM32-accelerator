#include "AD9554.h"

/*==============================================================================
 * 私有函数声明
 *============================================================================*/
static void AD9554_WriteRegister(uint8_t reg_addr, uint8_t *data, uint8_t length);
static void AD9554_ReadRegister(uint8_t reg_addr, uint8_t *data, uint8_t length);

/*==============================================================================
 * 全局变量
 *============================================================================*/
static uint8_t g_ad9554_initialized = 0;

/*==============================================================================
 * AD9554 初始化
 *============================================================================*/
void AD9554_Init(void)
{
    // 注意：GPIO初始化应由CubeMX生成，用户需要在CubeMX中配置对应的引脚
    // 或者手动配置AD9554.h中定义的引脚
    
    // AD9554硬件复位
    AD9554_Reset();
    
    // 延时等待复位完成
    AD9554_DelayMs(100);
    
    // 配置CFR1寄存器
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(AD9554_REG_CFR1);
    AD9554_SPI_WriteByte(0x02);
    AD9554_SPI_WriteByte(0x10);
    AD9554_SPI_WriteByte(0x00);
    AD9554_SPI_WriteByte(0x40);  // 比较器power down
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    
    // 配置CFR2寄存器
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(AD9554_REG_CFR2);
    AD9554_SPI_WriteByte(0x00);
    AD9554_SPI_WriteByte(0x08);
    
    // 根据系统频率配置PLL
    #if AD9554_SYSCLK_MHZ > 400
        #error "系统频率超过芯片最大值400MHz"
    #endif
    
    #if AD9554_SYSCLK_MHZ >= 250
        AD9554_SPI_WriteByte((AD9554_PLL_MULTIPLIER << 3) | 0x04 | 0x03);
    #else
        AD9554_SPI_WriteByte(AD9554_PLL_MULTIPLIER << 3);
    #endif
    
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    
    // 设置初始状态
    HAL_GPIO_WritePin(AD9554_OSK_PORT, AD9554_OSK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_PWR_PORT, AD9554_PWR_PIN, GPIO_PIN_RESET);
    
    g_ad9554_initialized = 1;
}

/*==============================================================================
 * AD9554 软件SPI字节写入
 *============================================================================*/
void AD9554_SPI_WriteByte(uint8_t data)
{
    uint8_t i;
    
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_RESET);
        
        if (data & 0x80)
        {
            HAL_GPIO_WritePin(AD9554_SPI_SDIO_PORT, AD9554_SPI_SDIO_PIN, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(AD9554_SPI_SDIO_PORT, AD9554_SPI_SDIO_PIN, GPIO_PIN_RESET);
        }
        
        // 短暂延时确保数据稳定
        AD9554_DelayUs(1);
        
        HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_SET);
        AD9554_DelayUs(1);
        
        data <<= 1;
    }
    
    HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_RESET);
}

/*==============================================================================
 * AD9554 软件SPI字节读取
 *============================================================================*/
uint8_t AD9554_SPI_ReadByte(void)
{
    uint8_t i, data = 0;
    
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_RESET);
        AD9554_DelayUs(1);
        
        data <<= 1;
        if (HAL_GPIO_ReadPin(AD9554_SPI_SDO_PORT, AD9554_SPI_SDO_PIN))
        {
            data |= 0x01;
        }
        
        HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_SET);
        AD9554_DelayUs(1);
    }
    
    HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_RESET);
    return data;
}

/*==============================================================================
 * AD9554 硬件复位
 *============================================================================*/
void AD9554_Reset(void)
{
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(AD9554_RES_PORT, AD9554_RES_PIN, GPIO_PIN_RESET);
    AD9554_DelayMs(10);
    HAL_GPIO_WritePin(AD9554_RES_PORT, AD9554_RES_PIN, GPIO_PIN_SET);
    AD9554_DelayMs(10);
    HAL_GPIO_WritePin(AD9554_RES_PORT, AD9554_RES_PIN, GPIO_PIN_RESET);
    AD9554_DelayMs(10);
    
    // 复位后初始化SPI引脚状态
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(AD9554_SPI_SCLK_PORT, AD9554_SPI_SCLK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9554_IOUPDATE_PORT, AD9554_IOUPDATE_PIN, GPIO_PIN_RESET);
}

/*==============================================================================
 * AD9554 I/O更新
 *============================================================================*/
void AD9554_IOUpdate(void)
{
    HAL_GPIO_WritePin(AD9554_IOUPDATE_PORT, AD9554_IOUPDATE_PIN, GPIO_PIN_RESET);
    AD9554_DelayUs(1);
    HAL_GPIO_WritePin(AD9554_IOUPDATE_PORT, AD9554_IOUPDATE_PIN, GPIO_PIN_SET);
    AD9554_DelayUs(2);
    HAL_GPIO_WritePin(AD9554_IOUPDATE_PORT, AD9554_IOUPDATE_PIN, GPIO_PIN_RESET);
}

/*==============================================================================
 * AD9554 设置频率
 *============================================================================*/
void AD9554_SetFrequency(double frequency_hz)
{
    uint32_t ftw;
    
    if (!g_ad9554_initialized)
    {
        return;
    }
    
    // 计算频率调节字
    ftw = AD9554_FrequencyToFTW(frequency_hz);
    
    // 写入FTW0寄存器
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(AD9554_REG_FTW0);
    AD9554_SPI_WriteByte((uint8_t)(ftw >> 24));
    AD9554_SPI_WriteByte((uint8_t)(ftw >> 16));
    AD9554_SPI_WriteByte((uint8_t)(ftw >> 8));
    AD9554_SPI_WriteByte((uint8_t)(ftw));
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    
    // 更新输出
    AD9554_IOUpdate();
}

/*==============================================================================
 * AD9554 设置幅度
 *============================================================================*/
void AD9554_SetAmplitude(uint16_t amplitude)
{
    if (!g_ad9554_initialized)
    {
        return;
    }
    
    // 限制幅度范围 (0-0x3FFF)
    if (amplitude > 0x3FFF)
    {
        amplitude = 0x3FFF;
    }
    
    // 写入ASF寄存器
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(AD9554_REG_ASF);
    AD9554_SPI_WriteByte((uint8_t)(amplitude >> 8));
    AD9554_SPI_WriteByte((uint8_t)(amplitude));
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    
    // 更新输出
    AD9554_IOUpdate();
}

/*==============================================================================
 * AD9554 设置相位
 *============================================================================*/
void AD9554_SetPhase(uint16_t phase)
{
    if (!g_ad9554_initialized)
    {
        return;
    }
    
    // 写入POW0寄存器
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(AD9554_REG_POW0);
    AD9554_SPI_WriteByte((uint8_t)(phase >> 8));
    AD9554_SPI_WriteByte((uint8_t)(phase));
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
    
    // 更新输出
    AD9554_IOUpdate();
}

/*==============================================================================
 * AD9554 设置扫描模式
 *============================================================================*/
void AD9554_SetScanMode(AD9554_ScanMode_t mode)
{
    if (!g_ad9554_initialized)
    {
        return;
    }
    
    // 根据扫描模式设置PS0和PS1
    switch (mode)
    {
        case AD9554_SCAN_DOWN:
            HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_RESET);
            break;
            
        case AD9554_SCAN_UP:
            HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_RESET);
            break;
            
        case AD9554_SCAN_DOUBLE:
            HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_SET);
            break;
            
        default:
            HAL_GPIO_WritePin(AD9554_PS0_PORT, AD9554_PS0_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AD9554_PS1_PORT, AD9554_PS1_PIN, GPIO_PIN_RESET);
            break;
    }
}

/*==============================================================================
 * 频率转换为FTW (频率调节字)
 *============================================================================*/
uint32_t AD9554_FrequencyToFTW(double frequency_hz)
{
    // FTW = (频率 / 系统时钟) * 2^32
    // 考虑到浮点精度，使用预计算的系数
    return (uint32_t)(frequency_hz * AD9554_FTW_MULTIPLIER);
}

/*==============================================================================
 * FTW转换为频率
 *============================================================================*/
double AD9554_FTWToFrequency(uint32_t ftw)
{
    // 频率 = (FTW * 系统时钟) / 2^32
    return (double)ftw / AD9554_FTW_MULTIPLIER;
}

/*==============================================================================
 * 寄存器写入 (私有函数)
 *============================================================================*/
static void AD9554_WriteRegister(uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    uint8_t i;
    
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(reg_addr);
    
    for (i = 0; i < length; i++)
    {
        AD9554_SPI_WriteByte(data[i]);
    }
    
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
}

/*==============================================================================
 * 寄存器读取 (私有函数)
 *============================================================================*/
static void AD9554_ReadRegister(uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    uint8_t i;
    
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_RESET);
    AD9554_SPI_WriteByte(reg_addr | 0x80);  // 读取操作需要设置最高位
    
    for (i = 0; i < length; i++)
    {
        data[i] = AD9554_SPI_ReadByte();
    }
    
    HAL_GPIO_WritePin(AD9554_SPI_CS_PORT, AD9554_SPI_CS_PIN, GPIO_PIN_SET);
}

/*==============================================================================
 * 弱定义的延时函数 (用户可以重新定义)
 *============================================================================*/
__weak void AD9554_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

__weak void AD9554_DelayUs(uint32_t us)
{
    // 简单的微秒延时实现
    volatile uint32_t count = us * (SystemCoreClock / 1000000U) / 10U;
    while (count--)
    {
        __NOP();
    }
}

