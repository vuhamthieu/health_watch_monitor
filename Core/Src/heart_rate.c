/* heart_rate.c - Robust BPM from MAX30102 IR: DC removal + low-pass + dynamic peak detect */

#include "heart_rate.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* Internal filter/peak constants (tuned for MAX30102_SAMPLE_RATE_HZ = 100 Hz) */
#define HR_LP_BETA               0.180f  /* Low-pass smoothing for AC component */
#define HR_ENV_BETA              0.050f  /* Envelope tracker speed */
#define HR_THRESH_SCALE          0.550f  /* Dynamic threshold = envelope * scale */
#define HR_THRESH_FLOOR          40.0f   /* Minimum threshold to avoid noise triggers */
#define HR_NEG_ARM_FACTOR        0.350f  /* Need negative swing before accepting next peak */
#define HR_BPM_JUMP_LIMIT        35u     /* Reject sudden BPM jumps caused by motion */
#define HR_OUTLIER_MIN_BPM       40u
#define HR_OUTLIER_MAX_BPM       160u
#define HR_OUTLIER_NEAR_REST_JUMP 25u

/* state */
static uint32_t s_last_ir = 0;
static uint32_t s_sample_index = 0;

/* DC removal + low-pass */
static float    s_dc_estimate = 0.0f;
static float    s_lp_out      = 0.0f;
static float    s_prev_lp     = 0.0f;
static float    s_envelope    = 0.0f;

/* Peak detection */
static bool     s_prev_rising     = false;
static bool     s_peak_armed      = false;
static uint32_t s_last_peak_sample = 0;

/* BPM moving average */
static uint16_t s_bpm_buf[HR_MA_WINDOW];
static uint8_t  s_bpm_idx    = 0;
static uint8_t  s_bpm_count  = 0;
static uint16_t s_bpm_output = 0;

static bool     s_finger_present = false;

static bool hr_is_outlier(uint16_t bpm_inst)
{
    if (bpm_inst < HR_OUTLIER_MIN_BPM || bpm_inst > HR_OUTLIER_MAX_BPM) {
        return true;
    }

    if (s_bpm_count >= 3u) {
        uint16_t avg = s_bpm_output;
        uint16_t diff = (bpm_inst > avg)
                      ? (uint16_t)(bpm_inst - avg)
                      : (uint16_t)(avg - bpm_inst);

        if (avg >= 55u && avg <= 110u && diff > HR_OUTLIER_NEAR_REST_JUMP) {
            return true;
        }
    }

    return false;
}

void HR_Reset(void)
{
    memset(s_bpm_buf, 0, sizeof(s_bpm_buf));
    s_last_ir          = 0;
    s_sample_index     = 0;
    s_dc_estimate      = 0.0f;
    s_lp_out           = 0.0f;
    s_prev_lp          = 0.0f;
    s_envelope         = 0.0f;
    s_prev_rising      = false;
    s_peak_armed       = false;
    s_last_peak_sample = 0;
    s_bpm_idx        = 0;
    s_bpm_count      = 0;
    s_bpm_output     = 0;
    s_finger_present = false;
}

void HR_AddSample(uint32_t ir_sample)
{
    const uint32_t fs_hz = (uint32_t)MAX30102_SAMPLE_RATE_HZ;
    const uint32_t min_ibi_samples = (60u * fs_hz) / (uint32_t)HR_VALID_MAX;
    const uint32_t max_ibi_samples = (60u * fs_hz) / (uint32_t)HR_VALID_MIN;

    s_last_ir = ir_sample;
    s_sample_index++;

    /* Finger detection */
    s_finger_present = (ir_sample >= MAX30102_IR_MIN_VALID);
    if (!s_finger_present) {
        s_dc_estimate      = 0.0f;
        s_lp_out           = 0.0f;
        s_prev_lp          = 0.0f;
        s_envelope         = 0.0f;
        s_prev_rising      = false;
        s_peak_armed       = false;
        s_last_peak_sample = 0;
        s_bpm_count        = 0;
        s_bpm_output       = 0;
        return;
    }

    /* 1) DC removal via one-pole high-pass: hp = x - DC(x) */
    float x = (float)ir_sample;
    s_dc_estimate += HR_DC_FILTER_ALPHA * (x - s_dc_estimate);
    float hp = x - s_dc_estimate;

    /* 2) Low-pass filter to reduce high-frequency and line/interference noise */
    s_lp_out += HR_LP_BETA * (hp - s_lp_out);

    /* Envelope for dynamic threshold */
    float abs_lp = fabsf(s_lp_out);
    s_envelope += HR_ENV_BETA * (abs_lp - s_envelope);

    float threshold = s_envelope * HR_THRESH_SCALE;
    if (threshold < HR_THRESH_FLOOR) {
        threshold = HR_THRESH_FLOOR;
    }

    /* Arm detector only after a meaningful negative swing (full pulse cycle) */
    if (s_lp_out < (-HR_NEG_ARM_FACTOR * threshold)) {
        s_peak_armed = true;
    }

    /* 3) Peak detection: local maxima + dynamic threshold + refractory */
    bool rising = (s_lp_out > s_prev_lp);
    if (s_prev_rising && !rising) {
        float peak_amp = s_prev_lp;
        uint32_t peak_sample = s_sample_index - 1u;

        if (s_peak_armed && peak_amp > threshold) {
            uint32_t gap = (s_last_peak_sample > 0u) ? (peak_sample - s_last_peak_sample) : 0u;

            if (s_last_peak_sample == 0u || gap >= min_ibi_samples) {
                if (s_last_peak_sample != 0u && gap <= max_ibi_samples && gap > 0u) {
                    uint16_t bpm_inst = (uint16_t)(((60u * fs_hz) + (gap / 2u)) / gap);

                    if (bpm_inst >= HR_VALID_MIN && bpm_inst <= HR_VALID_MAX) {
                        if (hr_is_outlier(bpm_inst)) {
                            s_last_peak_sample = peak_sample;
                            s_peak_armed = false;
                            s_prev_rising = rising;
                            s_prev_lp = s_lp_out;
                            return;
                        }

                        bool jump_ok = true;
                        if (s_bpm_count > 0u) {
                            uint16_t diff = (bpm_inst > s_bpm_output)
                                          ? (uint16_t)(bpm_inst - s_bpm_output)
                                          : (uint16_t)(s_bpm_output - bpm_inst);
                            jump_ok = (diff <= HR_BPM_JUMP_LIMIT);
                        }

                        if (jump_ok) {
                            s_bpm_buf[s_bpm_idx] = bpm_inst;
                            s_bpm_idx = (uint8_t)((s_bpm_idx + 1u) % HR_MA_WINDOW);
                            if (s_bpm_count < HR_MA_WINDOW) {
                                s_bpm_count++;
                            }

                            uint32_t sum = 0u;
                            for (uint8_t i = 0u; i < s_bpm_count; i++) {
                                sum += s_bpm_buf[i];
                            }
                            s_bpm_output = (uint16_t)(sum / s_bpm_count);
                        }
                    }
                }

                s_last_peak_sample = peak_sample;
                s_peak_armed = false;
            }
        }
    }

    s_prev_rising = rising;
    s_prev_lp = s_lp_out;
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

uint32_t HR_GetLastIR(void)
{
    return s_last_ir;
}

void HR_GetAlertStatus(bool *is_tachy, bool *is_brady)
{
    *is_tachy = (s_bpm_count > 0 && s_bpm_output > HR_TACHY_THRESHOLD);
    *is_brady = (s_bpm_count > 0 && s_bpm_output < HR_BRADY_THRESHOLD && s_bpm_output > 0);
}
