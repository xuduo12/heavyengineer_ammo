#include "VT13_rc_ctrl.h"
#include "main.h"
#include "crc_ref.h"
//#include "detect_task.h"

// remote control data
VT13_RC_ctrl_t VT13_rc_ctrl;
VT13_data_rc_t VT13_data_rc;
/**
 * @brief          获取遥控器数据指针
 * @param[in]      none
 * @retval         遥控器数据指针
 */
const VT13_RC_ctrl_t *get_VT13_remote_control_point(void)
{
    return &VT13_rc_ctrl;
}

/**
 * @brief          遥控器协议解析
 * @param[in]      VT13_buf: 原生数据指针
 * @param[out]     rc_ctrl: 遥控器数据指
 * @retval         none
 */
void VT13_to_rc( uint8_t *VT13_buf, VT13_RC_ctrl_t *rc_ctrl)
{
   
    if (VT13_buf == NULL || rc_ctrl == NULL)
    {
        return;
    }

  
	if(VT13_buf[0] == 0xa9 && VT13_buf[1] == 0x53 && Verify_CRC16_Check_Sum(VT13_buf, 21)) 
	{			

		rc_ctrl->rc.ch[0] = (VT13_buf[2] | (VT13_buf[3] << 8)) & 0x07ff;        //!< Channel 0
	
		rc_ctrl->rc.ch[1] = ((VT13_buf[3] >> 3) | (VT13_buf[4] << 5)) & 0x07ff; //!< Channel 1
	
		rc_ctrl->rc.ch[2] = ((VT13_buf[4] >> 6) | (VT13_buf[5] << 2) |          //!< Channel 2
													(VT13_buf[6] << 10)) & 0x07ff;
	
		rc_ctrl->rc.ch[3] = ((VT13_buf[6] >> 1) | (VT13_buf[7] << 7)) & 0x07ff; //!< Channel 3
	
		rc_ctrl->rc.mode_sw = ((VT13_buf[7] >> 4) & 0x0003); 
	
		rc_ctrl->rc.stop = ((VT13_buf[7] >> 6) & 0x01);
	
		rc_ctrl->rc.left_button = ((VT13_buf[7] >> 7) & 0x01);//fn
	
		rc_ctrl->rc.right_button = ((VT13_buf[8] >> 0) & 0x01);
	
		rc_ctrl->rc.wheel = ((VT13_buf[8] >> 1) | (VT13_buf[9] << 7)) & 0x07FF;
	
		rc_ctrl->rc.shutter = (VT13_buf[9] >> 4) & 0x01;//??
		
	
		rc_ctrl->mouse.x = (VT13_buf[10] | (VT13_buf[11] << 8));
	
		rc_ctrl->mouse.y = (VT13_buf[12] | (VT13_buf[13] << 8));
	
		rc_ctrl->mouse.z = (VT13_buf[14] | (VT13_buf[15] << 8));
		
	
		rc_ctrl->mouse.press_l = (VT13_buf[16] >> 0) & 0x03;

		rc_ctrl->mouse.press_r = (VT13_buf[16] >> 2) & 0x03;
	
		rc_ctrl->mouse.middle = (VT13_buf[16] >> 4) & 0x03;
	
		rc_ctrl->key.v = (VT13_buf[17] | (VT13_buf[18] << 8));
		
	
		rc_ctrl->crc16 = (VT13_buf[19] | (VT13_buf[20] << 8));
		

		rc_ctrl->rc.ch[0] -= RC_CH_VALUE_OFFSET;
		rc_ctrl->rc.ch[1] -= RC_CH_VALUE_OFFSET;
		rc_ctrl->rc.ch[2] -= RC_CH_VALUE_OFFSET;
		rc_ctrl->rc.ch[3] -= RC_CH_VALUE_OFFSET;
		rc_ctrl->rc.wheel -= RC_CH_VALUE_OFFSET;
	}
}

void VT13_read_data(VT13_data_rc_t *VT13_data_rc)
{
	VT13_data_rc->speed_y = VT13_data_rc->VT13_ctrl->rc.ch[1];
    VT13_data_rc->speed_x = VT13_data_rc->VT13_ctrl->rc.ch[0];
    VT13_data_rc->speed_yaw = VT13_data_rc->VT13_ctrl->rc.ch[3];
	switch (VT13_data_rc->VT13_ctrl->mode_sw)
    {
        case 0: VT13_data_rc->status_chassis = 0; VT13_data_rc->status_gimbal = 0; break;
        case 1: VT13_data_rc->status_chassis = 1; VT13_data_rc->status_gimbal = 1; break; 
        case 2: VT13_data_rc->status_chassis = 2; VT13_data_rc->status_gimbal = 2; break; 

        default: break;
    }

    if(VT13_data_rc->status_gimbal == 1 || VT13_data_rc->status_gimbal == 2)
    {
        switch (VT13_data_rc->VT13_ctrl->rc.s[1])
        {
            case 2: VT13_data_rc->status_ammo = 0; break;
            case 3: VT13_data_rc->status_ammo = 1; break; 
            case 1: VT13_data_rc->status_ammo = 2; break; 

            default: break;
        }
    }
    else if(VT13_data_rc->status_gimbal != 1)
    {
        VT13_data_rc->status_ammo = 0;
    }
}