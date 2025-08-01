// filter_identification.c (Corrected for Stack Overflow)

#include "filter_identification.h"
#include <math.h>
#include <stdio.h> // 為了穩健性，建議包含此頭文件

/*
================================================================================
 內部輔助函數
================================================================================
*/

// 1. 濾波器類型識別
static FilterType classify_filter_type(const float32_t* H_measured_cmplx) {
    float32_t gain_dc_sq, gain_hf_sq;
    float32_t gain_dc, gain_hf;

    // 計算直流增益 |H(0)|
    arm_cmplx_mag_squared_f32((float32_t*)&H_measured_cmplx[0], &gain_dc_sq, 1);
    arm_sqrt_f32(gain_dc_sq, &gain_dc);

    // 計算高頻增益 |H(end)|
    arm_cmplx_mag_squared_f32((float32_t*)&H_measured_cmplx[(NUM_FREQ_POINTS - 1) * 2], &gain_hf_sq, 1);
    arm_sqrt_f32(gain_hf_sq, &gain_hf);

    // 閾值判斷
    const float32_t high_thresh = 0.7f;
    const float32_t low_thresh = 0.3f;

    if (gain_dc > high_thresh && gain_hf < low_thresh) return FILTER_TYPE_LPF;
    if (gain_dc < low_thresh && gain_hf > high_thresh) return FILTER_TYPE_HPF;
    if (gain_dc < low_thresh && gain_hf < low_thresh) return FILTER_TYPE_BPF;
    if (gain_dc > high_thresh && gain_hf > high_thresh) return FILTER_TYPE_BSF;

    return FILTER_TYPE_UNKNOWN;
}


// 2. 關鍵：求解超定最小二乘問題 Ax = b
static arm_status solve_least_squares_qr(
    arm_matrix_instance_f32* A,
    arm_matrix_instance_f32* b,
    arm_matrix_instance_f32* x)
{
    // **修正**: 將大型陣列宣告為 static，避免堆疊溢位
    // 陣列大小根據最大可能的需求（通用算法 num_coeffs=5）設定
    static float32_t R_inv_data[5 * 5];
    static float32_t A_T_data[5 * (2 * NUM_FREQ_POINTS)];
    static float32_t A_T_A_data[5 * 5];
    static float32_t A_T_b_data[5 * 1];

    // **修正**: 移除未使用的變數 R
    arm_matrix_instance_f32 R_inv = {x->numRows, x->numRows, R_inv_data};
    arm_matrix_instance_f32 A_T = {A->numCols, A->numRows, A_T_data};
    arm_matrix_instance_f32 A_T_A = {x->numRows, x->numRows, A_T_A_data};
    arm_matrix_instance_f32 A_T_b = {x->numRows, 1, A_T_b_data};
    
    arm_status status;

    status = arm_mat_trans_f32(A, &A_T);
    if (status != ARM_MATH_SUCCESS) return status;

    status = arm_mat_mult_f32(&A_T, A, &A_T_A);
    if (status != ARM_MATH_SUCCESS) return status;

    status = arm_mat_mult_f32(&A_T, b, &A_T_b);
    if (status != ARM_MATH_SUCCESS) return status;
    
    // 求 A'*A 的逆
    status = arm_mat_inverse_f32(&A_T_A, &R_inv);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }

    // x = (A'A)^-1 * (A'b)
    status = arm_mat_mult_f32(&R_inv, &A_T_b, x);
    
    return status;
}

