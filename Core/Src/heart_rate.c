/* heart_rate.c - BPM from IR signal: DC removal high-pass + adaptive peak detect
 * TODO: tune HR_DC_FILTER_ALPHA with real MAX30102 data */

#include "heart_rate.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* state */
static uint32_t s_ir_buf[HR_BUFFER_SIZE];
static uint16_t s_buf_head  = 0;
static uint32_t s_sample_count = 0;

/* DC removal */
static float    s_dc_filter  = 0.0f;
static float    s_prev_dc    = 0.0f;

/* Peak detection */
static float    s_threshold  = 0.0f;
static float    s_prev_ac    = 0.0f;
static bool     s_peak_found = false;
static uint32_t s_last_peak_tick = 0;

/* BPM moving average */
static uint16_t s_bpm_buf[HR_MA_WINDOW];
static uint8_t  s_bpm_idx    = 0;
static uint8_t  s_bpm_count  = 0;
static uint16_t s_bpm_output = 0;

static bool     s_finger_present = false;

void HR_Reset(void)
{
    memset(s_ir_buf,  0, sizeof(s_ir_buf));
    memset(s_bpm_buf, 0, sizeof(s_bpm_buf));
    s_buf_head       = 0;
    s_sample_count   = 0;
    s_dc_filter      = 0.0f;
    s_prev_dc        = 0.0f;
    s_threshold      = 0.0f;
    s_prev_ac        = 0.0f;
    s_peak_found     = false;
    s_last_peak_tick = 0;
    s_bpm_idx        = 0;
    s_bpm_count      = 0;
    s_bpm_output     = 0;
    s_finger_present = false;
}

void HR_AddSample(uint32_t ir_sample)
{
    s_ir_buf[s_buf_head] = ir_sample;
    s_buf_head = (s_buf_head + 1) % HR_BUFFER_SIZE;
    s_sample_count++;

    /* Finger detection */
    s_finger_present = (ir_sample >= MAX30102_IR_MIN_VALID);
    if (!s_finger_present) return;

    /* DC removal (high-pass): y[n] = α × (y[n-1] + x[n] - x[n-1]) */
    float x  = (float)ir_sample;
    float dc = HR_DC_FILTER_ALPHA * (s_prev_dc + x - (float)
               s_ir_buf[(s_buf_head + HR_BUFFER_SIZE - 2) % HR_BUFFER_SIZE]);
    float ac = x - dc;
    s_prev_dc = dc;

    /* Adaptive threshold: 60% of recent max AC */
    if (fabsf(ac) > s_threshold) s_threshold = fabsf(ac) * 0.6f;
    else s_threshold *= 0.9999f; /* Slowly decay threshold */

    /* Rising edge peak detection */
    if (!s_peak_found && ac > s_threshold && ac > s_prev_ac) {
        s_peak_found = true;
    }
    else if (s_peak_found && ac < s_prev_ac) {
        /* Falling from peak */
        s_peak_found = false;
        uint32_t now = osKernelSysTick();
        uint32_t ibi = now - s_last_peak_tick; /* inter-beat interval (ms) */

        if (s_last_peak_tick != 0 && ibi > 0) {
            uint16_t bpm_inst = (uint16_t)(60000u / ibi);

            if (bpm_inst >= HR_VALID_MIN && bpm_inst <= HR_VALID_MAX) {
                /* Moving average */
                s_bpm_buf[s_bpm_idx] = bpm_inst;
                s_bpm_idx = (s_bpm_idx + 1) % HR_MA_WINDOW;
                if (s_bpm_count < HR_MA_WINDOW) s_bpm_count++;

                uint32_t sum = 0;
                for (uint8_t i = 0; i < s_bpm_count; i++) sum += s_bpm_buf[i];
                s_bpm_output = (uint16_t)(sum / s_bpm_count);
            }
        }
        s_last_peak_tick = now;
    }
    s_prev_ac = ac;
}

bool HR_GetBPM(uint16_t *bpm_out)
{
    if (!s_finger_present || s_bpm_count == 0) return false;
    *bpm_out = s_bpm_output;
    return true;
}

bool HR_FingerPresent(void)
{
    return s_finger_present;
}

void HR_GetAlertStatus(bool *is_tachy, bool *is_brady)
{
    *is_tachy = (s_bpm_count > 0 && s_bpm_output > HR_TACHY_THRESHOLD);
    *is_brady = (s_bpm_count > 0 && s_bpm_output < HR_BRADY_THRESHOLD && s_bpm_output > 0);
}
