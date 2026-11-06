#include "BMI088.h"
#include <math.h>
#include <string.h>

extern SPI_HandleTypeDef hspi1;

#ifndef DEG2RAD
#define DEG2RAD 0.017453292519943295f
#endif

float acc_res = 0;
float gyr_res = 0;

#define BSP_SPI_Accel_CS(state) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (state) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define BSP_SPI_Gyro_CS(state)  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (state) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define ACCEL_CS(state) BSP_SPI_Accel_CS(state)
#define GYRO_CS(state)  BSP_SPI_Gyro_CS(state)

static void BSP_SPI_Transmit(uint8_t *tx, uint16_t len, uint16_t timeout)
{
    HAL_SPI_Transmit(&hspi1, tx, len, timeout);
}

static void BSP_SPI_Receive(uint8_t *rx, uint16_t len, uint16_t timeout)
{
    HAL_SPI_Receive(&hspi1, rx, len, timeout);
}

static void DWT_Delay_ms(uint32_t ms)
{
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = ms * 168000UL; /* 168MHz 涓婚 */
    while ((DWT->CYCCNT - start) < ticks) {}
}

static void BMI088_CS_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CS 榛樿鎷夐珮锛堢墖閫夐噴鏀撅級 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

static void Accel_WriteReg(uint8_t reg, uint8_t val) {
    uint8_t cmd[2] = {reg & 0x7F, val};
    BSP_SPI_Accel_CS(0);
    BSP_SPI_Transmit(cmd, 2, 10);
    BSP_SPI_Accel_CS(1);
}

/* internal: read Accel register */
static uint8_t Accel_ReadReg(uint8_t reg) {
    uint8_t addr = reg | 0x80;
    uint8_t val[2] = {0};
    BSP_SPI_Accel_CS(0);
    BSP_SPI_Transmit(&addr, 1, 10);
    BSP_SPI_Receive(val, 2, 10);
    BSP_SPI_Accel_CS(1);
    return val[1];
}

/* internal: write Gyro register */
static void Gyro_WriteReg(uint8_t reg, uint8_t val) {
    uint8_t cmd[2] = {reg & 0x7F, val};
    BSP_SPI_Gyro_CS(0);
    BSP_SPI_Transmit(cmd, 2, 10);
    BSP_SPI_Gyro_CS(1);
}

/* internal: read Gyro register */
static uint8_t Gyro_ReadReg(uint8_t reg) {
    uint8_t addr = reg | 0x80, val = 0;
    BSP_SPI_Gyro_CS(0);
    BSP_SPI_Transmit(&addr, 1, 10);
    BSP_SPI_Receive(&val, 1, 10);
    BSP_SPI_Gyro_CS(1);
    return val;
}

