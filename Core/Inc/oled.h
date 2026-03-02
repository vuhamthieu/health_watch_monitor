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
    ICON_HEART_SMALL = 0,   /* 16x16 retro heart                            */
    ICON_HEART_BIG,         /* 24x24 beating heart (frame 0 normal)         */
    ICON_HEART_BIG2,        /* 24x24 beating heart (frame 1 expanded)       */
    ICON_SHOE,              /* 16x16 retro shoe / steps                     */
    ICON_BT_ON,             /* 12x16 Bluetooth symbol                       */
    ICON_BT_OFF,            /* 12x16 Bluetooth symbol with cross            */
    ICON_CHARGING,          /* 12x16 lightning bolt                         */
    ICON_ALERT,             /* 16x16 exclamation triangle                   */
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

/** @brief  Draw a single 6×8 character scaled up by @p scale (e.g. 2 = 12×16). */
void OLED_DrawCharScaled(uint8_t x, uint8_t y, char ch, uint8_t scale, uint8_t color);

/** @brief  Draw a string with uniform integer scaling. */
void OLED_DrawStrScaled(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);

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
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                          uint32_t value, uint32_t max_val);
void OLED_DrawArc(uint8_t cx, uint8_t cy, uint8_t r, uint8_t pct, uint8_t color);
void OLED_DrawBatteryIcon(uint8_t x, uint8_t y, uint8_t bars,
                          bool charging, bool full);

/* ========================================================================== *
 *  API — Screen Pages
 * ========================================================================== */
#include "sensor_data.h"
#include "ui_menu.h"

/* Retro homescreen */
void OLED_PageHome(const SoftClock_t *clk, const BatteryStatus_t *bat,
                   bool bt_enabled, bool bt_connected);

/* Scrollable main menu */
void OLED_PageMenu(uint8_t cursor, uint8_t scroll_offset);

/* HR measurement: beating heart + circular progress ring */
void OLED_PageHRMeasure(MeasPhase_t phase, uint8_t progress,
                        uint8_t anim_frame, uint16_t bpm_result);

/* SpO2 measurement */
void OLED_PageSpO2Measure(MeasPhase_t phase, uint8_t progress,
                          uint8_t spo2_result);

/* EEG placeholder */
void OLED_PageEEG(void);

/* Workout screen */
void OLED_PageWorkout(WorkoutMode_t mode, bool active,
                      uint32_t reps, uint32_t elapsed_s);

/* Stopwatch */
void OLED_PageStopwatch(uint32_t elapsed_ms, bool running);

/* 7-day bar chart statistics */
void OLED_PageStats(const WeekStats_t *stats);

/* Settings list */
void OLED_PageSettings(uint8_t cursor, bool bt_en,
                       bool raise_wake, bool fall_det, uint8_t brightness);

/* Power/sleep menu */
void OLED_PagePowerMenu(uint8_t cursor);

/* Splash screen */
void OLED_PageSplash(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
