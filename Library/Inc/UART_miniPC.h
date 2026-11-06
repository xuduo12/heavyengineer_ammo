#include "main.h"
#include "fifo.h"
#include "referee.h"

#ifndef UART_MINIPC
#define UART_MINIPC


#define UART_MINI_PC_INFO_NAV_LEN 8
#define UART_MINI_PC_INFO_AIM_LEN 12
#define UART_MINI_PC_INFO_AIM_SEND_LEN 8
#define UART_MINI_PC_INFO_NAV_SEND_LEN 28


#define UART_RX_BUF_LENGHT     24
#define UART_MINIPC_FIFO_BUF_LENGTH 512
#define UART_PROTOCOL_FRAME_MAX_SIZE      128

#define UART_TX_BUF_LENGHT     256


#define NAV_HEADER_SOF 0xA5
#define NAV_HEADER_EOF 0x44

#define AIM_HEADER_SOF 0x43
#define AIM_HEADER_EOF 0xFF

#define AIM_SEND_HEADER_SOF 0x53
#define AIM_SEND_HEADER_EOF 0x45

#define NAV_SEND_HEADER_SOF 0x53
#define NAV_SEND_HEADER_EOF 0x45


typedef enum 
{
  STEP_FIRST = 0,
  STEP_SOF = 1,
  STEP_LEN = 2,
  STEP_EOF = 3,
} unpack_step_e;

typedef struct __attribute__((packed)) 
{
  uint8_t        protocol_packet[UART_PROTOCOL_FRAME_MAX_SIZE];
  unpack_step_e  unpack_step;
  uint16_t       index;
}unpack_data_t;

typedef struct __attribute__((packed))
{
  uint8_t sof;            //0x43
  uint8_t aim_ready;
  uint8_t fire_ready;
  int16_t aim_yaw;
  int16_t aim_pitch;
  int16_t aim_x;
  int16_t aim_y;
  uint8_t eof;            //0x44
}data_miniPC_aim_t;

typedef struct __attribute__((packed))
{
  uint8_t sof;            
  int16_t x_speed;      
  int16_t y_speed;      
  uint16_t z_angle;      
  uint8_t eof;
}data_miniPC_nav_t;

typedef struct __attribute__((packed))
{
  uint8_t sof;                          
  uint8_t mode;                   
  uint16_t yaw;    
  uint16_t pitch;
  uint8_t shoot;          
  uint8_t eof;
}data_miniPC_aim_send_t;

typedef struct __attribute__((packed))
{
  ext_game_state_t current_game_state;
  ext_game_robot_HP_t current_HP;
  ext_game_robot_pos_t current_pos;
  custom_robot_data_t opposite_outpost;
  custom_robot_data_t my_outpost;
}data_mini_PC_nav_send_t;




void uart_miniPC_Init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num);
void uart_miniPC_unpack_data(int n);
void uart_miniPC_nav_read_data(uint8_t *byte);
void uart_miniPC_aim_read_data(uint8_t *byte);
void miniPC_Init(void);
void uart_miniPC_send_data(void);
int float_2_uint(float x , float x_min , float x_max , int bits);

#endif