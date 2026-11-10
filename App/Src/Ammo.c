#include "Ammo.h"
#include "chassis.h"
#include "cmsis_os.h"
#include "remote_control.h"
#include "VT13_rc_ctrl.h"
#include "math.h"
#include "ADRC.h"

/* ---------------- 可调参数 ---------------- */
#define AMMO_PERIOD_MS     2
#define SHOOT_FLAG_HOLD_MS 4500
#define MOTOR_FLT_FS_HZ    1000.0f
#define AMMO_ARM_BLANK_MS  6000     /* 切到发射状态后先空转这么久才允许发弹(ms) */
#define DETECT_MOTOR       3        /* 只看这一路电机(3=0x204 一级上轮) */

//3.0:937.5 3.1:968.3 3.2:999.2 3.3:1030.0 3.4:1060.8 3.5:1091.6 3.6:1122.5 3.7:1153.3 3.8:1184.1 3.9:1214.9
//4.0:1245.8 4.1:1276.6 4.2:1307.4 4.3:1338.2 4.4:1369.1 4.5:1399.9 4.6:1430.7 4.7:1461.5 4.8:1492.4 4.9:1523.2
//5.0:1554.0 5.1:1584.8 5.2:1615.7 5.3:1646.5 5.4:1677.3 5.5:1708.1 5.6:1739.0 5.7:1769.8 5.8:1800.6 5.9:1831.4
//6.0:1862.3 6.1:1893.1 6.2:1923.9 6.3:1954.7 6.4:1985.6 6.5:2016.4 6.6:2047.2 6.7:2078.0 6.8:2108.9 6.9:2139.7
//7.0:2170.5 7.1:2201.3 7.2:2232.2 7.3:2263.0 7.4:2293.8 7.5:2324.6 7.6:2355.5 7.7:2386.3 7.8:2417.1 7.9:2447.9
//8.0:2478.8 8.1:2509.6 8.2:2540.4 8.3:2571.2 8.4:2602.1 8.5:2632.9 8.6:2663.7 8.7:2694.5 8.8:2725.4 8.9:2756.2
//9.0:2787.0 9.1:2817.8 9.2:2848.7 9.3:2879.5 9.4:2910.3 9.5:2941.1 9.6:2972.0 9.7:3002.8 9.8:3033.6 9.9:3064.4
//10.0:3095.3 10.1:3126.1 10.2:3156.9 10.3:3187.7 10.4:3218.6 10.5:3249.4 10.6:3280.2 10.7:3311.0 10.8:3341.9 10.9:3372.7
//11.0:3403.5 11.1:3434.3 11.2:3465.2 11.3:3496.0 11.4:3526.8 11.5:3557.6 11.6:3588.5 11.7:3619.3 11.8:3650.1 11.9:3680.9
//12.0:3711.8 12.1:3742.6 12.2:3773.4 12.3:3804.2 12.4:3835.1 12.5:3865.9 12.6:3896.7 12.7:3927.5 12.8:3958.4 12.9:3989.2
//13.0:4020.0 13.1:4050.8 13.2:4081.7 13.3:4112.5 13.4:4143.3 13.5:4174.1 13.6:4205.0 13.7:4235.8 13.8:4266.6 13.9:4297.4
//14.0:4328.3 14.1:4359.1 14.2:4389.9 14.3:4420.7 14.4:4451.6 14.5:4482.4 14.6:4513.2 14.7:4544.0 14.8:4574.9 14.9:4505.7
//15.0:4676.5 15.1:4707.3 15.2:4738.2 15.3:4769.0 15.4:4799.8 15.5:4830.6 15.6:4861.5 15.7:4892.3 15.8:4923.1 15.9:4953.9
//16.0:4984.8 16.1:5015.6 16.2:5046.4 16.3:5077.2 16.4:5108.1 16.5:5138.9 16.6:5169.7 16.7:5200.5 16.8:5231.4 16.9:5262.2

