#include "motor.h"
#include "Chassis.h"
#include "Ammo.h"
#include <math.h>   /* expf / fabsf */
data_motor_t data_motor = {0};
pid_type_def PID_DJ_motor_2006_pos[4] = {0};

/*float转换至uint*/
int float_to_uint(float x , float x_min , float x_max , int bits)
{
	/* Converts a float to an unsigned int, given range and number of bits */
	float span = x_max - x_min;
	float offset = x_min;
	return (int) ((x-offset)*((float)((1<<bits)-1))/span);
}
/*float转换至uint*/


/*uint转换至float*/
float uint_to_float(int x_int , float x_min , float x_max , int bits)
{
	/* converts unsigned int to float, given range and number of bits */
	float span = x_max - x_min;
	float offset = x_min;
	return ((float)x_int)*span/((float)((1<<bits)-1)) + offset;
}
/*uint转换至float*/



/**
  * @brief          can发数据
  * @param hcan     can位（&hcan1/&hcan2）
  * @param id       can id
  * @param data     发送数据
  * @param len      数据长度
  * @return         none 
  */
uint8_t canx_send_data(CAN_HandleTypeDef *hcan, uint16_t id, uint8_t *data, uint32_t len)
{	
	CAN_TxHeaderTypeDef	tx_header;
	
	tx_header.StdId = id;
	tx_header.ExtId = 0;
	tx_header.IDE   = 0;
	tx_header.RTR   = 0;
	tx_header.DLC   = len;
	if(HAL_CAN_AddTxMessage(hcan, &tx_header, data, (uint32_t*)CAN_TX_MAILBOX0) != HAL_OK) 
    {
		if(HAL_CAN_AddTxMessage(hcan, &tx_header, data, (uint32_t*)CAN_TX_MAILBOX1) != HAL_OK) 
        {
			HAL_CAN_AddTxMessage(hcan, &tx_header, data, (uint32_t*)CAN_TX_MAILBOX2);
        }
    }
    return 0;
}

/* ---------------- 转速滤波参数 ---------------- */
#define MOTOR_FLT_FS_HZ     1000.0f     /* CAN 反馈率 1kHz */
#define MOTOR_FLT_MA_WIN    6           /* 滑动平均窗口(<=8) */
#define MOTOR_FLT_LP_FC_HZ  10.0f       /* 一阶低通截止频率 */

/* 与 Ammo.c 里同名可调变量互斥: 这里直接引用那边的定义, 不在本文件重复定义 */
extern volatile uint8_t motor_flt_ma_en;
extern volatile uint8_t motor_flt_ma_win;
extern volatile uint8_t motor_flt_lp_en;
extern volatile float   motor_flt_lp_fc;

/**
  * @brief          DJ电机转速滤波 (滑动平均 -> 一阶低通)
  *                 必须在 CAN 接收中断里每帧调用, 依赖 1kHz 的调用周期
  * @param motor    单个电机报文
  * @param raw      本帧原始转速 (有符号, rpm)
  * @return         none
  * @note           同时维护 speed_ma / speed_lp / speed_dip_peak
  */
static void DJ_motor_speed_filter(motor_measure_DJ_t *motor , float raw)
{
    uint8_t win = motor_flt_ma_win;
    if (win == 0) win = 1;
    if (win > MOTOR_FLT_MA_WIN) win = MOTOR_FLT_MA_WIN;

    /* ---- 1) 滑动平均 ---- */
    if (motor_flt_ma_en)
    {
        if (!motor->flt_init || motor->ma_win != win)
        {
            /* 首次进入, 或窗口长度被在线改动 -> 重新预填 */
            for (uint8_t i = 0; i < MOTOR_FLT_MA_WIN; i++) motor->ma_buf[i] = raw;
            motor->ma_sum  = raw * (float)win;
            motor->ma_idx  = 0;
            motor->ma_cnt  = win;
            motor->ma_win  = win;
            motor->flt_init = 1;
            motor->speed_ma = raw;
        }
        else
        {
            if (motor->ma_cnt >= win) motor->ma_sum -= motor->ma_buf[motor->ma_idx];
            motor->ma_buf[motor->ma_idx] = raw;
            motor->ma_sum += raw;
            motor->ma_idx = (motor->ma_idx + 1) % MOTOR_FLT_MA_WIN;
            if (motor->ma_cnt < win) motor->ma_cnt++;
            motor->speed_ma = motor->ma_sum / (float)motor->ma_cnt;
        }
    }
    else
    {
        motor->flt_init = 0;
        motor->speed_ma = raw;
    }

    /* ---- 2) 一阶低通 ---- */
    if (motor_flt_lp_en)
    {
        float fc = motor_flt_lp_fc;
        if (!motor->lp_init)
        {
            motor->speed_lp = motor->speed_ma;   /* 首帧直接跟随, 避免从 0 爬升 */
            motor->lp_init  = 1;
        }
        else
        {
            float alpha;
            if (fc <= 0.0f || fc > MOTOR_FLT_FS_HZ * 0.5f)
                alpha = 1.0f;                     /* 关闭低通, 直接跟随 */
            else
                alpha = 1.0f - expf(-2.0f * 3.14159265f * fc / MOTOR_FLT_FS_HZ);
            motor->speed_lp += alpha * (motor->speed_ma - motor->speed_lp);
        }
    }
    else
    {
        motor->lp_init  = 0;
        motor->speed_lp = motor->speed_ma;
    }

    /* ---- 3) 掉速峰值保持: 滤波后与原始值之差的幅度 ----
     * 只在"滤波后转速高于原始转速"时记正掉幅, 即真实的掉速瞬间;
     * 由 2ms 任务读走后清零。 */
    float dip = fabsf(motor->speed_lp) - fabsf(raw);
    if (dip > motor->speed_dip_peak) motor->speed_dip_peak = dip;
}

