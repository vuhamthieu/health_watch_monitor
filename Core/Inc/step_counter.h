/**
 * @file    step_counter.h
 * @brief   Pedometer algorithm using MPU-6050 accelerometer data.
 *
 * Algorithm overview:
 *   1. Compute acceleration magnitude: mag = √(ax² + ay² + az²)
 *   2. Apply simple low-pass filter to remove noise.
 *   3. Detect peaks above STEP_ACCEL_THRESHOLD with minimum interval
 *      STEP_MIN_INTERVAL_MS to avoid double-counting.
 *   4. Each valid peak = 1 step.
 *
 * Call StepCounter_Update() at MPU6050_SAMPLE_RATE_HZ (100 Hz).
 */

#ifndef __STEP_COUNTER_H
#define __STEP_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Reset the step counter and internal state.
 */
void StepCounter_Reset(void);

/**
 * @brief  Update step counter with a new accelerometer sample.
 *         Must be called at a fixed rate (100 Hz recommended).
 * @param  ax, ay, az  Acceleration components in g.
 * @return true if a new step was detected this sample.
 */
bool StepCounter_Update(float ax, float ay, float az);

/**
 * @brief  Get the total accumulated step count.
 */
uint32_t StepCounter_GetSteps(void);

/**
 * @brief  Get estimated distance in metres.
 */
float StepCounter_GetDistance(void);

/**
 * @brief  Get estimated calories burned.
 */
float StepCounter_GetCalories(void);

/**
 * @brief  Set stride length (metres, default = STEP_STRIDE_LENGTH_M).
 */
void StepCounter_SetStrideLength(float stride_m);

/**
 * @brief  Get current activity state based on recent acceleration patterns.
 *         Returns values compatible with ActivityState_t in sensor_data.h.
 */
int StepCounter_GetActivityState(void);

#ifdef __cplusplus
}
#endif

#endif /* __STEP_COUNTER_H */
