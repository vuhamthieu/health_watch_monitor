/**
 * @file    max30102.c
 * @brief   MAX30102 heart rate / SpO2 sensor driver implementation.
 *
 * TODO (Phase 2 — Hardware Bring-up):
 *  - Verify I2C at address 0x57 with Part ID register (should return 0x15)
 *  - Test FIFO read: observe IR/Red values with and without finger
 *
 * TODO (Phase 3 — Driver):
 *  - Fine-tune LED current (MAX30102_SetLEDCurrent) for your hardware
 *  - Implement interrupt-driven FIFO read (INT pin if wired)
 */

#include "max30102.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ========================================================================== *
 *  MAX30102 Register Map (subset)
 * ========================================================================== */
#define MAX30102_REG_INT_STATUS1    0x00
#define MAX30102_REG_INT_STATUS2    0x01
#define MAX30102_REG_INT_ENABLE1    0x02
#define MAX30102_REG_INT_ENABLE2    0x03
#define MAX30102_REG_FIFO_WR_PTR    0x04
#define MAX30102_REG_OVF_COUNTER    0x05
#define MAX30102_REG_FIFO_RD_PTR    0x06
#define MAX30102_REG_FIFO_DATA      0x07
#define MAX30102_REG_FIFO_CONFIG    0x08
#define MAX30102_REG_MODE_CONFIG    0x09
#define MAX30102_REG_SPO2_CONFIG    0x0A
#define MAX30102_REG_LED1_PA        0x0C   /* Red LED pulse amplitude */
#define MAX30102_REG_LED2_PA        0x0D   /* IR LED pulse amplitude  */
#define MAX30102_REG_PILOT_PA       0x10
#define MAX30102_REG_MULTI_LED1     0x11
#define MAX30102_REG_MULTI_LED2     0x12
#define MAX30102_REG_TEMP_INT       0x1F
#define MAX30102_REG_TEMP_FRAC      0x20
#define MAX30102_REG_TEMP_CONFIG    0x21
#define MAX30102_REG_PART_ID        0xFF

#define MAX30102_PART_ID_VALUE      0x15

/* ========================================================================== *
 *  Internal helpers
 * ========================================================================== */
