#include "dm_imu.h"
#include "can.h"
#include "motor.h"
imu_t imu;


void IMU_RequestData(CAN_HandleTypeDef* hcan, uint16_t can_id, uint8_t reg)
{
	CAN_TxHeaderTypeDef tx_header;
	uint8_t cmd[4] = {(uint8_t)can_id, (uint8_t)(can_id>>8), reg, 0xCC};
	uint32_t mailbox;
	
	tx_header.StdId = 0x6FF;
	tx_header.ExtId = 0;
	tx_header.RTR = CAN_RTR_DATA;
	tx_header.IDE = CAN_ID_STD;
	tx_header.DLC = 4;
	tx_header.TransmitGlobalTime = DISABLE;
	
	if(HAL_CAN_GetTxMailboxesFreeLevel(hcan) > 0)
	{
		HAL_CAN_AddTxMessage(hcan, &tx_header, cmd, &mailbox);
	}
}

void IMU_UpdateAccel(uint8_t* pData)
{
	int16_t accel[3];
	
	accel[0] = (int16_t)(pData[3] << 8 | pData[2]);
	accel[1] = (int16_t)(pData[5] << 8 | pData[4]);
	accel[2] = (int16_t)(pData[7] << 8 | pData[6]);
	
	imu.accel[0] = uint_to_float(accel[0], ACCEL_CAN_MIN, ACCEL_CAN_MAX, 16);
	imu.accel[1] = uint_to_float(accel[1], ACCEL_CAN_MIN, ACCEL_CAN_MAX, 16);
	imu.accel[2] = uint_to_float(accel[2], ACCEL_CAN_MIN, ACCEL_CAN_MAX, 16);
}

void IMU_UpdateGyro(uint8_t* pData)
{
	uint16_t gyro[3];
	
	gyro[0] = (uint16_t)(pData[3] << 8 | pData[2]);
	gyro[1] = (uint16_t)(pData[5] << 8 | pData[4]);
	gyro[2] = (uint16_t)(pData[7] << 8 | pData[6]);
	
	imu.gyro[0] = uint_to_float(gyro[0], GYRO_CAN_MIN, GYRO_CAN_MAX, 16);
	imu.gyro[1] = uint_to_float(gyro[1], GYRO_CAN_MIN, GYRO_CAN_MAX, 16);
	imu.gyro[2] = uint_to_float(gyro[2], GYRO_CAN_MIN, GYRO_CAN_MAX, 16);
}

void IMU_UpdateEuler(uint8_t* pData)
{
	int16_t euler[3];
	
	euler[0] = (int16_t)(pData[3] << 8 | pData[2]);
	euler[1] = (int16_t)(pData[5] << 8 | pData[4]);
	euler[2] = (int16_t)(pData[7] << 8 | pData[6]);
	
	imu.pitch = uint_to_float(euler[0], PITCH_CAN_MIN, PITCH_CAN_MAX, 16);
	imu.yaw = uint_to_float(euler[1], 180.0f, -180.0f, 16);
	imu.roll = uint_to_float(euler[2], (ROLL_CAN_MIN+360), (ROLL_CAN_MAX+360), 16);

       if(imu.yaw >= 180)
       {
           imu.yaw -= 360;
       }
       else if(imu.yaw < -180)
       {
           imu.yaw += 360;
       }

       if(imu.roll >= 180)
       {
           imu.roll -= 360;
       }
       else if(imu.roll < -180)
       {
           imu.roll += 360;
       }

	}
      
   
void IMU_UpdateQuaternion(uint8_t* pData)
{
	int16_t w = (int16_t)(pData[1] << 6 | ((pData[2] & 0xFC) >> 2));
	int16_t x = (int16_t)((pData[2] & 0x03) << 12 | (pData[3] << 4) | ((pData[4] & 0xF0) >> 4));
	int16_t y = (int16_t)((pData[4] & 0x0F) << 10 | (pData[5] << 2) | ((pData[6] & 0xC0) >> 6));
	int16_t z = (int16_t)((pData[6] & 0x3F) << 8 | pData[7]);
	
	imu.q[0] = uint_to_float(w, Quaternion_MIN, Quaternion_MAX, 14);
	imu.q[1] = uint_to_float(x, Quaternion_MIN, Quaternion_MAX, 14);
	imu.q[2] = uint_to_float(y, Quaternion_MIN, Quaternion_MAX, 14);
	imu.q[3] = uint_to_float(z, Quaternion_MIN, Quaternion_MAX, 14);
}

void IMU_UpdateData(uint8_t* pData)
{
	switch(pData[0])
	{
		case 1:
			IMU_UpdateAccel(pData);
			break;
		case 2:
			IMU_UpdateGyro(pData);
			break;
		case 3:
			IMU_UpdateEuler(pData);
			break;
		case 4:
			IMU_UpdateQuaternion(pData);
			break;
	}
}