#include "filter_identification.h"
#include <math.h>
#include <string.h>
#include <stdio.h> 

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- 内部使用的结构体 ---
typedef struct {
    const float32_t* s_cmplx;
    const float32_t* H_mag;
    uint32_t num_points;
} OptParams;

// --- 内部静态函数 - 前向声明 ---
static void fit_physical_model(TransferFunctionParams* sys_best, float32_t* min_aic, const float32_t* s_cmplx, const float32_t* H_measured, uint32_t num_points, float32_t floor_db);
static FilterType identify_filter_type_forced_c(const float32_t* H_measured, uint32_t num_points, FilterCharacteristics* info_out);
static void linear_interpolate_c(float32_t* mag_db_out, float32_t* phase_deg_out, const float32_t* f_in, const float32_t* H_in, uint32_t n_in);
static void fit_ssk_stabilized_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db);
static void fit_lpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db);
static void fit_hpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db);
static void fit_bpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db);
static void fit_1st_order_lpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n);
static void fit_1st_order_hpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n);
static void fit_zero_pole_decoupled_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, const FilterCharacteristics* info);
static void solve_2param_stabilized_c(float32_t* p1, float32_t* p0, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db, uint8_t model_type);
static float32_t calculate_aic_c(const TransferFunctionParams* sys, int k, const float32_t* s, const float32_t* H, uint32_t num_points);
static arm_status solve_least_squares(arm_matrix_instance_f32* A, arm_matrix_instance_f32* b, arm_matrix_instance_f32* x);
static float32_t golden_section_search(float32_t a, float32_t b, float32_t tol, float32_t (*f)(float32_t, const void*), const void* params);
static float32_t rss_mag_lpf(float32_t a0, const void* p);
static float32_t rss_mag_hpf(float32_t a0, const void* p);
static void complex_divide_f32(const float32_t* pSrcA, const float32_t* pSrcB, float32_t* pDst, uint32_t numSamples);

// =========================================================================
//                            PUBLIC API FUNCTION
// =========================================================================
FilterType process_filter_response_c(
    float32_t* magnitude_db_out,
    float32_t* phase_deg_out,
    const float32_t* f_hz_input,
    const float32_t* H_measured_cmplx,
    uint32_t num_input_points,
    float32_t noise_floor_db)
{
    static float32_t s_cmplx[INPUT_NUM_FREQ_POINTS * 2];
    for (uint32_t i = 0; i < num_input_points; ++i) {
        s_cmplx[i * 2] = 0.0f;
        s_cmplx[i * 2 + 1] = 2.0f * M_PI * f_hz_input[i];
    }

    TransferFunctionParams sys_fit;
    float32_t min_aic;
    fit_physical_model(&sys_fit, &min_aic, s_cmplx, H_measured_cmplx, num_input_points, noise_floor_db);
    
    FilterCharacteristics info;
    FilterType filter_type = identify_filter_type_forced_c(H_measured_cmplx, num_input_points, &info);
    printf("Forced classification result: %s\r\n", 
        (filter_type == FILTER_TYPE_LPF) ? "Low-Pass" :
        (filter_type == FILTER_TYPE_HPF) ? "High-Pass" :
        (filter_type == FILTER_TYPE_BPF) ? "Band-Pass" : "Band-Stop");

    if (min_aic < 50.0f && !isnan(sys_fit.a0)) {
        printf("-> AIC (%.2f) < 50. Using [Physical Model].\r\n", min_aic);
        for (uint32_t i = 0; i < OUTPUT_NUM_POINTS; ++i) {
            // --- USE DOUBLE FOR HIGH-PRECISION CALCULATION ---
            double f_out = 1000.0 + i * 200.0;
            double w_out = 2.0 * M_PI * f_out;
            double w_out2 = w_out * w_out;
            double a0 = sys_fit.a0, a1 = sys_fit.a1, b0 = sys_fit.b0, b1 = sys_fit.b1, b2 = sys_fit.b2;

            double num_re = b0 - b2 * w_out2;
            double num_im = b1 * w_out;
            double den_re = a0 - w_out2;
            double den_im = a1 * w_out;

            double den_mag_sq = den_re * den_re + den_im * den_im;
            if (den_mag_sq < 1e-38) den_mag_sq = 1e-38;
            
            double H_out_re = (num_re * den_re + num_im * den_im) / den_mag_sq;
            double H_out_im = (num_im * den_re - num_re * den_im) / den_mag_sq;

            double mag = sqrt(H_out_re * H_out_re + H_out_im * H_out_im);
            if (mag < 1e-19) mag = 1e-19; 
            
            magnitude_db_out[i] = (float32_t)(20.0 * log10(mag));
            phase_deg_out[i] = (float32_t)(atan2(H_out_im, H_out_re) * 180.0 / M_PI);
        }
    } else {
        printf("-> AIC (%.2f) >= 50. Using [Interpolation Model].\r\n", min_aic);
        linear_interpolate_c(magnitude_db_out, phase_deg_out, f_hz_input, H_measured_cmplx, num_input_points);
    }
    
    // (Unwrap logic is unchanged)
    for (uint32_t i = 1; i < OUTPUT_NUM_POINTS; i++) {
        float32_t diff = phase_deg_out[i] - phase_deg_out[i-1];
        if (diff > 180.0f) {
            for (uint32_t j = i; j < OUTPUT_NUM_POINTS; j++) phase_deg_out[j] -= 360.0f;
        } else if (diff < -180.0f) {
            for (uint32_t j = i; j < OUTPUT_NUM_POINTS; j++) phase_deg_out[j] += 360.0f;
        }
    }
    return filter_type;
}
// =========================================================================
//                       HYBRID MODEL IMPLEMENTATION
// =========================================================================

