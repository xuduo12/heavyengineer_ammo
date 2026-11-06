#ifndef ALG_FILTER_H
#define ALG_FILTER_H
#define ARM_MATH_CM4
#define __FPU_PRESENT 1
#include "arm_math.h"
#include "arm_math.h"
#include "FreeRTOS.h"
typedef struct {
    float32_t *pData;
    uint32_t blockSize;
    arm_fir_instance_f32 *S;
    float32_t freq[2];
    uint8_t type ;
} fliter_config;
typedef struct {
    float32_t buffer[8];
    uint32_t index;
    uint32_t count;
    float32_t running_sum;
    uint8_t filter_size;
    uint8_t initialized;
} MovingAvgFilter_t;
typedef struct {
    float32_t prev_output;
    float32_t alpha;
    uint8_t initialized;
    float32_t cutoff_freq;
    float32_t sample_freq;
} LowpassFilter_t;
typedef struct {
    uint8_t filter_size;
    float32_t cutoff_freq;
    float32_t sample_freq;
} FilterInitConfig_t;
typedef void (*FilterCallback)(float32_t input, fliter_config *config, float32_t *output);
void Move_aver_fliter(float32_t *input, float32_t *output, fliter_config *fliter_cfg) ;
void Lowpass_fliter(float32_t *input, float32_t *output, fliter_config *fliter_cfg);
MovingAvgFilter_t* MovingAvgFilter_Register(FilterInitConfig_t *config);
LowpassFilter_t* LowpassFilter_Register(FilterInitConfig_t *config);
void MovingAvgFilter_Process(MovingAvgFilter_t *filter, float32_t input, float32_t *output);
void LowpassFilter_Process(LowpassFilter_t *filter, float32_t input, float32_t *output);
void MovingAvgFilter_Free(MovingAvgFilter_t *filter);
void LowpassFilter_Free(LowpassFilter_t *filter);
float32_t fhan(float32_t x1, float32_t x2, float32_t r, float32_t h0);
float fhan_correct(float x1, float x2, float r, float h);
float float_constrain(float Value, float minValue, float maxValue);
int float_rounding(float raw);
#endif