
#include "my_button_config.h"
#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"

//================================================================================
// 内部变量和定义 (Internal Variables & Definitions)
//================================================================================

// 行端口和引脚数组，方便循环操作
static GPIO_TypeDef* row_ports[KEYPAD_NUM_ROWS] = {
    KEYPAD_ROW1_PORT,
    KEYPAD_ROW2_PORT,
    KEYPAD_ROW3_PORT
};

static const uint16_t row_pins[KEYPAD_NUM_ROWS] = {
    KEYPAD_ROW1_PIN,
    KEYPAD_ROW2_PIN,
    KEYPAD_ROW3_PIN
};

// 列端口和引脚数组
static GPIO_TypeDef* col_ports[KEYPAD_NUM_COLS] = {
    KEYPAD_COL1_PORT,
    KEYPAD_COL2_PORT,
    KEYPAD_COL3_PORT
};

static const uint16_t col_pins[KEYPAD_NUM_COLS] = {
    KEYPAD_COL1_PIN,
    KEYPAD_COL2_PIN,
    KEYPAD_COL3_PIN
};

// 按键映射表
static const uint8_t key_map[KEYPAD_NUM_ROWS][KEYPAD_NUM_COLS] = {
    {3, 2, 1},
    {6, 5, 4},
    {9, 8, 7}
};


//================================================================================
// 函数实现 (Function Implementations)
//================================================================================

/**
 * @brief  初始化矩阵键盘所需的GPIO引脚
 */
void Matrix_Keypad_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 1. 使能GPIOE时钟
    // 注意: 这个时钟使能函数可能已在别处 (如 main.c) 调用过
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // 2. 配置行引脚为推挽输出 (Row Pins as Output Push-Pull)
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    // 将所有行引脚一次性初始化
    GPIO_InitStruct.Pin = KEYPAD_ROW1_PIN | KEYPAD_ROW2_PIN | KEYPAD_ROW3_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // 初始状态下将所有行都设置为高电平 (非激活状态)
    HAL_GPIO_WritePin(KEYPAD_ROW1_PORT, KEYPAD_ROW1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KEYPAD_ROW2_PORT, KEYPAD_ROW2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KEYPAD_ROW3_PORT, KEYPAD_ROW3_PIN, GPIO_PIN_SET);

    // 3. 配置列引脚为上拉输入 (Column Pins as Input with Pull-up)
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 使用内部上拉电阻
    
    // 将所有列引脚一次性初始化
    GPIO_InitStruct.Pin = KEYPAD_COL1_PIN | KEYPAD_COL2_PIN | KEYPAD_COL3_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/**
 * @brief  执行一次物理扫描，返回当前按下的原始键值
 * @retval uint8_t: 物理按下的键值 (1-9), 或 NO_KEY_PRESSED
 */
static uint8_t Matrix_Keypad_Get_Raw_Key(void) {
    uint8_t row, col;

    for (row = 0; row < KEYPAD_NUM_ROWS; row++) {
        // 将所有行设置为高电平
        for (int i = 0; i < KEYPAD_NUM_ROWS; i++) {
            HAL_GPIO_WritePin(row_ports[i], row_pins[i], GPIO_PIN_SET);
        }

        // 将当前扫描的行拉低
        HAL_GPIO_WritePin(row_ports[row], row_pins[row], GPIO_PIN_RESET);

        // 延时一小段时间，确保电平稳定
        // 对于高速MCU，一个小延时是好习惯，但通常可以省略
        // for(volatile int d=0; d<10; d++);

        // 读取所有列
        for (col = 0; col < KEYPAD_NUM_COLS; col++) {
            if (HAL_GPIO_ReadPin(col_ports[col], col_pins[col]) == GPIO_PIN_RESET) {
                 // 按键被按下，恢复所有行电平并返回键值
                HAL_GPIO_WritePin(row_ports[row], row_pins[row], GPIO_PIN_SET);
                return key_map[row][col];
            }
        }
    }
    
    // 没有按键按下，恢复所有行电平
    for (int i = 0; i < KEYPAD_NUM_ROWS; i++) {
        HAL_GPIO_WritePin(row_ports[i], row_pins[i], GPIO_PIN_SET);
    }
    
    return NO_KEY_PRESSED;
}


/**
 * @brief  扫描键盘以获取按键值 (带消抖和单次触发)
 */
uint8_t Matrix_Keypad_Scan(void) {
    // 用于消抖和状态跟踪的静态变量
    static uint8_t last_raw_key = NO_KEY_PRESSED;
    static uint8_t stable_key = NO_KEY_PRESSED;
    static uint8_t key_reported = NO_KEY_PRESSED;
    static uint32_t debounce_start_tick = 0;

    // 1. 获取当前物理按键状态
    uint8_t current_raw_key = Matrix_Keypad_Get_Raw_Key();

    // 2. 消抖逻辑
    // 如果当前按键状态与上次不同，说明状态发生变化，重置消抖计时器
    if (current_raw_key != last_raw_key) {
        debounce_start_tick = osKernelGetTickCount();
        last_raw_key = current_raw_key;
        return NO_KEY_PRESSED; // 状态不稳定，直接返回
    }

    // 如果状态与上次相同，且已经稳定持续了一段时间
    if ((osKernelGetTickCount() - debounce_start_tick) > KEYPAD_DEBOUNCE_TIME_MS) {
        // 认为当前按键状态是稳定的
        stable_key = current_raw_key;

        // 3. 单次触发逻辑
        // 只有当稳定按键值与上次已报告的值不同时，才产生新的输出
        if (stable_key != key_reported) {
            key_reported = stable_key; // 更新已报告的值
            // 只在按键被按下的瞬间(从无到有)返回键值
            if (stable_key != NO_KEY_PRESSED) {
                return stable_key;
            }
        }
    }

    return NO_KEY_PRESSED;
}
