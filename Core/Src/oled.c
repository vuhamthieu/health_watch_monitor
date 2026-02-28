/**
 * @file    oled.c
 * @brief   High-level OLED rendering — text, graphics primitives, screen pages.
 *
 * TODO (Phase 3 / Phase 5):
 *  - Add font bitmaps (6×8 and 8×16) as const arrays in Flash
 *  - Add 16×16 icon bitmaps
 *  - Implement all OLED_Page*() functions
 */

#include "oled.h"
#include "app_config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========================================================================== *
 *  Font: 6×8  (ASCII 32–126, stored in Flash)
 *  Each character = 6 bytes (columns), 8 rows of pixels per column.
 *
 *  TODO: Replace placeholder with a real 6×8 font table.
 * ========================================================================== */
static const uint8_t FONT_6x8[][6] = {
    /* 0x20 ' ' */ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /* 0x21 '!' */ { 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 },
    /* ... TODO: fill in the full ASCII table ... */
    /* 0x7E '~' */ { 0x08, 0x10, 0x10, 0x08, 0x08, 0x10 },
};

#define FONT_6x8_W  6u
#define FONT_6x8_H  8u
#define FONT_6x8_FIRST 0x20
#define FONT_6x8_LAST  0x7E

/* ========================================================================== *
 *  Font: 8×16  (placeholder — TODO: add proper double-height font)
 * ========================================================================== */
#define FONT_8x16_W  8u
#define FONT_8x16_H  16u

/* ========================================================================== *
 *  Icon bitmaps 16×16 (2 bytes wide × 16 rows = 32 bytes each)
 *  TODO: draw real icons
 * ========================================================================== */
static const uint8_t ICON_HEART_BMP[32]  = { /* TODO */ 0 };
static const uint8_t ICON_SPO2_BMP[32]   = { /* TODO */ 0 };
static const uint8_t ICON_STEPS_BMP[32]  = { /* TODO */ 0 };
static const uint8_t ICON_BT_ON_BMP[32]  = { /* TODO */ 0 };
static const uint8_t ICON_BT_OFF_BMP[32] = { /* TODO */ 0 };
static const uint8_t ICON_BATTERY_BMP[32]= { /* TODO */ 0 };
static const uint8_t ICON_ALERT_BMP[32]  = { /* TODO */ 0 };
static const uint8_t ICON_SLEEP_BMP[32]  = { /* TODO */ 0 };

static const uint8_t * const ICONS[ICON_COUNT] = {
    ICON_HEART_BMP, ICON_SPO2_BMP, ICON_STEPS_BMP,
    ICON_BT_ON_BMP, ICON_BT_OFF_BMP, ICON_BATTERY_BMP,
    ICON_ALERT_BMP, ICON_SLEEP_BMP,
};

/* ========================================================================== *
 *  Lifecycle
 * ========================================================================== */
void OLED_Init(void)
{
    SH1106_Init();
}

void OLED_Clear(void)
{
    SH1106_Clear();
    SH1106_Flush();
}

void OLED_Flush(void)
{
    SH1106_Flush();
}

/* ========================================================================== *
 *  Text rendering
 * ========================================================================== */
void OLED_DrawChar(uint8_t x, uint8_t y, char ch, OledFont_t font, uint8_t color)
{
    if (font == OLED_FONT_6x8)
    {
        if ((uint8_t)ch < FONT_6x8_FIRST || (uint8_t)ch > FONT_6x8_LAST) ch = '?';
        const uint8_t *glyph = FONT_6x8[(uint8_t)ch - FONT_6x8_FIRST];
        for (uint8_t col = 0; col < FONT_6x8_W; col++) {
            uint8_t col_data = glyph[col];
            for (uint8_t row = 0; row < FONT_6x8_H; row++) {
                uint8_t pixel = (col_data >> row) & 0x01;
                SH1106_DrawPixel(x + col, y + row, color ? pixel : !pixel);
            }
        }
    }
    else /* OLED_FONT_8x16 */
    {
        /* TODO: implement 8×16 font rendering */
        (void)ch; (void)x; (void)y; (void)color;
    }
}

uint8_t OLED_DrawStr(uint8_t x, uint8_t y, const char *str, OledFont_t font, uint8_t color)
{
    uint8_t w = (font == OLED_FONT_6x8) ? FONT_6x8_W : FONT_8x16_W;
    while (*str && x + w <= OLED_WIDTH) {
        OLED_DrawChar(x, y, *str++, font, color);
        x += w;
    }
    return x;
}

