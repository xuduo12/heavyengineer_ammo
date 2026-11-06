#include "Chassis.h"
#include "PID.h"
#include "math.h"
#include "cmsis_os.h"
#include "remote_control.h"
#include "Data.h"
#include "struct_typedef.h"
#include "dm_imu.h"

extern data_rc_t data_rc; 
extern data_g2c_gimbal_t data_g2c_gimbal;
extern data_g2c_chassis_t data_g2c_chassis;
extern data_ins_t data_ins;
extern data_referee_t data_referee;
// void Chassis_Task(void const * argument)
// {   
//     while(1)
//     {
//         osDelay(2);
//     }
// }
