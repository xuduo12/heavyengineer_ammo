#ifndef DATA_H
#define DATA_H
#include "main.h"
#include "CAN_receive.h"
#include "UART_miniPC.h"
#include "referee.h"

void usart_printf(UART_HandleTypeDef *huart, const char *fmt, ...);


#endif