// 3. 移植通用SSK算法 (fit_ssk_general)
static void fit_ssk_general_arm(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,
    const float32_t* H_measured_cmplx)
{
    // --- 記憶體定義 ---
    const int num_coeffs = 5;
    const int num_rows = 2 * NUM_FREQ_POINTS;
    
    // **修正**: 將大型陣列宣告為 static，避免堆疊溢位
    static float32_t A_real_data[num_rows * num_coeffs];
    static float32_t b_real_data[num_rows];
    static float32_t D_prev_cmplx[NUM_FREQ_POINTS * 2];
    static float32_t W_data[NUM_FREQ_POINTS];
    // 下面這兩個陣列很小，可以保留在堆疊上
    float32_t coeffs_scaled_data[num_coeffs];
    float32_t coeffs_prev_data[num_coeffs];

    // --- 初始化 ---
    const int max_iter = 15;
    const float32_t tolerance = 1e-9f;
    float32_t w_scale;
    arm_sqrt_f32(w_rad[0] * w_rad[NUM_FREQ_POINTS - 1], &w_scale);
    const float32_t w_scale_sq = w_scale * w_scale;

    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        D_prev_cmplx[i * 2] = 1.0f;
        D_prev_cmplx[i * 2 + 1] = 0.0f;
    }
    arm_fill_f32(0.0f, coeffs_prev_data, num_coeffs);


    // --- 迭代求解 ---
    for (int iter = 0; iter < max_iter; iter++) {
        // 1. 計算權重
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t mag_sq;
            arm_cmplx_mag_squared_f32(&D_prev_cmplx[i*2], &mag_sq, 1);
            W_data[i] = 1.0f / (mag_sq + 1e-12f);
        }

        // 2. 構建矩陣
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_re = 0;
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            float32_t s2_im = 0;
            float32_t H_re = H_measured_cmplx[i * 2];
            float32_t H_im = H_measured_cmplx[i * 2 + 1];
            float32_t Hs_re = H_re * s_re - H_im * s_im;
            float32_t Hs_im = H_re * s_im + H_im * s_re;
            float32_t b_re = -(H_re * s2_re - H_im * s2_im);
            float32_t b_im = -(H_re * s2_im + H_im * s2_re);
            float32_t sqrt_W;
            arm_sqrt_f32(W_data[i], &sqrt_W);

            int row_re = i;
            int row_im = i + NUM_FREQ_POINTS;

            A_real_data[row_re * num_coeffs + 0] = Hs_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 0] = Hs_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 1] = H_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 1] = H_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 2] = -s2_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 2] = -s2_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 3] = -s_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 3] = -s_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 4] = -1.0f * sqrt_W;
            A_real_data[row_im * num_coeffs + 4] = 0.0f * sqrt_W;
            
            b_real_data[row_re] = b_re * sqrt_W;
            b_real_data[row_im] = b_im * sqrt_W;
        }

        // 3. 求解
        arm_matrix_instance_f32 A_mat = {num_rows, num_coeffs, A_real_data};
        arm_matrix_instance_f32 b_mat = {num_rows, 1, b_real_data};
        arm_matrix_instance_f32 x_mat = {num_coeffs, 1, coeffs_scaled_data};
        
        arm_status status = solve_least_squares_qr(&A_mat, &b_mat, &x_mat);
        if (status != ARM_MATH_SUCCESS) {
            result->b0 = NAN;
            return;
        }

        // 4. 更新
        float32_t a1_s = coeffs_scaled_data[0];
        float32_t a0_s = coeffs_scaled_data[1];
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            D_prev_cmplx[i * 2]     = s2_re + a1_s * 0 + a0_s;
            D_prev_cmplx[i * 2 + 1] = 0     + a1_s * s_im + 0;
        }

        // 5. 檢查收斂
        if (iter > 0) {
            float32_t diff_norm, prev_norm, ratio;
            float32_t temp_vec[num_coeffs];
            arm_sub_f32(coeffs_scaled_data, coeffs_prev_data, temp_vec, num_coeffs);
            arm_power_f32(temp_vec, num_coeffs, &diff_norm);
            arm_power_f32(coeffs_prev_data, num_coeffs, &prev_norm);
            if (prev_norm > 1e-12f) {
                ratio = diff_norm / prev_norm;
                if (ratio < tolerance) {
                    break;
                }
            }
        }
        arm_copy_f32(coeffs_scaled_data, coeffs_prev_data, num_coeffs);
    }
    
    // --- 反歸一化並存儲結果 ---
    result->a1 = coeffs_scaled_data[0] * w_scale;
    result->a0 = coeffs_scaled_data[1] * w_scale_sq;
    result->b2 = coeffs_scaled_data[2];
    result->b1 = coeffs_scaled_data[3] * w_scale;
    result->b0 = coeffs_scaled_data[4] * w_scale_sq;
}

