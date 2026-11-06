#include "ADRC.h"
#include <math.h>

static void ADRC_td_update(ADRC_type_def *ADRC, float target);
static void ADRC_eso_update(ADRC_type_def *ADRC, float y, float u);
static float ADRC_lsef_calc(ADRC_type_def *ADRC);
static float ADRC_fhan(float v1, float v2, float r, float h);

void ADRC_Init(ADRC_type_def *ADRC, ADRC_order_e order, float w0, float wc, float b0, float r, float h, float out_max)
{
    ADRC->order = order;
    ADRC->td_r  = r;
    ADRC->td_h  = h;
    ADRC->td_v1 = 0.0f;
    ADRC->td_v2 = 0.0f;
    ADRC->eso_w0 = w0;
    ADRC->eso_b0 = (fabsf(b0) >= ADRC_B0_MIN) ? b0 : ADRC_B0_MIN;
    if (order == ADRC_VEL)
    {
        ADRC->eso_beta1 = 2.0f * w0;
        ADRC->eso_beta2 = w0 * w0;
        ADRC->eso_beta3 = 0.0f;
    }
    else
    {
        ADRC->eso_beta1 = 3.0f * w0;
        ADRC->eso_beta2 = 3.0f * w0 * w0;
        ADRC->eso_beta3 = w0 * w0 * w0;
    }
    ADRC->eso_z1 = 0.0f;
    ADRC->eso_z2 = 0.0f;
    ADRC->eso_z3 = 0.0f;
    ADRC->lsef_wc = wc;
    if (order == ADRC_VEL)
    {
        ADRC->lsef_kp = wc;
        ADRC->lsef_kd = 0.0f;
    }
    else
    {
        ADRC->lsef_kp = wc * wc;
        ADRC->lsef_kd = 2.0f * wc;
    }
    ADRC->out_max = out_max;
    ADRC->set   = 0.0f;
    ADRC->fdb   = 0.0f;
    ADRC->out   = 0.0f;
    ADRC->error = 0.0f;
    ADRC->disturbance = 0.0f;
}

static float ADRC_fhan(float v1, float v2, float r, float h)
{
    float d  = r * h;
    float d0 = h * d;
    float y  = v1 + h * v2;
    float a;
    float a1;
    a1 = sqrtf(d * d + 8.0f * r * fabsf(y));
    if (fabsf(y) <= d0)
    {
        a = v2 + y / h;
    }
    else
    {
        if (y > 0.0f)
            a = v2 + (a1 - d) / 2.0f;
        else
            a = v2 - (a1 - d) / 2.0f;
    }
    if (fabsf(a) <= d)
    {
        return -r * a / d;
    }
    else
    {
        return (a > 0.0f) ? -r : r;
    }
}


static void ADRC_td_update(ADRC_type_def *ADRC, float target)
{
    float v1 = ADRC->td_v1;
    float v2 = ADRC->td_v2;
    float r  = ADRC->td_r;
    float h  = ADRC->td_h;
    float fh = ADRC_fhan(v1 - target, v2, r, h);
    ADRC->td_v1 = v1 + h * v2;
    ADRC->td_v2 = v2 + h * fh;
}

static void ADRC_eso_update(ADRC_type_def *ADRC, float y, float u)
{
    float h  = ADRC->td_h;
    float b0 = ADRC->eso_b0;
    float b1 = ADRC->eso_beta1;
    float b2 = ADRC->eso_beta2;
    float b3 = ADRC->eso_beta3;
    float e = ADRC->eso_z1 - y;
    if (ADRC->order == ADRC_VEL)
    {
        ADRC->eso_z1 = ADRC->eso_z1 + h * (ADRC->eso_z2 - b1 * e + b0 * u);
        ADRC->eso_z2 = ADRC->eso_z2 + h * (-b2 * e);
        ADRC->eso_z3 = 0.0f;
    }
    else
    {
        ADRC->eso_z1 = ADRC->eso_z1 + h * (ADRC->eso_z2 - b1 * e);
        ADRC->eso_z2 = ADRC->eso_z2 + h * (ADRC->eso_z3 - b2 * e + b0 * u);
        ADRC->eso_z3 = ADRC->eso_z3 + h * (-b3 * e);
    }
}

