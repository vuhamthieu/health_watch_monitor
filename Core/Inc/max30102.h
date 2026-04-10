/**
 * @file    max30102.h
 * @brief   MAX30102 pulse oximeter / heart-rate sensor driver.
 *          Communicates via I2C1 (PB6/PB7), address 0x57.
 *          Reads Red + IR LED data from the 32-sample FIFO.
 */

#ifndef __MAX30102_H
#define __MAX30102_H

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
    MAX30102_OK         = 0,
    MAX30102_ERR_I2C    = 1,
    MAX30102_ERR_PARTID = 2,   /**< Wrong part ID — wrong chip / not found  */
} MAX30102_Status_t;

/* ========================================================================== *
 *  Raw FIFO sample
 * ========================================================================== */
typedef struct {
    uint32_t red;   /**< 18-bit Red LED ADC value                           */
    uint32_t ir;    /**< 18-bit IR LED ADC value                            */
} MAX30102_Sample_t;

/* ========================================================================== *
 *  Configuration
 * ========================================================================== */
typedef enum {
    MAX30102_MODE_HR    = 0x02,  /**< Heart-rate mode (IR LED only)         */
    MAX30102_MODE_SPO2  = 0x03,  /**< SpO2 mode (Red + IR)                  */
    MAX30102_MODE_MULTI = 0x07,  /**< Multi-LED mode                        */
} MAX30102_Mode_t;

typedef enum {
    MAX30102_SR_50   = 0x00,
    MAX30102_SR_100  = 0x01,
    MAX30102_SR_200  = 0x02,
    MAX30102_SR_400  = 0x03,
    MAX30102_SR_800  = 0x04,
    MAX30102_SR_1000 = 0x05,
} MAX30102_SampleRate_t;

typedef enum {
    MAX30102_PW_69   = 0x00,  /**< 69 µs → 15-bit ADC                      */
    MAX30102_PW_118  = 0x01,  /**< 118 µs → 16-bit                         */
    MAX30102_PW_215  = 0x02,  /**< 215 µs → 17-bit                         */
    MAX30102_PW_411  = 0x03,  /**< 411 µs → 18-bit                         */
} MAX30102_PulseWidth_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Reset and initialise the MAX30102 for SpO2 + HR mode.
 * @return MAX30102_OK on success.
 */
MAX30102_Status_t MAX30102_Init(void);

/**
 * @brief  Software reset the sensor (clears FIFO and registers).
 */
MAX30102_Status_t MAX30102_Reset(void);

/**
 * @brief  Check if the sensor is present on I2C bus (Part ID check).
 */
bool MAX30102_IsConnected(void);

/**
 * @brief  Check if a finger is placed on the sensor.
 *         Based on IR value ≥ MAX30102_IR_MIN_VALID (app_config.h).
 */
bool MAX30102_FingerDetected(void);

/**
 * @brief  Read all available samples from the FIFO (up to MAX30102_FIFO_DEPTH).
 * @param  buf      Output buffer for samples.
 * @param  max_count Maximum samples to read (size of buf).
 * @param  read_count Actual number of samples read.
 * @return MAX30102_OK on success.
 */
MAX30102_Status_t MAX30102_ReadFIFO(MAX30102_Sample_t *buf,
                                    uint8_t max_count,
                                    uint8_t *read_count);

/**
 * @brief  Flush the FIFO (reset FIFO pointers without reading).
 */
MAX30102_Status_t MAX30102_FlushFIFO(void);

/**
 * @brief  Set LED pulse amplitude.
 * @param  red_pa   Red LED current (0x00–0xFF, ~0 to 51 mA).
 * @param  ir_pa    IR LED current.
 */
MAX30102_Status_t MAX30102_SetLEDCurrent(uint8_t red_pa, uint8_t ir_pa);
void              MAX30102_DumpRegs(void);  /**< Print key registers via printf (debug) */

/**
 * @brief  Enter/exit low-power shutdown mode (LEDs off, I2C stays alive).
 */
MAX30102_Status_t MAX30102_SetShutdown(bool shutdown);

/**
 * @brief  Set operating mode.
 */
MAX30102_Status_t MAX30102_SetMode(MAX30102_Mode_t mode);

/**
 * @brief  Set sample rate and pulse width.
 */
MAX30102_Status_t MAX30102_SetSampleRate(MAX30102_SampleRate_t sr,
                                         MAX30102_PulseWidth_t pw);

#ifdef __cplusplus
}
#endif

#endif /* __MAX30102_H */
