#ifndef __DM_IMU_H
#define __DM_IMU_H

#include "stm32f4xx_hal.h"
#include "can.h"

#define ACCEL_CAN_MAX (58.8f)
#define ACCEL_CAN_MIN (-58.8f)
#define GYRO_CAN_MAX (34.88f)
#define GYRO_CAN_MIN (-34.88f)
#define PITCH_CAN_MAX (90.0f)
#define PITCH_CAN_MIN (-90.0f)
#define ROLL_CAN_MAX (180.0f)
#define ROLL_CAN_MIN (-180.0f)
#define YAW_CAN_MAX (180.0f)
#define YAW_CAN_MIN (-180.0f)
#define TEMP_MIN (0.0f)
#define TEMP_MAX (60.0f)
#define Quaternion_MIN (-1.0f)
#define Quaternion_MAX (1.0f)

typedef struct
{
	float pitch;
	float roll;
	float yaw;

	float gyro[3];
	float accel[3];
	
	float q[4];

	float cur_temp;

} imu_t;

extern imu_t imu;

// 函数声明
void IMU_UpdateData(uint8_t* pData);
void IMU_RequestData(CAN_HandleTypeDef* hcan, uint16_t can_id, uint8_t reg);
int float_to_uint(float x_float, float x_min, float x_max, int bits);
float uint_to_float(int x_int, float x_min, float x_max, int bits);

// 数据寄存器类型
#define IMU_REG_ACCEL 0x01
#define IMU_REG_GYRO  0x02
#define IMU_REG_EULER 0x03
#define IMU_REG_QUAT  0x04

#endif