// --- 專用於 LPF 的迭代約束算法 ---
static void fit_lpf_iterative_constrained_arm(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,
    const float32_t* H_measured_cmplx)
{
    // --- 記憶體定義 ---
    const int num_coeffs = 2;
    const int num_rows = 2 * NUM_FREQ_POINTS;
    
    // **修正**: 將大型陣列宣告為 static
    static float32_t A_real_data[num_rows * num_coeffs];
    static float32_t b_real_data[num_rows];
    static float32_t D_prev_cmplx[NUM_FREQ_POINTS * 2];
    static float32_t W_data[NUM_FREQ_POINTS];
    float32_t coeffs_scaled_data[num_coeffs];
    float32_t coeffs_prev_data[num_coeffs];

    // --- 初始化 ---
    const int max_iter = 15;
    const float32_t tolerance = 1e-9f;
    float32_t w_scale;
    arm_sqrt_f32(w_rad[0] * w_rad[NUM_FREQ_POINTS - 1], &w_scale);
    const float32_t w_scale_sq = w_scale * w_scale;

    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        D_prev_cmplx[i * 2] = 1.0f;
        D_prev_cmplx[i * 2 + 1] = 0.0f;
    }
    arm_fill_f32(0.0f, coeffs_prev_data, num_coeffs);

    // --- 迭代求解 ---
    for (int iter = 0; iter < max_iter; iter++) {
        // 1. 計算權重
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t mag_sq;
            arm_cmplx_mag_squared_f32(&D_prev_cmplx[i*2], &mag_sq, 1);
            W_data[i] = 1.0f / (mag_sq + 1e-12f);
        }

        // 2. 構建矩陣
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            float32_t H_re = H_measured_cmplx[i * 2];
            float32_t H_im = H_measured_cmplx[i * 2 + 1];
            float32_t Hs_re = -H_im * s_im;
            float32_t Hs_im =  H_re * s_im;
            float32_t b_re = -(H_re * s2_re);
            float32_t b_im = -(H_im * s2_re);
            float32_t sqrt_W;
            arm_sqrt_f32(W_data[i], &sqrt_W);

            int row_re = i;
            int row_im = i + NUM_FREQ_POINTS;

            A_real_data[row_re * num_coeffs + 0] = Hs_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 0] = Hs_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 1] = (H_re - 1.0f) * sqrt_W;
            A_real_data[row_im * num_coeffs + 1] = H_im * sqrt_W;
            
            b_real_data[row_re] = b_re * sqrt_W;
            b_real_data[row_im] = b_im * sqrt_W;
        }

        // 3. 求解
        arm_matrix_instance_f32 A_mat = {num_rows, num_coeffs, A_real_data};
        arm_matrix_instance_f32 b_mat = {num_rows, 1, b_real_data};
        arm_matrix_instance_f32 x_mat = {num_coeffs, 1, coeffs_scaled_data};
        if (solve_least_squares_qr(&A_mat, &b_mat, &x_mat) != ARM_MATH_SUCCESS) {
            result->b0 = NAN; return;
        }

        // 4. 更新
        float32_t a1_s = coeffs_scaled_data[0];
        float32_t a0_s = coeffs_scaled_data[1];
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            D_prev_cmplx[i * 2]     = s2_re + a0_s;
            D_prev_cmplx[i * 2 + 1] = a1_s * s_im;
        }

        // 5. 檢查收斂
        if (iter > 0) {
            float32_t diff_norm_sq, prev_norm_sq, ratio;
            float32_t temp_vec[num_coeffs];
            arm_sub_f32(coeffs_scaled_data, coeffs_prev_data, temp_vec, num_coeffs);
            arm_dot_prod_f32(temp_vec, temp_vec, num_coeffs, &diff_norm_sq);
            arm_dot_prod_f32(coeffs_prev_data, coeffs_prev_data, num_coeffs, &prev_norm_sq);
            if (prev_norm_sq > 1e-12f) {
                ratio = diff_norm_sq / prev_norm_sq;
                if (ratio < tolerance) break;
            }
        }
        arm_copy_f32(coeffs_scaled_data, coeffs_prev_data, num_coeffs);
    }
    
    // --- 反歸一化並存儲結果 ---
    result->a1 = coeffs_scaled_data[0] * w_scale;
    result->a0 = coeffs_scaled_data[1] * w_scale_sq;
    result->b2 = 0.0f;
    result->b1 = 0.0f;
    result->b0 = result->a0;
}

