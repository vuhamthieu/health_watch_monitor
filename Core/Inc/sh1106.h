/**
 * @file    sh1106.h
 * @brief   Low-level driver for SH1106 OLED controller (128×64, I2C).
 *          Common on 1.3-inch OLED modules.  Uses a full 1 KB framebuffer.
 *
 * USAGE:
 *   SH1106_Init();
 *   SH1106_Clear();
 *   SH1106_DrawPixel(x, y, 1);
 *   SH1106_Flush();   // pushes framebuffer to display
 */

#ifndef __SH1106_H
#define __SH1106_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ========================================================================== *
 *  Constants
 * ========================================================================== */
#define SH1106_WIDTH        128u
#define SH1106_HEIGHT       64u
#define SH1106_BUF_SIZE     (SH1106_WIDTH * SH1106_HEIGHT / 8u)  /* 1024 B  */
#define SH1106_PAGES        (SH1106_HEIGHT / 8u)                     /* 8 pages */

/* SH1106 has 2 "invisible" columns on each side → data starts at column 2  */
#define SH1106_COL_OFFSET   2u

/* ========================================================================== *
 *  HAL_StatusTypeDef return alias
 * ========================================================================== */
typedef enum {
    SH1106_OK    = 0,
    SH1106_ERROR = 1,
} SH1106_Status_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise the SH1106 controller (send init command sequence).
 * @return SH1106_OK on success, SH1106_ERROR on I2C fault.
 */
SH1106_Status_t SH1106_Init(void);

/**
 * @brief  Clear the internal framebuffer (does NOT flush to display).
 */
void SH1106_Clear(void);

/**
 * @brief  Fill the entire framebuffer with one colour.
 * @param  color  0 = black (off), 1 = white (on).
 */
void SH1106_Fill(uint8_t color);

/**
 * @brief  Set a single pixel in the framebuffer.
 * @param  x      Column 0–127.
 * @param  y      Row 0–63.
 * @param  color  0 = off, 1 = on.
 */
void SH1106_DrawPixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief  Read a pixel from the framebuffer.
 * @return 0 or 1.
 */
uint8_t SH1106_GetPixel(uint8_t x, uint8_t y);

/**
 * @brief  Flush the entire framebuffer to the OLED over I2C.
 *         Acquires xI2cMutex internally.
 * @return SH1106_OK on success.
 */
SH1106_Status_t SH1106_Flush(void);

/**
 * @brief  Flush only the dirty pages (optimised path, optional).
 */
SH1106_Status_t SH1106_FlushPage(uint8_t page);

/**
 * @brief  Turn the display on or off (OLED panel power).
 */
void SH1106_SetDisplayOn(bool on);

/**
 * @brief  Set display contrast (0x00–0xFF, default 0xCF).
 */
void SH1106_SetContrast(uint8_t contrast);

/**
 * @brief  Set display inversion mode.
 */
void SH1106_SetInvert(bool invert);

/**
 * @brief  Return a pointer to the raw framebuffer (1024 bytes).
 *         Useful for blitting pre-rendered bitmaps.
 */
uint8_t *SH1106_GetBuffer(void);

/* ========================================================================== *
 *  Low-level helpers (used internally; exposed for advanced use)
 * ========================================================================== */
SH1106_Status_t SH1106_WriteCmd(uint8_t cmd);
SH1106_Status_t SH1106_WriteData(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SH1106_H */
