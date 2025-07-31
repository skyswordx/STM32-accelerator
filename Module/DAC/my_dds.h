/* dds_wave_generator.h */
#ifndef DDS_WAVE_GENERATOR_H
#define DDS_WAVE_GENERATOR_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

// --- Public Configuration ---
// You can adjust this buffer size. A larger buffer gives the CPU more time
// to calculate samples but increases latency. 128 or 256 is a good start.
// IMPORTANT: Must be an even number.
#define DDS_DMA_BUFFER_SIZE 128
#define WAVE_TABLE_SIZE 64

// --- Type Definitions ---
// DDS Generator Handle Structure
typedef struct {
    // --- DDS State ---
    uint64_t phase_accumulator;         // 64-bit for very high precision
    uint64_t frequency_control_word;    // Controls the output frequency

    // --- Waveform Data ---
    const uint16_t* wave_table;         // Pointer to the arbitrary waveform data
    uint32_t wave_table_size_log2;      // Log2 of the wave table size for fast shifting

    // --- System Configuration ---
    uint32_t update_frequency;          // The fixed frequency of the DAC trigger timer (f_update)

    // --- HAL Handles ---
    DAC_HandleTypeDef* hdac;
    uint32_t dac_channel;
    TIM_HandleTypeDef* htimer;

    // --- Internal State ---
    uint16_t dma_buffer[DDS_DMA_BUFFER_SIZE]; // Internal Ping-Pong buffer

} DDS_Generator_t;

// --- Public API Functions ---

/**
 * @brief Initializes the DDS generator structure.
 * @param dds Pointer to the DDS_Generator_t handle.
 * @param hdac Pointer to the DAC HAL handle.
 * @param dac_channel The DAC channel to use (e.g., DAC_CHANNEL_1).
 * @param htimer Pointer to the Timer HAL handle used for triggering.
 * @param update_frequency The fixed update frequency set in the timer (e.g., 1000000 for 1MHz).
 */
void DDS_Init(DDS_Generator_t* dds, DAC_HandleTypeDef* hdac, uint32_t dac_channel, TIM_HandleTypeDef* htimer, uint32_t update_frequency);

/**
 * @brief Sets the arbitrary waveform table for the DDS generator.
 * @param dds Pointer to the DDS_Generator_t handle.
 * @param table Pointer to the array of waveform data.
 * @param table_size The number of samples in the table. MUST be a power of 2 (e.g., 64, 128, 256).
 */
void DDS_SetWaveform(DDS_Generator_t* dds, const uint16_t* table, uint32_t table_size);

/**
 * @brief Sets the output frequency of the waveform.
 * @param dds Pointer to the DDS_Generator_t handle.
 * @param frequency The desired output frequency in Hz.
 */
void DDS_SetFrequency(DDS_Generator_t* dds, float frequency);

/**
 * @brief Starts the DDS waveform generation.
 * @param dds Pointer to the DDS_Generator_t handle.
 */
void DDS_Start(DDS_Generator_t* dds);

/**
 * @brief Stops the DDS waveform generation.
 * @param dds Pointer to the DDS_Generator_t handle.
 */
void DDS_Stop(DDS_Generator_t* dds);

// --- Callback Handlers (to be called from HAL DAC callbacks) ---
// These functions MUST be called from the corresponding HAL callbacks in stm32h7xx_it.c
void DDS_Callback_HalfTransfer(DDS_Generator_t* dds);
void DDS_Callback_FullTransfer(DDS_Generator_t* dds);

#endif // DDS_WAVE_GENERATOR_H