void OLED_Printf(uint8_t x, uint8_t y, OledFont_t font, uint8_t color, const char *fmt, ...)
{
    char buf[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OLED_DrawStr(x, y, buf, font, color);
}

/* ========================================================================== *
 *  Graphics Primitives
 * ========================================================================== */
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
    /* Bresenham line algorithm */
    int dx =  abs((int)x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs((int)y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        SH1106_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 = (uint8_t)(x0 + sx); }
        if (e2 <= dx) { err += dx; y0 = (uint8_t)(y0 + sy); }
    }
}

void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    OLED_DrawLine(x,         y,         x + w - 1, y,         color);
    OLED_DrawLine(x,         y + h - 1, x + w - 1, y + h - 1, color);
    OLED_DrawLine(x,         y,         x,         y + h - 1, color);
    OLED_DrawLine(x + w - 1, y,         x + w - 1, y + h - 1, color);
}

void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t row = y; row < y + h; row++) {
        for (uint8_t col = x; col < x + w; col++) {
            SH1106_DrawPixel(col, row, color);
        }
    }
}

void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bmp)
{
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint16_t idx = (uint16_t)(row * ((w + 7) / 8)) + col / 8;
            uint8_t  bit = (uint8_t)(7 - (col % 8));
            uint8_t  pixel = (bmp[idx] >> bit) & 0x01;
            SH1106_DrawPixel(x + col, y + row, pixel);
        }
    }
}

void OLED_DrawIcon(uint8_t x, uint8_t y, OledIcon_t icon)
{
    if (icon >= ICON_COUNT) return;
    OLED_DrawBitmap(x, y, 16, 16, ICONS[icon]);
}

/* ========================================================================== *
 *  Indicators
 * ========================================================================== */
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                          uint32_t value, uint32_t max_val)
{
    OLED_DrawRect(x, y, w, h, 1);
    if (max_val == 0) return;
    uint8_t fill = (uint8_t)(((uint32_t)value * (w - 2)) / max_val);
    if (fill > 0) OLED_FillRect(x + 1, y + 1, fill, h - 2, 1);
}

void OLED_DrawLevelDots(uint8_t x, uint8_t y, uint8_t level, uint8_t max_level)
{
    for (uint8_t i = 0; i < max_level; i++) {
        uint8_t filled = (i < level) ? 1u : 0u;
        if (filled) {
            OLED_FillRect(x + i * 7, y, 5, 5, 1);
        } else {
            OLED_DrawRect(x + i * 7, y, 5, 5, 1);
        }
    }
}

/* ========================================================================== *
 *  Screen Pages
 *  TODO (Phase 5): Implement proper layouts, fonts, icons
 * ========================================================================== */

void OLED_PageMain(uint16_t bpm, uint8_t spo2, uint32_t steps,
                   uint8_t clk_h, uint8_t clk_m,
                   bool bt_conn, bool hr_alert, bool spo2_alert)
{
    SH1106_Clear();

    /* Time — top centre */
    OLED_Printf(40, 0, OLED_FONT_8x16, 1, "%02d:%02d", clk_h, clk_m);

    /* Heart rate */
    OLED_DrawIcon(0, 16, ICON_HEART);
    if (bpm > 0 && bpm <= 250)
        OLED_Printf(18, 20, OLED_FONT_6x8, 1, "%3d bpm", bpm);
    else
        OLED_DrawStr(18, 20, "--- bpm", OLED_FONT_6x8, 1);
    if (hr_alert) OLED_DrawIcon(100, 16, ICON_ALERT);

    /* SpO2 */
    OLED_DrawIcon(0, 36, ICON_SPO2);
    if (spo2 > 0)
        OLED_Printf(18, 40, OLED_FONT_6x8, 1, " %3d %%", spo2);
    else
        OLED_DrawStr(18, 40, "  --- %%", OLED_FONT_6x8, 1);
    if (spo2_alert) OLED_DrawIcon(100, 36, ICON_ALERT);

    /* Steps */
    OLED_DrawIcon(64, 36, ICON_STEPS);
    OLED_Printf(82, 40, OLED_FONT_6x8, 1, "%5lu", (unsigned long)steps);

    /* BT icon */
    OLED_DrawIcon(110, 0, bt_conn ? ICON_BT_ON : ICON_BT_OFF);

    SH1106_Flush();
}

void OLED_PageHeartRate(uint16_t bpm, bool alert, const char *status_str)
{
    SH1106_Clear();
    OLED_DrawStr(4, 0, "HEART RATE", OLED_FONT_8x16, 1);
    OLED_DrawLine(0, 18, 127, 18, 1);
    OLED_DrawIcon(8, 26, ICON_HEART);
    if (bpm > 0 && bpm <= 250)
        OLED_Printf(32, 24, OLED_FONT_8x16, 1, "%3d BPM", bpm);
    else
        OLED_DrawStr(32, 24, "---", OLED_FONT_8x16, 1);
    if (alert)    OLED_DrawStr(30, 48, "! ALERT !", OLED_FONT_6x8, 1);
    if (status_str) OLED_DrawStr(0, 56, status_str, OLED_FONT_6x8, 1);
    SH1106_Flush();
}

