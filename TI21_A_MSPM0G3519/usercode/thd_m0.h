#ifndef THD_M0_H
#define THD_M0_H

#include "ti/driverlib/driverlib.h" 
#include "arm_math.h"
#include <stdint.h>
#include <math.h>

#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846f
#endif

#define ADC_SAMPLES       1024
#define DIFFT_MAX_SIZE    ADC_SAMPLES
#define DIFFT_MAX_HARMONICS 9
#define DIFFT_ADC_MAX     4095.0f
#define DIFFT_ADC_MID     (DIFFT_ADC_MAX / 2.0f)

/* 1. 谐波归一化结构体 */
typedef struct
{
    float h2;
    float h3;   
    float h4;   
    float h5;   
    float h6;
    float h7;
    float h8;
    float h9;
    float thd;  
} HarmonicNorm;

/* 2. FFT 分析结果核心结构体 */
typedef struct
{
    float dc;          // 直流偏置
    float f;           // 精确频率 (Hz)
    float k0;          // 频谱精确索引

    float Amp[DIFFT_MAX_HARMONICS]; // 各次谐波幅值 [0]=基波, [1]=2次...
    uint32_t harmonics;             // 实际测量的谐波数量

    float vpp_mv;       // 峰峰值 (mV)
    uint8_t wavetype;   // 0-未知, 1-正弦, 2-三角, 3-方波
    
    HarmonicNorm harmonicNorm; 

} DIFFT_Result;

/**
 * @brief 核心分析函数
 */
uint8_t difft_run_adc(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result);

#endif 
