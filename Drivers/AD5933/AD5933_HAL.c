#include "AD5933_HAL.h"

// AD5933配置结构体实例
AD5933_Config ad5933_config = {
    .hi2c = NULL,
    .device_addr = AD5933_ADDR,
    .clk_source = 0x00,      // 内部时钟
    .gain = 0x01,            // x1增益
    .output_range = 0x00,    // 2Vpp
    .start_freq = 30000,     // 30kHz
    .freq_increment = 100,   // 100Hz
    .num_increments = 10,    // 10个点
    .settling_cycles = 0x3f  // 默认稳定周期
};

/**
 * @brief 初始化AD5933
 * @param hi2c I2C句柄指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;
    
    if (hi2c == NULL) {
        return HAL_ERROR;
    }
    
    ad5933_config.hi2c = hi2c;
    
    // 测试I2C通信
    status = HAL_I2C_IsDeviceReady(hi2c, AD5933_ADDR, 3, 1000);
    if (status != HAL_OK) {
        return status;
    }
    
    // 执行复位
    status = AD5933_WriteByte(AD5933_REG_CONTROL_LOW, 0x10 | ad5933_config.clk_source);
    if (status != HAL_OK) return status;
    
    HAL_Delay(150); // 复位延时
    
    status = AD5933_WriteByte(AD5933_REG_CONTROL_LOW, 0x00 | ad5933_config.clk_source);
    if (status != HAL_OK) return status;
    
    return HAL_OK;
}

/**
 * @brief 写入单个字节到AD5933寄存器
 * @param reg_addr 寄存器地址
 * @param data 要写入的数据
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_WriteByte(uint8_t reg_addr, uint8_t data)
{
    if (ad5933_config.hi2c == NULL) {
        return HAL_ERROR;
    }
    
    // 使用HAL_I2C_Mem_Write更符合HAL库规范
    return HAL_I2C_Mem_Write(ad5933_config.hi2c, AD5933_ADDR, reg_addr,
                            I2C_MEMADD_SIZE_8BIT, &data, 1, 1000);
}

/**
 * @brief 从AD5933寄存器读取单个字节
 * @param reg_addr 寄存器地址
 * @param data 读取的数据指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_ReadByte(uint8_t reg_addr, uint8_t *data)
{
    if (ad5933_config.hi2c == NULL || data == NULL) {
        return HAL_ERROR;
    }
    
    // 使用HAL_I2C_Mem_Read更符合HAL库规范
    return HAL_I2C_Mem_Read(ad5933_config.hi2c, AD5933_ADDR, reg_addr, 
                           I2C_MEMADD_SIZE_8BIT, data, 1, 1000);
}

/**
 * @brief 写入16位数据到AD5933寄存器
 * @param reg_addr 起始寄存器地址
 * @param data 要写入的16位数据
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_WriteWord(uint8_t reg_addr, uint16_t data)
{
    HAL_StatusTypeDef status;
    
    // 写入高字节
    status = AD5933_WriteByte(reg_addr, (data >> 8) & 0xFF);
    if (status != HAL_OK) return status;
    
    // 写入低字节
    return AD5933_WriteByte(reg_addr + 1, data & 0xFF);
}

/**
 * @brief 从AD5933寄存器读取16位数据
 * @param reg_addr 起始寄存器地址
 * @param data 读取的16位数据指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_ReadWord(uint8_t reg_addr, uint16_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t high_byte, low_byte;
    
    if (data == NULL) {
        return HAL_ERROR;
    }
    
    // 读取高字节
    status = AD5933_ReadByte(reg_addr, &high_byte);
    if (status != HAL_OK) return status;
    
    // 读取低字节
    status = AD5933_ReadByte(reg_addr + 1, &low_byte);
    if (status != HAL_OK) return status;
    
    *data = ((uint16_t)high_byte << 8) | low_byte;
    
    return HAL_OK;
}

/**
 * @brief 初始化频率设置
 * @param freq_hz 起始频率 (Hz)
 * @param freq_increment_hz 频率增量 (Hz)
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_FreInit(float freq_hz, float freq_increment_hz)
{
    HAL_StatusTypeDef status;
    uint32_t start_freq_code, freq_inc_code;
    
    // 设置为待机模式
    status = AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, 
                             AD5933_CMD_STANDBY | ad5933_config.output_range | ad5933_config.gain);
    if (status != HAL_OK) return status;
    
    // 计算起始频率代码
    if (freq_hz != 0.0f) {
        start_freq_code = (uint32_t)(freq_hz * 4.0f * 134217728.0f / (float)AD5933_FREQ);
        ad5933_config.start_freq = (uint32_t)freq_hz;
        
        // 写入起始频率寄存器
        status = AD5933_WriteByte(AD5933_REG_START_FREQ_HIGH, (start_freq_code >> 16) & 0xFF);
        if (status != HAL_OK) return status;
        
        status = AD5933_WriteByte(AD5933_REG_START_FREQ_MID, (start_freq_code >> 8) & 0xFF);
        if (status != HAL_OK) return status;
        
        status = AD5933_WriteByte(AD5933_REG_START_FREQ_LOW, start_freq_code & 0xFF);
        if (status != HAL_OK) return status;
    }
    
    // 计算频率增量代码
    if (freq_increment_hz != 0.0f) {
        freq_inc_code = (uint32_t)(freq_increment_hz * 4.0f * 134217728.0f / (float)AD5933_FREQ);
        ad5933_config.freq_increment = (uint32_t)freq_increment_hz;
    } else {
        freq_inc_code = 0;
        ad5933_config.freq_increment = 0;
    }
    
    // 写入频率增量寄存器
    status = AD5933_WriteByte(AD5933_REG_FREQ_INC_HIGH, (freq_inc_code >> 16) & 0xFF);
    if (status != HAL_OK) return status;
    
    status = AD5933_WriteByte(AD5933_REG_FREQ_INC_MID, (freq_inc_code >> 8) & 0xFF);
    if (status != HAL_OK) return status;
    
    status = AD5933_WriteByte(AD5933_REG_FREQ_INC_LOW, freq_inc_code & 0xFF);
    if (status != HAL_OK) return status;
    
    // 写入扫描点数
    status = AD5933_WriteByte(AD5933_REG_NUM_INC_HIGH, (ad5933_config.num_increments >> 8) & 0xFF);
    if (status != HAL_OK) return status;
    
    status = AD5933_WriteByte(AD5933_REG_NUM_INC_LOW, ad5933_config.num_increments & 0xFF);
    if (status != HAL_OK) return status;
    
    // 写入稳定时间
    status = AD5933_WriteByte(AD5933_REG_SETTLING_HIGH, (ad5933_config.settling_cycles >> 8) & 0xFF);
    if (status != HAL_OK) return status;
    
    status = AD5933_WriteByte(AD5933_REG_SETTLING_LOW, ad5933_config.settling_cycles & 0xFF);
    if (status != HAL_OK) return status;
    
    // 复位
    status = AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, 
                             AD5933_CMD_STANDBY | ad5933_config.output_range | ad5933_config.gain);
    if (status != HAL_OK) return status;
    
    status = AD5933_WriteByte(AD5933_REG_CONTROL_LOW, 0x10 | ad5933_config.clk_source);
    if (status != HAL_OK) return status;
    
    HAL_Delay(150);
    
    status = AD5933_WriteByte(AD5933_REG_CONTROL_LOW, 0x00 | ad5933_config.clk_source);
    if (status != HAL_OK) return status;
    
    // 初始化起始频率
    status = AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, 
                             AD5933_CMD_INIT_START_FREQ | ad5933_config.output_range | ad5933_config.gain);
    if (status != HAL_OK) return status;
    
    HAL_Delay(30);
    
    // 开始频率扫描
    status = AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, 
                             AD5933_CMD_START_FREQ_SWEEP | ad5933_config.output_range | ad5933_config.gain);
    
    return status;
}

/**
 * @brief 开始测试
 * @param add_ok 是否是新的扫描点 (>0: 新扫描点, 0: 重复扫描)
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_StartTest(uint8_t add_ok)
{
    uint8_t command;
    
    if (add_ok > 0) {
        // 开始新的频率扫描
        command = AD5933_CMD_INCREMENT_FREQ | ad5933_config.output_range | ad5933_config.gain;
    } else {
        // 重复上次扫描
        command = AD5933_CMD_REPEAT_FREQ | ad5933_config.output_range | ad5933_config.gain;
    }
    
    return AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, command);
}

/**
 * @brief 读取阻抗数据
 * @param impedance_data 阻抗数据结构体指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_ReadImpedance(ImpeType *impedance_data)
{
    HAL_StatusTypeDef status;
    uint8_t status_reg = 0;
    uint16_t real_data, imag_data;
    
    if (impedance_data == NULL) {
        return HAL_ERROR;
    }
    
    // 等待数据有效 (添加超时保护)
    uint32_t timeout_count = 0;
    const uint32_t MAX_TIMEOUT = 1000; // 最大等待次数
    
    do {
        status = AD5933_ReadByte(AD5933_REG_STATUS, &status_reg);
        if (status != HAL_OK) return status;
        
        HAL_Delay(5);
        timeout_count++;
        
        if (timeout_count > MAX_TIMEOUT) {
            return HAL_TIMEOUT;
        }
    } while ((status_reg & AD5933_STATUS_DATA_VALID) == 0);
    
    // 读取实部数据
    status = AD5933_ReadWord(AD5933_REG_REAL_DATA_HIGH, &real_data);
    if (status != HAL_OK) return status;
    
    impedance_data->Re = (int16_t)real_data;
    
    // 读取虚部数据
    status = AD5933_ReadWord(AD5933_REG_IMAG_DATA_HIGH, &imag_data);
    if (status != HAL_OK) return status;
    
    impedance_data->Im = (int16_t)imag_data;
    
    // 计算阻抗幅值
    float re_abs = fabsf((float)impedance_data->Re);
    float im_abs = fabsf((float)impedance_data->Im);
    impedance_data->Impedance = sqrtf(re_abs * re_abs + im_abs * im_abs);
    
    // 计算相位 (使用atan2f函数更准确)
    impedance_data->Phase = atan2f((float)impedance_data->Im, (float)impedance_data->Re);
    
    // 将相位转换到[0, 2π]范围
    if (impedance_data->Phase < 0) {
        impedance_data->Phase += 2.0f * PI;
    }
    
    return HAL_OK;
}

/**
 * @brief 执行一次完整的阻抗测试
 * @param impedance_data 阻抗数据结构体指针
 * @param add_ok 是否是新的扫描点
 * @retval HAL状态
 */
