#include "IST8310.h"
#include "cmsis_os.h"
static uint8_t ist8310_init_matrix[4][2] ={
    {0x0B , 0x08},
    {0x41 , 0x24},
    {0x42 , 0xC0},
    {0x0A , 0x0B},
}; 


void ist8310_init(void)
{
    static const uint8_t sleepTime = 50;
    static const uint8_t wait_time = 1;
    uint8_t ist8310_state[5];
    while(ist8310_state[4] != 0x10)
    {
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
        osDelay(sleepTime);
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
        osDelay(sleepTime);
        HAL_I2C_Mem_Read(&hi2c3 , IST8310_IIC_ADDRESS <<1 , IST8310_REG_WHO_ADDRESS , I2C_MEMADD_SIZE_8BIT , &ist8310_state[4] , 1 , 10);
    }

    static int i;
    for(i = 0 ; i < 4 ; i++)
    {
        while(ist8310_state[i] != ist8310_init_matrix[i][1])
        {
            HAL_I2C_Mem_Write(&hi2c3 , IST8310_IIC_ADDRESS <<1 , ist8310_init_matrix[i][0] , I2C_MEMADD_SIZE_8BIT , &ist8310_init_matrix[i][1] , 1 , 10);
            ist8310_delay_us(wait_time);
            HAL_I2C_Mem_Read(&hi2c3 , IST8310_IIC_ADDRESS <<1 , ist8310_init_matrix[i][0] , I2C_MEMADD_SIZE_8BIT , &ist8310_state[i] , 1 , 10);
            ist8310_delay_us(wait_time);
        }
    }
}



void ist8310_data_read(float ist8310_data[4])
{
    uint8_t ist8310_data_uint8[6];
    uint8_t ist8310_temp_uint8[2];
    int16_t temp_ist8310_data = 0;
    HAL_I2C_Mem_Read(&hi2c3 , IST8310_IIC_ADDRESS <<1 , IST8310_REG_PITCH_ADDRESS , I2C_MEMADD_SIZE_8BIT , ist8310_data_uint8 , 6 , 10);
    temp_ist8310_data = (int16_t)((ist8310_data_uint8[1] << 8) | ist8310_data_uint8[0]);
    ist8310_data[0] = MAG_SEN * temp_ist8310_data;
    temp_ist8310_data = (int16_t)((ist8310_data_uint8[3] << 8) | ist8310_data_uint8[2]);
    ist8310_data[1] = MAG_SEN * temp_ist8310_data;
    temp_ist8310_data = (int16_t)((ist8310_data_uint8[5] << 8) | ist8310_data_uint8[4]);
    ist8310_data[2] = MAG_SEN * temp_ist8310_data;
    HAL_I2C_Mem_Read(&hi2c3 , IST8310_IIC_ADDRESS <<1 , IST8310_REG_TEMP_ADDRESS , I2C_MEMADD_SIZE_8BIT , ist8310_temp_uint8 , 2 , 10);
    temp_ist8310_data = (int16_t)((ist8310_temp_uint8[1] << 8) | ist8310_temp_uint8[0]);
    ist8310_data[3] = temp_ist8310_data;
    osDelay(1);
}



void ist8310_delay_us(uint16_t us)
{
    uint32_t ticks = 0;
    uint32_t told = 0, tnow = 0, tcnt = 0;
    uint32_t reload = 0;
    reload = SysTick->LOAD;
    ticks = us * 72;
    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}