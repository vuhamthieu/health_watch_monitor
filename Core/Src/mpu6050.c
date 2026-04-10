/**
 * @file    mpu6050.c
 * @brief   MPU-6050 accelerometer / gyroscope driver implementation.
 *
 * TODO (Phase 2 — Hardware Bring-up):
 *  - Verify I2C communication: read WHO_AM_I register (should return 0x68)
 *  - Configure SMPLRT_DIV for 100 Hz: SMPLRT_DIV = (1000 / 100) - 1 = 9
 *  - Run MPU6050_Calibrate() once at startup (device must be flat and still)
 *
 * TODO (Phase 3 — Driver):
 *  - Implement low-power cycling for sleep mode
 *  - Add gyro integration for orientation (optional)
 */

#include "mpu6050.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>

/* ========================================================================== *
 *  MPU-6050 Register Map (subset)
 * ========================================================================== */
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B  /* 6 bytes: AX_H, AX_L, AY_H, AY_L, AZ_H, AZ_L */
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_GYRO_XOUT_H     0x43  /* 6 bytes */
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_WHO_AM_I        0x75

#define MPU6050_WHO_AM_I_VALUE      0x68

/* ========================================================================== *
 *  Sensitivity scales
 * ========================================================================== */
static float s_accel_scale = 1.0f / 16384.0f; /* ±2g  → LSB/g = 16384 */
static float s_gyro_scale  = 1.0f / 131.0f;   /* ±250°/s → LSB/dps = 131 */

/* ========================================================================== *
 *  Calibration offsets
 * ========================================================================== */
static float s_accel_offset[3] = { 0.0f, 0.0f, 0.0f };

/* Runtime-detected I2C address (auto-probes 0x68 and 0x69) */
static uint16_t s_i2c_addr = MPU6050_I2C_ADDR;
static uint8_t  s_who_am_i = 0;

/* ========================================================================== *
 *  Internal helpers
 * ========================================================================== */
