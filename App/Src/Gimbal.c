#include "Gimbal.h"
#include "cmsis_os.h"
#include "struct_typedef.h"
#include "motor.h"
#include "UART_miniPC.h"
#include "VT13_rc_ctrl.h"
#include "dm_imu.h"
#include "CAN_receive.h"
#include "Chassis.h"
#include "remote_control.h"
#define rad2angle 57.29577951308402f
extern data_g2c_gimbal_t data_g2c_gimbal;
extern data_ins_t data_ins;
extern data_motor_t data_motor;
extern data_rc_t data_rc;
extern VT13_data_rc_t VT13_data_rc;
extern RC_ctrl_t rc_ctrl;
extern data_referee_t data_referee;
extern data_miniPC_aim_send_t data_miniPC_aim_send ;
extern data_miniPC_aim_send_t data_miniPC_nav_send ;
extern data_c2g_gimbal_t data_c2g_gimbal;
extern data_c2g_chassis_t data_c2g_chassis;
void Gimbal_Task(void const * argument)
{
    while(1)
 	{
        switch(VT13_data_rc.status_gimbal)
        {
            case 0: 
                DM_motor_disable(&hcan2, 0x11);
                break;
            case 1: 
                DM_motor_enable(&hcan2, 0x11);
                break;
            case 2:
                DM_motor_enable(&hcan2, 0x11);
                break;
            default: break;
        }
 		osDelay(5);
 	}
}
