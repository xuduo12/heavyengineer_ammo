#ifndef PITCH_H
#define PITCH_H

#include "main.h"

/* ================= 俯仰(DM 电机 0x11) MIT 位置控制 =================
 *
 * 控制对象 : data_motor.Pit_motor_measure.pos (DM4310, CAN2, ID=0x11)
 * 反馈单位 : 度   (motor.c 里 uint_to_float(...) * 57.6 得到)
 * 指令单位 : 弧度 (DM_motor_MIT_ctrl 的 pos 参数, 发之前要 / 57.3)
 * 任务周期 : 5ms
 *
 * 精度说明:
 *   pos 是 16 位位置量, 量程 ±12.5rad = ±716 度
 *   分辨率 = 25rad / 65535 * 57.3 = 0.022 度/LSB
 *   所以 pos 本身读数精度就是 0.02 度, 读不出 0.01 度
 *   靠 pitch_ki 慢积分把量化抖动的均值误差压到 0.01 度以下
 *   稳态精度看 pitch_err_deg 的长时间平均值, 不要看单次读数
 * ================================================================ */

#define PITCH_PERIOD_MS      2        /* 控制周期(ms) */
#define PITCH_MOTOR_ID       0x11     /* DM 电机 CAN ID */
#define PITCH_RAD2DEG        57.29577951308402f  /* 弧度 -> 度 */

/* ---------------- 可调参数(调试器可直接改) ---------------- */
extern volatile float pitch_target_deg;   /* 目标角度, 单位度 */

extern volatile float pitch_kp;           /* MIT 位置增益 */
extern volatile float pitch_kd;           /* MIT 阻尼增益 */
extern volatile float pitch_ff_tor;       /* 前馈力矩(Nm), 用来托住俯仰重量 */

extern volatile uint8_t pitch_ki_en;      /* 1=启用慢积分, 0=关闭 */
extern volatile float   pitch_ki;         /* 积分增益, 每 5ms 累加一次 */
extern volatile float   pitch_i_max;      /* 积分输出限幅(Nm) */
extern volatile float   pitch_i_out;      /* 当前积分输出(Nm), 只读观察 */

extern volatile float pitch_target_min;   /* 目标下限(度) */
extern volatile float pitch_target_max;   /* 目标上限(度) */

/* ---------------- 只读观测 ---------------- */
extern volatile float pitch_pos_deg;      /* 当前实测角度(度) */
extern volatile float pitch_err_deg;      /* 当前误差(度) */

void Pitch_Task(void const * argument);

#endif
