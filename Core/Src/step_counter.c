/* step_counter.c - step counting: LPF + hysteresis peak detection
 * TODO: tune STEP_ACCEL_THRESHOLD after testing on real hardware */

#include "step_counter.h"
#include "app_config.h"
#include "sensor_data.h"
#include "cmsis_os.h"
#include <math.h>
#include <string.h>

/* state */
static uint32_t s_steps         = 0;
static float    s_stride_m      = STEP_STRIDE_LENGTH_M;
static float    s_prev_mag      = 0.0f;
static float    s_filtered_mag  = 0.0f;
static uint32_t s_last_step_tick = 0;
static bool     s_above_thresh  = false;

/* Activity detection: keep last N magnitude values */
#define ACTIVITY_WINDOW  10
static float    s_mag_history[ACTIVITY_WINDOW];
static uint8_t  s_mag_idx = 0;
static float    s_mag_variance = 0.0f;

/* Low-pass filter alpha (0.0 = very smooth, 1.0 = no filter) */
#define LPF_ALPHA  0.3f

void StepCounter_Reset(void)
{
    s_steps         = 0;
    s_prev_mag      = 0.0f;
    s_filtered_mag  = 0.0f;
    s_last_step_tick = 0;
    s_above_thresh  = false;
    memset(s_mag_history, 0, sizeof(s_mag_history));
    s_mag_idx       = 0;
    s_mag_variance  = 0.0f;
}

/* call at 100 Hz from sensorTask */
bool StepCounter_Update(float ax, float ay, float az)
{
    bool new_step = false;

    /* Compute magnitude */
    float mag = sqrtf(ax * ax + ay * ay + az * az);

    /* Low-pass filter */
    s_filtered_mag = LPF_ALPHA * mag + (1.0f - LPF_ALPHA) * s_filtered_mag;

    /* Update activity window */
    s_mag_history[s_mag_idx] = mag;
    s_mag_idx = (s_mag_idx + 1) % ACTIVITY_WINDOW;

    /* Compute variance for activity classification */
    float mean = 0.0f;
    for (uint8_t i = 0; i < ACTIVITY_WINDOW; i++) mean += s_mag_history[i];
    mean /= ACTIVITY_WINDOW;
    float var = 0.0f;
    for (uint8_t i = 0; i < ACTIVITY_WINDOW; i++) {
        float diff = s_mag_history[i] - mean;
        var += diff * diff;
    }
    s_mag_variance = var / ACTIVITY_WINDOW;

    /* Peak detection with hysteresis */
    uint32_t now = osKernelSysTick();

    if (!s_above_thresh && s_filtered_mag > STEP_ACCEL_THRESHOLD) {
        s_above_thresh = true;
    } else if (s_above_thresh && s_filtered_mag < (STEP_ACCEL_THRESHOLD * 0.8f)) {
        /* Falling edge of peak — count step if minimum interval passed */
        if ((now - s_last_step_tick) >= STEP_MIN_INTERVAL_MS) {
            s_steps++;
            s_last_step_tick = now;
            new_step = true;
        }
        s_above_thresh = false;
    }

    s_prev_mag = mag;
    return new_step;
}

uint32_t StepCounter_GetSteps(void)
{
    return s_steps;
}

float StepCounter_GetDistance(void)
{
    return (float)s_steps * s_stride_m;
}

float StepCounter_GetCalories(void)
{
    return (float)s_steps * STEP_CALORIE_PER_STEP;
}

void StepCounter_SetStrideLength(float stride_m)
{
    s_stride_m = stride_m;
}

int StepCounter_GetActivityState(void)
{
    /* Simple classification based on magnitude variance:
     *   variance < 0.01  → STATIONARY
     *   0.01 – 0.5       → WALKING
     *   > 0.5            → RUNNING
     */
    if (s_mag_variance < 0.01f) return (int)ACTIVITY_STATIONARY;
    if (s_mag_variance < 0.50f) return (int)ACTIVITY_WALKING;
    return (int)ACTIVITY_RUNNING;
}
