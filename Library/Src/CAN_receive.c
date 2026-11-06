#include "CAN_receive.h"
#include "motor.h"
#include "dm_imu.h"
#include "Ammo.h"
extern data_motor_t data_motor;
extern data_g2c_gimbal_t data_g2c_gimbal;
extern data_g2c_chassis_t data_g2c_chassis;
extern data_c2g_gimbal_t data_c2g_gimbal;
extern data_c2g_chassis_t data_c2g_chassis;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
    if(hcan == &hcan1)
    {//回去的id按照实际修改
        switch (rx_header.StdId)
        {
            case 0x201: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[0], rx_data); break;	
            case 0x202: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[1], rx_data); break;	
            case 0x203: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[2], rx_data); break;	
            case 0x204: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[3], rx_data); break;	
            case 0x205: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[4], rx_data); break;
            case 0x206: DJ_motor_fbdata(&data_motor.Ammo_motor_measure[5], rx_data); break;
			default: break;
        }
    }
    else if(hcan == &hcan2)
    {
        switch (rx_header.StdId)
        {
            case 0x21: DM_motor_fbdata(&data_motor.Pit_motor_measure, rx_data); break;
            case 0x111: c2g_chassis_fbdata(&data_c2g_chassis, rx_data); break;
        
          default:break;
				}
    }
}

/**
  * @brief can过滤器初始化
  * @return none 
  */
void can_filter_init(void)
{

    CAN_FilterTypeDef can_filter_st;
    can_filter_st.FilterActivation = ENABLE;
    can_filter_st.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter_st.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter_st.FilterIdHigh = 0x0000;
    can_filter_st.FilterIdLow = 0x0000;
    can_filter_st.FilterMaskIdHigh = 0x0000;
    can_filter_st.FilterMaskIdLow = 0x0000;
    can_filter_st.FilterBank = 0;
    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter_st);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);


    can_filter_st.SlaveStartFilterBank = 14;
    can_filter_st.FilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan2, &can_filter_st);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
}