static const float ammo_set_base[6] = {5015.6f, 4861.5f, 4861.5f, 4923.1f, 443f, 4830.6f};
static const float ammo_set_sign[6]    = {1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

//弹速补偿
volatile uint8_t   ammo_comp_en        = 1;
volatile float     ammo_comp_step[6]   = {-2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f};
volatile float     ammo_comp_max[6]    = {500.0f, 500.0f, 500.0f, 500.0f, 500.0f, 500.0f};
volatile uint16_t  ammo_comp_window_ms = 2000;
volatile float     ammo_comp[6]        = {0};
static TickType_t  ammo_comp_last_tick = 0;

// 掉速门限
volatile float    ammo_dip_trig_rpm    = 50.0f;
volatile float    ammo_dip_rearm_rpm   = 20.0f;
volatile uint16_t ammo_rearm_quiet_ms  = 50;
volatile uint16_t ammo_min_interval_ms = 0;

/* 调试用: 最近一次计数的掉幅与时刻 */
volatile float      ammo_dbg_dip  = 0.0f;
volatile TickType_t ammo_dbg_tick = 0;

//速度环算法选择
#define USE_PID  1
#define USE_ADRC 0

//转速滤波开关与参数
volatile uint8_t motor_flt_ma_en    = 1;
volatile uint8_t motor_flt_ma_win   = 10;
volatile uint8_t motor_flt_lp_en    = 1;
volatile float   motor_flt_lp_fc    = 10.0f;
volatile float   motor_flt_lp_alpha = 0.0f;

pid_type_def PID_ammo_vel[6];
ADRC_type_def ADRC_ammo[6];
fp32 ammo2up_PID[3]      = {1.6f, 0.005f, 0.002f};
fp32 ammo2left_PID[3]    = {1.6f, 0.005f, 0.002f};
fp32 ammo2right_PID[3]   = {1.6f, 0.005f, 0.002f};
fp32 ammo1up_PID[3]      = {1.6f, 0.005f, 0.002f};
fp32 ammo1left_PID[3]    = {1.6f, 0.005f, 0.002f};
fp32 ammo1right_PID[3]   = {1.6f, 0.005f, 0.002f};
float ammo_speed_lpf[6]  = {0};
int16_t ammo_out[6]      = {0};
float   ammo_dip_peak[6] = {0};  /* 触发那一拍六路掉幅(rpm) */
float   ammo_dip_max[6]  = {0};  /* 本次使能以来六路最大掉幅(rpm) */
uint16_t ammo_drop_count = 0;

static uint16_t shoot_flag_last_count = 0;
static TickType_t shoot_flag_until    = 0;
static TickType_t detect_blank_until  = 0;
static TickType_t last_shot_tick      = 0;
static TickType_t rearm_quiet_since   = 0;
static uint8_t    shot_armed          = 1;
static uint16_t   comp_shot_seen      = 0;    /* 补偿侧已处理的打弹数 */

/* ---------------- 外部变量 ---------------- */
extern data_g2c_gimbal_t data_g2c_gimbal;
extern data_c2g_gimbal_t data_c2g_gimbal;
extern data_c2g_chassis_t data_c2g_chassis;
extern data_motor_t data_motor;
extern data_rc_t data_rc;
extern data_referee_t data_referee;
extern VT13_data_rc_t VT13_data_rc;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern data_g2c_gimbal_t data_g2c_gimbal;

/* ---------------- 函数声明 ---------------- */
static void Ammo_FltUpdateCoeff(void);
static void Ammo_TakeDipPeak(float dip[6]);
void Ammo_DropDetect(const float dip[6]);
void Ammo_DropDetect_Arm(void);
void Ammo_DropDetect_Reset(void);
void Ammo_ShootFlag(void);

/* ---------------- 主任务 ---------------- */
void Ammo_Task(void const * argument)
{
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(AMMO_PERIOD_MS);
    Ammo_FltUpdateCoeff();
    PID_init(&PID_ammo_vel[0], PID_DELTA, ammo2up_PID,    4500.0f, 350.0f);//2
    PID_init(&PID_ammo_vel[1], PID_DELTA, ammo2left_PID,  4500.0f, 350.0f);
    PID_init(&PID_ammo_vel[2], PID_DELTA, ammo2right_PID, 4500.0f, 350.0f);
    PID_init(&PID_ammo_vel[3], PID_DELTA, ammo1up_PID,    4500.0f, 350.0f);//1
    PID_init(&PID_ammo_vel[4], PID_DELTA, ammo1left_PID,  4500.0f, 350.0f);
    PID_init(&PID_ammo_vel[5], PID_DELTA, ammo1right_PID, 4500.0f, 350.0f);
    /* ---- ADRC 参数: w0, wc, b0, r, h, out_max ---- */
    ADRC_Init(&ADRC_ammo[0], ADRC_VEL, 12.0f, 3.0f, 5.0f, 4500.0f, 0.001f, 4000.0f);
    ADRC_Init(&ADRC_ammo[1], ADRC_VEL, 12.0f, 3.0f, 5.0f, 4500.0f, 0.001f, 4000.0f);
    ADRC_Init(&ADRC_ammo[2], ADRC_VEL, 12.0f, 3.0f, 5.0f, 4500.0f, 0.001f, 4000.0f);
    ADRC_Init(&ADRC_ammo[3], ADRC_VEL, 15.0f, 3.0f, 4.5f, 4500.0f, 0.001f, 4000.0f);
    ADRC_Init(&ADRC_ammo[4], ADRC_VEL, 15.0f, 4.0f, 5.0f, 4500.0f, 0.001f, 4000.0f);
    ADRC_Init(&ADRC_ammo[5], ADRC_VEL, 15.0f, 4.0f, 5.0f, 4500.0f, 0.001f, 4000.0f);
    static uint8_t last_ammo_state = 0xFF;
    while(1)
    {
        static float last_fc = -1.0f;
        if (motor_flt_lp_fc != last_fc)          /* 截止频率被在线改动则重算系数 */
        {
            last_fc = motor_flt_lp_fc;
            Ammo_FltUpdateCoeff();
        }
        for (int i = 0; i < 6; i++)
            ammo_speed_lpf[i] = data_motor.Ammo_motor_measure[i].speed_lp;
        float dip_peak[6];
        Ammo_TakeDipPeak(dip_peak);
        if (VT13_data_rc.status_ammo != last_ammo_state)
        {
            last_ammo_state = VT13_data_rc.status_ammo;
            if (last_ammo_state == 1) Ammo_DropDetect_Arm();
        }
        if (VT13_data_rc.status_ammo == 1 && (int32_t)(xTaskGetTickCount() - detect_blank_until) >= 0)
            Ammo_DropDetect(dip_peak);
        else
            shot_armed = 0;
        Ammo_ShootFlag();
        switch(VT13_data_rc.status_ammo)
        {
            case 0:
                for (int i = 0; i < 6; i++)
                {
                    ADRC_Reset(&ADRC_ammo[i]);
                    PID_clear(&PID_ammo_vel[i]);
                    ammo_out[i] = 0;
                }
                Ammo_DropDetect_Reset();
                DJ_124_ctrl(&hcan1, 0x200, 0, 0, 0, 0);//0-3
                DJ_124_ctrl(&hcan1, 0x1FF, 0, 0, 0, 0);//4-5
                break;
            case 1:
                {
                    float set_all[6];

                    /* 发数补偿: 每打一发, 设定值加一步, 让弹速逐发往上走 */
                    if (ammo_comp_en)
                    {
                        TickType_t now = xTaskGetTickCount();

                        if (ammo_comp_window_ms > 0 && ammo_comp_last_tick != 0 &&
                            (int32_t)(now - ammo_comp_last_tick) >
                            (int32_t)pdMS_TO_TICKS(ammo_comp_window_ms))
                        {
                            /* 超时没打弹 -> 基准复位, 从头重新累计 */
                            for (uint8_t i = 0; i < 6; i++) ammo_comp[i] = 0.0f;
                            comp_shot_seen      = ammo_drop_count;
                            ammo_comp_last_tick = 0;
                        }

                        if (comp_shot_seen != ammo_drop_count)
                        {
                            comp_shot_seen = ammo_drop_count;
                            for (uint8_t i = 0; i < 6; i++)
                            {
                                ammo_comp[i] += ammo_comp_step[i];
                                if (ammo_comp[i] > ammo_comp_max[i])
                                    ammo_comp[i] =  ammo_comp_max[i];
                                if (ammo_comp[i] < -ammo_comp_max[i])
                                    ammo_comp[i] = -ammo_comp_max[i];
                            }
                            ammo_comp_last_tick = now;
                        }
                    }

                    for (uint8_t i = 0; i < 6; i++)
                        set_all[i] = ammo_set_sign[i] * (ammo_set_base[i] + ammo_comp[i]);

                    for (int i = 0; i < 6; i++)
                    {
                        #if USE_PID
                        ammo_out[i] = PID_calc(&PID_ammo_vel[i], ammo_speed_lpf[i], set_all[i]);
                        #elif USE_ADRC
                        ammo_out[i] = ADRC_Calc(&ADRC_ammo[i], ammo_speed_lpf[i], set_all[i], 0);
                        #endif
                    }
                }
                DJ_124_ctrl(&hcan1, 0x200, ammo_out[0], ammo_out[1], ammo_out[2], ammo_out[3]);//0-3
                DJ_124_ctrl(&hcan1, 0x1FF, ammo_out[4], ammo_out[5], 0, 0);//4-5
                break;
            case 2:
                for (int i = 0; i < 6; i++)
                {
                    ADRC_Reset(&ADRC_ammo[i]);
                    PID_clear(&PID_ammo_vel[i]);
                    ammo_out[i] = 0;
                }
                Ammo_DropDetect_Reset();
                DJ_124_ctrl(&hcan1, 0x200, 0, 0, 0, 0);//0-3
                DJ_124_ctrl(&hcan1, 0x1FF, 0, 0, 0, 0);//4-5
                break;
        }
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ---------------- 低通系数重算 ---------------- */
static void Ammo_FltUpdateCoeff(void)
{
    float fc = motor_flt_lp_fc;
    if (fc <= 0.0f || fc > MOTOR_FLT_FS_HZ * 0.5f)
        motor_flt_lp_alpha = 1.0f;
    else
        motor_flt_lp_alpha = 1.0f - expf(-2.0f * 3.14159265f * fc / MOTOR_FLT_FS_HZ);
}

/* ---------------- 发射标志 ---------------- */
/* shoot_flag=0 -> 拨盘停转, 1 -> 允许打弹。
 * status_gimbal==2: 空转期内恒 0, 之后检测到发弹置 0 并保持 SHOOT_FLAG_HOLD_MS 毫秒
 * status_gimbal==1: 恒 1
 * status_gimbal==0: 恒 0 */
void Ammo_ShootFlag(void)
{
    if (VT13_data_rc.status_gimbal == 2)
    {
        TickType_t now = xTaskGetTickCount();

        /* 起轮空转期: 拨盘停转, 不准打弹 */
        if (VT13_data_rc.status_ammo == 1 &&
            (int32_t)(now - detect_blank_until) < 0)
        {
            data_g2c_gimbal.g2c_info_shoot_flag = 0;
            return;
        }

        if (ammo_drop_count != shoot_flag_last_count)
        {
            shoot_flag_last_count = ammo_drop_count;
            shoot_flag_until = now + pdMS_TO_TICKS(SHOOT_FLAG_HOLD_MS);
        }

        if ((int32_t)(now - shoot_flag_until) < 0)
            data_g2c_gimbal.g2c_info_shoot_flag = 0;
        else
            data_g2c_gimbal.g2c_info_shoot_flag = 1;
    }
    if (VT13_data_rc.status_gimbal == 1)
    {
        shoot_flag_until = 0;
        data_g2c_gimbal.g2c_info_shoot_flag = 1;
    }
    if (VT13_data_rc.status_gimbal == 0)
    {
        shoot_flag_until = 0;
        data_g2c_gimbal.g2c_info_shoot_flag = 0;
    }
}

/* ---------------- 掉速峰值取走 ---------------- */
static void Ammo_TakeDipPeak(float dip[6])
{
    for (uint8_t i = 0; i < 6; i++)
    {
        dip[i] = data_motor.Ammo_motor_measure[i].speed_dip_peak;
        data_motor.Ammo_motor_measure[i].speed_dip_peak = 0.0f;
    }
}

/* ---------------- 发弹检测(单电机 + 滞回) ---------------- */
void Ammo_DropDetect(const float dip[6])
{
    TickType_t now = xTaskGetTickCount();
    float d = dip[DETECT_MOTOR];

    if (d > ammo_dip_max[DETECT_MOTOR]) ammo_dip_max[DETECT_MOTOR] = d;

    if (shot_armed)
    {
        if (d > ammo_dip_trig_rpm)
        {
            uint8_t allow = 1;
            if (ammo_min_interval_ms > 0 &&
                (int32_t)(now - last_shot_tick) < (int32_t)pdMS_TO_TICKS(ammo_min_interval_ms))
                allow = 0;

            if (allow)
            {
                for (uint8_t i = 0; i < 6; i++) ammo_dip_peak[i] = dip[i];
                ammo_drop_count++;
                last_shot_tick = now;
                ammo_dbg_dip  = d;
                ammo_dbg_tick = now;
            }
            shot_armed = 0;
            rearm_quiet_since = 0;
        }
    }
    else
    {
        /* 已解除武装期间的峰值也记一下, 便于看清"第二拍"是从哪来的 */
        if (d > ammo_dip_max[DETECT_MOTOR] && d > ammo_dbg_dip) ammo_dbg_dip = d;
        if (d > ammo_dip_rearm_rpm)
        {
            rearm_quiet_since = 0;
        }
        else if (rearm_quiet_since == 0)
        {
            rearm_quiet_since = now;
        }
        else if ((int32_t)(now - rearm_quiet_since) >= (int32_t)pdMS_TO_TICKS(ammo_rearm_quiet_ms))
        {
            shot_armed = 1;
        }
    }
}

/* ---------------- 进入发射状态: 清记录 + 起轮屏蔽 ---------------- */
void Ammo_DropDetect_Arm(void)
{
    detect_blank_until = xTaskGetTickCount() + pdMS_TO_TICKS(AMMO_ARM_BLANK_MS);
    for (uint8_t i = 0; i < 6; i++)
    {
        ammo_dip_peak[i] = 0.0f;
        ammo_dip_max[i]  = 0.0f;
        data_motor.Ammo_motor_measure[i].speed_dip_peak = 0.0f;
    }
    shot_armed        = 1;
    rearm_quiet_since = 0;
    last_shot_tick    = 0;
    ammo_dbg_dip      = 0.0f;
    ammo_dbg_tick     = 0;
}

/* ---------------- 停机 ---------------- */
void Ammo_DropDetect_Reset(void)
{
    for (uint8_t i = 0; i < 6; i++)
    {
        ammo_dip_peak[i] = 0.0f;
        data_motor.Ammo_motor_measure[i].speed_dip_peak = 0.0f;
    }
    ammo_drop_count = 0;

    shoot_flag_last_count = 0;
    shoot_flag_until      = 0;
    shot_armed            = 1;
    rearm_quiet_since     = 0;
    last_shot_tick        = 0;
}
