/* dds_wave_generator.c (Version 2 - Cache Aware) */
#include "my_dds.h"
#include <math.h>
#include <string.h>


// --- 私有函式原型 ---
static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples);
static uint32_t calculate_log2(uint32_t n);

// --- 公開函式實作 ---

void DDS_Init(DDS_Generator_t* dds, DAC_HandleTypeDef* hdac, uint32_t dac_channel, TIM_HandleTypeDef* htimer, uint32_t update_frequency) {
    memset(dds, 0, sizeof(DDS_Generator_t));
    dds->hdac = hdac;
    dds->dac_channel = dac_channel;
    dds->htimer = htimer;
    dds->update_frequency = update_frequency;
    
    // 預設將幅度初始化為滿幅度
    dds->amplitude = 1.0f;
}

void DDS_SetAmplitude(DDS_Generator_t* dds, float amplitude) {
    if (amplitude < 0.0f) {
        dds->amplitude = 0.0f;
    } else if (amplitude > 1.0f) {
        dds->amplitude = 1.0f;
    } else {
        dds->amplitude = amplitude;
    }
}

void DDS_SetWaveform(DDS_Generator_t* dds, const uint16_t* table, uint32_t table_size) {
    dds->wave_table = table;
    dds->wave_table_size_log2 = calculate_log2(table_size);
}

void DDS_SetFrequency(DDS_Generator_t* dds, float frequency) {
    dds->frequency_control_word = (uint64_t)((double)frequency * (double)(1ULL << 63) * 2.0 / (double)dds->update_frequency);
}

void DDS_Start(DDS_Generator_t* dds) {
    // 初始填充DMA緩衝區
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);
    
    // 清理D-Cache以確保DMA能讀取到新數據
    SCB_CleanDCache_by_Addr((uint32_t*)dds->dma_buffer, sizeof(dds->dma_buffer));
    
    // 啟動DAC的DMA傳輸 (循環模式)
    HAL_DAC_Start_DMA(dds->hdac, dds->dac_channel, (uint32_t*)dds->dma_buffer, DDS_DMA_BUFFER_SIZE, DAC_ALIGN_12B_R);
    
    // 啟動觸發定時器
    HAL_TIM_Base_Start(dds->htimer);
}

void DDS_Stop(DDS_Generator_t* dds) {
    HAL_TIM_Base_Stop(dds->htimer);
    HAL_DAC_Stop_DMA(dds->hdac, dds->dac_channel);
}

// --- 回呼處理函式 ---

void DDS_Callback_HalfTransfer(DDS_Generator_t* dds) {
    // 填充前半部分 (Ping)
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
    // 清理對應部分的D-Cache
    SCB_CleanDCache_by_Addr((uint32_t*)&dds->dma_buffer[0], sizeof(uint16_t) * (DDS_DMA_BUFFER_SIZE / 2));
}

void DDS_Callback_FullTransfer(DDS_Generator_t* dds) {
    // 填充後半部分 (Pong)
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);
    // 清理對應部分的D-Cache
    SCB_CleanDCache_by_Addr((uint32_t*)&dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], sizeof(uint16_t) * (DDS_DMA_BUFFER_SIZE / 2));
}

// --- 私有函式實作 ---

static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples) {
    if (!dds->wave_table || dds->wave_table_size_log2 == 0) {
        for (uint32_t i = 0; i < num_samples; i++) { buffer[i] = 2048; }
        return;
    }

    const float dac_center = 2047.5f;

    for (uint32_t i = 0; i < num_samples; i++) {
        // 1. 相位累加
        dds->phase_accumulator += dds->frequency_control_word;
        uint32_t table_index = (uint32_t)(dds->phase_accumulator >> (64 - dds->wave_table_size_log2));
        
        // 2. 查表獲取滿幅度取樣點
        uint16_t master_sample = dds->wave_table[table_index];

        // 3. 即時幅度縮放
        float centered_sample = (float)master_sample - dac_center;
        float scaled_sample = centered_sample * dds->amplitude;
        uint16_t final_sample = (uint16_t)(scaled_sample + dac_center);

        // 4. 將最終取樣點寫入緩衝區
        buffer[i] = final_sample;
    }
}

static uint32_t calculate_log2(uint32_t n) {
    if (n == 0) return 0;
    uint32_t count = 0;
    while ((n = n >> 1) > 0) { count++; }
    return count;
}