static HAL_StatusTypeDef reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(
        &APP_I2C_HANDLE, MAX30102_I2C_ADDR, buf, 2, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef reg_read(uint8_t reg, uint8_t *out, uint8_t len)
{
    return HAL_I2C_Mem_Read(
        &APP_I2C_HANDLE, MAX30102_I2C_ADDR, reg,
        I2C_MEMADD_SIZE_8BIT, out, len, I2C_TIMEOUT_MS);
}

/* ========================================================================== *
 *  Init
 * ========================================================================== */
MAX30102_Status_t MAX30102_Init(void)
{
    uint8_t part_id = 0;
    if (reg_read(MAX30102_REG_PART_ID, &part_id, 1) != HAL_OK) {
        return MAX30102_ERR_I2C;
    }
    if (part_id != MAX30102_PART_ID_VALUE) {
        return MAX30102_ERR_PARTID;
    }

    /* Software reset */
    MAX30102_Reset();

    /* FIFO configuration: no averaging, rollover ENABLED (wr wraps past rd),
     * almost-full=15. Without rollover, wr wraps to rd making FIFO look empty. */
    reg_write(MAX30102_REG_FIFO_CONFIG, 0x1F);

    /* Mode: SpO2 (Red + IR) */
    reg_write(MAX30102_REG_MODE_CONFIG, (uint8_t)MAX30102_MODE_SPO2);

    /* SpO2: ADC range 4096nA, 100 sps, 411µs pulse width (18-bit) */
    reg_write(MAX30102_REG_SPO2_CONFIG,
              (0x02 << 5) |                          /* ADC range: 4096 nA */
              ((uint8_t)MAX30102_SR_100 << 2) |     /* 100 sps */
              (uint8_t)MAX30102_PW_411);

    /* LED currents: 0.2 mA/LSB. Use ~12 mA baseline for wrist modules. */
    MAX30102_SetLEDCurrent(0x3F, 0x3F);

    /* Clear FIFO */
    MAX30102_FlushFIFO();

    return MAX30102_OK;
}

/* ========================================================================== *
 *  Reset
 * ========================================================================== */
MAX30102_Status_t MAX30102_Reset(void)
{
    reg_write(MAX30102_REG_MODE_CONFIG, 0x40); /* RESET bit */
    osDelay(100);
    return MAX30102_OK;
}

bool MAX30102_IsConnected(void)
{
    uint8_t part_id = 0;
    if (reg_read(MAX30102_REG_PART_ID, &part_id, 1) != HAL_OK) return false;
    return (part_id == MAX30102_PART_ID_VALUE);
}

bool MAX30102_FingerDetected(void)
{
    /* Read one FIFO sample and check IR level */
    uint8_t wr_ptr, rd_ptr;
    reg_read(MAX30102_REG_FIFO_WR_PTR, &wr_ptr, 1);
    reg_read(MAX30102_REG_FIFO_RD_PTR, &rd_ptr, 1);
    if (wr_ptr == rd_ptr) return false;

    uint8_t raw[6];
    reg_read(MAX30102_REG_FIFO_DATA, raw, 6);
    uint32_t ir = ((uint32_t)(raw[3] & 0x03) << 16) |
                  ((uint32_t)raw[4] << 8) |
                  raw[5];
    return (ir >= MAX30102_IR_MIN_VALID);
}

/* ========================================================================== *
 *  FIFO read
 * ========================================================================== */
MAX30102_Status_t MAX30102_ReadFIFO(MAX30102_Sample_t *buf,
                                    uint8_t max_count,
                                    uint8_t *read_count)
{
    uint8_t wr_ptr = 0, rd_ptr = 0, ovf = 0;
    *read_count = 0;

    if (reg_read(MAX30102_REG_FIFO_WR_PTR, &wr_ptr, 1) != HAL_OK) return MAX30102_ERR_I2C;
    if (reg_read(MAX30102_REG_FIFO_RD_PTR, &rd_ptr, 1) != HAL_OK) return MAX30102_ERR_I2C;
    if (reg_read(MAX30102_REG_OVF_COUNTER, &ovf,    1) != HAL_OK) return MAX30102_ERR_I2C;

    /* Clear overflow flag — but DON'T bail out. Read whatever is available now.
     * The old logic of setting rd=wr on OVF caused an infinite 0-read loop:
     * the main loop is too slow to outrun overflow, so it was always OVF → always 0. */
    if (ovf > 0) {
        reg_write(MAX30102_REG_OVF_COUNTER, 0);
    }

    int avail = (int)wr_ptr - (int)rd_ptr;
    if (avail < 0) avail += 32;

    if (avail == 0) {
        *read_count = 0;
        return MAX30102_OK;
    }

    uint8_t to_read = ((uint8_t)avail < max_count) ? (uint8_t)avail : max_count;

    /* Single burst read: one I2C transaction for all samples.
     * Sequential reads of the FIFO_DATA register auto-increment the read pointer
     * on the chip side — each 6-byte block is one sample (3 bytes Red, 3 bytes IR). */
    uint8_t temp_buf[16 * 6]; /* max_count capped at 16 by caller */
    if (HAL_I2C_Mem_Read(&APP_I2C_HANDLE, MAX30102_I2C_ADDR,
                         MAX30102_REG_FIFO_DATA, I2C_MEMADD_SIZE_8BIT,
                         temp_buf, (uint16_t)(to_read * 6u),
                         I2C_TIMEOUT_MS) != HAL_OK) {
        return MAX30102_ERR_I2C;
    }

    for (uint8_t i = 0; i < to_read; i++) {
        buf[i].red = ((uint32_t)(temp_buf[i*6u + 0u] & 0x03u) << 16) |
                     ((uint32_t) temp_buf[i*6u + 1u]           <<  8) |
                      (uint32_t) temp_buf[i*6u + 2u];
        buf[i].ir  = ((uint32_t)(temp_buf[i*6u + 3u] & 0x03u) << 16) |
                     ((uint32_t) temp_buf[i*6u + 4u]           <<  8) |
                      (uint32_t) temp_buf[i*6u + 5u];
    }

    *read_count = to_read;
    return MAX30102_OK;
}

void MAX30102_DumpRegs(void)
{
    uint8_t mode = 0, spo2 = 0, led1 = 0, led2 = 0, wr = 0, rd = 0, ovf = 0;
    reg_read(MAX30102_REG_MODE_CONFIG,  &mode, 1);
    reg_read(MAX30102_REG_SPO2_CONFIG,  &spo2, 1);
    reg_read(MAX30102_REG_LED1_PA,      &led1, 1);
    reg_read(MAX30102_REG_LED2_PA,      &led2, 1);
    reg_read(MAX30102_REG_FIFO_WR_PTR,  &wr,   1);
    reg_read(MAX30102_REG_FIFO_RD_PTR,  &rd,   1);
    reg_read(MAX30102_REG_OVF_COUNTER,  &ovf,  1);
    printf("[MAX-DBG] mode=0x%02X spo2cfg=0x%02X led1=0x%02X led2=0x%02X "
           "wr=%u rd=%u ovf=%u\r\n",
           mode, spo2, led1, led2, wr, rd, ovf);
}

MAX30102_Status_t MAX30102_FlushFIFO(void)
{
    reg_write(MAX30102_REG_FIFO_WR_PTR, 0);
    reg_write(MAX30102_REG_OVF_COUNTER, 0);
    reg_write(MAX30102_REG_FIFO_RD_PTR, 0);
    return MAX30102_OK;
}

MAX30102_Status_t MAX30102_SetLEDCurrent(uint8_t red_pa, uint8_t ir_pa)
{
    reg_write(MAX30102_REG_LED1_PA, red_pa);
    reg_write(MAX30102_REG_LED2_PA, ir_pa);
    return MAX30102_OK;
}

MAX30102_Status_t MAX30102_SetShutdown(bool shutdown)
{
    uint8_t val;
    reg_read(MAX30102_REG_MODE_CONFIG, &val, 1);
    if (shutdown)  val |=  0x80;
    else           val &= ~0x80;
    reg_write(MAX30102_REG_MODE_CONFIG, val);
    return MAX30102_OK;
}

MAX30102_Status_t MAX30102_SetMode(MAX30102_Mode_t mode)
{
    return (reg_write(MAX30102_REG_MODE_CONFIG, (uint8_t)mode) == HAL_OK)
           ? MAX30102_OK : MAX30102_ERR_I2C;
}

MAX30102_Status_t MAX30102_SetSampleRate(MAX30102_SampleRate_t sr,
                                          MAX30102_PulseWidth_t pw)
{
    uint8_t val = (uint8_t)((0x02 << 5) | ((uint8_t)sr << 2) | (uint8_t)pw);
    return (reg_write(MAX30102_REG_SPO2_CONFIG, val) == HAL_OK)
           ? MAX30102_OK : MAX30102_ERR_I2C;
}