void OLED_PageSpO2(uint8_t spo2, bool alert, const char *status_str)
{
    SH1106_Clear();
    OLED_DrawStr(20, 0, "SpO2", OLED_FONT_8x16, 1);
    OLED_DrawLine(0, 18, 127, 18, 1);
    OLED_DrawIcon(8, 26, ICON_SPO2);
    if (spo2 > 0)
        OLED_Printf(32, 24, OLED_FONT_8x16, 1, " %3d %%", spo2);
    else
        OLED_DrawStr(32, 24, "---", OLED_FONT_8x16, 1);
    if (alert)    OLED_DrawStr(24, 48, "LOW SpO2!", OLED_FONT_6x8, 1);
    if (status_str) OLED_DrawStr(0, 56, status_str, OLED_FONT_6x8, 1);
    SH1106_Flush();
}

void OLED_PageSteps(uint32_t steps, float dist_m, float cal_kcal, uint32_t goal)
{
    SH1106_Clear();
    OLED_DrawStr(24, 0, "STEPS", OLED_FONT_8x16, 1);
    OLED_DrawLine(0, 18, 127, 18, 1);
    OLED_Printf(0, 20, OLED_FONT_8x16, 1, "%5lu", (unsigned long)steps);
    OLED_DrawProgressBar(0, 38, 128, 8, steps, goal);
    OLED_Printf(0, 48, OLED_FONT_6x8, 1, "%.2fkm  %.0fkcal", dist_m / 1000.0f, (double)cal_kcal);
    SH1106_Flush();
}

void OLED_PageBluetooth(bool connected, const char *device_name)
{
    SH1106_Clear();
    OLED_DrawStr(16, 0, "BLUETOOTH", OLED_FONT_8x16, 1);
    OLED_DrawLine(0, 18, 127, 18, 1);
    OLED_DrawIcon(56, 22, connected ? ICON_BT_ON : ICON_BT_OFF);
    OLED_DrawStr(8, 42, connected ? "CONNECTED" : "SEARCHING...", OLED_FONT_6x8, 1);
    if (connected && device_name)
        OLED_DrawStr(0, 54, device_name, OLED_FONT_6x8, 1);
    SH1106_Flush();
}

void OLED_PageSettings(const char *items[], uint8_t count, uint8_t selected)
{
    SH1106_Clear();
    OLED_DrawStr(24, 0, "SETTINGS", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 10, 127, 10, 1);

    uint8_t visible = (count < 6u) ? count : 6u;
    for (uint8_t i = 0; i < visible; i++) {
        uint8_t y = (uint8_t)(12 + i * 9);
        if (i == selected) {
            OLED_FillRect(0, y - 1, 128, 9, 1);
            OLED_DrawStr(4, y, items[i], OLED_FONT_6x8, 0); /* inverted text */
        } else {
            OLED_DrawStr(4, y, items[i], OLED_FONT_6x8, 1);
        }
    }
    SH1106_Flush();
}

void OLED_PageSleepOptions(uint8_t selected)
{
    /* Draw popup overlay — clear a rectangle in the centre */
    OLED_FillRect(14, 16, 100, 32, 0);
    OLED_DrawRect(14, 16, 100, 32, 1);
    OLED_DrawStr(30, 18, "SLEEP?", OLED_FONT_6x8, 1);
    OLED_DrawLine(14, 27, 113, 27, 1);

    /* Option 0: Sleep Now */
    if (selected == 0) {
        OLED_FillRect(16, 28, 96, 8, 1);
        OLED_DrawStr(20, 29, "Sleep Now", OLED_FONT_6x8, 0);
    } else {
        OLED_DrawStr(20, 29, "Sleep Now", OLED_FONT_6x8, 1);
    }

    /* Option 1: Cancel */
    if (selected == 1) {
        OLED_FillRect(16, 37, 96, 8, 1);
        OLED_DrawStr(28, 38, "Cancel", OLED_FONT_6x8, 0);
    } else {
        OLED_DrawStr(28, 38, "Cancel", OLED_FONT_6x8, 1);
    }

    SH1106_Flush();
}

void OLED_PageSplash(void)
{
    SH1106_Clear();
    OLED_DrawStr(20, 20, "HealthWatch", OLED_FONT_8x16, 1);
    OLED_DrawStr(36, 40, "v1.0", OLED_FONT_6x8, 1);
    SH1106_Flush();
}