/**
  * @brief          DJ电机数据解包
  * @param motor    单位电机地址
  * @param rx_data  接收数据
  * @return         none 
  */
void DJ_motor_fbdata(motor_measure_DJ_t *motor , uint8_t *rx_data)
{
    motor->last_ecd = motor->ecd;
    motor->last_speed = motor->speed_rpm;
    motor->ecd = (uint16_t)((rx_data)[0] << 8 | (rx_data)[1]);
    motor->speed_rpm = (int16_t)((rx_data)[2] << 8 | (rx_data)[3]);   /* 转速是有符号补码 */
    motor->given_current = (int16_t)((rx_data)[4] << 8 | (rx_data)[5]);
    motor->temperate = (rx_data)[6];

    /* 每帧做一次滤波 + 掉速峰值维护 (1kHz) */
    DJ_motor_speed_filter(motor, (float)motor->speed_rpm);
}


/**
  * @brief          3508/2006电流开环
  * @param hcan     &hcan1 / &hcan2
  * @param motor_id 发送id
  * @param motor1   电机1电流
  * @param motor2   电机2电流
  * @param motor3   电机3电流
  * @param motor4   电机4电流
  * @return         none 
  */
void DJ_124_ctrl(CAN_HandleTypeDef* hcan , uint16_t motor_id , int16_t motor1 , int16_t motor2 , int16_t motor3 , int16_t motor4)
{
    static uint8_t tx_data[8];

    tx_data[0] = motor1 >> 8;
    tx_data[1] = motor1;
    tx_data[2] = motor2 >> 8;
    tx_data[3] = motor2;
    tx_data[4] = motor3 >> 8;
    tx_data[5] = motor3;
    tx_data[6] = motor4 >> 8;
    tx_data[7] = motor4;

    canx_send_data(hcan, motor_id, tx_data, 0x08);
}


/**
  * @brief 3508/2006速度环
  * @param hcan &hcan1 / &hcan2
  * @param motor_id 发送ID
  * @param pid_vel pid参数结构体
  * @param motor_measure 电机反馈数据
  * @param vel1 电机1转速
  * @param vel2 电机2转速
  * @param vel3 电机3转速
  * @param vel4 电机4转速
  * @return none 
  */
void DJ_124_ctrl_vel(CAN_HandleTypeDef* hcan , uint16_t motor_id , pid_type_def pid_vel[4] , motor_measure_DJ_t motor_measure[4] , int16_t vel1 , int16_t vel2 , int16_t vel3 , int16_t vel4)
{
  int16_t motor_ctrl_vel[4] = {0};
  motor_ctrl_vel[0] = vel1;
  motor_ctrl_vel[1] = vel2;
  motor_ctrl_vel[2] = vel3;
  motor_ctrl_vel[3] = vel4;
  static int16_t motor_I[4] = {0};

  static uint8_t i = 0;
  for(i = 0 ; i < 4 ; i += 1)
  {
    motor_I[i] = PID_calc(&pid_vel[i] , motor_measure[i].speed_rpm , motor_ctrl_vel[i]);
  }

  DJ_124_ctrl(hcan , motor_id , motor_I[0] , motor_I[1] , motor_I[2] , motor_I[3]);
}


 /**
  * @brief 3508/2006位置环
  * @param hcan &hcan1 / &hcan2
  * @param motor_id 发送ID
  * @param pid_vel pid参数结构体
  * @param motor_measure 电机反馈数据
  * @param pos1 电机1位置
  * @param pos2 电机2位置
  * @param pos3 电机3位置
  * @param pos4 电机4位置
  * @return none 
  */
