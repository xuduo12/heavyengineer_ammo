// 适配 2027 通讯协议 V1.0.0
// USART6 = 图传链路(921600): 0310/0311 自定义数据 + VT13 遥控器
// 两路数据流都进同一个 FIFO, 由 referee_unpack_fifo_data() 里的状态机分流
#include "referee.h"
#include "VT13_rc_ctrl.h"

data_referee_t data_referee = {0};
uint8_t usart6_buf[2][USART6_RX_BUF_LENGHT];
fifo_s_t uart_referee_fifo;
uint8_t uart_referee_fifo_buf[UART_REFEREE_FIFO_BUF_LENGTH];
referee_unpack_data_t referee_unpack_obj;

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;

extern VT13_RC_ctrl_t VT13_rc_ctrl;

/* VT13 遥控器整帧固定 21 字节(首字节 0xA9, 与裁判系统的 0xA5 不同) */
#define VT13_FRAME_LEN     21
#define VT13_SOF           0xA9

/**
 * @brief  把一段连续的串口字节按帧分流给 VT13 或裁判系统解析器
 * @note   一次 DMA 搬运里可能含有多个帧(裁判帧与遥控帧混在一起)，
 *         所以这里按字节扫描 0xA5 帧头，而不是依赖"整段长度等于某一帧长"。
 *         数据全部是逐字节流式处理, 帧与帧之间不需要对齐。
 */
static void usart6_feed(const uint8_t *buf, uint16_t len)
{
    static uint8_t  vt13_frame[VT13_FRAME_LEN];
    static uint16_t vt13_cnt = 0;
    /* 裁判系统帧的收集状态: 0=等 A5, 1=收帧头剩余, 2=收帧体 */
    static uint8_t  ref_hdr[REF_PROTOCOL_HEADER_SIZE];
    static uint16_t ref_hdr_cnt = 0;
    static uint16_t ref_frame_len = 0;   /* 整帧长度(含头/尾) */
    static uint16_t ref_data_cnt = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t b = buf[i];

        /* ---- VT13 正在收帧 ---- */
        if (vt13_cnt > 0)
        {
            vt13_frame[vt13_cnt++] = b;
            if (vt13_cnt == VT13_FRAME_LEN)
            {
                VT13_to_rc(vt13_frame, &VT13_rc_ctrl);
                vt13_cnt = 0;
            }
            continue;
        }

        /* ---- 裁判系统正在收帧头 ---- */
        if (ref_hdr_cnt > 0)
        {
            ref_hdr[ref_hdr_cnt++] = b;
            if (ref_hdr_cnt == REF_PROTOCOL_HEADER_SIZE)
            {
                uint16_t data_len;
                memcpy(&data_len, &ref_hdr[1], sizeof(uint16_t));
                /* 整帧 = 5(头) + 2(cmd_id) + data_len + 2(CRC16) */
                ref_frame_len = REF_PROTOCOL_HEADER_SIZE + sizeof(uint16_t) + data_len
                              + REF_PROTOCOL_CRC16_SIZE;

                if (data_len == 0 || ref_frame_len > REF_PROTOCOL_FRAME_MAX_SIZE)
                {
                    /* 长度非法, 丢弃这个假帧头 */
                    ref_hdr_cnt = 0;
                    ref_frame_len = 0;
                }
                else if (ref_frame_len == VT13_FRAME_LEN)
                {
                    /* 长度正好 21 -> 这是 VT13, 转交给遥控器 */
                    memcpy(vt13_frame, ref_hdr, REF_PROTOCOL_HEADER_SIZE);
                    vt13_cnt = REF_PROTOCOL_HEADER_SIZE;
                }
                else
                {
                    memcpy(referee_unpack_obj.protocol_packet, ref_hdr,
                           REF_PROTOCOL_HEADER_SIZE);
                    referee_unpack_obj.index = REF_PROTOCOL_HEADER_SIZE;
                    referee_unpack_obj.data_len = data_len;
                    referee_unpack_obj.unpack_step = STEP_DATA_CRC16;
                    ref_data_cnt = REF_PROTOCOL_HEADER_SIZE;
                }
                ref_hdr_cnt = 0;
            }
            continue;
        }

        /* ---- 裁判系统正在收帧体 ---- */
        if (ref_data_cnt > 0)
        {
            referee_unpack_obj.protocol_packet[referee_unpack_obj.index++] = b;
            ref_data_cnt++;
            if (ref_data_cnt == ref_frame_len)
            {
                if (Verify_CRC16_Check_Sum((uint8_t *)referee_unpack_obj.protocol_packet,
                                           ref_frame_len))
                {
                    referee_read_data((uint8_t *)referee_unpack_obj.protocol_packet);
                }
                ref_data_cnt = 0;
                ref_frame_len = 0;
            }
            continue;
        }

        /* ---- 空闲: 找帧头 ---- */
        if (b == HEADER_SOF)
        {
            /* 先按可能的 VT13 帧首字节暂存，等凑够 5 字节判断长度 */
            ref_hdr[0] = b;
            ref_hdr_cnt = 1;
        }
    }
}