uint8_t BMI088_Init(void) {
    uint8_t dummy = 0;
    BMI088_CS_GPIO_Init();

    ACCEL_CS(0);
    uint8_t dummy_addr = REG_ACC_CHIP_ID | 0x80;
    BSP_SPI_Transmit(&dummy_addr, 1, 10);
    BSP_SPI_Receive(&dummy, 1, 10); // 瑙﹀彂 CSB1 涓婂崌娌?
    ACCEL_CS(1);
    DWT_Delay_ms(5);
    // 2. 妫€鏌?Accel 鑺墖 ID
    if (Accel_ReadReg(REG_ACC_CHIP_ID) != BMI088_ACC_CHIP_ID_VAL) {
        return 1;
    }
    // 3. Accel 杞浣?
    Accel_WriteReg(REG_ACC_SOFTRESET, BMI088_SOFTRESET_VAL);
    DWT_Delay_ms(50);

    // 鍏堝啓 ACC_PWR_CONF(0x7C) = 0x00锛屼粠鎸傝捣鍒囧埌娲诲姩
    Accel_WriteReg(REG_ACC_PWR_CONF, 0x00);
    DWT_Delay_ms(10);
    // 鍐嶅啓 ACC_PWR_CTRL(0x7D) = 0x04锛屽紑鍚姞閫熷害璁′富鐢垫簮
    Accel_WriteReg(REG_ACC_PWR_CTRL, 0x04);
    DWT_Delay_ms(50);
    // ================= Gyro 鍚姩娴?=================
    // 5. 妫€鏌?Gyro 鑺墖 ID
    if (Gyro_ReadReg(REG_GYRO_CHIP_ID) != BMI088_GYRO_CHIP_ID_VAL) {
        return 2;
    }
    // 6. Gyro 杞浣?
    Gyro_WriteReg(REG_GYRO_SOFTRESET, BMI088_SOFTRESET_VAL);
    DWT_Delay_ms(50);
    // 7. 鍞ら啋 Gyro (鍐?0x00 杩涘叆 Normal Mode)
    Gyro_WriteReg(REG_GYRO_LPM1, 0x00);
    DWT_Delay_ms(30);  // Gyro 鍞ら啋闇€瑕?30ms 绋冲畾
    // 8. 閰嶇疆閲忕▼涓庨噰鏍风巼
    BMI088_SetFormat(ACC_ODR_1600Hz, ACCEL_FS_6G, GYR_ODR_1000Hz_BW_116Hz, GYRO_FS_2000DPS);

    // ================= 涓柇閰嶇疆 =================
    // Accel INT1: 鎺ㄦ尳杈撳嚭(0<<1), 浣庣數骞虫湁鏁?0<<2), 寮€鍚腑鏂?1<<3)
    Accel_WriteReg(REG_ACC_INT1_IO_CTRL, 0x08);
    Accel_WriteReg(REG_ACC_INT_MAP_DATA, (1 << 2)); // 鏄犲皠 DRDY 鍒?INT1
    // Gyro INT3: 鎺ㄦ尳杈撳嚭, 浣庣數骞虫湁鏁?-> 瀵瑰簲 0x00
    Gyro_WriteReg(REG_GYRO_INT3_INT4_IO_CONF, 0x00);
    Gyro_WriteReg(REG_GYRO_CTRL, 0x80);             // 寮€鍚?Gyro 鐨?DRDY
    Gyro_WriteReg(REG_GYRO_INT3_INT4_IO_MAP, 0x01); // 鏄犲皠 DRDY 鍒?INT3

    return 0;
}

void BMI088_SetFormat(AccelODR_t a_odr, AccelFS_t a_fsr, GyroODR_BW_t g_odr_bw, GyroFS_t g_fsr) {
    // Accel Config: Must set BIT 7 to 1
    Accel_WriteReg(REG_ACC_CONF, 0x80 | (0x02 << 4) | a_odr);
    Accel_WriteReg(REG_ACC_RANGE, a_fsr);

    // Gyro Config: Must set BIT 7 to 1
    Gyro_WriteReg(REG_GYRO_BANDWIDTH, 0x80 | g_odr_bw);
    Gyro_WriteReg(REG_GYRO_RANGE, g_fsr);

    // Calculate Multipliers
    acc_res = (3.0f * (float)(1 << a_fsr)) / 32768.0f * 9.81f; // 3G, 6G, 12G, 24G -> m/s虏
    gyr_res = (2000.0f / (float)(1 << g_fsr)) / 32768.0f * DEG2RAD; // 2000, 1000, 500... -> rad/s
}

uint8_t BMI088_IsDataReady(void) {
     // Accel DRDY check
     return (Accel_ReadReg(REG_ACC_STATUS) & 0x80);
}

