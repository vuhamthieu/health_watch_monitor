/**
 * @file    heart_rate.h
 * @brief   Heart rate calculation from MAX30102 IR LED signal.
 *
 * Algorithm overview:
 *   1. Accumulate IR samples from MAX30102 FIFO into a circular buffer.
 *   2. Apply DC removal (high-pass filter) to extract AC component.
 *   3. Apply low-pass smoothing to suppress high-frequency/motion noise.
 *   4. Detect peaks using adaptive (dynamic) threshold + refractory rules.
 *   5. Calculate inter-beat interval (IBI) in ms → BPM = 60000 / IBI.
 *   6. Apply moving-average smoothing over last beats.
 *   7. Reject readings outside HR_VALID_MIN – HR_VALID_MAX range.
 *
 * Feed raw IR samples via HR_AddSample().
 * Read current BPM via HR_GetBPM().
 */

#ifndef __HEART_RATE_H
#define __HEART_RATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ========================================================================== *
 *  Configuration
 * ========================================================================== */
#define HR_BUFFER_SIZE      100u    /**< Reserved legacy buffer depth          */
#define HR_MA_WINDOW        6u      /**< Moving average window (beats)         */
#define HR_DC_FILTER_ALPHA  0.01f   /**< DC estimator alpha for high-pass path */

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Reset the heart rate algorithm state.
 */
void HR_Reset(void);

/**
 * @brief  Add a new IR sample from the MAX30102 FIFO.
 *         Call at MAX30102_SAMPLE_RATE_HZ (100 Hz).
 * @param  ir_sample  Raw 18-bit IR ADC value.
 */
void HR_AddSample(uint32_t ir_sample);

/**
 * @brief  Get the most recently calculated heart rate.
 * @param  bpm_out  Output: heart rate in BPM.
 * @return true if the value is valid (finger detected, reading stable).
 *         false → caller should display "---".
 */
bool HR_GetBPM(uint16_t *bpm_out);

/**
 * @brief  Check if a finger is present based on recent IR signal level.
 */
bool     HR_FingerPresent(void);
uint32_t HR_GetLastIR(void);       /**< Last raw IR sample (debug)    */

/**
 * @brief  Check if heart rate is in alert zone (tachy or brady).
 * @param  is_tachy  Output: true if BPM > HR_TACHY_THRESHOLD.
 * @param  is_brady  Output: true if BPM < HR_BRADY_THRESHOLD.
 */
void HR_GetAlertStatus(bool *is_tachy, bool *is_brady);

#ifdef __cplusplus
}
#endif

#endif /* __HEART_RATE_H */
