#include "Data.h"
#include "cmsis_os.h"
#include "remote_control.h"
#include "UART_miniPC.h"
#include "c2c.h"
#include "dm_imu.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include "ammo.h"
#include "ADRC.h"
extern fifo_s_t referee_fifo;
extern data_motor_t data_motor;
extern data_g2c_gimbal_t data_g2c_gimbal;
extern data_g2c_chassis_t data_g2c_chassis;
extern data_c2g_chassis_t data_c2g_chassis;
extern data_ins_t data_ins;
extern pid_type_def PID_ammo_vel[6];
extern ADRC_type_def ADRC_ammo[6];
extern float ammo_speed_lpf[6];   /* Ammo.c 里滑动平均滤波后的转速 */
/* Ammo.c 里打弹掉速记录（每发更新一次） */
extern float    ammo_dip_peak[6];
extern float    ammo_dip_max[6];
extern uint16_t ammo_drop_count;
extern volatile float    ammo_dip_trig_rpm;
extern volatile float    ammo_dip_rearm_rpm;
extern volatile uint16_t ammo_rearm_quiet_ms;
extern volatile uint16_t ammo_min_interval_ms;
extern volatile uint8_t  motor_flt_ma_win;
extern volatile float    motor_flt_lp_fc;
data_rc_t data_rc;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

void Data_Task(void const * argument)
{
    referee_Init();
    data_rc.RC_ctrl = get_remote_control_point();
    VT13_data_rc.RC_ctrl = get_VT13_remote_control_point();
    while (1)
    {
        rc_read_data(&data_rc);
        data_g2c_gimbal.g2c_info_gimbal_status = data_rc.status_gimbal;//云台状态机
        data_g2c_gimbal.g2c_info_ammo_status = data_rc.status_ammo;//发射状态机
        data_g2c_chassis.g2c_info_chassis_status = data_rc.status_chassis;
        data_g2c_chassis.g2c_info_speed_x = data_rc.speed_x;
        data_g2c_chassis.g2c_info_speed_y = data_rc.speed_y;
        data_g2c_chassis.g2c_info_speed_yaw = data_rc.speed_yaw;
        g2c_send_data(&hcan2);
        usart_printf(&huart1, "%f,%f,%f,%f,%f,%f\r\n",
        ammo_speed_lpf[0], ammo_speed_lpf[1], ammo_speed_lpf[2], ammo_speed_lpf[3], ammo_speed_lpf[4], ammo_speed_lpf[5]);
        osDelay(2);
    }
}

void usart_printf(UART_HandleTypeDef *huart, const char *fmt, ...)
{
    static uint8_t tx_buf[256];
    va_list ap;
    int len;
    va_start(ap, fmt);
    len = vsnprintf((char *)tx_buf, sizeof(tx_buf), fmt, ap);
    va_end(ap);
    if (len <= 0) return;  
    if (len > (int)sizeof(tx_buf) - 1)
        len = sizeof(tx_buf) - 1;
    HAL_UART_Transmit_DMA(huart, tx_buf, (uint16_t)len);
}