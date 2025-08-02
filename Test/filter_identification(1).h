#ifndef INC_FILTER_IDENTIFICATION_H_
#define INC_FILTER_IDENTIFICATION_H_

#include "arm_math.h"

// =========================================================================
//                              CONFIGURATION
// =========================================================================
#define INPUT_NUM_FREQ_POINTS   496 
#define OUTPUT_NUM_POINTS       2496

// =========================================================================
//                                DATA TYPES
// =========================================================================

typedef enum {
    FILTER_TYPE_LPF,
    FILTER_TYPE_HPF,
    FILTER_TYPE_BPF,
    FILTER_TYPE_BSF
} FilterType;

typedef struct {
    float32_t b0, b1, b2;
    float32_t a0, a1;
} TransferFunctionParams;

// (NEW) This struct is now in the header file
typedef struct {
    float32_t dc_gain_db;
    float32_t hf_gain_db;
    float32_t min_gain_db;
    float32_t max_gain_db;
    uint32_t  min_gain_idx;
    uint32_t  max_gain_idx;
} FilterCharacteristics;

// =========================================================================
//                            PUBLIC FUNCTION PROTOTYPE
// =========================================================================
FilterType process_filter_response_c(
    float32_t* magnitude_db_out,
    float32_t* phase_deg_out,
    const float32_t* f_hz_input,
    const float32_t* H_measured_cmplx,
    uint32_t num_input_points,
    float32_t noise_floor_db
);

#endif /* INC_FILTER_IDENTIFICATION_H_ */