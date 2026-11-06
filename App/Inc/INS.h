#ifndef INS_H
#define INS_H

#include "main.h"
#include "stdbool.h"
#include "BMI088.h"
#include "VQF_filter.h"
#include "dm_imu.h"

typedef struct
{
    float pitch;           // 俯仰角（度，±90） 
    float roll;            // 横滚角（度，±180）
    float yaw;             // 偏航角（度，±180）
    float q[4];            //累计偏航角（度），VQF 解算，可跨 ±180 连续累加
    float yaw_total;       //角速度（rad/s）
    float gyro_roll;       // x 轴角速度 ≈ roll 角速率
    float gyro_pitch;      // y 轴角速度 ≈ pitch 角速率
    float gyro_yaw;        // z 轴角速度 ≈ yaw 角速率
    float accel[3];        //加速度（m/s²）
    float cur_temp;        //BMI088 温度
} data_ins_t;

extern data_ins_t data_ins;

void INS_Board_Init(void);
void INS_UpdateDataIns(const float gyro[3], const float accel[3]);
void INS_Task(void const * argument);

#endif
