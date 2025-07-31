/* dds_wave_generator.h */
#ifndef DDS_WAVE_GENERATOR_H
#define DDS_WAVE_GENERATOR_H


#include "stm32h7xx_hal.h"
#include <stdint.h>

#define DDS_DMA_BUFFER_SIZE 128
#define WAVE_TABLE_SIZE 64

typedef struct {
    // --- DDS 狀態 ---
    uint64_t phase_accumulator;
    uint64_t frequency_control_word;
    
    // --- 幅度控制 ---
    float amplitude; // 範圍：0.0f 到 1.0f

    // --- 波形數據 ---
    const uint16_t* wave_table;
    uint32_t wave_table_size_log2;

    // --- 系統配置 ---
    uint32_t update_frequency;
    DAC_HandleTypeDef* hdac;
    uint32_t dac_channel;
    TIM_HandleTypeDef* htimer;

    // --- 內部狀態 ---
    uint16_t dma_buffer[DDS_DMA_BUFFER_SIZE];

} DDS_Generator_t;

// --- 公開 API 函式 ---

/**
 * @brief 初始化DDS產生器結構。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 * @param hdac 指向 DAC HAL 控制代碼的指標。
 * @param dac_channel 要使用的DAC通道 (例如 DAC_CHANNEL_1)。
 * @param htimer 用於觸發的Timer HAL控制代碼指標。
 * @param update_frequency 在定時器中設定的固定更新頻率 (例如 1000000 代表 1MHz)。
 */
void DDS_Init(DDS_Generator_t* dds, DAC_HandleTypeDef* hdac, uint32_t dac_channel, TIM_HandleTypeDef* htimer, uint32_t update_frequency);

/**
 * @brief 設定DDS產生器的波形表。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 * @param table 指向波形數據陣列的指標。
 * @param table_size 表中的取樣點數量。必須是2的冪 (例如 64, 128, 256)。
 */
void DDS_SetWaveform(DDS_Generator_t* dds, const uint16_t* table, uint32_t table_size);

/**
 * @brief 設定波形的輸出頻率。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 * @param frequency 期望的輸出頻率 (Hz)。
 */
void DDS_SetFrequency(DDS_Generator_t* dds, float frequency);

/**
 * @brief 設定波形的輸出幅度。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 * @param amplitude 期望的幅度，範圍從 0.0 (靜音) 到 1.0 (滿幅度)。
 */
void DDS_SetAmplitude(DDS_Generator_t* dds, float amplitude);

/**
 * @brief 啟動DDS波形產生。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 */
void DDS_Start(DDS_Generator_t* dds);

/**
 * @brief 停止DDS波形產生。
 * @param dds 指向 DDS_Generator_t 控制代碼的指標。
 */
void DDS_Stop(DDS_Generator_t* dds);

// --- 回呼處理函式 (需要在 HAL DAC 回呼中呼叫) ---
void DDS_Callback_HalfTransfer(DDS_Generator_t* dds);
void DDS_Callback_FullTransfer(DDS_Generator_t* dds);

#endif // DDS_WAVE_GENERATOR_H