static HAL_StatusTypeDef reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(
        &APP_I2C_HANDLE, s_i2c_addr, buf, 2, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef reg_read(uint8_t reg, uint8_t *out, uint8_t len)
{
    return HAL_I2C_Mem_Read(
        &APP_I2C_HANDLE, s_i2c_addr, reg,
        I2C_MEMADD_SIZE_8BIT, out, len, I2C_TIMEOUT_MS);
}

/* ========================================================================== *
 *  Init
 * ========================================================================== */
MPU6050_Status_t MPU6050_Init(void)
{
    uint8_t who_am_i = 0;

    /* Auto-probe: try 0x68 first, fall back to 0x69 (AD0 tied HIGH) */
    s_i2c_addr = MPU6050_I2C_ADDR;           /* 0x68 << 1 */
    if (HAL_I2C_IsDeviceReady(&APP_I2C_HANDLE, s_i2c_addr, 2, 10) != HAL_OK) {
        s_i2c_addr = (0x69u << 1);            /* try AD0=HIGH variant */
        if (HAL_I2C_IsDeviceReady(&APP_I2C_HANDLE, s_i2c_addr, 2, 10) != HAL_OK) {
            return MPU6050_ERR_I2C;           /* not found at either address */
        }
    }

    /* WHO_AM_I check — accept clones (0x72, 0x98, 0x19…) that ACK but
     * return a non-0x68 ID. Only reject 0x00/0xFF which mean bus stuck. */
    if (reg_read(MPU6050_REG_WHO_AM_I, &who_am_i, 1) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    if (who_am_i == 0x00 || who_am_i == 0xFF) {
        return MPU6050_ERR_WHOAMI;
    }
    s_who_am_i = who_am_i;

    /* Wake up (clear SLEEP bit in PWR_MGMT_1) */
    reg_write(MPU6050_REG_PWR_MGMT_1, 0x00);
    osDelay(100); /* Allow sensor to stabilise */

    /* Sample rate: 100 Hz → SMPLRT_DIV = 9  (1000 / (1 + 9) = 100 Hz) */
    reg_write(MPU6050_REG_SMPLRT_DIV, 9);

    /* DLPF = 44 Hz bandwidth (CONFIG register bits[2:0] = 3) */
    reg_write(MPU6050_REG_CONFIG, 0x03);

    /* Set FSR */
    MPU6050_SetAccelFSR(MPU6050_ACCEL_FSR);
    MPU6050_SetGyroFSR(MPU6050_GYRO_FSR);

    return MPU6050_OK;
}

/* ========================================================================== *
 *  Raw read
 * ========================================================================== */
MPU6050_Status_t MPU6050_ReadRaw(MPU6050_RawData_t *out)
{
    uint8_t buf[14]; /* ACCEL(6) + TEMP(2) + GYRO(6) */
    if (reg_read(MPU6050_REG_ACCEL_XOUT_H, buf, 14) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }

    out->accel_raw[0] = (int16_t)((buf[0]  << 8) | buf[1]);
    out->accel_raw[1] = (int16_t)((buf[2]  << 8) | buf[3]);
    out->accel_raw[2] = (int16_t)((buf[4]  << 8) | buf[5]);
    out->temp_raw     = (int16_t)((buf[6]  << 8) | buf[7]);
    out->gyro_raw[0]  = (int16_t)((buf[8]  << 8) | buf[9]);
    out->gyro_raw[1]  = (int16_t)((buf[10] << 8) | buf[11]);
    out->gyro_raw[2]  = (int16_t)((buf[12] << 8) | buf[13]);

    return MPU6050_OK;
}

/* ========================================================================== *
 *  Scaled read
 * ========================================================================== */
MPU6050_Status_t MPU6050_Read(MPU6050_Data_t *out)
{
    MPU6050_RawData_t raw;
    if (MPU6050_ReadRaw(&raw) != MPU6050_OK) return MPU6050_ERR_I2C;

    out->accel_g[0] = raw.accel_raw[0] * s_accel_scale - s_accel_offset[0];
    out->accel_g[1] = raw.accel_raw[1] * s_accel_scale - s_accel_offset[1];
    out->accel_g[2] = raw.accel_raw[2] * s_accel_scale - s_accel_offset[2];

    out->gyro_dps[0] = raw.gyro_raw[0] * s_gyro_scale;
    out->gyro_dps[1] = raw.gyro_raw[1] * s_gyro_scale;
    out->gyro_dps[2] = raw.gyro_raw[2] * s_gyro_scale;

    out->temp_c = (float)raw.temp_raw / 340.0f + 36.53f;

    out->accel_magnitude = sqrtf(
        out->accel_g[0] * out->accel_g[0] +
        out->accel_g[1] * out->accel_g[1] +
        out->accel_g[2] * out->accel_g[2]);

    return MPU6050_OK;
}

/* ========================================================================== *
 *  Calibration
 * ========================================================================== */
MPU6050_Status_t MPU6050_Calibrate(void)
{
    float sum[3] = { 0 };
    const uint8_t samples = 100;

    for (uint8_t i = 0; i < samples; i++) {
        MPU6050_Data_t d;
        if (MPU6050_Read(&d) != MPU6050_OK) return MPU6050_ERR_I2C;
        sum[0] += d.accel_g[0];
        sum[1] += d.accel_g[1];
        sum[2] += d.accel_g[2] - 1.0f; /* Remove 1g gravity on Z axis */
        osDelay(10);
    }

    s_accel_offset[0] = sum[0] / samples;
    s_accel_offset[1] = sum[1] / samples;
    s_accel_offset[2] = sum[2] / samples;

    return MPU6050_OK;
}

/* ========================================================================== *
 *  FSR configuration
 * ========================================================================== */
MPU6050_Status_t MPU6050_SetAccelFSR(uint8_t fsr)
{
    uint8_t bits;
    switch (fsr) {
        case 2:  bits = 0x00; s_accel_scale = 1.0f / 16384.0f; break;
        case 4:  bits = 0x08; s_accel_scale = 1.0f / 8192.0f;  break;
        case 8:  bits = 0x10; s_accel_scale = 1.0f / 4096.0f;  break;
        case 16: bits = 0x18; s_accel_scale = 1.0f / 2048.0f;  break;
        default: return MPU6050_ERR_I2C;
    }
    return (reg_write(MPU6050_REG_ACCEL_CONFIG, bits) == HAL_OK)
           ? MPU6050_OK : MPU6050_ERR_I2C;
}

MPU6050_Status_t MPU6050_SetGyroFSR(uint16_t fsr)
{
    uint8_t bits;
    switch (fsr) {
        case 250:  bits = 0x00; s_gyro_scale = 1.0f / 131.0f;  break;
        case 500:  bits = 0x08; s_gyro_scale = 1.0f / 65.5f;   break;
        case 1000: bits = 0x10; s_gyro_scale = 1.0f / 32.8f;   break;
        case 2000: bits = 0x18; s_gyro_scale = 1.0f / 16.4f;   break;
        default: return MPU6050_ERR_I2C;
    }
    return (reg_write(MPU6050_REG_GYRO_CONFIG, bits) == HAL_OK)
           ? MPU6050_OK : MPU6050_ERR_I2C;
}

/* ========================================================================== *
 *  Low-power mode
 * ========================================================================== */
MPU6050_Status_t MPU6050_SetLowPower(bool enable)
{
    /* SLEEP bit in PWR_MGMT_1 */
    uint8_t val = enable ? 0x40 : 0x00;
    return (reg_write(MPU6050_REG_PWR_MGMT_1, val) == HAL_OK)
           ? MPU6050_OK : MPU6050_ERR_I2C;
}

bool MPU6050_IsConnected(void)
{
    uint8_t who_am_i = 0;
    if (reg_read(MPU6050_REG_WHO_AM_I, &who_am_i, 1) != HAL_OK) return false;
    return (who_am_i == MPU6050_WHO_AM_I_VALUE);
}

uint8_t MPU6050_GetAddr(void)
{
    return (uint8_t)(s_i2c_addr >> 1);
}

uint8_t MPU6050_GetWhoAmI(void)
{
    return s_who_am_i;
}
