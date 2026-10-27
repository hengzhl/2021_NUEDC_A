#include "thd_m0.h"
#include <math.h>

/* 声明外部旋转因子表和实例（由 CMSIS-DSP 库提供） */
extern const float32_t twiddleCoef_rfft_1024[1024];
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len512;

static arm_rfft_fast_instance_f32 rfft_inst;
static uint8_t rfft_inited = 0;

static float difft_buf[DIFFT_MAX_SIZE];
static float power_spectrum[DIFFT_MAX_SIZE / 2];

/*========================================================
  辅助函数：计算谐波比和失真度
========================================================*/
static void compute_post_metrics(DIFFT_Result *res)
{
    float A1 = res->Amp[0];
    if (A1 < 1e-6f) {
        // [修改点 3]：初始化所有 9 次谐波比为 0
        res->harmonicNorm.h2 = 0; res->harmonicNorm.h3 = 0;
        res->harmonicNorm.h4 = 0; res->harmonicNorm.h5 = 0;
        res->harmonicNorm.h6 = 0; res->harmonicNorm.h7 = 0;
        res->harmonicNorm.h8 = 0; res->harmonicNorm.h9 = 0;
        res->harmonicNorm.thd = 0;
        return;
    }

    float threshold = A1 * 0.001f;
    for (int i = 1; i < 9; i++) {
        if (res->Amp[i] < threshold) {
            res->Amp[i] = 0.0f; // 低于基波 0.1% 的分量强制归零
        }
    }


    // [修改点 4]：计算 2-9 次谐波比
    res->harmonicNorm.h2 = res->Amp[1] / A1;
    res->harmonicNorm.h3 = res->Amp[2] / A1;
    res->harmonicNorm.h4 = res->Amp[3] / A1;
    res->harmonicNorm.h5 = res->Amp[4] / A1;
    res->harmonicNorm.h6 = res->Amp[5] / A1;
    res->harmonicNorm.h7 = res->Amp[6] / A1;
    res->harmonicNorm.h8 = res->Amp[7] / A1;
    res->harmonicNorm.h9 = res->Amp[8] / A1;

    float sum_sq = 0;
    
    for (int i = 1; i < 9; i++) { 
        sum_sq += res->Amp[i] * res->Amp[i];
    }
    res->harmonicNorm.thd = sqrtf(sum_sq) / A1;
}

/*========================================================
  辅助函数：识别波形
========================================================*/
static void detect_wavetype(DIFFT_Result *res)
{
    float h3 = res->harmonicNorm.h3;
    if (h3 < 0.05f) {
        res->wavetype = 1; // Sine
    } else if (h3 > 0.08f && h3 < 0.15f) {
        res->wavetype = 2; // Triangle
    } else if (h3 > 0.25f && h3 < 0.45f) {
        res->wavetype = 3; // Square
    } else {
        res->wavetype = 0; // Unknown
    }
}

/*========================================================
  辅助函数：计算峰峰值 (mV)
========================================================*/
static void compute_vpp(DIFFT_Result *res)
{
    float A1 = res->Amp[0];
    float theoretical_vpp = 0;

    switch (res->wavetype) {
        case 1: theoretical_vpp = 2.0f * A1; break;
        case 2: theoretical_vpp = A1 * (MATH_PI * MATH_PI) / 4.0f; break;
        case 3: theoretical_vpp = A1 * MATH_PI / 2.0f; break;
        default: theoretical_vpp = 2.0f * A1; break;
    }
    res->vpp_mv = theoretical_vpp * 1650.0f;
}

/*========================================================
  核心分析函数
========================================================*/
uint8_t difft_run_adc(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result)
{
    if (N == 0 || N > DIFFT_MAX_SIZE || (N & (N - 1)) != 0) return 0xFF;

    if (!rfft_inited) {
        /* 手动初始化 1024 点 RFFT 实例，绕过沉重的通用 init 函数 */
        rfft_inst.fftLenRFFT = 1024;
        rfft_inst.pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_1024;
        rfft_inst.Sint = arm_cfft_sR_f32_len512; 
        rfft_inited = 1;
    }

    float sum = 0;
    for (uint32_t i = 0; i < N; i++) sum += (float)adc[i];
    result->dc = sum / (float)N;

    for (uint32_t i = 0; i < N; i++) {
        float val = ((float)adc[i] - result->dc) / DIFFT_ADC_MID;
        float w = 0.5f * (1.0f - cosf(2.0f * MATH_PI * i / (N - 1)));
        difft_buf[i] = val * w;
    }

    /* 执行 FFT */
    arm_rfft_fast_f32(&rfft_inst, difft_buf, difft_buf, 0);

    power_spectrum[0] = 0; 
    for (uint32_t k = 1; k < N / 2; k++) {
        float re = difft_buf[2 * k];
        float im = difft_buf[2 * k + 1];
        power_spectrum[k] = re * re + im * im;
    }

    float max_e = 0;
    uint32_t p_idx = 2;
    for (uint32_t k = 2; k < N / 2 - 1; k++) {
        if (power_spectrum[k] > max_e) {
            max_e = power_spectrum[k];
            p_idx = k;
        }
    }

    if (max_e < 1e-10f) return 0x01;

    const int span = 4;
    float e_sum = 0, e_idx_sum = 0;
    int ks = (p_idx > span) ? (p_idx - span) : 1;
    int ke = (p_idx + span < N / 2) ? (p_idx + span) : (N / 2 - 1);

    for (int k = ks; k <= ke; k++) {
        float E = power_spectrum[k];
        e_sum += E;
        e_idx_sum += E * (float)k;
    }

    result->k0 = e_idx_sum / e_sum;
    result->f = result->k0 * (Fs / (float)N);
    result->Amp[0] = sqrtf(e_sum * 8.0f / 3.0f) / (float)N * 2.0f;

    float bin_size = Fs / (float)N;
    for (uint32_t h = 2; h <= DIFFT_MAX_HARMONICS; h++) {
        float target_f = h * result->f;
        int target_bin = (int)lroundf(target_f / bin_size);
        if (target_bin < 2 || target_bin >= (int)N / 2 - 2) {
            result->Amp[h - 1] = 0;
            continue;
        }
        float h_e_sum = 0;
        for (int k = target_bin - span; k <= target_bin + span; k++) {
            if (k > 0 && k < (int)N / 2) h_e_sum += power_spectrum[k];
        }
        result->Amp[h - 1] = sqrtf(h_e_sum * 8.0f / 3.0f) / (float)N * 2.0f;
    }

    compute_post_metrics(result);
    detect_wavetype(result);
    compute_vpp(result);

    return 0;
}