static void fit_physical_model(TransferFunctionParams* sys_best, float32_t* min_aic, const float32_t* s_cmplx, const float32_t* H_measured, uint32_t num_points, float32_t floor_db) {
    FilterCharacteristics info;
    FilterType type_guess = identify_filter_type_forced_c(H_measured, num_points, &info);
    
    TransferFunctionParams competitors[4];
    float32_t aic_values[4] = {INFINITY, INFINITY, INFINITY, INFINITY};
    const char* names[4] = {"", "", "", ""};
    int competitor_count = 0;

    printf(" -> Holding physical model competition...\r\n");

    fit_ssk_stabilized_c(&competitors[competitor_count], s_cmplx, H_measured, num_points, floor_db);
    aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 5, s_cmplx, H_measured, num_points);
    names[competitor_count] = "General 2nd-Order";
    competitor_count++;

    if (type_guess == FILTER_TYPE_LPF) {
        fit_lpf_constrained_c(&competitors[competitor_count], s_cmplx, H_measured, num_points, floor_db);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 2, s_cmplx, H_measured, num_points);
        names[competitor_count] = "LPF Constrained 2nd-Order";
        competitor_count++;
        
        fit_1st_order_lpf_constrained_c(&competitors[competitor_count], s_cmplx, H_measured, num_points);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 1, s_cmplx, H_measured, num_points);
        names[competitor_count] = "LPF Constrained 1st-Order";
        competitor_count++;
    
    } else if (type_guess == FILTER_TYPE_HPF) {
        fit_hpf_constrained_c(&competitors[competitor_count], s_cmplx, H_measured, num_points, floor_db);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 2, s_cmplx, H_measured, num_points);
        names[competitor_count] = "HPF Constrained 2nd-Order";
        competitor_count++;
        
        fit_1st_order_hpf_constrained_c(&competitors[competitor_count], s_cmplx, H_measured, num_points);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 1, s_cmplx, H_measured, num_points);
        names[competitor_count] = "HPF Constrained 1st-Order";
        competitor_count++;

    } else if (type_guess == FILTER_TYPE_BPF) {
        fit_bpf_constrained_c(&competitors[competitor_count], s_cmplx, H_measured, num_points, floor_db);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 2, s_cmplx, H_measured, num_points);
        names[competitor_count] = "BPF Constrained 2nd-Order";
        competitor_count++;

    } else if (type_guess == FILTER_TYPE_BSF) {
        fit_zero_pole_decoupled_c(&competitors[competitor_count], s_cmplx, H_measured, num_points, &info);
        aic_values[competitor_count] = calculate_aic_c(&competitors[competitor_count], 3, s_cmplx, H_measured, num_points);
        names[competitor_count] = "BSF Decoupled Expert";
        competitor_count++;
    }
    
    *min_aic = INFINITY;
    int winner_idx = 0;
    for (int i = 0; i < competitor_count; ++i) {
        printf("   - Model [%s]: AIC = %.2f\r\n", names[i], aic_values[i]);
        if (aic_values[i] < *min_aic && !isnan(aic_values[i])) {
            *min_aic = aic_values[i];
            winner_idx = i;
        }
    }
    
    if (competitor_count > 0 && *min_aic != INFINITY) {
      memcpy(sys_best, &competitors[winner_idx], sizeof(TransferFunctionParams));
      printf("--- Physical model winner: [%s] (AIC=%.2f) ---\r\n", names[winner_idx], *min_aic);
    } else {
      sys_best->a0 = NAN; 
      *min_aic = INFINITY;
    }
}

