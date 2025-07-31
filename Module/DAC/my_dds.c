/* dds_wave_generator.c (Version 2 - Cache Aware) */
#include "my_dds.h"
#include <math.h>
#include <string.h>

// --- Private Function Prototypes ---
static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples);
static uint32_t calculate_log2(uint32_t n);

// --- Public Function Implementations ---

void DDS_Init(DDS_Generator_t* dds, DAC_HandleTypeDef* hdac, uint32_t dac_channel, TIM_HandleTypeDef* htimer, uint32_t update_frequency) {
    memset(dds, 0, sizeof(DDS_Generator_t));
    dds->hdac = hdac;
    dds->dac_channel = dac_channel;
    dds->htimer = htimer;
    dds->update_frequency = update_frequency;
}

void DDS_SetWaveform(DDS_Generator_t* dds, const uint16_t* table, uint32_t table_size) {
    dds->wave_table = table;
    dds->wave_table_size_log2 = calculate_log2(table_size);
}

void DDS_SetFrequency(DDS_Generator_t* dds, float frequency) {
    dds->frequency_control_word = (uint64_t)((double)frequency * (double)(1ULL << 63) * 2.0 / (double)dds->update_frequency);
}

void DDS_Start(DDS_Generator_t* dds) {
    // Fill both halves of the buffer initially
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);
    
    // ******************** CACHE MAINTENANCE ********************
    // Clean the D-Cache for the entire buffer to ensure DMA sees the new data.
    // The address must be 32-byte aligned, and the size must be a multiple of 32.
    // We clean the whole buffer to be safe.
    SCB_CleanDCache_by_Addr((uint32_t*)dds->dma_buffer, sizeof(dds->dma_buffer));
    // ***********************************************************

    HAL_DAC_Start_DMA(dds->hdac, dds->dac_channel, (uint32_t*)dds->dma_buffer, DDS_DMA_BUFFER_SIZE, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(dds->htimer);
}

void DDS_Stop(DDS_Generator_t* dds) {
    HAL_TIM_Base_Stop(dds->htimer);
    HAL_DAC_Stop_DMA(dds->hdac, dds->dac_channel);
}

// --- Callback Handlers ---

void DDS_Callback_HalfTransfer(DDS_Generator_t* dds) {
    // Refill the first half (Ping)
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
    
    // ******************** CACHE MAINTENANCE ********************
    // Clean the D-Cache only for the part of the buffer we just modified.
    SCB_CleanDCache_by_Addr((uint32_t*)&dds->dma_buffer[0], sizeof(uint16_t) * (DDS_DMA_BUFFER_SIZE / 2));
    // ***********************************************************
}

void DDS_Callback_FullTransfer(DDS_Generator_t* dds) {
    // Refill the second half (Pong)
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);

    // ******************** CACHE MAINTENANCE ********************
    // Clean the D-Cache only for the part of the buffer we just modified.
    SCB_CleanDCache_by_Addr((uint32_t*)&dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], sizeof(uint16_t) * (DDS_DMA_BUFFER_SIZE / 2));
    // ***********************************************************
}

// --- Private Function Implementations ---

static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples) {
    if (!dds->wave_table || dds->wave_table_size_log2 == 0) {
        for (uint32_t i = 0; i < num_samples; i++) {
            buffer[i] = 2048;
        }
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        dds->phase_accumulator += dds->frequency_control_word;
        uint32_t table_index = (uint32_t)(dds->phase_accumulator >> (64 - dds->wave_table_size_log2));
        buffer[i] = dds->wave_table[table_index];
    }
}

static uint32_t calculate_log2(uint32_t n) {
    if (n == 0) return 0;
    uint32_t count = 0;
    while ((n = n >> 1) > 0) {
        count++;
    }
    return count;
}
