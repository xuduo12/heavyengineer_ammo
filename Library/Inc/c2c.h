#ifndef C2C_H
#define C2C_H
#include "main.h"

#define G2C_GIMBAL_ID  0x100
#define G2C_CHASSIS_ID 0x101
#define C2G_GIMBAL_ID  0x110
#define C2G_CHASSIS_ID 0x111

// /**
//   * @brief                            C板通信云台发向底盘 云台部分结构体
//   * @param c2c_info_gimbal_status     云台状态
//   * @param c2c_info_ammo_status       发射状态
//   * @param c2c_info_chassis_yaw       底盘Yaw轴角度（跟随云台）
//    */
// typedef struct
// {
// 	 uint16_t yaw_ecd;
//    int32_t yaw;
// 	 int8_t  g2c_shoot_flag;
   
// }data_g2c_gimbal_t;

// /**
//   * @brief                            C板通信云台发向底盘 底盘部分结构体
//   * @param c2c_info_chassis_status    底盘状态
//   * @param c2c_info_x_vel             x速度
//   * @param c2c_info_y_vel             y速度
//   * @param c2c_info_yaw_vel           yaw自转速度
//    */
// typedef struct
// {
//    uint8_t c2c_info_chassis_status;
//    int16_t c2c_info_x_vel;
//    int16_t c2c_info_y_vel;
//    int16_t c2c_info_yaw_vel;
// }data_g2c_chassis_t;

/**
  * @brief                            C板通信云台发向底盘 底盘部分结构体
  * @param c2c_info_gimbal_power      云台供电状态
  * @param c2c_info_ammo_power        底盘供电状态
   */
typedef struct
{
   uint8_t c2c_info_gimbal_power;
   uint8_t c2c_info_ammo_power;
	uint8_t c2c_info_gimbal_status;
	uint8_t c2c_info_ammo_status;
	int16_t gimbal_yaw_add;
	int16_t gimbal_pitch_add;
	uint8_t ammo_power; 
}data_c2g_gimbal_t;

typedef struct
{
   int32_t initial_speed;
   int8_t launching_frequency;
}data_c2g_chassis_t;


/**
  * @brief                            C板通信云台发向底盘 云台部分结构体
  * @param g2c_info_gimbal_status     云台状态
  * @param g2c_info_ammo_status       发射状态
  * @param g2c_info_shoot_flag        射击标志(预制状态)
  * @param g2c_info_imu_yaw           IMU yaw角度
   */
typedef struct
{
   uint8_t g2c_info_gimbal_status;
	uint8_t g2c_info_ammo_status;
   uint8_t g2c_info_shoot_flag;
   int16_t g2c_info_imu_yaw;
}data_g2c_gimbal_t;

/**
  * @brief                            C板通信云台发向底盘 底盘部分结构体
  * @param g2c_info_chassis_status    底盘状态
  * @param g2c_info_speed_x           x速度
  * @param g2c_info_speed_y           y速度
  * @param g2c_info_speed_yaw         yaw速度
   */
typedef struct
{
   uint8_t g2c_info_chassis_status;
   int16_t g2c_info_speed_x;
   int16_t g2c_info_speed_y;
   int16_t g2c_info_speed_yaw;
}data_g2c_chassis_t;

void g2c_chassis_fbdata(data_g2c_chassis_t *c2c , uint8_t *rx_data);
void g2c_gimbal_fbdata(data_g2c_gimbal_t *c2c , uint8_t *rx_data);
void c2g_gimbal_fbdata(data_c2g_gimbal_t *c2c , uint8_t *rx_data);
void c2g_chassis_fbdata(data_c2g_chassis_t *c2c , uint8_t *rx_data);
uint8_t can_send_data(CAN_HandleTypeDef *hcan, uint16_t id, uint8_t *data, uint32_t len);
void g2c_send_data(CAN_HandleTypeDef *hcan);
void c2g_send_data(CAN_HandleTypeDef *hcan);

#endif