static FilterType identify_filter_type_forced_c(const float32_t* H_measured, uint32_t num_points, FilterCharacteristics* info_out) {
    static float32_t gain_db[INPUT_NUM_FREQ_POINTS];
    
    for (uint32_t i = 0; i < num_points; ++i) {
        float32_t mag_sq;
        arm_cmplx_mag_squared_f32((float32_t*)&H_measured[i * 2], &mag_sq, 1);
        if (mag_sq < 1e-38f) mag_sq = 1e-38f;
        gain_db[i] = 10.0f * log10f(mag_sq);
    }
    
    uint32_t n_points_avg = (uint32_t)roundf(num_points * 0.05f);
    if (n_points_avg < 2) n_points_avg = 2;
    
    arm_mean_f32(&gain_db[0], n_points_avg, &info_out->dc_gain_db);
    arm_mean_f32(&gain_db[num_points - n_points_avg], n_points_avg, &info_out->hf_gain_db);
    arm_max_f32(gain_db, num_points, &info_out->max_gain_db, &info_out->max_gain_idx);
    arm_min_f32(gain_db, num_points, &info_out->min_gain_db, &info_out->min_gain_idx);
    
    float32_t gain_span = info_out->max_gain_db - info_out->min_gain_db;
    
    if (gain_span < 5.0f) {
        return (info_out->dc_gain_db > info_out->hf_gain_db) ? FILTER_TYPE_LPF : FILTER_TYPE_HPF;
    }
    
    int is_peak_dominant = (info_out->max_gain_db - info_out->dc_gain_db > 5.0f) && (info_out->max_gain_db - info_out->hf_gain_db > 5.0f);
    int is_notch_dominant = (info_out->dc_gain_db - info_out->min_gain_db > 5.0f) && (info_out->hf_gain_db - info_out->min_gain_db > 5.0f);
    
    if (is_peak_dominant && (info_out->max_gain_idx > n_points_avg) && (info_out->max_gain_idx < num_points - n_points_avg)) {
        return FILTER_TYPE_BPF;
    }
    if (is_notch_dominant && (info_out->min_gain_idx > n_points_avg) && (info_out->min_gain_idx < num_points - n_points_avg)) {
        return FILTER_TYPE_BSF;
    }
    if (info_out->dc_gain_db > info_out->hf_gain_db) {
        return FILTER_TYPE_LPF;
    }
    return FILTER_TYPE_HPF;
}

static void linear_interpolate_c(float32_t* mag_db_out, float32_t* phase_deg_out, const float32_t* f_in, const float32_t* H_in, uint32_t n_in) {
    for (uint32_t i = 0; i < OUTPUT_NUM_POINTS; ++i) {
        float32_t f_out = 1000.0f + i * 200.0f;
        uint32_t idx = 0;
        while (idx < n_in - 2 && f_in[idx+1] < f_out) { idx++; }
        
        float32_t f1 = f_in[idx], f2 = f_in[idx + 1];
        float32_t factor = (f2 - f1 < 1e-6f) ? 0.0f : (f_out - f1) / (f2 - f1);
        
        float32_t h1_re = H_in[idx * 2], h1_im = H_in[idx * 2 + 1];
        float32_t h2_re = H_in[(idx + 1) * 2], h2_im = H_in[(idx + 1) * 2 + 1];
        
        float32_t h_out_re = h1_re + factor * (h2_re - h1_re);
        float32_t h_out_im = h1_im + factor * (h2_im - h1_im);

        float32_t mag_sq;
        arm_cmplx_mag_squared_f32(&h_out_re, &mag_sq, 1);
        if (mag_sq < 1e-38f) mag_sq = 1e-38f;

        mag_db_out[i] = 10.0f * log10f(mag_sq);
        phase_deg_out[i] = atan2f(h_out_im, h_out_re) * 180.0f / M_PI;
    }
}

// =========================================================================
//                       FITTING ALGORITHMS & HELPERS
// =========================================================================