HAL_StatusTypeDef AD5933_StartOnceTest(ImpeType *impedance_data, uint8_t add_ok)
{
    HAL_StatusTypeDef status;
    
    status = AD5933_StartTest(add_ok);
    if (status != HAL_OK) return status;
    
    return AD5933_ReadImpedance(impedance_data);
}

/**
 * @brief 测量温度
 * @retval 温度值 (摄氏度)
 */
float AD5933_Temperature_Test(void)
{
    HAL_StatusTypeDef status;
    uint8_t status_reg = 0;
    uint16_t temp_data;
    int16_t signed_temp;
    
    // 启动温度测量
    status = AD5933_WriteByte(AD5933_REG_CONTROL_HIGH, 
                             AD5933_CMD_MEASURE_TEMP | ad5933_config.output_range | ad5933_config.gain);
    if (status != HAL_OK) return -999.0f;
    
    // 等待温度测量完成 (添加超时保护)
    uint32_t timeout_count = 0;
    const uint32_t MAX_TIMEOUT = 200; // 最大等待次数
    
    do {
        status = AD5933_ReadByte(AD5933_REG_STATUS, &status_reg);
        if (status != HAL_OK) return -999.0f;
        
        HAL_Delay(10);
        timeout_count++;
        
        if (timeout_count > MAX_TIMEOUT) {
            return -999.0f; // 超时返回错误值
        }
    } while ((status_reg & AD5933_STATUS_TEMP_VALID) == 0);
    
    // 读取温度数据
    status = AD5933_ReadWord(AD5933_REG_TEMP_DATA_HIGH, &temp_data);
    if (status != HAL_OK) return -999.0f;
    
    signed_temp = (int16_t)temp_data;
    
    // 转换为温度值
    if (signed_temp < 8192) {
        return (float)signed_temp / 32.0f;
    } else {
        return (float)(signed_temp - 16384) / 32.0f;
    }
}

