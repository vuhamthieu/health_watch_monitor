/**
 * @file    oled.h
 * @brief   High-level OLED rendering API (text, numbers, icons, pages).
 *          Built on top of sh1106.h.  Works with the 128×64 framebuffer.
 *
 * Coordinate system:  (0,0) = top-left.
 *   x: 0–127 (column)
 *   y: 0–63  (row)
 */

#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sh1106.h"

/* ========================================================================== *
 *  Font sizes
 * ========================================================================== */
typedef enum {
    OLED_FONT_6x8  = 0,   /**< Small   – 6 wide, 8 tall                     */
    OLED_FONT_8x16 = 1,   /**< Medium  – 8 wide, 16 tall                    */
} OledFont_t;

/* ========================================================================== *
 *  Icon IDs (16×16 bitmaps stored in Flash)
 * ========================================================================== */
typedef enum {
    ICON_HEART      = 0,
    ICON_SPO2,
    ICON_STEPS,
    ICON_BT_ON,
    ICON_BT_OFF,
    ICON_BATTERY,
    ICON_ALERT,
    ICON_SLEEP,
    ICON_COUNT,
} OledIcon_t;

/* ========================================================================== *
 *  API — Lifecycle
 * ========================================================================== */

/**
 * @brief  Initialise the OLED (calls SH1106_Init + clears screen).
 */
void OLED_Init(void);

/**
 * @brief  Clear framebuffer and flush to display.
 */
void OLED_Clear(void);

/**
 * @brief  Push framebuffer to display.
 */
void OLED_Flush(void);

/* ========================================================================== *
 *  API — Text
 * ========================================================================== */

/**
 * @brief  Draw a single character.
 * @param  x, y   Top-left pixel position.
 * @param  ch     ASCII character (32–126).
 * @param  font   Font size.
 * @param  color  1 = white on black (normal), 0 = inverted.
 */
void OLED_DrawChar(uint8_t x, uint8_t y, char ch, OledFont_t font, uint8_t color);

/**
 * @brief  Draw a null-terminated string.
 * @return x position after the last character (for chaining).
 */
uint8_t OLED_DrawStr(uint8_t x, uint8_t y, const char *str, OledFont_t font, uint8_t color);

/**
 * @brief  Draw a formatted string (like printf, internally uses snprintf).
 */
void OLED_Printf(uint8_t x, uint8_t y, OledFont_t font, uint8_t color, const char *fmt, ...);

/* ========================================================================== *
 *  API — Graphics Primitives
 * ========================================================================== */

void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bmp);

/* ========================================================================== *
 *  API — Icons
 * ========================================================================== */

/**
 * @brief  Draw a 16×16 icon from the built-in icon table.
 */
void OLED_DrawIcon(uint8_t x, uint8_t y, OledIcon_t icon);

/* ========================================================================== *
 *  API — Progress / Indicators
 * ========================================================================== */

/**
 * @brief  Draw a horizontal progress bar.
 * @param  value   Current value.
 * @param  max_val Maximum value (bar = full at max_val).
 */
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                          uint32_t value, uint32_t max_val);

/**
 * @brief  Draw 5 filled/empty circles indicating signal/quality level (0–5).
 */
void OLED_DrawLevelDots(uint8_t x, uint8_t y, uint8_t level, uint8_t max_level);

/* ========================================================================== *
 *  API — Screen Pages (pre-composed layouts called by uiTask)
 * ========================================================================== */

/**
 * @brief  Draw the main watch-face screen.
 * @param  bpm       Heart rate (0 = invalid → show "---").
 * @param  spo2      SpO2 % (0 = invalid).
 * @param  steps     Step count.
 * @param  clk_h/m   Current time.
 * @param  bt_conn   Bluetooth connected flag.
 * @param  hr_alert  Show heart alert icon.
 * @param  spo2_alert Show SpO2 alert icon.
 */
void OLED_PageMain(uint16_t bpm, uint8_t spo2, uint32_t steps,
                   uint8_t clk_h, uint8_t clk_m,
                   bool bt_conn, bool hr_alert, bool spo2_alert);

/**
 * @brief  Draw the heart rate detail screen.
 */
void OLED_PageHeartRate(uint16_t bpm, bool alert, const char *status_str);

/**
 * @brief  Draw the SpO2 detail screen.
 */
void OLED_PageSpO2(uint8_t spo2, bool alert, const char *status_str);

/**
 * @brief  Draw the step counter detail screen.
 */
void OLED_PageSteps(uint32_t steps, float dist_m, float cal_kcal, uint32_t goal);

/**
 * @brief  Draw the Bluetooth status screen.
 */
void OLED_PageBluetooth(bool connected, const char *device_name);

/**
 * @brief  Draw the settings menu.
 * @param  items      Array of menu item strings.
 * @param  count      Number of items.
 * @param  selected   Index of currently highlighted item.
 */
void OLED_PageSettings(const char *items[], uint8_t count, uint8_t selected);

/**
 * @brief  Draw the Sleep Options overlay (centred popup).
 * @param  selected  0 = "Sleep Now", 1 = "Cancel".
 */
void OLED_PageSleepOptions(uint8_t selected);

/**
 * @brief  Draw a simple splash / boot screen.
 */
void OLED_PageSplash(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
