#ifndef ADC_PROCESSOR_H
#define ADC_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#define RAW_DATA_SIZE    1024
#define DISPLAY_POINTS   120

typedef struct {
    uint16_t min_val;
    uint16_t max_val;
    uint16_t avg_val;
    uint16_t trigger_idx;
} WaveMetrics; 

// 函数声明
void ADC_Filter_Smooth(uint16_t *data, uint16_t len);
WaveMetrics ADC_Analyze_Waveform(uint16_t *data, uint16_t len);
void ADC_Normalize_For_Display(uint16_t *in, uint8_t *out, WaveMetrics *m, float step);

#endif