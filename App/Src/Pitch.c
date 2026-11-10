#include "Pitch.h"
#include "cmsis_os.h"
#include "VT13_rc_ctrl.h"
#include "motor.h"
#include "remote_control.h"

extern data_motor_t data_motor;
extern data_rc_t    data_rc;
extern VT13_data_rc_t VT13_data_rc;
extern CAN_HandleTypeDef hcan2;

/* ================= 可调参数定义(调试器可直接改) ================= */

/* 目标角度, 单位度。沿用 Gimbal.c 里原来的 -4.25 度 */
volatile float pitch_target_deg = -6.36119f;

/* MIT 三个增益, 沿用 Gimbal.c 里原来的值 */
volatile float pitch_kp     = 400.0f;   /* 位置增益 */
volatile float pitch_kd     = 5.0f;    /* 阻尼增益 */
volatile float pitch_ff_tor = 2.8f;    /* 前馈力矩(Nm) */

/* 慢积分: 5ms 一次, ki=0.002 对应时间常数约 0.5s
 * 作用是把 0.022 度的量化抖动平均掉, 让稳态误差压到 0.01 度以下 */
volatile uint8_t pitch_ki_en = 1;      /* 1=启用, 0=关闭(关掉退回纯 MIT) */
volatile float   pitch_ki    = 0.2f; /* 积分增益 */
volatile float   pitch_i_max = 3.0f;   /* 积分输出限幅(Nm) */
volatile float   pitch_i_out = 0.0f;   /* 当前积分输出(Nm), 只读观察 */

/* 目标限幅, 单位度。DM 量程是 ±12.5rad = ±716 度, 这里先按 ±12.5 度限
 * 防止 float_to_uint 映射越界(它不做限幅, 超量程会溢出成错误编码) */
volatile float pitch_target_min = -12.5f;
volatile float pitch_target_max =  12.5f;

/* 只读观测 */
volatile float pitch_pos_deg = 0.0f;   /* 当前实测角度(度) */
volatile float pitch_err_deg = 0.0f;   /* 当前误差(度) */

/**
  * @brief  俯仰 MIT 位置控制任务, 周期 PITCH_PERIOD_MS
  * @param  argument 未使用
  * @retval 无
  * @note   status_ammo==1 时使能电机并做位置闭环, 其余状态停发并清积分。
  *         只在状态切换的边沿发一次 enable/disable, 不是每周期都发。
  */
void Pitch_Task(void const * argument)
{
    uint8_t last_state = 0xFF;         /* 上一次的发射状态, 0xFF 表示还没初始化 */

    pitch_i_out = 0.0f;

    while (1)
    {
        /* ---- 发射状态切换的边沿处理 ---- */
        if (VT13_data_rc.status_gimbal != last_state)
        {
            last_state = VT13_data_rc.status_gimbal;

            if (last_state != 1)
            {
                /* 非发射状态: 关电机, 清积分, 下次使能从零开始 */
                DM_motor_disable(&hcan2, PITCH_MOTOR_ID);
                pitch_i_out = 0.0f;
            }
            else
            {
                /* 进入发射状态: 使能电机 */
                DM_motor_enable(&hcan2, PITCH_MOTOR_ID);
            }
        }

        /* ---- 发射状态下做位置闭环 ---- */
        if (VT13_data_rc.status_gimbal == 1)
        {
            /* 反馈是度 */
            float pos_deg = data_motor.Pit_motor_measure.pos;
            float target  = pitch_target_deg;
            float err;

            /* 目标先限幅, 防止发给电机的值越界 */
            if (target > pitch_target_max) target = pitch_target_max;
            if (target < pitch_target_min) target = pitch_target_min;

            err = target - pos_deg;

            /* 给调试器观察 */
            pitch_pos_deg = pos_deg;
            pitch_err_deg = err;

            /* 慢积分: 把 0.022 度的量化台阶平均掉
             * 注意这个积分输出是加在力矩(torq)上的, 不是加在位置上 */
            if (pitch_ki_en)
            {
                pitch_i_out += pitch_ki * err;
                if (pitch_i_out >  pitch_i_max) pitch_i_out =  pitch_i_max;
                if (pitch_i_out < -pitch_i_max) pitch_i_out = -pitch_i_max;
            }
            else
            {
                pitch_i_out = 0.0f;
            }

            /* MIT 指令: 位置要转成弧度再发
             * torq = 前馈力矩 + 积分补偿 */
            DM_motor_MIT_ctrl(&hcan2, PITCH_MOTOR_ID,
                              target / PITCH_RAD2DEG,
                              0.0f,                 /* 速度给定, 位置环里给 0 */
                              pitch_kp,
                              pitch_kd,
                              pitch_ff_tor + pitch_i_out);
        }

        osDelay(2);
    }
}
