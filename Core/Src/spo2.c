/* spo2.c - SpO2% from Red+IR ratio (R = AC_red/DC_red / AC_ir/DC_ir)
 * TODO: calibrate formula against a real pulse oximeter */

#include "spo2.h"
#include "app_config.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

/* state */
static uint32_t s_red_buf[SPO2_BUFFER_SIZE];
static uint32_t s_ir_buf[SPO2_BUFFER_SIZE];
static uint16_t s_idx         = 0;
static uint16_t s_count       = 0;

static float    s_dc_red      = 0.0f;
static float    s_dc_ir       = 0.0f;

static uint8_t  s_spo2_output = 0;
static bool     s_valid       = false;
static bool     s_finger      = false;

void SpO2_Reset(void)
{
    memset(s_red_buf, 0, sizeof(s_red_buf));
    memset(s_ir_buf,  0, sizeof(s_ir_buf));
    s_idx        = 0;
    s_count      = 0;
    s_dc_red     = 0.0f;
    s_dc_ir      = 0.0f;
    s_spo2_output= 0;
    s_valid      = false;
    s_finger     = false;
}

void SpO2_AddSample(uint32_t red, uint32_t ir)
{
    /* Finger check */
    s_finger = (ir >= MAX30102_IR_MIN_VALID);

    s_red_buf[s_idx] = red;
    s_ir_buf[s_idx]  = ir;
    s_idx = (s_idx + 1) % SPO2_BUFFER_SIZE;
    if (s_count < SPO2_BUFFER_SIZE) s_count++;

    /* Update DC tracking (exponential moving average) */
    s_dc_red = SPO2_DC_ALPHA * s_dc_red + (1.0f - SPO2_DC_ALPHA) * (float)red;
    s_dc_ir  = SPO2_DC_ALPHA * s_dc_ir  + (1.0f - SPO2_DC_ALPHA) * (float)ir;

    /* Recalculate SpO2 once per full buffer window */
    if (s_count < SPO2_BUFFER_SIZE) return;
    if (!s_finger) { s_valid = false; return; }

    /* Compute AC_rms for Red and IR */
    float sum_sq_red = 0.0f, sum_sq_ir = 0.0f;
    for (uint16_t i = 0; i < SPO2_BUFFER_SIZE; i++) {
        float ac_red = (float)s_red_buf[i] - s_dc_red;
        float ac_ir  = (float)s_ir_buf[i]  - s_dc_ir;
        sum_sq_red += ac_red * ac_red;
        sum_sq_ir  += ac_ir  * ac_ir;
    }
    float rms_red = sqrtf(sum_sq_red / SPO2_BUFFER_SIZE);
    float rms_ir  = sqrtf(sum_sq_ir  / SPO2_BUFFER_SIZE);

    if (s_dc_ir < 1.0f || s_dc_red < 1.0f) { s_valid = false; return; }

    /* R = (AC_red / DC_red) / (AC_ir / DC_ir) */
    float R = (rms_red / s_dc_red) / (rms_ir / s_dc_ir);

    /* SpO2 ≈ 110 - 25×R  (empirical linear approximation) */
    float spo2_f = 110.0f - 25.0f * R;

    /* Clamp to valid range */
    if (spo2_f < (float)SPO2_VALID_MIN) spo2_f = (float)SPO2_VALID_MIN;
    if (spo2_f > 100.0f) spo2_f = 100.0f;

    s_spo2_output = (uint8_t)spo2_f;
    s_valid       = true;

    /* Reset buffer for next window */
    s_count = 0;
}

bool SpO2_GetValue(uint8_t *spo2_out)
{
    if (!s_valid || !s_finger) return false;
    *spo2_out = s_spo2_output;
    return true;
}

bool SpO2_IsLowAlert(void)
{
    return s_valid && s_finger && (s_spo2_output < SPO2_LOW_THRESHOLD);
}

bool SpO2_FingerPresent(void)
{
    return s_finger;
}
