#ifndef __ADRC_H__
#define __ADRC_H__

#include "stdint.h"

#define ADRC_TD_DEFAULT_R    5000.0f
#define ADRC_TD_DEFAULT_H0    0.01f
#define ADRC_ESO_DEFAULT_W0   30.0f
#define ADRC_LSEF_DEFAULT_WC  15.0f
#define ADRC_OUT_MAX_DEFAULT 16384.0f
#define ADRC_B0_MIN           1e-4f

typedef enum
{
    ADRC_VEL = 1,
    ADRC_POS = 2,
} ADRC_order_e;

typedef struct
{
    ADRC_order_e order;
    
    float td_v1;
    float td_v2;
    float td_r;
    float td_h;
    
    float eso_z1;
    float eso_z2;
    float eso_z3;
    float eso_b0;
    float eso_beta1;
    float eso_beta2;
    float eso_beta3;
    float eso_w0;

    float lsef_kp;
    float lsef_kd;
    float lsef_wc;

    float set;
    float fdb;
    float out;
    float out_max;
    float error;
    float disturbance;
} ADRC_type_def;

void ADRC_Init(ADRC_type_def *ADRC, ADRC_order_e order, float w0, float wc, float b0, float r, float h, float out_max);
float ADRC_Calc(ADRC_type_def *ADRC, float ref, float set, int max);
void ADRC_SetESOBandwidth(ADRC_type_def *ADRC, float w0);
void ADRC_SetControllerBandwidth(ADRC_type_def *ADRC, float wc);
void ADRC_SetB0(ADRC_type_def *ADRC, float b0);
void ADRC_SetOutMax(ADRC_type_def *ADRC, float out_max);
void ADRC_Reset(ADRC_type_def *ADRC);
#endif