/**
 * @brief 设置AD5933配置参数
 * @param clk_source 时钟源: 0x08 外部, 0x00 内部
 * @param gain 增益: 0x01 x1, 0x00 x5
 * @param output_range 输出范围: 0x00(2Vpp), 0x02(0.2Vpp), 0x04(0.4Vpp), 0x06(1Vpp)
 */
void AD5933_SetConfig(uint8_t clk_source, uint8_t gain, uint8_t output_range)
{
    ad5933_config.clk_source = clk_source;
    ad5933_config.gain = gain;
    ad5933_config.output_range = output_range;
}

/**
 * @brief 设置频率参数
 * @param start_freq 起始频率 (Hz)
 * @param freq_increment 频率增量 (Hz)
 * @param num_increments 扫描点数
 */
void AD5933_SetFrequency(uint32_t start_freq, uint32_t freq_increment, uint16_t num_increments)
{
    ad5933_config.start_freq = start_freq;
    ad5933_config.freq_increment = freq_increment;
    ad5933_config.num_increments = num_increments;
}

/**
 * @brief 设置稳定时间
 * @param settling_cycles 稳定周期数
 */
void AD5933_SetSettlingTime(uint16_t settling_cycles)
{
    ad5933_config.settling_cycles = settling_cycles;
}
