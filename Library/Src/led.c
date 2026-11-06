#include "led.h"
#include "cmsis_os.h"

/**
  * @brief 点亮红色LED
  * @param none
  * @return none 
  */
void led_R_on()
{ 
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
}


/**
  * @brief 点亮绿色LED
  * @param none
  * @return none 
  */
void led_G_on()
{ 
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
}


/**
  * @brief 点亮蓝色LED
  * @param none
  * @return none 
  */
void led_B_on()
{ 
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
}


/**
  * @brief 熄灭红色LED
  * @param none
  * @return none 
  */
void led_R_off()
{ 
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
}


/**
  * @brief 熄灭绿色LED
  * @param none
  * @return none 
  */
void led_G_off()
{ 
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
}


/**
  * @brief 熄灭蓝色LED
  * @param none
  * @return none 
  */
void led_B_off()
{ 
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
}


/**
 * @brief 闪烁红色LED
 * @param time_on     单周期点亮时间
 * @param time_off    单周期熄灭时间
 * @param time        循环次数
 * @return none 
 */
void led_R_flicker(int16_t time_on , int16_t time_off , int16_t time)
{ 
  static int i;
  for(i = 0 ; i < time ; i ++)
  {
      HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
      osDelay(time_on);
      HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
      osDelay(time_off);
  }  
}


/**
 * @brief 闪烁绿色LED
 * @param time_on     单周期点亮时间
 * @param time_off    单周期熄灭时间
 * @param time        循环次数
 * @return none 
 */
void led_G_flicker(int16_t time_on , int16_t time_off , int16_t time)
{ 
  static int i;
  for(i = 0 ; i < time ; i ++)
  {
      HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
      osDelay(time_on);
      HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
      osDelay(time_off);
  }  
}


/**
 * @brief 闪烁蓝色LED
 * @param time_on     单周期点亮时间
 * @param time_off    单周期熄灭时间
 * @param time        循环次数
 * @return none 
 */
void led_B_flicker(int16_t time_on , int16_t time_off , int16_t time)
{ 
  static int i;
  for(i = 0 ; i < time ; i ++)
  {
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
      osDelay(time_on);
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
      osDelay(time_off);
  }  
}
