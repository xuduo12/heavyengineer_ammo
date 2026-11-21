// 适配 2027 通讯协议 V1.0.0
// 裁判系统数据(0310/0311) + VT13 遥控器数据 共用 USART6, 按帧长度分流
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

/* VT13 遥控器一帧固定 21 字节; 裁判系统帧最短也大于它, 用它区分两路数据 */
#define VT13_FRAME_LEN   21

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

        /* 按帧长度分流: 21 字节是 VT13 遥控器, 其余交给裁判系统解析 */
        if (this_time_rx_len == VT13_FRAME_LEN)
        {
            VT13_to_rc(rx_buf, &VT13_rc_ctrl);
        }
        else if (this_time_rx_len > 0)
        {
            fifo_s_puts(&uart_referee_fifo, (char *)rx_buf, this_time_rx_len);
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
