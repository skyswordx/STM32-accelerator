/* dds_wave_generator.c */
#include "my_dds.h"
#include <math.h> // For log2
#include <string.h> // For memset

// --- Private Function Prototypes ---
static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples);
static uint32_t calculate_log2(uint32_t n);

// --- Public Function Implementations ---

void DDS_Init(DDS_Generator_t* dds, DAC_HandleTypeDef* hdac, uint32_t dac_channel, TIM_HandleTypeDef* htimer, uint32_t update_frequency) {
    memset(dds, 0, sizeof(DDS_Generator_t)); // Clear the structure
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
    // The core DDS formula: FCW = (f_out * 2^bits) / f_update
    // We use 64-bit for both phase accumulator and FCW for maximum precision.
    dds->frequency_control_word = (uint64_t)((double)frequency * (double)(1ULL << 63) * 2.0 / (double)dds->update_frequency);
}

void DDS_Start(DDS_Generator_t* dds) {
    // First, fill both halves of the buffer with initial waveform data
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);

    // Start the DAC with DMA in circular mode
    HAL_DAC_Start_DMA(dds->hdac, dds->dac_channel, (uint32_t*)dds->dma_buffer, DDS_DMA_BUFFER_SIZE, DAC_ALIGN_12B_R);

    // Start the trigger timer
    HAL_TIM_Base_Start(dds->htimer);
}

void DDS_Stop(DDS_Generator_t* dds) {
    HAL_TIM_Base_Stop(dds->htimer);
    HAL_DAC_Stop_DMA(dds->hdac, dds->dac_channel);
}

// --- Callback Handlers ---

void DDS_Callback_HalfTransfer(DDS_Generator_t* dds) {
    // DMA has finished sending the first half of the buffer (Ping).
    // The CPU now has time to refill the first half while DMA sends the second half (Pong).
    DDS_GenerateSamples(dds, &dds->dma_buffer[0], DDS_DMA_BUFFER_SIZE / 2);
}

void DDS_Callback_FullTransfer(DDS_Generator_t* dds) {
    // DMA has finished sending the second half of the buffer (Pong).
    // The CPU now has time to refill the second half while DMA sends the first half (Ping).
    DDS_GenerateSamples(dds, &dds->dma_buffer[DDS_DMA_BUFFER_SIZE / 2], DDS_DMA_BUFFER_SIZE / 2);
}

// --- Private Function Implementations ---

/**
 * @brief The core DDS engine. Fills a buffer with generated samples.
 */
static void DDS_GenerateSamples(DDS_Generator_t* dds, uint16_t* buffer, uint32_t num_samples) {
    if (!dds->wave_table || dds->wave_table_size_log2 == 0) {
        // Safety check: if no waveform is set, fill with silence (or a default value)
        for (uint32_t i = 0; i < num_samples; i++) {
            buffer[i] = 2048; // Mid-point for 12-bit DAC
        }
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        // Accumulate phase
        dds->phase_accumulator += dds->frequency_control_word;

        // Get the top bits of the accumulator to use as a table index
        // The number of top bits to take is determined by the table size (log2)
        uint32_t table_index = (uint32_t)(dds->phase_accumulator >> (64 - dds->wave_table_size_log2));
        
        // Look up the sample in the wave table and place it in the buffer
        buffer[i] = dds->wave_table[table_index];
    }
}

/**
 * @brief Calculates the integer base-2 logarithm.
 */
static uint32_t calculate_log2(uint32_t n) {
    if (n == 0) return 0;
    uint32_t count = 0;
    while ((n = n >> 1) > 0) {
        count++;
    }
    return count;
}