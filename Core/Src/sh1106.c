/**
 * @file    sh1106.c
 * @brief   SH1106 OLED low-level driver implementation.
 *
 * TODO (Phase 3):
 *  - Implement SH1106_Init() with full init command sequence
 *  - Implement SH1106_Flush() — write framebuffer page by page over I2C
 *  - Implement SH1106_DrawPixel() — set/clear bit in s_framebuf[]
 */

#include "sh1106.h"
#include "sensor_data.h"   /* for xI2cMutex — TODO: add i2cMutex to sensor_data.h */
#include "cmsis_os.h"
#include <string.h>

/* ========================================================================== *
 *  Framebuffer (1024 bytes, stored in RAM)
 * ========================================================================== */
static uint8_t s_framebuf[SH1106_BUF_SIZE];

/* ========================================================================== *
 *  SH1106 Init Command Sequence
 * ========================================================================== */
static const uint8_t SH1106_INIT_CMDS[] = {
    0xAE,       /* Display OFF                          */
    0xD5, 0x80, /* Set display clock divide ratio       */
    0xA8, 0x3F, /* Set multiplex ratio: 64 lines (0x3F)*/
    0xD3, 0x00, /* Set display offset: 0                */
    0x40,       /* Set display start line: 0            */
    0xAD, 0x8B, /* Enable internal DC-DC (SH1106)       */
    0xA1,       /* Set segment re-map: col 127 → SEG0   */
    0xC8,       /* Set COM output scan direction: remapped */
    0xDA, 0x12, /* Set COM pins hardware config         */
    0x81, 0xCF, /* Set contrast: 0xCF                   */
    0xD9, 0xF1, /* Set pre-charge period                */
    0xDB, 0x40, /* Set VCOMH deselect level             */
    0xA4,       /* Entire display on: resume RAM        */
    0xA6,       /* Set normal display (not inverted)    */
    0xAF,       /* Display ON                           */
};

/* ========================================================================== *
 *  Low-level I2C helpers
 * ========================================================================== */
SH1106_Status_t SH1106_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd }; /* Co=0, D/C#=0 → command byte */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &APP_I2C_HANDLE, OLED_I2C_ADDR, buf, 2, I2C_TIMEOUT_MS);
    return (ret == HAL_OK) ? SH1106_OK : SH1106_ERROR;
}

SH1106_Status_t SH1106_WriteData(const uint8_t *data, uint16_t len)
{
    /* Prepend 0x40 control byte (Co=0, D/C#=1 → data stream) */
    /* TODO: For efficiency, use a DMA or chunked approach */
    uint8_t ctrl = 0x40;
    HAL_StatusTypeDef ret;

    /* Send control byte then data as a single I2C transaction using
     * the memory-write function — or use two sequential transmits */
    ret = HAL_I2C_Mem_Write(
        &APP_I2C_HANDLE, OLED_I2C_ADDR, ctrl,
        I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, I2C_TIMEOUT_MS);
    return (ret == HAL_OK) ? SH1106_OK : SH1106_ERROR;
}

/* ========================================================================== *
 *  Init
 * ========================================================================== */
SH1106_Status_t SH1106_Init(void)
{
    /* TODO: acquire xI2cMutex before sending commands */
    for (uint8_t i = 0; i < sizeof(SH1106_INIT_CMDS); i++) {
        if (SH1106_WriteCmd(SH1106_INIT_CMDS[i]) != SH1106_OK) {
            return SH1106_ERROR;
        }
    }
    SH1106_Clear();
    SH1106_Flush();
    return SH1106_OK;
}

/* ========================================================================== *
 *  Framebuffer operations
 * ========================================================================== */
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

/* ========================================================================== *
 *  Flush — send all 8 pages to the OLED
 * ========================================================================== */
SH1106_Status_t SH1106_Flush(void)
{
    /* TODO: acquire xI2cMutex (from sensor_data.h / main.h) */
    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        if (SH1106_FlushPage(page) != SH1106_OK) {
            return SH1106_ERROR;
        }
    }
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

/* ========================================================================== *
 *  Display control
 * ========================================================================== */
void SH1106_SetDisplayOn(bool on)
{
    SH1106_WriteCmd(on ? 0xAF : 0xAE);
}

void SH1106_SetContrast(uint8_t contrast)
{
    SH1106_WriteCmd(0x81);
    SH1106_WriteCmd(contrast);
}

void SH1106_SetInvert(bool invert)
{
    SH1106_WriteCmd(invert ? 0xA7 : 0xA6);
}