// --- CORRECTED CORE FITTING ALGORITHM ---
static void fit_ssk_stabilized_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db) {
    const int N_COEFFS = 5;
    const int N_ROWS = 2 * n;
    // 使用宏定义确保静态数组大小在编译时确定
    static float32_t A_data[INPUT_NUM_FREQ_POINTS*2*5], b_data[INPUT_NUM_FREQ_POINTS*2], D_prev[INPUT_NUM_FREQ_POINTS * 2];
    float32_t c_s[N_COEFFS], c_prev[N_COEFFS] = {0};
    
    float32_t w_scale; 
    arm_sqrt_f32(s[1]*s[2*n-1], &w_scale);
    for(uint32_t i=0; i<n; ++i){ D_prev[2*i]=1.0f; D_prev[2*i+1]=0.0f; }

    float32_t thresh_mag = powf(10.0f, (floor_db + 6.0f) / 20.0f);
    static uint8_t is_sig[INPUT_NUM_FREQ_POINTS];
    for(uint32_t i=0; i<n; ++i){ float32_t mag; arm_cmplx_mag_f32((float32_t*)&H[i*2], &mag, 1); is_sig[i] = mag > thresh_mag; }
    
    for(int iter=0; iter<40; ++iter) {
        for(uint32_t i=0; i<n; ++i) {
            float32_t D_mag_sq; arm_cmplx_mag_squared_f32(&D_prev[i*2], &D_mag_sq, 1);
            float32_t W = is_sig[i] ? 1.0f/(D_mag_sq + 1e-12f) : 1e-12f;
            float32_t sqrt_W; arm_sqrt_f32(W, &sqrt_W);
            
            float32_t s_im_s = s[2*i+1]/w_scale;
            float32_t s2_re_s = -s_im_s*s_im_s;
            float32_t H_re = H[2*i], H_im = H[2*i+1];
            
            // Re-derivation from H*a1*s + H*a0 - b2*s^2 - b1*s - b0 = -H*s^2
            // Coeffs of [a1_s, a0_s, b2, b1_s, b0_s]
            float32_t c0_re = -H_im*s_im_s;        float32_t c0_im =  H_re*s_im_s;         // H*s
            float32_t c1_re = H_re;                float32_t c1_im =  H_im;                // H
            float32_t c2_re = -s2_re_s;            float32_t c2_im =  0.0f;                // -s^2
            float32_t c3_re = 0.0f;                float32_t c3_im = -s_im_s;              // -s
            float32_t c4_re = -1.0f;               float32_t c4_im =  0.0f;                // -1
            
            float32_t b_re = -H_re * s2_re_s;
            float32_t b_im = -H_im * s2_re_s;
            
            int r_re = i, r_im = i+n;
            A_data[r_re*N_COEFFS+0]=c0_re*sqrt_W; A_data[r_im*N_COEFFS+0]=c0_im*sqrt_W;
            A_data[r_re*N_COEFFS+1]=c1_re*sqrt_W; A_data[r_im*N_COEFFS+1]=c1_im*sqrt_W;
            A_data[r_re*N_COEFFS+2]=c2_re*sqrt_W; A_data[r_im*N_COEFFS+2]=c2_im*sqrt_W;
            A_data[r_re*N_COEFFS+3]=c3_re*sqrt_W; A_data[r_im*N_COEFFS+3]=c3_im*sqrt_W;
            A_data[r_re*N_COEFFS+4]=c4_re*sqrt_W; A_data[r_im*N_COEFFS+4]=c4_im*sqrt_W;
            
            b_data[r_re] = b_re * sqrt_W; 
            b_data[r_im] = b_im * sqrt_W;
        }
        arm_matrix_instance_f32 A={N_ROWS,N_COEFFS,A_data}, b={N_ROWS,1,b_data}, x={N_COEFFS,1,c_s};
        if(solve_least_squares(&A,&b,&x)!=ARM_MATH_SUCCESS){ res->a0=NAN; return; }
        float32_t norm_cs;
        arm_dot_prod_f32(c_s, c_s, N_COEFFS, &norm_cs);
        if (iter > 0 && norm_cs > 1e12f) { // 如果系数向量的模平方过大
            printf(" -> Warning: Unstable solution detected in SSK. Reverting.\r\n");
            arm_copy_f32(c_prev, c_s, N_COEFFS); // 拒绝本次更新，沿用上次结果
        }
        if(iter>0 && (c_s[0]<0.0f || c_s[1]<0.0f)){
            c_s[0]=0.7f*c_prev[0] + 0.3f*fmaxf(0.0f,c_s[0]);
            c_s[1]=0.7f*c_prev[1] + 0.3f*fmaxf(0.0f,c_s[1]);
        }
        float32_t d_v[5], d_n, p_n; arm_sub_f32(c_s,c_prev,d_v,5); arm_dot_prod_f32(d_v,d_v,5,&d_n); arm_dot_prod_f32(c_prev,c_prev,5,&p_n);
        if(iter>0 && p_n>1e-12f && (d_n/p_n)<1e-24f) break;
        
        arm_copy_f32(c_s, c_prev, 5);
        for(uint32_t i=0; i<n; ++i){ 
            float32_t s_im_s=s[2*i+1]/w_scale; 
            D_prev[2*i]=-s_im_s*s_im_s + c_s[1]; 
            D_prev[2*i+1]=s_im_s*c_s[0];
        }
    }
    res->a1 = fmaxf(0.0f, c_s[0] * w_scale);
    res->a0 = fmaxf(0.0f, c_s[1] * w_scale * w_scale);
    res->b2 = c_s[2];
    res->b1 = c_s[3] * w_scale;
    res->b0 = c_s[4] * w_scale * w_scale;
}