void referee_Init(void)
{
    fifo_s_init(&uart_referee_fifo, uart_referee_fifo_buf, UART_REFEREE_FIFO_BUF_LENGTH);
    uart6_dma_Init(usart6_buf[0], usart6_buf[1], USART6_RX_BUF_LENGHT);
}

void referee_unpack_fifo_data(void)
{
    uint8_t byte = 0;
    uint8_t sof = HEADER_SOF;
    referee_unpack_data_t *p_obj = &referee_unpack_obj;

    while (fifo_s_used(&uart_referee_fifo))
    {
        byte = fifo_s_get(&uart_referee_fifo);
        switch (p_obj->unpack_step)
        {
        case STEP_HEADER_SOF:
        {
            if (byte == sof)
            {
                p_obj->unpack_step = STEP_LENGTH_LOW;
                p_obj->protocol_packet[p_obj->index++] = byte;
            }
            else
            {
                p_obj->index = 0;
            }
        }
        break;

        case STEP_LENGTH_LOW:
        {
            p_obj->data_len = byte;
            p_obj->protocol_packet[p_obj->index++] = byte;
            p_obj->unpack_step = STEP_LENGTH_HIGH;
        }
        break;

        case STEP_LENGTH_HIGH:
        {
            p_obj->data_len |= (byte << 8);
            p_obj->protocol_packet[p_obj->index++] = byte;

            if (p_obj->data_len < (REF_PROTOCOL_FRAME_MAX_SIZE - REF_HEADER_CRC_CMDID_LEN))
            {
                p_obj->unpack_step = STEP_FRAME_SEQ;
            }
            else
            {
                p_obj->unpack_step = STEP_HEADER_SOF;
                p_obj->index = 0;
            }
        }
        break;

        case STEP_FRAME_SEQ:
        {
            p_obj->protocol_packet[p_obj->index++] = byte;
            p_obj->unpack_step = STEP_HEADER_CRC8;
        }
        break;

        case STEP_HEADER_CRC8:
        {
            p_obj->protocol_packet[p_obj->index++] = byte;

            if (p_obj->index == 5)
            {
                if (Verify_CRC8_Check_Sum(p_obj->protocol_packet, 5))
                {
                    p_obj->unpack_step = STEP_DATA_CRC16;
                }
                else
                {
                    p_obj->unpack_step = STEP_HEADER_SOF;
                    p_obj->index = 0;
                }
            }
        }
        break;

        case STEP_DATA_CRC16:
        {
            if (p_obj->index < (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
            {
                p_obj->protocol_packet[p_obj->index++] = byte;
            }
            else if (p_obj->index >= (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
            {
                p_obj->unpack_step = STEP_HEADER_SOF;
                p_obj->index = 0;
                if (Verify_CRC16_Check_Sum(p_obj->protocol_packet, 9 + p_obj->data_len))
                {
                    referee_read_data(p_obj->protocol_packet);
                }
            }
        }
        break;

        default:
        {
            p_obj->unpack_step = STEP_HEADER_SOF;
            p_obj->index = 0;
        }
        break;
        }
    }
}

void referee_read_data(uint8_t *frame)
{
    uint16_t cmd_id = 0;
    uint8_t index = 0;

    memcpy(&data_referee.FrameHeader, frame, 5);
    index += 5;
    memcpy(&cmd_id, frame + index, sizeof(uint16_t));
    index += sizeof(uint16_t);

    switch (cmd_id)
    {
    case 0x0001:
        memcpy(&data_referee.GameState, frame + index, sizeof(game_state_t));
        break;
    case 0x0002:
        memcpy(&data_referee.GameResult, frame + index, sizeof(game_result_t));
        break;
    case 0x0003:
        memcpy(&data_referee.GameRobotHP, frame + index, sizeof(game_robot_HP_t));
        break;
    case 0x0101:
        memcpy(&data_referee.EventData, frame + index, sizeof(event_data_t));
        break;
    case 0x0104:
        memcpy(&data_referee.RefereeWarning, frame + index, sizeof(referee_warning_t));
        break;
    case 0x0105:
        memcpy(&data_referee.DartInfo, frame + index, sizeof(dart_info_t));
        break;
    case 0x0201:
        memcpy(&data_referee.GameRobotState, frame + index, sizeof(game_robot_state_t));
        break;
    case 0x0202:
        memcpy(&data_referee.PowerHeatData, frame + index, sizeof(power_heat_data_t));
        break;
    case 0x0203:
        memcpy(&data_referee.GameRobotPos, frame + index, sizeof(game_robot_pos_t));
        break;
    case 0x0204:
        memcpy(&data_referee.BuffMusk, frame + index, sizeof(buff_musk_t));
        break;
    case 0x0206:
        memcpy(&data_referee.RobotHurt, frame + index, sizeof(robot_hurt_t));
        break;
    case 0x0207:
        memcpy(&data_referee.ShootData, frame + index, sizeof(shoot_data_t));
        break;
    case 0x0208:
        memcpy(&data_referee.ProjectileAllowance, frame + index, sizeof(projectile_allowance_t));
        break;
    case 0x0209:
        memcpy(&data_referee.RfidStatus, frame + index, sizeof(rfid_status_t));
        break;
    case 0x020A:
        memcpy(&data_referee.DartClientCmd, frame + index, sizeof(dart_client_cmd_t));
        break;
    case 0x020B:
        memcpy(&data_referee.GroundRobotPosition, frame + index, sizeof(ground_robot_position_t));
        break;
    case 0x020C:
        memcpy(&data_referee.RadarMarkData, frame + index, sizeof(radar_mark_data_t));
        break;
    case 0x020D:
        memcpy(&data_referee.SentryInfo, frame + index, sizeof(sentry_info_t));
        break;
    case 0x020E:
        memcpy(&data_referee.RadarInfo, frame + index, sizeof(radar_info_t));
        break;
    case 0x0301:
        memcpy(&data_referee.InteractionData, frame + index, sizeof(robot_interaction_data_t));
        break;
    case 0x0303:
        memcpy(&data_referee.MapCommand, frame + index, sizeof(map_command_t));
        break;
    case 0x0310:
        memcpy(&data_referee.robot_to_custom, frame + index, sizeof(custom_robot_data_2_t));
        break;
    case 0x0311:
        memcpy(&data_referee.custom_to_robot, frame + index, sizeof(custom_robot_data_t));
        break;
    default:
        break;
    }
}

void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);

    if (USART6->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart6);

        static uint16_t this_time_rx_len = 0;
        uint8_t *rx_buf;

        if ((huart6.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART6_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART6_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(huart6.hdmarx);
            rx_buf = usart6_buf[0];
        }
        else
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART6_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART6_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(huart6.hdmarx);
            rx_buf = usart6_buf[1];
        }

        /* 逐字节分流: 帧可能跨 DMA 段, 也可能一段里有多帧 */
        if (this_time_rx_len > 0)
        {
            usart6_feed(rx_buf, this_time_rx_len);
        }
    }
}

void uart6_dma_Init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num)
{
    // enable the DMA transfer for the receive and transmit request
    SET_BIT(huart6.Instance->CR3, USART_CR3_DMAR);
    SET_BIT(huart6.Instance->CR3, USART_CR3_DMAT);

    // enable idle interrupt
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);

    // disable DMA
    __HAL_DMA_DISABLE(&hdma_usart6_rx);

    while (hdma_usart6_rx.Instance->CR & DMA_SxCR_EN)
    {
        __HAL_DMA_DISABLE(&hdma_usart6_rx);
    }

    __HAL_DMA_CLEAR_FLAG(&hdma_usart6_rx, DMA_LISR_TCIF1);

    hdma_usart6_rx.Instance->PAR = (uint32_t) & (USART6->DR);
    // memory buffer 1
    hdma_usart6_rx.Instance->M0AR = (uint32_t)(rx1_buf);
    // memory buffer 2
    hdma_usart6_rx.Instance->M1AR = (uint32_t)(rx2_buf);
    // data length
    __HAL_DMA_SET_COUNTER(&hdma_usart6_rx, dma_buf_num);

    // enable double memory buffer
    SET_BIT(hdma_usart6_rx.Instance->CR, DMA_SxCR_DBM);

    // enable DMA
    __HAL_DMA_ENABLE(&hdma_usart6_rx);

    // disable DMA (tx)
    __HAL_DMA_DISABLE(&hdma_usart6_tx);

    while (hdma_usart6_tx.Instance->CR & DMA_SxCR_EN)
    {
        __HAL_DMA_DISABLE(&hdma_usart6_tx);
    }

    hdma_usart6_tx.Instance->PAR = (uint32_t) & (USART6->DR);
}
