#include "VT13_rc_ctrl.h"
#include "main.h"
#include "crc_ref.h"
//#include "detect_task.h"

/* ================= 发射控制模式(编译期选择) =================
 * 0 = 遥控器(默认):
 *       shutter      -> status_ammo 在 0/1 之间切换
 *       right_button -> 按住置 2, 松开回 0
 * 1 = 键鼠模式:
 *       mouse.press_l -> 左键按下 status_ammo = 1, 松开 = 0
 * 改这里之后重新 make 即可, 不需要改别的代码。
 */
#define AMMO_CTRL_USE_MOUSE   0

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
	switch (VT13_data_rc->VT13_ctrl->rc.mode_sw)
    {
        case 0: VT13_data_rc->status_chassis = 0; VT13_data_rc->status_gimbal = 0; break;
        case 1: VT13_data_rc->status_chassis = 1; VT13_data_rc->status_gimbal = 1; break; 
        case 2: VT13_data_rc->status_chassis = 2; VT13_data_rc->status_gimbal = 2; break; 

        default: break;
    }

    /* 发射状态(具体行为由 AMMO_CTRL_USE_MOUSE 决定, 见文件头部):
     *   遥控器模式: shutter 在 0/1 间切换, right_button 按住置 2 / 松开回 0
     *   键鼠模式  : mouse.press_l 按下置 1 / 松开置 0
     */
    static uint8_t shutter_last = 0;
    static uint8_t right_btn_last = 0;
    uint8_t shutter_now = VT13_data_rc->VT13_ctrl->rc.shutter;
    uint8_t right_btn   = VT13_data_rc->VT13_ctrl->rc.right_button;
    uint8_t mouse_left  = VT13_data_rc->VT13_ctrl->mouse.press_l;

    if(VT13_data_rc->status_gimbal == 1 || VT13_data_rc->status_gimbal == 2)
    {
#if AMMO_CTRL_USE_MOUSE
        /* ---------- 键鼠模式: 鼠标左键按住 = 发射(1), 松开 = 停(0) ---------- */
        VT13_data_rc->status_ammo = (mouse_left == 1) ? 1 : 0;
        (void)shutter_last;
        (void)right_btn_last;
#else
        /* ---------- 遥控器模式 ---------- */
        if (right_btn == 1)
        {
            /* 按住右键: 置 2 */
            VT13_data_rc->status_ammo = 2;
        }
        else
        {
            if (right_btn_last == 1)
            {
                /* 刚刚松开右键 -> 回到 0 */
                VT13_data_rc->status_ammo = 0;
            }
            else if (shutter_now == 1 && shutter_last == 0)
            {
                /* shutter 按下 -> 在 0/1 之间切换 */
                VT13_data_rc->status_ammo = (VT13_data_rc->status_ammo == 0) ? 1 : 0;
            }
        }
        (void)mouse_left;
#endif
    }
    else
    {
        /* 不在发射状态: 状态清零 */
        VT13_data_rc->status_ammo = 0;
    }

    /* 记录本次按键状态, 保证只有真正的边沿才响应 */
    shutter_last   = shutter_now;
    right_btn_last = right_btn;
}