static void fit_zero_pole_decoupled_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, const FilterCharacteristics* info) {
    float32_t w0_guess = s[info->min_gain_idx * 2 + 1];
    float32_t num_b0 = w0_guess * w0_guess;
    static float32_t N_guess_cmplx[INPUT_NUM_FREQ_POINTS * 2], H_proc_cmplx[INPUT_NUM_FREQ_POINTS * 2];

    for(uint32_t i=0; i<n; ++i) {
        float32_t s_im = s[i*2+1];
        N_guess_cmplx[i*2] = -s_im*s_im + num_b0;
        N_guess_cmplx[i*2+1] = 0.0f;
    }

    complex_divide_f32(N_guess_cmplx, H, H_proc_cmplx, n);
    float32_t weight_threshold_mag = powf(10.0f, (info->min_gain_db + 6.0f) / 20.0f);

    const int N_COEFFS=2, N_ROWS=2*n;
    static float32_t A_data[INPUT_NUM_FREQ_POINTS*2*2], b_data[INPUT_NUM_FREQ_POINTS*2];
    for(uint32_t i=0; i<n; ++i) {
        float32_t s_im = s[i*2+1];
        int r_re = i, r_im = i + n;
        float32_t mag_h;
        arm_cmplx_mag_f32((float32_t*)&H[i*2], &mag_h, 1);
        float32_t w = (mag_h > weight_threshold_mag) ? 1.0f : 0.01f;
        A_data[r_re*N_COEFFS+0] = 0.0f;               A_data[r_im*N_COEFFS+0] = s_im * w;
        A_data[r_re*N_COEFFS+1] = 1.0f * w;           A_data[r_im*N_COEFFS+1] = 0.0f;
        b_data[r_re] = (H_proc_cmplx[i*2]   - (-s_im*s_im)) * w;
        b_data[r_im] = (H_proc_cmplx[i*2+1] - 0.0f) * w;
    }

    float32_t coeffs_data[2];
    arm_matrix_instance_f32 A = {N_ROWS, N_COEFFS, A_data};
    arm_matrix_instance_f32 b = {N_ROWS, 1, b_data};
    arm_matrix_instance_f32 x = {N_COEFFS, 1, coeffs_data};

    if (solve_least_squares(&A, &b, &x) == ARM_MATH_SUCCESS) {
        res->a1 = fmaxf(0.0f, coeffs_data[0]);
        res->a0 = fmaxf(0.0f, coeffs_data[1]);
    } else {
        res->a1 = NAN; res->a0 = NAN;
    }
    res->b2 = 1.0f; res->b1 = 0.0f; res->b0 = num_b0;
}


static float32_t calculate_aic_c(const TransferFunctionParams* sys, int k, const float32_t* s, const float32_t* H, uint32_t num_points) {
    if (isnan(sys->a0)) return INFINITY;
    double rss = 0.0; // Use double for sum
    for(uint32_t i=0; i<num_points; ++i) {
        // --- USE DOUBLE FOR HIGH-PRECISION CALCULATION ---
        double s_im = s[i*2+1];
        double s_im2 = s_im * s_im;
        double a0=sys->a0, a1=sys->a1, b0=sys->b0, b1=sys->b1, b2=sys->b2;

        double num_re = b0 - b2*s_im2;
        double num_im = b1*s_im;
        double den_re = a0 - s_im2;
        double den_im = a1*s_im;
        double den_mag_sq = den_re*den_re + den_im*den_im;
        if (den_mag_sq < 1e-38) den_mag_sq = 1e-38;

        double H_fit_re = (num_re*den_re + num_im*den_im) / den_mag_sq;
        double H_fit_im = (num_im*den_re - num_re*den_im) / den_mag_sq;
        
        double err_re = (double)H[i*2] - H_fit_re;
        double err_im = (double)H[i*2+1] - H_fit_im;
        rss += err_re*err_re + err_im*err_im;
    }
    if (rss < 1e-18) rss = 1e-18; // Prevent log(0)
    return 2.0f*k + num_points * logf((float32_t)rss);
}

