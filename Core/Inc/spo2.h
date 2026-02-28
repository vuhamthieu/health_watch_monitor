/**
 * @file    spo2.h
 * @brief   SpO2 (blood oxygen saturation) calculation from MAX30102 Red+IR signals.
 *
 * Algorithm overview:
 *   SpO2 = 110 - 25 × R
 *   where R = (AC_red / DC_red) / (AC_ir / DC_ir)
 *
 *   AC component = RMS of the AC-coupled signal.
 *   DC component = moving average (low-pass).
 *
 *   The formula above is a first-order linear approximation.
 *   For higher accuracy, use a calibration table (R → SpO2 LUT).
 *
 * Feed paired (Red, IR) samples via SpO2_AddSample().
 * Read result via SpO2_GetValue().
 */

#ifndef __SPO2_H
#define __SPO2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ========================================================================== *
 *  Configuration
 * ========================================================================== */
#define SPO2_BUFFER_SIZE    50u     /**< Samples per SpO2 calculation window */
#define SPO2_DC_ALPHA       0.95f   /**< DC tracking low-pass alpha          */

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Reset the SpO2 algorithm state and buffers.
 */
void SpO2_Reset(void);

/**
 * @brief  Add a new paired (Red, IR) sample.
 *         Call at MAX30102_SAMPLE_RATE_HZ (100 Hz).
 * @param  red  18-bit raw Red LED ADC value.
 * @param  ir   18-bit raw IR LED ADC value.
 */
void SpO2_AddSample(uint32_t red, uint32_t ir);

/**
 * @brief  Get the most recently calculated SpO2 value.
 * @param  spo2_out  Output: SpO2 in % (0–100).
 * @return true if value is valid; false → display "---".
 */
bool SpO2_GetValue(uint8_t *spo2_out);

/**
 * @brief  Check if SpO2 is below the alert threshold (SPO2_LOW_THRESHOLD).
 */
bool SpO2_IsLowAlert(void);

/**
 * @brief  Check if a finger is present based on IR and Red signal levels.
 */
bool SpO2_FingerPresent(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPO2_H */