//void DJ_124_ctrl_pos(CAN_HandleTypeDef* hcan , uint16_t motor_id , pid_type_def pid_pos[4] , motor_measure_DJ_t motor_measure[4] , int16_t pos1 , int16_t pos2 , int16_t pos3 , int16_t pos4)
//{
//  int16_t motor_ctrl_pos[4] = {0};
//  motor_ctrl_pos[0] = pos1;
//  motor_ctrl_pos[1] = pos2;
//  motor_ctrl_pos[2] = pos3;
//  motor_ctrl_pos[3] = pos4;
//  static int16_t motor_I[4] = {0};

//  static uint8_t i = 0;
//  for(i = 0 ; i < 4 ; i += 1)
//  {
//    motor_I[i] = PID_Calc_out(&pid_pos[i] , motor_measure[i].ecd , motor_ctrl_pos[i]);
//  }
//  PID_init(&PID_DJ_motor_2006_pos[0] , PID_DJ_MOTOR1_2006_POS_Kp , PID_DJ_MOTOR1_2006_POS_Ki , PID_DJ_MOTOR1_2006_POS_Kd , PID_DJ_MOTOR1_2006_POS_imax , PID_DJ_MOTOR1_2006_POS_maxout);
//  PID_init(&PID_DJ_motor_2006_pos[1] , PID_DJ_MOTOR2_2006_POS_Kp , PID_DJ_MOTOR2_2006_POS_Ki , PID_DJ_MOTOR2_2006_POS_Kd , PID_DJ_MOTOR2_2006_POS_imax , PID_DJ_MOTOR2_2006_POS_maxout);
//  PID_init(&PID_DJ_motor_2006_pos[2] , PID_DJ_MOTOR3_2006_POS_Kp , PID_DJ_MOTOR3_2006_POS_Ki , PID_DJ_MOTOR3_2006_POS_Kd , PID_DJ_MOTOR3_2006_POS_imax , PID_DJ_MOTOR3_2006_POS_maxout);
//  PID_init(&PID_DJ_motor_2006_pos[3] , PID_DJ_MOTOR4_2006_POS_Kp , PID_DJ_MOTOR4_2006_POS_Ki , PID_DJ_MOTOR4_2006_POS_Kd , PID_DJ_MOTOR4_2006_POS_imax , PID_DJ_MOTOR4_2006_POS_maxout);
//  DJ_124_ctrl_vel(hcan , motor_id , PID_DJ_motor_2006_pos , motor_measure , motor_I[0] , motor_I[1] , motor_I[2] , motor_I[3]);
//}


/**
  * @brief          DM电机数据解包
  * @param motor    单位电机地址
  * @param rx_data  接收数据
  * @return         none 
  */
//void DM_motor_fbdata(motor_measure_DM_t *motor , uint8_t *rx_data)
//{
//	motor->id = (rx_data[0])&0x0F;
//	motor->err = (rx_data[0])>>4;
//	motor->p_int=(rx_data[1]<<8)|rx_data[2];
//	motor->v_int=(rx_data[3]<<4)|(rx_data[4]>>4);
//	motor->t_int=((rx_data[4]&0xF)<<8)|rx_data[5];
//	motor->pos = uint_to_float(motor->p_int , DM_P_MIN , DM_P_MAX, 16) * 57.6f; // (-12.5,12.5)->(-720,720)
//	motor->vel = uint_to_float(motor->v_int , DM_V_MIN , DM_V_MAX, 12); // (-45.0,45.0)
//	motor->tor = uint_to_float(motor->t_int , DM_T_MIN , DM_T_MAX, 12);  // (-18.0,18.0)
//	motor->T_mos = (float)(rx_data[6]);
//	motor->T_Rotor = (float)(rx_data[7]);

//  while(motor->pos > 180)
//  {
//    motor->pos -= 360;
//  }
//   while(motor->pos <= -180)
//  {
//    motor->pos += 360;
//  }
//}

/**
 * @brief          DM电机数据解包
 * @param motor    单位电机地址
 * @param rx_data  接收数据
 * @return         none
 */