static arm_status solve_least_squares(arm_matrix_instance_f32* A, arm_matrix_instance_f32* b, arm_matrix_instance_f32* x) {
    const uint16_t num_coeffs = A->numCols;
    const uint16_t num_rows = A->numRows;
    const float32_t lambda = 1e-2f;

    // Use heap allocation for potentially large matrices if stack is a concern,
    // but static is safer if size is known and fits.
    static float32_t At_data[5 * (2*INPUT_NUM_FREQ_POINTS)];
    static float32_t AtA_data[5 * 5];
    static float32_t AtA_inv_data[5 * 5];
    static float32_t Atb_data[5 * 1];

    arm_matrix_instance_f32 At; arm_mat_init_f32(&At, num_coeffs, num_rows, At_data);
    arm_matrix_instance_f32 AtA; arm_mat_init_f32(&AtA, num_coeffs, num_coeffs, AtA_data);
    arm_matrix_instance_f32 AtA_inv; arm_mat_init_f32(&AtA_inv, num_coeffs, num_coeffs, AtA_inv_data);
    arm_matrix_instance_f32 Atb; arm_mat_init_f32(&Atb, num_coeffs, 1, Atb_data);

    arm_status status;
    status = arm_mat_trans_f32(A, &At); if (status != ARM_MATH_SUCCESS) return status;
    status = arm_mat_mult_f32(&At, A, &AtA); if (status != ARM_MATH_SUCCESS) return status;

    for (int i = 0; i < num_coeffs; i++) { AtA_data[i * num_coeffs + i] += lambda; }
    
    status = arm_mat_inverse_f32(&AtA, &AtA_inv); if (status != ARM_MATH_SUCCESS) return status;
    status = arm_mat_mult_f32(&At, b, &Atb); if (status != ARM_MATH_SUCCESS) return status;
    status = arm_mat_mult_f32(&AtA_inv, &Atb, x);
    return status;
}

static void complex_divide_f32(const float32_t* pSrcA, const float32_t* pSrcB, float32_t* pDst, uint32_t numSamples) {
    for (uint32_t i = 0; i < numSamples; i++) {
        float32_t a_re = pSrcA[2*i], a_im = pSrcA[2*i + 1];
        float32_t b_re = pSrcB[2*i], b_im = pSrcB[2*i + 1];
        float32_t den_mag_sq = b_re * b_re + b_im * b_im;
        if (den_mag_sq < 1e-20f) den_mag_sq = 1e-20f;
        pDst[2*i]     = (a_re * b_re + a_im * b_im) / den_mag_sq;
        pDst[2*i + 1] = (a_im * b_re - a_re * b_im) / den_mag_sq;
    }
}






// =========================================================================
//                       FITTING ALGORITHMS & HELPERS
// =========================================================================







