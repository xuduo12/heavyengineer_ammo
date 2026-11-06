#ifndef IST8310_DATA_H
#define IST8310_DATA_H 
#include "main.h"
#include "i2c.h"

#define IST8310_IIC_ADDRESS 0x0E            /*IST8310传感器I2C地址*/
#define IST8310_REG_WHO_ADDRESS 0x00        /*IST8310 验证位寄存器起始位地址*/
#define IST8310_REG_PITCH_ADDRESS 0x03      /*IST8310 Pitch寄存器起始位地址*/
#define IST8310_REG_YAW_ADDRESS 0x05        /*IST8310 Yaw寄存器起始位地址*/
#define IST8310_REG_ROW_ADDRESS 0x07        /*IST8310 Row寄存器起始位地址*/
#define IST8310_REG_TEMP_ADDRESS 0x1C       /*IST8310 温度寄存器起始位地址*/

#define MAG_SEN 0.3f


void ist8310_init(void);
void ist8310_data_read(float ist8310_data[4]);
void ist8310_delay_us(uint16_t us);

#endif