void DM_motor_fbdata(motor_measure_DM_t *motor, uint8_t *rx_data)
{
  motor->id = (rx_data[0]) & 0x0F;                       // 电机控制id：data[0]高四位
  motor->err = (rx_data[0]) >> 4;                        // 错误状态data[0]的后四位
  motor->p_int = (rx_data[1] << 8) | rx_data[2];         // 位置值data[1][2]
  motor->v_int = (rx_data[3] << 4) | (rx_data[4] >> 4);  // 速度值data[3]和data[4]高四位
  motor->t_int = ((rx_data[4] & 0xF) << 8) | rx_data[5]; // 电机扭矩data[4]后四位和data[5]
  // 线性映射，将浮点型数据转换成有符号的定点数据
  motor->pos = uint_to_float(motor->p_int, DM_P_MIN, DM_P_MAX, 16) * 57.6; // (-12.5,12.5)->(-720,720)
  motor->vel = uint_to_float(motor->v_int, DM_V_MIN, DM_V_MAX, 12);        // (-45.0,45.0)
  motor->tor = uint_to_float(motor->t_int, DM_T_MIN, DM_T_MAX, 12);        // (-18.0,18.0)
  motor->T_mos = (float)(rx_data[6]);                                      // 驱动上mos的平均温度
  motor->T_Rotor = (float)(rx_data[7]);                                    // 电机线圈的平均温度
}
/**
  * @brief MIT模式控制
  * @param hcan 所在can接口
  * @param motor_id 电机id
  * @param pos 电机角度
  * @param vel 电机速度
  * @param kp 电机p
  * @param kd 电机d
  * @param torq 电机扭矩
  * @return none 
  */
 void DM_motor_MIT_ctrl(CAN_HandleTypeDef* hcan, uint16_t motor_id, float pos, float vel,float kp, float kd, float torq)
 {
   uint8_t data[8] = {0};
   uint16_t pos_tmp , vel_tmp , kp_tmp , kd_tmp , tor_tmp;
   uint16_t id = motor_id;
 
   pos_tmp = float_to_uint(pos,  DM_P_MIN,  DM_P_MAX,  16);
   vel_tmp = float_to_uint(vel,  DM_V_MIN,  DM_V_MAX,  12);
   kp_tmp  = float_to_uint(kp,   DM_KP_MIN, DM_KP_MAX, 12);
   kd_tmp  = float_to_uint(kd,   DM_KD_MIN, DM_KD_MAX, 12);
   tor_tmp = float_to_uint(torq, DM_T_MIN,  DM_T_MAX,  12);
 
   data[0] = (pos_tmp >> 8);
   data[1] = pos_tmp;
   data[2] = (vel_tmp >> 4);
   data[3] = ((vel_tmp&0xF)<<4)|(kp_tmp>>8);
   data[4] = kp_tmp;
   data[5] = (kd_tmp >> 4);
   data[6] = ((kd_tmp&0xF)<<4)|(tor_tmp>>8);
   data[7] = tor_tmp;
   
   canx_send_data(hcan, id, data, 8);
 }

void DM_motor_enable(CAN_HandleTypeDef* hcan , uint16_t motor_id)
{
  uint8_t data[8];
  data[0] = 0xFF;
	data[1] = 0xFF;
	data[2] = 0xFF;
	data[3] = 0xFF;
	data[4] = 0xFF;
	data[5] = 0xFF;
	data[6] = 0xFF;
	data[7] = 0xFC;
	canx_send_data(hcan , motor_id , data, 8);
}


void DM_motor_disable(CAN_HandleTypeDef* hcan , uint16_t motor_id)
{
  uint8_t data[8];
  data[0] = 0xFF;
	data[1] = 0xFF;
	data[2] = 0xFF;
	data[3] = 0xFF;
	data[4] = 0xFF;
	data[5] = 0xFF;
	data[6] = 0xFF;
	data[7] = 0xFD;
	canx_send_data(hcan , motor_id , data, 8);
}


void DM_motor_zeroset(CAN_HandleTypeDef* hcan , uint16_t motor_id)
{
  uint8_t data[8];
  data[0] = 0xFF;
	data[1] = 0xFF;
	data[2] = 0xFF;
	data[3] = 0xFF;
	data[4] = 0xFF;
	data[5] = 0xFF;
	data[6] = 0xFF;
	data[7] = 0xFE;
	canx_send_data(hcan , motor_id , data, 8);
}