static void solve_2param_stabilized_c(float32_t* p1, float32_t* p0, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db, uint8_t model_type) {
    const int N_COEFFS = 2;
    const int N_ROWS = 2 * n;
    static float32_t A_data[INPUT_NUM_FREQ_POINTS*2*2], b_data[INPUT_NUM_FREQ_POINTS*2], D_prev[INPUT_NUM_FREQ_POINTS * 2];
    float32_t c_s[N_COEFFS], c_prev[N_COEFFS] = {0};
    
    float32_t w_scale;
    arm_sqrt_f32(s[1] * s[2*n-1], &w_scale);
    for(uint32_t i=0; i<n; ++i){ D_prev[2*i]=1.0f; D_prev[2*i+1]=0.0f; }

    float32_t thresh_mag = powf(10.0f, (floor_db + 6.0f) / 20.0f);
    static uint8_t is_sig[INPUT_NUM_FREQ_POINTS];
    for(uint32_t i=0; i<n; ++i){ float32_t mag; arm_cmplx_mag_f32((float32_t*)&H[i*2], &mag, 1); is_sig[i] = mag > thresh_mag; }
    
    for(int iter=0; iter<40; ++iter) {
        for(uint32_t i=0; i<n; ++i) {
            float32_t D_mag_sq; arm_cmplx_mag_squared_f32(&D_prev[i*2], &D_mag_sq, 1);
            float32_t W = is_sig[i] ? 1.0f/(D_mag_sq + 1e-12f) : 1e-12f;
            float32_t sqrt_W; arm_sqrt_f32(W, &sqrt_W);
            
            float32_t s_im_s = s[2*i+1]/w_scale;
            float32_t s2_re_s = -s_im_s*s_im_s;
            float32_t H_re = H[2*i], H_im = H[2*i+1];
            
            float32_t a0_re, a0_im, a1_re, a1_im, b_re, b_im;

            switch(model_type) {
                case 0: // LPF: A=[H*s_s, H-1], b=-H*s_s^2
                    a1_re = -H_im*s_im_s; a1_im = H_re*s_im_s;
                    a0_re = H_re - 1.0f;  a0_im = H_im;
                    b_re = -H_re*s2_re_s; b_im = -H_im*s2_re_s;
                    break;
                case 1: // HPF: A=[H*s_s, H], b=s_s^2(1-H)
                    a1_re = -H_im*s_im_s; a1_im = H_re*s_im_s;
                    a0_re = H_re;         a0_im = H_im;
                    b_re = s2_re_s*(1.0f-H_re); b_im = s2_re_s*(-H_im);
                    break;
                case 2: // BPF: A=[s_s-H*s_s, H], b=-H*s_s^2
                    a1_re = -(-H_im*s_im_s); a1_im = s_im_s - H_re*s_im_s;
                    a0_re = H_re;            a0_im = H_im;
                    b_re = -H_re*s2_re_s;    b_im = -H_im*s2_re_s;
                    break;
                default: 
                    a1_re=a1_im=a0_re=a0_im=b_re=b_im=0.0f;
                    break;
            }

            int r_re = i, r_im = i + n;
            A_data[r_re*N_COEFFS+0] = a1_re*sqrt_W; A_data[r_im*N_COEFFS+0] = a1_im*sqrt_W;
            A_data[r_re*N_COEFFS+1] = a0_re*sqrt_W; A_data[r_im*N_COEFFS+1] = a0_im*sqrt_W;
            b_data[r_re] = b_re*sqrt_W; b_data[r_im] = b_im*sqrt_W;
        }

        arm_matrix_instance_f32 A={N_ROWS,N_COEFFS,A_data}, b={N_ROWS,1,b_data}, x={N_COEFFS,1,c_s};
        if(solve_least_squares(&A,&b,&x)!=ARM_MATH_SUCCESS){ *p1=NAN; *p0=NAN; return; }
        
        // --- NEW: 增加解的范数检查 ---
        float32_t norm_cs;
        arm_dot_prod_f32(c_s, c_s, N_COEFFS, &norm_cs);
        if (iter > 0 && norm_cs > 1e12f) {
             printf(" -> Warning: Unstable solution detected in 2-param. Reverting.\r\n");
            arm_copy_f32(c_prev, c_s, N_COEFFS);
        }
        // --- END NEW ---
        
        if(iter>0 && (c_s[0]<0.0f || c_s[1]<0.0f)){
            c_s[0]=0.7f*c_prev[0] + 0.3f*fmaxf(0.0f,c_s[0]);
            c_s[1]=0.7f*c_prev[1] + 0.3f*fmaxf(0.0f,c_s[1]);
        }
        
        float32_t d_v[2], d_n, p_n; arm_sub_f32(c_s,c_prev,d_v,2); arm_dot_prod_f32(d_v,d_v,2,&d_n); arm_dot_prod_f32(c_prev,c_prev,2,&p_n);
        if(iter>0 && p_n>1e-12f && (d_n/p_n)<1e-24f) break;

        arm_copy_f32(c_s, c_prev, 2);
        for(uint32_t i=0; i<n; ++i){ 
            float32_t s_im_s=s[2*i+1]/w_scale; 
            D_prev[2*i] = -s_im_s*s_im_s + c_s[1]; 
            D_prev[2*i+1] = s_im_s*c_s[0];
        }
    }
    *p1 = c_s[0] * w_scale;
    *p0 = c_s[1] * w_scale * w_scale;
}
static void fit_lpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db) {
    printf(" -> Using Robust Direct LPF Fitter...\r\n");
    const int N_COEFFS = 2; // a1_scaled, a0_scaled
    const int N_ROWS = 2 * n;
    static float32_t A_data[INPUT_NUM_FREQ_POINTS * 2 * 2];
    static float32_t b_data[INPUT_NUM_FREQ_POINTS * 2];

    float32_t w_scale;
    arm_sqrt_f32(s[1] * s[2*n - 1], &w_scale);
    float32_t w_scale_sq = w_scale * w_scale;

    // 方程: (H*s_s)*a1_s + (H-1)*a0_s = -H*s_s^2
    for (uint32_t i = 0; i < n; ++i) {
        float32_t s_im = s[i*2+1];
        float32_t H_re = H[i*2];
        float32_t H_im = H[i*2+1];
        
        float32_t s_im_s = s_im / w_scale;
        float32_t s2_re_s = -s_im_s * s_im_s;

        float32_t col1_re = -H_im * s_im_s; // real(H*s_s)
        float32_t col1_im =  H_re * s_im_s; // imag(H*s_s)
        float32_t col2_re = H_re - 1.0f;    // real(H-1)
        float32_t col2_im = H_im;           // imag(H-1)
        
        float32_t b_vec_re = -H_re * s2_re_s;
        float32_t b_vec_im = -H_im * s2_re_s;
        
        int r_re = i, r_im = i + n;
        A_data[r_re * N_COEFFS + 0] = col1_re; A_data[r_re * N_COEFFS + 1] = col2_re;
        A_data[r_im * N_COEFFS + 0] = col1_im; A_data[r_im * N_COEFFS + 1] = col2_im;
        b_data[r_re] = b_vec_re; b_data[r_im] = b_vec_im;
    }

    float32_t coeffs_scaled[N_COEFFS];
    arm_matrix_instance_f32 A = {N_ROWS, N_COEFFS, A_data};
    arm_matrix_instance_f32 b = {N_ROWS, 1, b_data};
    arm_matrix_instance_f32 x = {N_COEFFS, 1, coeffs_scaled};

    if (solve_least_squares(&A, &b, &x) == ARM_MATH_SUCCESS) {
        res->a1 = fmaxf(0.0f, coeffs_scaled[0] * w_scale);
        res->a0 = fmaxf(0.0f, coeffs_scaled[1] * w_scale_sq);
    } else {
        res->a1 = NAN; res->a0 = NAN;
    }
    
    res->b2 = 0.0f; res->b1 = 0.0f; res->b0 = res->a0;
}
static void fit_hpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db) {
    solve_2param_stabilized_c(&res->a1, &res->a0, s, H, n, floor_db, 1);
    res->b2 = 1.0f; res->b1 = 0.0f; res->b0 = 0.0f;
}
static void fit_bpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n, float32_t floor_db) {
    solve_2param_stabilized_c(&res->a1, &res->a0, s, H, n, floor_db, 2);
    res->b2 = 0.0f; res->b1 = res->a1; res->b0 = 0.0f;
}

