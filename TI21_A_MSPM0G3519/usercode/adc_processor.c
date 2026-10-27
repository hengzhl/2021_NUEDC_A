#include "adc_processor.h"
#include <math.h>

/**
 * @brief 均值平滑滤波 (抑制ADC高频毛刺)
 * 使用4点滑动平均
 */
void ADC_Filter_Smooth(uint16_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len - 4; i++) {
        data[i] = (data[i] + data[i+1] + data[i+2] + data[i+3]) >> 2;
    }
}

/**
 * @brief 特征提取：计算最值、均值并寻找上升沿触发点
 */
WaveMetrics ADC_Analyze_Waveform(uint16_t *data, uint16_t len) {
    WaveMetrics m = {4095, 0, 0, 0};
    uint32_t sum = 0;

    // 1. 扫描最值和均值
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] < m.min_val) m.min_val = data[i];
        if (data[i] > m.max_val) m.max_val = data[i];
        sum += data[i];
    }
    m.avg_val = sum / len;

    // 2. 寻找上升沿触发点 (带迟滞判断以防噪声干扰)
    // 阈值设为均值，迟滞宽度设为总幅值的 5%
    uint16_t hysteresis = (m.max_val - m.min_val) / 20; 
    for (uint16_t i = 1; i < len - 200; i++) { // 预留足够长的后续数据点用于显示
        if (data[i] < (m.avg_val - hysteresis) && data[i+1] >= m.avg_val) {
            m.trigger_idx = i;
            break;
        }
    }
    return m;
}

/**
 * @brief 时域重采样与量化
 * @param step 步长 = (采样率 / 信号频率 * 显示周期数) / DISPLAY_POINTS
 */
void ADC_Normalize_For_Display(uint16_t *in, uint8_t *out, WaveMetrics *m, float step) {
    float range = (float)(m->max_val - m->min_val);
    if (range < 1.0f) range = 1.0f; // 防止除零

    for (uint16_t i = 0; i < DISPLAY_POINTS; i++) {
        // 计算采样索引
        float read_idx = m->trigger_idx + (i * step);
        
        // 线性插值（防止高频时波形变折线）
        uint16_t idx_low = (uint16_t)read_idx;
        uint16_t idx_high = idx_low + 1;
        float weight = read_idx - idx_low;
        
        float val = (1.0f - weight) * in[idx_low] + weight * in[idx_high];

        // 归一化到 0-200 (适配串口屏控件高度)
        float normalized = ((val - m->min_val) / range) * 200.0f;
        out[i] = (uint8_t)normalized;
    }
}