#include "UART_miniPC.h"

fifo_s_t uart_miniPC_fifo;
uint8_t usart1_buf[2][UART_RX_BUF_LENGHT];
uint8_t usart1_send_buf[2][UART_TX_BUF_LENGHT];
extern uint8_t referee_fifo_buf[UART_MINIPC_FIFO_BUF_LENGTH];
uint8_t uart_miniPC_fifo_buf[UART_MINIPC_FIFO_BUF_LENGTH];
data_miniPC_nav_t data_miniPC_nav = {0};
data_miniPC_aim_t data_miniPC_aim = {0};
data_miniPC_aim_send_t data_miniPC_aim_send = {0};
data_miniPC_aim_send_t data_miniPC_nav_send = {0};
unpack_data_t uart_miniPC_unpack_obj = {0};

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

extern uint8_t usart6_buf[2][UART_RX_BUF_LENGHT];
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;

uint8_t tx[9] = {0};
uint8_t mode = 0;

void miniPC_Init(void)
{
    uart_miniPC_Init(usart1_buf[0], usart1_buf[1], UART_RX_BUF_LENGHT);
    // uart_miniPC_send_Init(usart1_send_buf[0], usart1_send_buf[1], UART_TX_BUF_LENGHT);
    fifo_s_init(&uart_miniPC_fifo, uart_miniPC_fifo_buf, UART_MINIPC_FIFO_BUF_LENGTH);
}

void uart_miniPC_Init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num)
{
    // enable the DMA transfer for the receiver and tramsmit request
    // 使能DMA串口接收和发送
    SET_BIT(huart1.Instance->CR3, USART_CR3_DMAR);
    SET_BIT(huart1.Instance->CR3, USART_CR3_DMAT);
    // enalbe idle interrupt
    // 使能空闲中断
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE); // 开启中断
    // disable DMA
    // 失效DMA
    __HAL_DMA_DISABLE(&hdma_usart1_rx);

    while (hdma_usart1_rx.Instance->CR & DMA_SxCR_EN) // 使能双缓冲区
    {
        __HAL_DMA_DISABLE(&hdma_usart1_rx);
    }

    __HAL_DMA_CLEAR_FLAG(&hdma_usart1_rx, DMA_LISR_TCIF1);

    hdma_usart1_rx.Instance->PAR = (uint32_t)&(USART1->DR);
    // memory buffer 1
    // 内存缓冲区1
    hdma_usart1_rx.Instance->M0AR = (uint32_t)(rx1_buf); // 设备初始缓冲区，数据存到指定内存区域，地址换为32位指针
    // memory buffer 2
    // 内存缓冲区2
    hdma_usart1_rx.Instance->M1AR = (uint32_t)(rx2_buf); // 设备备用缓冲区
    // data length
    // 数据长度
    __HAL_DMA_SET_COUNTER(&hdma_usart1_rx, dma_buf_num);

    // enable double memory buffer
    // 使能双缓冲区
    SET_BIT(hdma_usart1_rx.Instance->CR, DMA_SxCR_DBM); // 启用双缓冲模式

    // enable DMA
    // 使能DMA
    __HAL_DMA_ENABLE(&hdma_usart1_rx);

    // disable DMA
    // 失效DMA
    __HAL_DMA_DISABLE(&hdma_usart1_tx);

    while (hdma_usart1_tx.Instance->CR & DMA_SxCR_EN)
    {
        __HAL_DMA_DISABLE(&hdma_usart1_tx);
    }

    hdma_usart1_tx.Instance->PAR = (uint32_t)&(USART1->DR); // DR数据寄存器，外设地址
}

void uart_miniPC_nav_read_data(uint8_t *frame)
{
    memcpy(&data_miniPC_nav, frame, sizeof(data_miniPC_nav_t));
}

void uart_miniPC_aim_read_data(uint8_t *frame)
{
    memcpy(&data_miniPC_aim, frame, sizeof(data_miniPC_aim_t));

//    while (data_miniPC_aim.aim_yaw > 18000)
//    {
//        data_miniPC_aim.aim_yaw -= 36000;
//    }
//    while (data_miniPC_aim.aim_yaw < -18000)
//    {
//        data_miniPC_aim.aim_yaw += 36000;
//    }

//    while (data_miniPC_aim.aim_pitch > 18000)
//    {
//        data_miniPC_aim.aim_pitch -= 36000;
//    }
//    while (data_miniPC_aim.aim_pitch < -18000)
//    {
//        data_miniPC_aim.aim_pitch += 36000;
//    }

//    data_miniPC_aim.aim_yaw = data_miniPC_aim.aim_yaw * 22.756 / 100;
//    data_miniPC_aim.aim_pitch = data_miniPC_aim.aim_pitch * 22.765 / 100;
}

void uart_miniPC_unpack_data(int n)
{
    for (int i = 0; i < sizeof(data_miniPC_nav); i++)
    {
        if (i + sizeof(data_miniPC_aim) < UART_RX_BUF_LENGHT)
        {
            if (usart1_buf[n][i] == NAV_HEADER_SOF && usart1_buf[n][i + sizeof(data_miniPC_nav) - 1] == NAV_HEADER_EOF)
            {
                uart_miniPC_nav_read_data(usart1_buf[0]);
                mode = 0;
            }
            
            if (usart1_buf[n][i] == AIM_HEADER_SOF && usart1_buf[n][i + sizeof(data_miniPC_aim) - 1] == AIM_HEADER_EOF)
            {
                uart_miniPC_aim_read_data(usart1_buf[0]);
                mode = 1;
            }
        }
    }
}

void uart_miniPC_send_data(void)
{
    if (mode == 1)
    {
        HAL_UART_Transmit_DMA(&huart1, (uint8_t *)&data_miniPC_aim_send, 8); // 首次调用dma发送，后续因为会触发传输完成中断，因而无需再次调用
    }
    else if (mode == 0)
    {
			HAL_UART_Transmit_DMA(&huart1, (uint8_t *)&data_miniPC_nav_send, 28);///报错不一定正常
    }
}

void USART1_IRQHandler(void)
{
    static volatile uint8_t res;
    HAL_UART_IRQHandler(&huart1);
    if (USART1->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart1);

        static uint16_t this_time_rx_len = 0;

        if ((huart1.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET) // 如果是缓冲区0
        {
            __HAL_DMA_DISABLE(huart1.hdmarx);                                             // 失能dma
            __HAL_DMA_SET_COUNTER(huart1.hdmarx, UART_RX_BUF_LENGHT);                     // 原本存储空间
            huart1.hdmarx->Instance->CR |= DMA_SxCR_CT;                                   // 将缓冲区变成1
            __HAL_DMA_ENABLE(huart1.hdmarx);                                              // 使能dma
            uart_miniPC_unpack_data(0);
        }

        else // 如果是缓冲区1
        {
            __HAL_DMA_DISABLE(huart1.hdmarx);                                             // 失能dma
            __HAL_DMA_SET_COUNTER(huart1.hdmarx, UART_RX_BUF_LENGHT);                     // 原本存储空间
            huart1.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);                                // 将缓冲区变成0
            __HAL_DMA_ENABLE(huart1.hdmarx);                                              // 使能dma
            uart_miniPC_unpack_data(1);      
        }
        if (this_time_rx_len > 0)
        {
            uart_miniPC_send_data();
        }
    }
}

int float_2_uint(float x, float x_min, float x_max, int bits)
{
    /* Converts a float to an unsigned int, given range and number of bits */
    float span = x_max - x_min;
    float offset = x_min;
    return (int)((x - offset) * ((float)((1 << bits) - 1)) / span);
}