static void fit_1st_order_lpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n) {
    static float32_t H_mag[INPUT_NUM_FREQ_POINTS];
    arm_cmplx_mag_f32((float32_t*)H, H_mag, n);

    OptParams params = {s, H_mag, n};
    float32_t w_min = s[1];
    float32_t w_max = s[2*n - 1];
    float32_t search_min = w_min * 0.01f;
    float32_t search_max = w_max * 100.0f;
    
    float32_t a0_fit = golden_section_search(search_min, search_max, 1e-6f * search_max, &rss_mag_lpf, &params);
    
    res->a1 = 0.0f; res->a0 = a0_fit;
    res->b2 = 0.0f; res->b1 = 0.0f; res->b0 = a0_fit;
}

static void fit_1st_order_hpf_constrained_c(TransferFunctionParams* res, const float32_t* s, const float32_t* H, uint32_t n) {
    static float32_t H_mag[INPUT_NUM_FREQ_POINTS];
    arm_cmplx_mag_f32((float32_t*)H, H_mag, n);
    
    OptParams params = {s, H_mag, n};
    float32_t w_min = s[1];
    float32_t w_max = s[2*n - 1];
    float32_t search_min = w_min * 0.01f;
    float32_t search_max = w_max * 100.0f;

    float32_t a0_fit = golden_section_search(search_min, search_max, 1e-6f * search_max, &rss_mag_hpf, &params);
    
    res->a1 = 0.0f; res->a0 = a0_fit;
    res->b2 = 0.0f; res->b1 = 1.0f; res->b0 = 0.0f;
}

static float32_t rss_mag_lpf(float32_t a0, const void* p) {
    const OptParams* params = (const OptParams*)p;
    float32_t rss = 0.0f;
    for (uint32_t i = 0; i < params->num_points; ++i) {
        float32_t w = params->s_cmplx[i*2+1];
        float32_t mag_fit_sq = (a0*a0) / (w*w + a0*a0);
        float32_t mag_fit;
        arm_sqrt_f32(mag_fit_sq, &mag_fit);
        float32_t error = mag_fit - params->H_mag[i];
        rss += error * error;
    }
    return rss;
}

static float32_t rss_mag_hpf(float32_t a0, const void* p) {
    const OptParams* params = (const OptParams*)p;
    float32_t rss = 0.0f;
    for (uint32_t i = 0; i < params->num_points; ++i) {
        float32_t w = params->s_cmplx[i*2+1];
        float32_t mag_fit_sq = (w*w) / (w*w + a0*a0);
        float32_t mag_fit;
        arm_sqrt_f32(mag_fit_sq, &mag_fit);
        float32_t error = mag_fit - params->H_mag[i];
        rss += error * error;
    }
    return rss;
}

static float32_t golden_section_search(float32_t a, float32_t b, float32_t tol, float32_t (*f)(float32_t, const void*), const void* params) {
    float32_t gr = (sqrtf(5.0f) - 1.0f) / 2.0f;
    float32_t c = b - gr * (b - a);
    float32_t d = a + gr * (b - a);
    float32_t fc = f(c, params);
    float32_t fd = f(d, params);

    while ((b - a) > tol) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - gr * (b - a);
            fc = f(c, params);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + gr * (b - a);
            fd = f(d, params);
        }
    }
    return (a + b) / 2.0f;
}






