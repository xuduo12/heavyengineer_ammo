#include "alg_filter.h"
#include <stdio.h>
#include "arm_math.h"
#include <math.h>
float d;
float a0;
float y;
float a1;
float a2;
float a;
float float_constrain(float Value, float minValue, float maxValue)
{
    if (Value < minValue)
        return minValue;
    else if (Value > maxValue)
        return maxValue;
    else
        return Value;
}
int float_rounding(float raw)
{
    static int integer;
    static float decimal;
    integer = (int)raw;
    decimal = raw - integer;
    if (decimal > 0.5f)
        integer++;
    return integer;
}
float fhan_correct(float x1, float x2, float r, float h) {
    float d, d0, y, a0, a;
    d = r * h;
    d0 = h * d;
    y = x1 + h * x2;
    a0 = sqrtf(d * d + 8.0f * r * fabsf(y));
    if (fabsf(y) > d0) {
        a = x2 + (a0 - d) / 2.0f * ((y > 0) ? 1.0f : -1.0f);
    } else {
        a = x2 + y / h;
    }
    if (fabsf(a) > d) {
        return -r * ((a > 0) ? 1.0f : -1.0f);
    } else {
        return -r * a / d;
    }
}
void Move_aver_fliter(float32_t *input, float32_t *output, fliter_config *fliter_cfg) {
    enum {
        BUFFER_BITS = 3,
        BUFFER_SIZE = 1 << BUFFER_BITS,
        BUFFER_MASK = BUFFER_SIZE - 1
    };
    static float32_t buffer[BUFFER_SIZE] = {0};
    static uint32_t index = 0;
    static uint32_t count = 0;
    static float32_t running_sum = 0.0f;
    uint8_t filter_size = fliter_cfg->blockSize;
    filter_size = (filter_size > BUFFER_SIZE) ? BUFFER_SIZE : filter_size;
    if (count >= filter_size) {
        uint8_t old_index = (index - filter_size) & BUFFER_MASK;
        running_sum -= buffer[old_index];
    } else {
        count++;
    }
    buffer[index & BUFFER_MASK] = input[0];
    running_sum += input[0];
    index++;
    output[0] = running_sum / count;
}
void Lowpass_fliter(float32_t *input, float32_t *output, fliter_config *fliter_cfg){
    static float32_t prev_output = 0.0f;
    static uint8_t initialized = 0;
    float32_t alpha = 0.1f;
    if (fliter_cfg->freq[0] > 0 && fliter_cfg->freq[1] > 0) {
        float32_t dt = 1.0f / fliter_cfg->freq[0];
        float32_t tau = 1.0f / (2.0f * PI * fliter_cfg->freq[1]);
        alpha = dt / (tau + dt);
        alpha= (alpha > 1.0f)? 1.0f:alpha ;
        alpha= (alpha < 0.0f)? 0.0f:alpha ;
    }
    if (!initialized) {
        prev_output = input[0];
        initialized = 1;
        output[0] = input[0];
        return;
    }
    output[0] = alpha * input[0] + (1.0f - alpha) * prev_output;
    prev_output = output[0];
}
MovingAvgFilter_t* MovingAvgFilter_Register(FilterInitConfig_t *config) {
    if (config == NULL) {
        return NULL;
    }
    MovingAvgFilter_t *filter = (MovingAvgFilter_t *)pvPortMalloc(sizeof(MovingAvgFilter_t));
    if (filter == NULL) {
        return NULL;
    }
    memset(filter->buffer, 0, sizeof(filter->buffer));
    filter->index = 0;
    filter->count = 0;
    filter->running_sum = 0.0f;
    filter->filter_size = (config->filter_size > 8) ? 8 : config->filter_size;
    filter->initialized = 1;
    return filter;
}
LowpassFilter_t* LowpassFilter_Register(FilterInitConfig_t *config) {
    if (config == NULL || config->sample_freq <= 0 || config->cutoff_freq <= 0) {
        return NULL;
    }
    LowpassFilter_t *filter = (LowpassFilter_t *)pvPortMalloc(sizeof(LowpassFilter_t));
    if (filter == NULL) {
        return NULL;
    }
    filter->prev_output = 0.0f;
    filter->initialized = 0;
    filter->cutoff_freq = config->cutoff_freq;
    filter->sample_freq = config->sample_freq;
    float32_t dt = 1.0f / config->sample_freq;
    float32_t tau = 1.0f / (2.0f * PI * config->cutoff_freq);
    filter->alpha = dt / (tau + dt);
    if (filter->alpha > 1.0f) filter->alpha = 1.0f;
    if (filter->alpha < 0.0f) filter->alpha = 0.0f;
    return filter;
}
void MovingAvgFilter_Process(MovingAvgFilter_t *filter, float32_t input, float32_t *output) {
    if (filter == NULL || output == NULL) {
        return;
    }
    const uint8_t BUFFER_MASK = 7;
    if (filter->count >= filter->filter_size) {
        uint8_t old_index = (filter->index - filter->filter_size) & BUFFER_MASK;
        filter->running_sum -= filter->buffer[old_index];
    } else {
        filter->count++;
    }
    filter->buffer[filter->index & BUFFER_MASK] = input;
    filter->running_sum += input;
    filter->index++;
    *output = filter->running_sum / filter->count;
}
void LowpassFilter_Process(LowpassFilter_t *filter, float32_t input, float32_t *output) {
    if (filter == NULL || output == NULL) {
        return;
    }
    if (!filter->initialized) {
        filter->prev_output = input;
        filter->initialized = 1;
        *output = input;
        return;
    }
    *output = filter->alpha * input + (1.0f - filter->alpha) * filter->prev_output;
    filter->prev_output = *output;
}
void MovingAvgFilter_Free(MovingAvgFilter_t *filter) {
    if (filter != NULL) {
        vPortFree(filter);
    }
}
void LowpassFilter_Free(LowpassFilter_t *filter) {
    if (filter != NULL) {
        vPortFree(filter);
    }
}