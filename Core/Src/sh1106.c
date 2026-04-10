/* sh1106.c - low-level SH1106 OLED driver: 1024-byte framebuffer + I2C flush */

#include "sh1106.h"
#include "sensor_data.h"
#include "cmsis_os.h"
#include <string.h>

static uint8_t s_framebuf[SH1106_BUF_SIZE];
static uint16_t s_oled_addr = OLED_I2C_ADDR;

static const uint8_t SH1106_INIT_CMDS[] = {
    0xAE,
    0xD5, 0x80,
    0xA8, 0x3F,
    0xD3, 0x00,
    0x40,
    0xAD, 0x8B, /* SH1106 internal DC-DC */
    0xA1,
    0xC8,
    0xDA, 0x12,
    0x81, 0xCF,
    0xD9, 0xF1,
    0xDB, 0x40,
    0xA4,
    0xA6,
    0xAF,
};

SH1106_Status_t SH1106_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd }; /* Co=0, D/C#=0 → command byte */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &APP_I2C_HANDLE, s_oled_addr, buf, 2, I2C_TIMEOUT_MS);
    return (ret == HAL_OK) ? SH1106_OK : SH1106_ERROR;
}

SH1106_Status_t SH1106_WriteData(const uint8_t *data, uint16_t len)
{
    /* Co=0, D/C#=1 → data stream */
    uint8_t ctrl = 0x40;
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(
        &APP_I2C_HANDLE, s_oled_addr, ctrl,
        I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, I2C_TIMEOUT_MS);
    return (ret == HAL_OK) ? SH1106_OK : SH1106_ERROR;
}

SH1106_Status_t SH1106_Init(void)
{
    if (HAL_I2C_IsDeviceReady(&APP_I2C_HANDLE, (uint16_t)(0x3C << 1), 2u, I2C_TIMEOUT_MS) == HAL_OK) {
        s_oled_addr = (uint16_t)(0x3C << 1);
    } else if (HAL_I2C_IsDeviceReady(&APP_I2C_HANDLE, (uint16_t)(0x3D << 1), 2u, I2C_TIMEOUT_MS) == HAL_OK) {
        s_oled_addr = (uint16_t)(0x3D << 1);
    } else {
        s_oled_addr = OLED_I2C_ADDR;
    }

    for (uint8_t i = 0; i < sizeof(SH1106_INIT_CMDS); i++) {
        if (SH1106_WriteCmd(SH1106_INIT_CMDS[i]) != SH1106_OK) {
            return SH1106_ERROR;
        }
    }
    SH1106_Clear();
    SH1106_Flush();
    return SH1106_OK;
}

void SH1106_Clear(void)
{
    memset(s_framebuf, 0x00, SH1106_BUF_SIZE);
}

void SH1106_Fill(uint8_t color)
{
    memset(s_framebuf, color ? 0xFF : 0x00, SH1106_BUF_SIZE);
}

void SH1106_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;

    uint16_t byte_idx = x + (y / 8) * SH1106_WIDTH;
    uint8_t  bit_mask = (uint8_t)(1u << (y % 8));

    if (color) {
        s_framebuf[byte_idx] |=  bit_mask;
    } else {
        s_framebuf[byte_idx] &= ~bit_mask;
    }
}

uint8_t SH1106_GetPixel(uint8_t x, uint8_t y)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return 0;
    uint16_t byte_idx = x + (y / 8) * SH1106_WIDTH;
    uint8_t  bit_mask = (uint8_t)(1u << (y % 8));
    return (s_framebuf[byte_idx] & bit_mask) ? 1u : 0u;
}

uint8_t *SH1106_GetBuffer(void)
{
    return s_framebuf;
}

SH1106_Status_t SH1106_Flush(void)
{
    /* All three devices (OLED, MPU-6050, MAX30102) share hi2c1.
     * Acquire the I2C bus mutex before transmitting; guard NULL so
     * this also works when called from SH1106_Init() before RTOS starts. */
    bool held = (i2cMutexHandle != NULL);
    if (held) osMutexWait(i2cMutexHandle, osWaitForever);

    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        if (SH1106_FlushPage(page) != SH1106_OK) {
            if (held) osMutexRelease(i2cMutexHandle);
            return SH1106_ERROR;
        }
    }

    if (held) osMutexRelease(i2cMutexHandle);
    return SH1106_OK;
}

SH1106_Status_t SH1106_FlushPage(uint8_t page)
{
    if (page >= SH1106_PAGES) return SH1106_ERROR;

    /* Set page address */
    SH1106_WriteCmd(0xB0 | page);
    /* Set column address (SH1106 starts at column 2) */
    SH1106_WriteCmd(0x00 | ((SH1106_COL_OFFSET) & 0x0F));        /* Low nibble  */
    SH1106_WriteCmd(0x10 | ((SH1106_COL_OFFSET >> 4) & 0x0F));   /* High nibble */

    /* Write 128 data bytes for this page */
    const uint8_t *page_data = &s_framebuf[page * SH1106_WIDTH];
    return SH1106_WriteData(page_data, SH1106_WIDTH);
}

void SH1106_SetDisplayOn(bool on)
{
    /* Acquire the shared I2C mutex so this doesn't race with SH1106_Flush. */
    bool held = (i2cMutexHandle != NULL);
    if (held) osMutexWait(i2cMutexHandle, osWaitForever);
    SH1106_WriteCmd(on ? 0xAF : 0xAE);
    if (held) osMutexRelease(i2cMutexHandle);
}

void SH1106_SetContrast(uint8_t contrast)
{
    bool held = (i2cMutexHandle != NULL);
    if (held) osMutexWait(i2cMutexHandle, osWaitForever);
    SH1106_WriteCmd(0x81);
    SH1106_WriteCmd(contrast);
    if (held) osMutexRelease(i2cMutexHandle);
}

void SH1106_SetInvert(bool invert)
{
    SH1106_WriteCmd(invert ? 0xA7 : 0xA6);
}