void BMI088_read(float gyro[3], float accel[3], float *temperature) {
    uint8_t a_buf[7], g_buf[6], t_buf[3];
    int16_t raw_val;

    // Read Accel (Requires 1 dummy byte)
    uint8_t a_addr = REG_ACC_XOUT_L | 0x80;
    ACCEL_CS(0);
    BSP_SPI_Transmit(&a_addr, 1, 10);
    BSP_SPI_Receive(a_buf, 7, 10); // a_buf[0] is dummy
    ACCEL_CS(1);

    // Read Gyro
    uint8_t g_addr = REG_GYRO_X_L | 0x80;
    GYRO_CS(0);
    BSP_SPI_Transmit(&g_addr, 1, 10);
    BSP_SPI_Receive(g_buf, 6, 10);
    GYRO_CS(1);

    // Read Temp
    uint8_t t_addr = REG_TEMP_M | 0x80;
    ACCEL_CS(0);
    BSP_SPI_Transmit(&t_addr, 1, 10);
    BSP_SPI_Receive(t_buf, 3, 10); // t_buf[0] is dummy
    ACCEL_CS(1);

    accel[0] = (int16_t)((a_buf[2] << 8) | a_buf[1]) * acc_res;
    accel[1] = (int16_t)((a_buf[4] << 8) | a_buf[3]) * acc_res;
    accel[2] = (int16_t)((a_buf[6] << 8) | a_buf[5]) * acc_res;

    gyro[0] = (int16_t)((g_buf[1] << 8) | g_buf[0]) * gyr_res;
    gyro[1] = (int16_t)((g_buf[3] << 8) | g_buf[2]) * gyr_res;
    gyro[2] = (int16_t)((g_buf[5] << 8) | g_buf[4]) * gyr_res;

    raw_val = (int16_t)((t_buf[1] << 3) | (t_buf[2] >> 5));
    if (raw_val > 1023) raw_val -= 2048;
    *temperature = (raw_val * 0.125f) + 23.0f;
}

void BMI088_Read_Fast(float gyro[3], float accel[3], float *temperature) {
    uint8_t a_buf[7] = {0};
    uint8_t g_buf[6] = {0};
    uint8_t t_buf[3] = {0};
    uint8_t addr;

    // Read Accel: BMI088 accel SPI has one dummy byte before the 6 data bytes.
    addr = REG_ACC_XOUT_L | 0x80;
    ACCEL_CS(0);
    BSP_SPI_Transmit(&addr, 1, 10);
    BSP_SPI_Receive(a_buf, 7, 10);
    ACCEL_CS(1);

    // Read Gyro: gyro SPI returns data immediately after the address phase.
    addr = REG_GYRO_X_L | 0x80;
    GYRO_CS(0);
    BSP_SPI_Transmit(&addr, 1, 10);
    BSP_SPI_Receive(g_buf, 6, 10);
    GYRO_CS(1);

    // Read Temp separately. TEMP_M/TEMP_L are at 0x22/0x23, not after ACC_XOUT_Z.
    addr = REG_TEMP_M | 0x80;
    ACCEL_CS(0);
    BSP_SPI_Transmit(&addr, 1, 10);
    BSP_SPI_Receive(t_buf, 3, 10);
    ACCEL_CS(1);

    // Keep acc_res/gyr_res from BMI088_SetFormat(); do not overwrite them here.
    accel[0] = (int16_t)((a_buf[2] << 8) | a_buf[1]) * acc_res;
    accel[1] = (int16_t)((a_buf[4] << 8) | a_buf[3]) * acc_res;
    accel[2] = (int16_t)((a_buf[6] << 8) | a_buf[5]) * acc_res;

    gyro[0] = (int16_t)((g_buf[1] << 8) | g_buf[0]) * gyr_res;
    gyro[1] = (int16_t)((g_buf[3] << 8) | g_buf[2]) * gyr_res;
    gyro[2] = (int16_t)((g_buf[5] << 8) | g_buf[4]) * gyr_res;

    int16_t temp_raw_val = (int16_t)((t_buf[1] << 3) | (t_buf[2] >> 5));
    if (temp_raw_val > 1023) {
        temp_raw_val -= 2048;
    }

    *temperature = (temp_raw_val * 0.125f) + 23.0f;
}