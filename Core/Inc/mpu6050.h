/**
 * @file    mpu6050.h
 * @brief   MPU-6050 accelerometer / gyroscope driver.
 *          Communicates via I2C1 (PB6/PB7).
 *          Uses raw register reads (no DMP firmware).
 */

#ifndef __MPU6050_H
#define __MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"
#include "app_config.h"

/* ========================================================================== *
 *  Status
 * ========================================================================== */
typedef enum {
    MPU6050_OK          = 0,
    MPU6050_ERR_I2C     = 1,
    MPU6050_ERR_WHOAMI  = 2,
} MPU6050_Status_t;

/* ========================================================================== *
 *  Raw sensor data
 * ========================================================================== */
typedef struct {
    int16_t accel_raw[3];   /**< Raw ADC values: [x, y, z]                  */
    int16_t gyro_raw[3];    /**< Raw ADC values: [x, y, z]                  */
    int16_t temp_raw;       /**< Raw temperature ADC value                  */
} MPU6050_RawData_t;

/* ========================================================================== *
 *  Scaled (SI) data
 * ========================================================================== */
typedef struct {
    float accel_g[3];       /**< Acceleration in g  [x, y, z]               */
    float gyro_dps[3];      /**< Angular velocity in deg/s [x, y, z]        */
    float temp_c;           /**< Temperature in °C                          */
    float accel_magnitude;  /**< √(ax²+ay²+az²) — useful for step detect    */
} MPU6050_Data_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise MPU-6050: wake up, configure accel/gyro FSR.
 * @return MPU6050_OK on success; MPU6050_ERR_WHOAMI if chip not found.
 */
MPU6050_Status_t MPU6050_Init(void);

/**
 * @brief  Read raw register values.
 */
MPU6050_Status_t MPU6050_ReadRaw(MPU6050_RawData_t *out);

/**
 * @brief  Read and convert to SI units.
 */
MPU6050_Status_t MPU6050_Read(MPU6050_Data_t *out);

/**
 * @brief  Perform accelerometer zero-offset calibration (device must be still).
 *         Takes ~100 samples and stores offsets in static variables.
 */
MPU6050_Status_t MPU6050_Calibrate(void);

/**
 * @brief  Set accelerometer full-scale range.
 * @param  fsr  2, 4, 8, or 16 (g).
 */
MPU6050_Status_t MPU6050_SetAccelFSR(uint8_t fsr);

/**
 * @brief  Set gyroscope full-scale range.
 * @param  fsr  250, 500, 1000, or 2000 (deg/s).
 */
MPU6050_Status_t MPU6050_SetGyroFSR(uint16_t fsr);

/**
 * @brief  Put MPU-6050 into low-power cycle mode.
 *         Wakeup frequency controlled by LP_WAKE_CTRL.
 */
MPU6050_Status_t MPU6050_SetLowPower(bool enable);

/**
 * @brief  Check if the sensor is alive on the I2C bus.
 */
bool MPU6050_IsConnected(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H */