static float ADRC_lsef_calc(ADRC_type_def *ADRC)
{
    float e1 = ADRC->td_v1 - ADRC->eso_z1;
    float u0;
    float zd;
    if (ADRC->order == ADRC_VEL)
    {
        u0 = ADRC->lsef_kp * e1;
        zd = ADRC->eso_z2;
    }
    else
    {
        float e2 = ADRC->td_v2 - ADRC->eso_z2;
        u0 = ADRC->lsef_kp * e1 + ADRC->lsef_kd * e2;
        zd = ADRC->eso_z3;
    }
    float b0 = ADRC->eso_b0;
    if (fabsf(b0) < ADRC_B0_MIN)
        b0 = (b0 >= 0.0f) ? ADRC_B0_MIN : -ADRC_B0_MIN;
    float u = (u0 - zd) / b0;
    return u;
}

float ADRC_Calc(ADRC_type_def *ADRC, float ref, float set, int max)
{
    ADRC->fdb = ref;
    ADRC->set = set;
    float error = set - ref;
    if (max != 0)
    {
        if (error >= (float)max / 2.0f)
            error -= (float)max;
        if (error < -(float)max / 2.0f)
            error += (float)max;
    }
    ADRC->error = error;
    if (max != 0)
    {
        float half_max = (float)max / 2.0f;
        float fmax     = (float)max;
        float e_z1 = ADRC->eso_z1 - ref;
        if (e_z1 >= half_max)
            ADRC->eso_z1 -= fmax;
        else if (e_z1 < -half_max)
            ADRC->eso_z1 += fmax;
        float e_v1 = ADRC->td_v1 - ADRC->eso_z1;
        if (e_v1 >= half_max)
            ADRC->td_v1 -= fmax;
        else if (e_v1 < -half_max)
            ADRC->td_v1 += fmax;
    }
    float target = set;
    if (max != 0)
    {
        float half_max = (float)max / 2.0f;
        float fmax     = (float)max;
        float diff = target - ADRC->eso_z1;
        if (diff >= half_max)
            target -= fmax;
        else if (diff < -half_max)
            target += fmax;
    }
    ADRC_td_update(ADRC, target);
    ADRC_eso_update(ADRC, ref, ADRC->out);
    float u = ADRC_lsef_calc(ADRC);
    if (u > ADRC->out_max)
        u = ADRC->out_max;
    if (u < -ADRC->out_max)
        u = -ADRC->out_max;
    ADRC->out = u;
    ADRC->disturbance = (ADRC->order == ADRC_VEL) ? ADRC->eso_z2 : ADRC->eso_z3;
    return u;
}

void ADRC_SetESOBandwidth(ADRC_type_def *ADRC, float w0)
{
    if (w0 <= 0.0f) return;
    ADRC->eso_w0 = w0;
    if (ADRC->order == ADRC_VEL)
    {
        ADRC->eso_beta1 = 2.0f * w0;
        ADRC->eso_beta2 = w0 * w0;
    }
    else
    {
        ADRC->eso_beta1 = 3.0f * w0;
        ADRC->eso_beta2 = 3.0f * w0 * w0;
        ADRC->eso_beta3 = w0 * w0 * w0;
    }
}

void ADRC_SetControllerBandwidth(ADRC_type_def *ADRC, float wc)
{
    if (wc <= 0.0f) return;
    ADRC->lsef_wc = wc;
    if (ADRC->order == ADRC_VEL)
    {
        ADRC->lsef_kp = wc;
    }
    else
    {
        ADRC->lsef_kp = wc * wc;
        ADRC->lsef_kd = 2.0f * wc;
    }
}

void ADRC_SetB0(ADRC_type_def *ADRC, float b0)
{
    ADRC->eso_b0 = (fabsf(b0) >= ADRC_B0_MIN) ? b0 : ADRC_B0_MIN;
}

void ADRC_SetOutMax(ADRC_type_def *ADRC, float out_max)
{
    ADRC->out_max = out_max;
}

void ADRC_Reset(ADRC_type_def *ADRC)
{
    ADRC->td_v1 = 0.0f;
    ADRC->td_v2 = 0.0f;
    ADRC->eso_z1 = 0.0f;
    ADRC->eso_z2 = 0.0f;
    ADRC->eso_z3 = 0.0f;
    ADRC->out = 0.0f;
    ADRC->error = 0.0f;
    ADRC->disturbance = 0.0f;
}