// --- 專用於 BSF 的迭代約束算法 ---
static void fit_bsf_iterative_constrained_arm(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,
    const float32_t* H_measured_cmplx)
{
    // --- 記憶體定義 ---
    const int num_coeffs = 2;
    const int num_rows = 2 * NUM_FREQ_POINTS;
    
    // **修正**: 將大型陣列宣告為 static
    static float32_t A_real_data[num_rows * num_coeffs];
    static float32_t b_real_data[num_rows];
    static float32_t D_prev_cmplx[NUM_FREQ_POINTS * 2];
    static float32_t W_data[NUM_FREQ_POINTS];
    float32_t coeffs_scaled_data[num_coeffs];
    float32_t coeffs_prev_data[num_coeffs];

    // --- 初始化 ---
    const int max_iter = 15;
    const float32_t tolerance = 1e-9f;
    float32_t w_scale;
    arm_sqrt_f32(w_rad[0] * w_rad[NUM_FREQ_POINTS - 1], &w_scale);
    const float32_t w_scale_sq = w_scale * w_scale;

    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        D_prev_cmplx[i * 2] = 1.0f;
        D_prev_cmplx[i * 2 + 1] = 0.0f;
    }
    arm_fill_f32(0.0f, coeffs_prev_data, num_coeffs);

    // --- 迭代求解 ---
    for (int iter = 0; iter < max_iter; iter++) {
        // 1. 計算權重
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t mag_sq;
            arm_cmplx_mag_squared_f32(&D_prev_cmplx[i*2], &mag_sq, 1);
            W_data[i] = 1.0f / (mag_sq + 1e-12f);
        }

        // 2. 構建矩陣
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            float32_t H_re = H_measured_cmplx[i * 2];
            float32_t H_im = H_measured_cmplx[i * 2 + 1];
            float32_t Hs_re = -H_im * s_im;
            float32_t Hs_im =  H_re * s_im;
            float32_t b_re = s2_re * (1.0f - H_re);
            float32_t b_im = s2_re * (-H_im);
            float32_t sqrt_W;
            arm_sqrt_f32(W_data[i], &sqrt_W);

            int row_re = i;
            int row_im = i + NUM_FREQ_POINTS;
            
            A_real_data[row_re * num_coeffs + 0] = Hs_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 0] = Hs_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 1] = (H_re - 1.0f) * sqrt_W;
            A_real_data[row_im * num_coeffs + 1] = H_im * sqrt_W;
            
            b_real_data[row_re] = b_re * sqrt_W;
            b_real_data[row_im] = b_im * sqrt_W;
        }

        // 3. 求解
        arm_matrix_instance_f32 A_mat = {num_rows, num_coeffs, A_real_data};
        arm_matrix_instance_f32 b_mat = {num_rows, 1, b_real_data};
        arm_matrix_instance_f32 x_mat = {num_coeffs, 1, coeffs_scaled_data};
        if (solve_least_squares_qr(&A_mat, &b_mat, &x_mat) != ARM_MATH_SUCCESS) {
            result->b0 = NAN; return;
        }

        // 4. 更新
        float32_t a1_s = coeffs_scaled_data[0];
        float32_t a0_s = coeffs_scaled_data[1];
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            D_prev_cmplx[i * 2]     = s2_re + a0_s;
            D_prev_cmplx[i * 2 + 1] = a1_s * s_im;
        }

        // 5. 檢查收斂
        if (iter > 0) {
            float32_t diff_norm_sq, prev_norm_sq, ratio;
            float32_t temp_vec[num_coeffs];
            arm_sub_f32(coeffs_scaled_data, coeffs_prev_data, temp_vec, num_coeffs);
            arm_dot_prod_f32(temp_vec, temp_vec, num_coeffs, &diff_norm_sq);
            arm_dot_prod_f32(coeffs_prev_data, coeffs_prev_data, num_coeffs, &prev_norm_sq);
            if (prev_norm_sq > 1e-12f) {
                ratio = diff_norm_sq / prev_norm_sq;
                if (ratio < tolerance) break;
            }
        }
        arm_copy_f32(coeffs_scaled_data, coeffs_prev_data, num_coeffs);
    }
    
    // --- 反歸一化並存儲結果 ---
    result->a1 = coeffs_scaled_data[0] * w_scale;
    result->a0 = coeffs_scaled_data[1] * w_scale_sq;
    result->b2 = 1.0f;
    result->b1 = 0.0f;
    result->b0 = result->a0;
}


