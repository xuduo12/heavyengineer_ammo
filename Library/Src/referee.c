#include "referee.h"
#include "VT13_rc_ctrl.h"
data_referee_t data_referee = {0};

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
static uint8_t sbus_rx_buf1[2][42];

extern VT13_RC_ctrl_t VT13_rc_ctrl;
void referee_Init()
{ 
    uart6_dma_init(sbus_rx_buf1[0], sbus_rx_buf1[1], 42);
}

void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);
      if(huart6.Instance->SR & UART_FLAG_RXNE)//接收到数据
    {
        __HAL_UART_CLEAR_PEFLAG(&huart6);
    }
    else if(USART6->SR & UART_FLAG_IDLE)
    {
        static uint16_t this_time_rx_len = 0;
        __HAL_UART_CLEAR_PEFLAG(&huart6);
        if ((hdma_usart6_rx.Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(&hdma_usart6_rx);
            //获取接收数据长度,长度 = 设定长度 - 剩余长度
            this_time_rx_len = 42 - hdma_usart6_rx.Instance->NDTR;
            //重新设定数据长度
            hdma_usart6_rx.Instance->NDTR = 42;
            //设定缓冲区1
            hdma_usart6_rx.Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(&hdma_usart6_rx);
            if(this_time_rx_len == 21)
            {
                //sbus_to_rc(sbus_rx_buf[0], &rc_ctrl);
                VT13_to_rc(sbus_rx_buf1[0], &VT13_rc_ctrl);
            }
            else // 其他数据放入FIFO
            {
                fifo_s_puts(&referee_fifo, (char *)uart8_buf[0], this_time_rx_len);
            }
        }
        else
        {
            __HAL_DMA_DISABLE(&hdma_usart6_rx);
            //获取接收数据长度,长度 = 设定长度 - 剩余长度
            this_time_rx_len = 42 - hdma_usart6_rx.Instance->NDTR;
            //重新设定数据长度
            hdma_usart6_rx.Instance->NDTR = 42;
            //设定缓冲区0
            DMA1_Stream1->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(&hdma_usart6_rx);
            if(this_time_rx_len == 21)
            {
                 VT13_to_rc(sbus_rx_buf1[1], &VT13_rc_ctrl);
            }
        }
    }
}

void UART8_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart8);

    if (UART8->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart8);
        static uint16_t this_time_rx_len = 0;

        if ((huart8.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(huart8.hdmarx);
            this_time_rx_len = USART8_RX_BUF_LENGTH - __HAL_DMA_GET_COUNTER(huart8.hdmarx);
            __HAL_DMA_SET_COUNTER(huart8.hdmarx, USART8_RX_BUF_LENGTH);
            huart8.hdmarx->Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(huart8.hdmarx);

            // 处理两种类型的数据
            if (this_time_rx_len == 21) // VT13遥控器数据长度
            {
                VT13_to_rc(uart8_buf[0], &VT13_rc_ctrl);
            }
            else // 其他数据放入FIFO
            {
                fifo_s_puts(&referee_fifo, (char *)uart8_buf[0], this_time_rx_len);
            }
        }
        else
        {
            __HAL_DMA_DISABLE(huart8.hdmarx);
            this_time_rx_len = USART8_RX_BUF_LENGTH - __HAL_DMA_GET_COUNTER(huart8.hdmarx);
            __HAL_DMA_SET_COUNTER(huart8.hdmarx, USART8_RX_BUF_LENGTH);
            huart8.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(huart8.hdmarx);

            // 处理两种类型的数据
            if (this_time_rx_len == 21) // VT13遥控器数据长度
            {
                VT13_to_rc(uart8_buf[1], &VT13_rc_ctrl);
            }
            else // 其他数据放入FIFO
            {
                fifo_s_puts(&referee_fifo, (char *)uart8_buf[1], this_time_rx_len);
            }
        }
    }
}


void uart6_dma_init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num)
{
   //enable the DMA transfer for the receiver request
    //使能DMA串口接收
    SET_BIT(huart6.Instance->CR3, USART_CR3_DMAR);
    //enalbe idle interrupt
    //使能空闲中断
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);
    //disable DMA
    //失效DMA
    __HAL_DMA_DISABLE(&hdma_usart6_rx);

    while(hdma_usart6_rx.Instance->CR & DMA_SxCR_EN)
    {
        __HAL_DMA_DISABLE(&hdma_usart6_rx);
    }

    hdma_usart6_rx.Instance->PAR = (uint32_t) & (USART6->DR);
    //memory buffer 1
    //内存缓冲区1
    hdma_usart6_rx.Instance->M0AR = (uint32_t)(rx1_buf);
    //memory buffer 2
    //内存缓冲区2
    hdma_usart6_rx.Instance->M1AR = (uint32_t)(rx2_buf);
    //data length
    //数据长度
    hdma_usart6_rx.Instance->NDTR = dma_buf_num;
    //enable double memory buffer
    //使能双缓冲区
    SET_BIT(hdma_usart6_rx.Instance->CR, DMA_SxCR_DBM);

    //enable DMA
    //使能DMA
    __HAL_DMA_ENABLE(&hdma_usart6_rx);
}