// --- 專用於 HPF 的迭代算法 ---
static void fit_hpf_iterative_arm(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,
    const float32_t* H_measured_cmplx)
{
    // --- 記憶體定義 ---
    const int num_coeffs = 3; // [a1_scaled, a0_scaled, b2]
    const int num_rows = 2 * NUM_FREQ_POINTS;
    
    // **修正**: 將大型陣列宣告為 static
    static float32_t A_real_data[num_rows * num_coeffs];
    static float32_t b_real_data[num_rows];
    static float32_t D_prev_cmplx[NUM_FREQ_POINTS * 2];
    static float32_t W_data[NUM_FREQ_POINTS];
    float32_t coeffs_scaled_data[num_coeffs];
    float32_t coeffs_prev_data[num_coeffs];

    // --- 初始化 ---
    const int max_iter = 15;
    const float32_t tolerance = 1e-9f;
    float32_t w_scale;
    arm_sqrt_f32(w_rad[0] * w_rad[NUM_FREQ_POINTS - 1], &w_scale);
    const float32_t w_scale_sq = w_scale * w_scale;

    for (int i = 0; i < NUM_FREQ_POINTS; i++) {
        D_prev_cmplx[i * 2] = 1.0f;
        D_prev_cmplx[i * 2 + 1] = 0.0f;
    }
    arm_fill_f32(0.0f, coeffs_prev_data, num_coeffs);

    // --- 迭代求解 ---
    for (int iter = 0; iter < max_iter; iter++) {
        // 1. 計算權重
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t mag_sq;
            arm_cmplx_mag_squared_f32(&D_prev_cmplx[i*2], &mag_sq, 1);
            W_data[i] = 1.0f / (mag_sq + 1e-12f);
        }

        // 2. 構建矩陣
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            float32_t H_re = H_measured_cmplx[i * 2];
            float32_t H_im = H_measured_cmplx[i * 2 + 1];
            float32_t Hs_re = -H_im * s_im;
            float32_t Hs_im =  H_re * s_im;
            float32_t b_re = -(H_re * s2_re);
            float32_t b_im = -(H_im * s2_re);
            float32_t sqrt_W;
            arm_sqrt_f32(W_data[i], &sqrt_W);

            int row_re = i;
            int row_im = i + NUM_FREQ_POINTS;
            
            A_real_data[row_re * num_coeffs + 0] = Hs_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 0] = Hs_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 1] = H_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 1] = H_im * sqrt_W;
            A_real_data[row_re * num_coeffs + 2] = -s2_re * sqrt_W;
            A_real_data[row_im * num_coeffs + 2] = 0.0f;
            
            b_real_data[row_re] = b_re * sqrt_W;
            b_real_data[row_im] = b_im * sqrt_W;
        }

        // 3. 求解
        arm_matrix_instance_f32 A_mat = {num_rows, num_coeffs, A_real_data};
        arm_matrix_instance_f32 b_mat = {num_rows, 1, b_real_data};
        arm_matrix_instance_f32 x_mat = {num_coeffs, 1, coeffs_scaled_data};
        if (solve_least_squares_qr(&A_mat, &b_mat, &x_mat) != ARM_MATH_SUCCESS) {
            result->b0 = NAN; return;
        }

        // 4. 更新
        float32_t a1_s = coeffs_scaled_data[0];
        float32_t a0_s = coeffs_scaled_data[1];
        for (int i = 0; i < NUM_FREQ_POINTS; i++) {
            float32_t s_im = w_rad[i] / w_scale;
            float32_t s2_re = -s_im * s_im;
            D_prev_cmplx[i * 2]     = s2_re + a0_s;
            D_prev_cmplx[i * 2 + 1] = a1_s * s_im;
        }

        // 5. 檢查收斂
        if (iter > 0) {
            float32_t diff_norm_sq, prev_norm_sq, ratio;
            float32_t temp_vec[num_coeffs];
            arm_sub_f32(coeffs_scaled_data, coeffs_prev_data, temp_vec, num_coeffs);
            arm_dot_prod_f32(temp_vec, temp_vec, num_coeffs, &diff_norm_sq);
            arm_dot_prod_f32(coeffs_prev_data, coeffs_prev_data, num_coeffs, &prev_norm_sq);
            if (prev_norm_sq > 1e-12f) {
                ratio = diff_norm_sq / prev_norm_sq;
                if (ratio < tolerance) break;
            }
        }
        arm_copy_f32(coeffs_scaled_data, coeffs_prev_data, num_coeffs);
    }
    
    // --- 反歸一化並存儲結果 ---
    result->a1 = coeffs_scaled_data[0] * w_scale;
    result->a0 = coeffs_scaled_data[1] * w_scale_sq;
    result->b2 = coeffs_scaled_data[2];
    result->b1 = 0.0f;
    result->b0 = 0.0f;
}

/*
================================================================================
 公共接口函數
================================================================================
*/

void identify_filter(
    ContinuousTransferFunction* result,
    const float32_t* w_rad,
    const float32_t* H_measured_cmplx)
{
    // 1. 自動識別濾波器類型
    result->identified_type = classify_filter_type(H_measured_cmplx);

    // 2. 呼叫相應的辨識算法
    switch(result->identified_type) {
        case FILTER_TYPE_LPF:
             fit_lpf_iterative_constrained_arm(result, w_rad, H_measured_cmplx);
             break;
        case FILTER_TYPE_HPF:
             fit_hpf_iterative_arm(result, w_rad, H_measured_cmplx);
             break;
        case FILTER_TYPE_BSF:
             fit_bsf_iterative_constrained_arm(result, w_rad, H_measured_cmplx);
             break;
        case FILTER_TYPE_BPF:
        case FILTER_TYPE_UNKNOWN:
        default:
            fit_ssk_general_arm(result, w_rad, H_measured_cmplx);
            break;
    }
}