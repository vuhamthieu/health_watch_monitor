/* oled.c - high-level rendering: text, graphics, full screen pages */

#include "oled.h"
#include "app_config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "micon_heart.h"
#include "micon_spo2.h"
#include "micon_workout.h"
#include "micon_stopwatch.h"
#include "micon_stats.h"
#include "micon_settings.h"
#include "micon_walking.h"
#include "micon_running.h"
#include "micon_connect.h"

/* ========================================================================== *
 *  Font: 6×8  (ASCII 32–126, stored in Flash)
 *  Each character = 6 bytes (columns), 8 rows of pixels per column.
 *
 *  TODO: Replace placeholder with a real 6×8 font table.
 * ========================================================================== */
/* 6x8 font: ASCII 0x20 to 0x7E, 6 column-bytes per glyph (LSB = top row) */
static const uint8_t FONT_6x8[][6] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 0x20 ' ' */
    { 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 }, /* 0x21 '!' */
    { 0x00, 0x07, 0x00, 0x07, 0x00, 0x00 }, /* 0x22 '"' */
    { 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00 }, /* 0x23 '#' */
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00 }, /* 0x24 '$' */
    { 0x23, 0x13, 0x08, 0x64, 0x62, 0x00 }, /* 0x25 '%' */
    { 0x36, 0x49, 0x55, 0x22, 0x50, 0x00 }, /* 0x26 '&' */
    { 0x00, 0x05, 0x03, 0x00, 0x00, 0x00 }, /* 0x27 '\'' */
    { 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00 }, /* 0x28 '(' */
    { 0x00, 0x41, 0x22, 0x1C, 0x00, 0x00 }, /* 0x29 ')' */
    { 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00 }, /* 0x2A '*' */
    { 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 }, /* 0x2B '+' */
    { 0x00, 0x50, 0x30, 0x00, 0x00, 0x00 }, /* 0x2C ',' */
    { 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 }, /* 0x2D '-' */
    { 0x00, 0x60, 0x60, 0x00, 0x00, 0x00 }, /* 0x2E '.' */
    { 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 }, /* 0x2F '/' */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 }, /* 0x30 '0' */
    { 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 }, /* 0x31 '1' */
    { 0x42, 0x61, 0x51, 0x49, 0x46, 0x00 }, /* 0x32 '2' */
    { 0x21, 0x41, 0x45, 0x4B, 0x31, 0x00 }, /* 0x33 '3' */
    { 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 }, /* 0x34 '4' */
    { 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 }, /* 0x35 '5' */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00 }, /* 0x36 '6' */
    { 0x01, 0x71, 0x09, 0x05, 0x03, 0x00 }, /* 0x37 '7' */
    { 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 }, /* 0x38 '8' */
    { 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00 }, /* 0x39 '9' */
    { 0x00, 0x36, 0x36, 0x00, 0x00, 0x00 }, /* 0x3A ':' */
    { 0x00, 0x56, 0x36, 0x00, 0x00, 0x00 }, /* 0x3B ';' */
    { 0x08, 0x14, 0x22, 0x41, 0x00, 0x00 }, /* 0x3C '<' */
    { 0x14, 0x14, 0x14, 0x14, 0x14, 0x00 }, /* 0x3D '=' */
    { 0x00, 0x41, 0x22, 0x14, 0x08, 0x00 }, /* 0x3E '>' */
    { 0x02, 0x01, 0x51, 0x09, 0x06, 0x00 }, /* 0x3F '?' */
    { 0x32, 0x49, 0x79, 0x41, 0x3E, 0x00 }, /* 0x40 '@' */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00 }, /* 0x41 'A' */
    { 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00 }, /* 0x42 'B' */
    { 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 }, /* 0x43 'C' */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00 }, /* 0x44 'D' */
    { 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 }, /* 0x45 'E' */
    { 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00 }, /* 0x46 'F' */
    { 0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00 }, /* 0x47 'G' */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 }, /* 0x48 'H' */
    { 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00 }, /* 0x49 'I' */
    { 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00 }, /* 0x4A 'J' */
    { 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00 }, /* 0x4B 'K' */
    { 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00 }, /* 0x4C 'L' */
    { 0x7F, 0x02, 0x04, 0x02, 0x7F, 0x00 }, /* 0x4D 'M' */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00 }, /* 0x4E 'N' */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00 }, /* 0x4F 'O' */
    { 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 }, /* 0x50 'P' */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00 }, /* 0x51 'Q' */
    { 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00 }, /* 0x52 'R' */
    { 0x46, 0x49, 0x49, 0x49, 0x31, 0x00 }, /* 0x53 'S' */
    { 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00 }, /* 0x54 'T' */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00 }, /* 0x55 'U' */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00 }, /* 0x56 'V' */
    { 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00 }, /* 0x57 'W' */
    { 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 }, /* 0x58 'X' */
    { 0x07, 0x08, 0x70, 0x08, 0x07, 0x00 }, /* 0x59 'Y' */
    { 0x61, 0x51, 0x49, 0x45, 0x43, 0x00 }, /* 0x5A 'Z' */
    { 0x00, 0x7F, 0x41, 0x41, 0x00, 0x00 }, /* 0x5B '[' */
    { 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 }, /* 0x5C '\\' */
    { 0x00, 0x41, 0x41, 0x7F, 0x00, 0x00 }, /* 0x5D ']' */
    { 0x04, 0x02, 0x01, 0x02, 0x04, 0x00 }, /* 0x5E '^' */
    { 0x40, 0x40, 0x40, 0x40, 0x40, 0x00 }, /* 0x5F '_' */
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x00 }, /* 0x60 '`' */
    { 0x20, 0x54, 0x54, 0x54, 0x78, 0x00 }, /* 0x61 'a' */
    { 0x7F, 0x48, 0x44, 0x44, 0x38, 0x00 }, /* 0x62 'b' */
    { 0x38, 0x44, 0x44, 0x44, 0x20, 0x00 }, /* 0x63 'c' */
    { 0x38, 0x44, 0x44, 0x48, 0x7F, 0x00 }, /* 0x64 'd' */
    { 0x38, 0x54, 0x54, 0x54, 0x18, 0x00 }, /* 0x65 'e' */
    { 0x08, 0x7E, 0x09, 0x01, 0x02, 0x00 }, /* 0x66 'f' */
    { 0x08, 0x14, 0x54, 0x54, 0x3C, 0x00 }, /* 0x67 'g' */
    { 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00 }, /* 0x68 'h' */
    { 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00 }, /* 0x69 'i' */
    { 0x20, 0x40, 0x44, 0x3D, 0x00, 0x00 }, /* 0x6A 'j' */
    { 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00 }, /* 0x6B 'k' */
    { 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00 }, /* 0x6C 'l' */
    { 0x7C, 0x04, 0x18, 0x04, 0x78, 0x00 }, /* 0x6D 'm' */
    { 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00 }, /* 0x6E 'n' */
    { 0x38, 0x44, 0x44, 0x44, 0x38, 0x00 }, /* 0x6F 'o' */
    { 0x7C, 0x14, 0x14, 0x14, 0x08, 0x00 }, /* 0x70 'p' */
    { 0x08, 0x14, 0x14, 0x18, 0x7C, 0x00 }, /* 0x71 'q' */
    { 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00 }, /* 0x72 'r' */
    { 0x48, 0x54, 0x54, 0x54, 0x20, 0x00 }, /* 0x73 's' */
    { 0x04, 0x3F, 0x44, 0x40, 0x20, 0x00 }, /* 0x74 't' */
    { 0x3C, 0x40, 0x40, 0x40, 0x3C, 0x00 }, /* 0x75 'u' */
    { 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00 }, /* 0x76 'v' */
    { 0x3C, 0x40, 0x20, 0x40, 0x3C, 0x00 }, /* 0x77 'w' */
    { 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 }, /* 0x78 'x' */
    { 0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00 }, /* 0x79 'y' */
    { 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 }, /* 0x7A 'z' */
    { 0x00, 0x08, 0x36, 0x41, 0x00, 0x00 }, /* 0x7B '{' */
    { 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00 }, /* 0x7C '|' */
    { 0x00, 0x41, 0x36, 0x08, 0x00, 0x00 }, /* 0x7D '}' */
    { 0x08, 0x10, 0x10, 0x08, 0x08, 0x10 }, /* 0x7E '~' */
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
 *  Icon bitmaps (column-byte format, LSB = top row)
 * ========================================================================== */

/* 16x16 small heart (retro pixel art) */
static const uint8_t ICON_HEART_SMALL_BMP[32] = {
    0x00,0x00, 0x38,0x1C, 0x7C,0x3E, 0xFE,0x7F,
    0xFE,0x7F, 0xFE,0x7F, 0xFC,0x3F, 0xF8,0x1F,
    0xF0,0x0F, 0xE0,0x07, 0xC0,0x03, 0x80,0x01,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

/* 24x24 heart normal (frame 0) — stored as 3 bytes/row × 24 rows */
static const uint8_t ICON_HEART_BIG_BMP[72] = {
    0x00,0x00,0x00, 0x1C,0x38,0x00, 0x3E,0x7C,0x00, 0x7F,0xFE,0x00,
    0xFF,0xFF,0x00, 0xFF,0xFF,0x00, 0xFF,0xFF,0x00, 0xFF,0xFF,0x00,
    0xFF,0xFF,0x00, 0x7F,0xFE,0x00, 0x3F,0xFC,0x00, 0x1F,0xF8,0x00,
    0x0F,0xF0,0x00, 0x07,0xE0,0x00, 0x03,0xC0,0x00, 0x01,0x80,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};

/* 24x24 heart expanded (frame 1 — beat animation) */
static const uint8_t ICON_HEART_BIG2_BMP[72] = {
    0x00,0x00,0x00, 0x0E,0x70,0x00, 0x3F,0xFC,0x00, 0x7F,0xFE,0x00,
    0xFF,0xFF,0x00, 0xFF,0xFF,0x00, 0xFF,0xFF,0x00, 0xFF,0xFF,0x00,
    0xFF,0xFF,0x00, 0xFF,0xFF,0x00, 0x7F,0xFE,0x00, 0x3F,0xFC,0x00,
    0x1F,0xF8,0x00, 0x0F,0xF0,0x00, 0x07,0xE0,0x00, 0x03,0xC0,0x00,
    0x01,0x80,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};

/* 12x16 Bluetooth symbol */
static const uint8_t ICON_BT_ON_BMP[32] = {
    0x18,0x00, 0x28,0x00, 0xC8,0x00, 0x68,0x00,
    0x38,0x00, 0x68,0x00, 0xC8,0x00, 0x28,0x00,
    0x18,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

/* 12x16 Bluetooth symbol with cross */
static const uint8_t ICON_BT_OFF_BMP[32] = {
    0x98,0x00, 0xA8,0x00, 0xC8,0x00, 0x68,0x00,
    0xB8,0x01, 0x68,0x02, 0xC8,0x04, 0xA8,0x00,
    0x98,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

/* 16x16 lightning bolt (charging indicator) */
static const uint8_t ICON_CHARGING_BMP[32] = {
    0x00,0x00, 0xE0,0x00, 0xF0,0x00, 0xF8,0x00,
    0xFC,0x7F, 0xFE,0x3F, 0xFF,0x1F, 0x1F,0x00,
    0x0F,0x00, 0x07,0x00, 0x03,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

/* 16x16 exclamation alert triangle */
static const uint8_t ICON_ALERT_BMP[32] = {
    0x00,0x00, 0x80,0x00, 0xC0,0x01, 0xE0,0x03,
    0x70,0x07, 0x38,0x0E, 0x1C,0x1C, 0xCE,0x38,
    0xC6,0x30, 0xE6,0x31, 0xC6,0x30, 0xFE,0x3F,
    0xFC,0x1F, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

static const uint8_t * const ICONS[ICON_COUNT] = {
    ICON_HEART_SMALL_BMP,
    ICON_HEART_BIG_BMP,
    ICON_HEART_BIG2_BMP,
    ICON_BT_ON_BMP,
    ICON_BT_OFF_BMP,
    ICON_CHARGING_BMP,
    ICON_ALERT_BMP,
};

/* ========================================================================== *
 *  Lifecycle
 * ========================================================================== */
void OLED_Init(void)  { SH1106_Init(); }
void OLED_Clear(void) { SH1106_Clear(); SH1106_Flush(); }
void OLED_Flush(void) { SH1106_Flush(); }

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

/* Scale any 6×8 glyph up by an integer factor (2 = 12×16, 3 = 18×24 …) */
void OLED_DrawCharScaled(uint8_t x, uint8_t y, char ch, uint8_t scale, uint8_t color)
{
    if ((uint8_t)ch < FONT_6x8_FIRST || (uint8_t)ch > FONT_6x8_LAST) ch = '?';
    const uint8_t *glyph = FONT_6x8[(uint8_t)ch - FONT_6x8_FIRST];
    for (uint8_t col = 0; col < FONT_6x8_W; col++) {
        uint8_t col_data = glyph[col];
        for (uint8_t row = 0; row < FONT_6x8_H; row++) {
            uint8_t pixel = (col_data >> row) & 0x01u;
            uint8_t px    = color ? pixel : (uint8_t)(!pixel);
            for (uint8_t sx = 0; sx < scale; sx++) {
                for (uint8_t sy = 0; sy < scale; sy++) {
                    SH1106_DrawPixel(
                        x + col*scale + sx,
                        y + row*scale + sy, px);
                }
            }
        }
    }
}

void OLED_DrawStrScaled(uint8_t x, uint8_t y, const char *str,
                        uint8_t scale, uint8_t color)
{
    while (*str && x + FONT_6x8_W * scale <= OLED_WIDTH) {
        OLED_DrawCharScaled(x, y, *str++, scale, color);
        x = (uint8_t)(x + FONT_6x8_W * scale);
    }
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

/* ========================================================================== *
 *  Arc drawing (progress ring)
 *  Draws only the arc from 270° (top) clockwise for pct% of the full circle.
 *  Uses midpoint circle and checks angle of each pixel.
 * ========================================================================== */
void OLED_DrawArc(uint8_t cx, uint8_t cy, uint8_t r, uint8_t pct, uint8_t color)
{
    /* pct 0–100 → degrees 0–360 swept clockwise from top (270°) */
    if (pct == 0u) return;
    int32_t sweep_deg = ((int32_t)pct * 360) / 100;

    int32_t x = r, y = 0, err = 1 - (int32_t)r;
    while (x >= y) {
        /* 8 octant points of midpoint circle */
        int32_t pts[8][2] = {
            { cx + x, cy - y }, { cx + y, cy - x },
            { cx - y, cy - x }, { cx - x, cy - y },
            { cx - x, cy + y }, { cx - y, cy + x },
            { cx + y, cy + x }, { cx + x, cy + y },
        };
        for (int i = 0; i < 8; i++) {
            int32_t dx = pts[i][0] - (int32_t)cx;
            int32_t dy = pts[i][1] - (int32_t)cy;
            /* atan2 in degrees, origin at top (270°), clockwise */
            /* angle = (atan2(dx, -dy) in degrees + 360) % 360 */
            /* Use integer approximation: compare quadrant */
            /* Full atan2 too heavy — use sine/cosine via lookup would be
               ideal but we'll approximate: map octant fraction by ratio    */
            /* Simpler: compute angle via 32-bit scaled atan2-like calc    */
            /* angle_deg = (atan2f(dx, -dy) * 180/π + 360) % 360          */
            float ang = atan2f((float)dx, -(float)dy) * 57.2957795f;
            if (ang < 0.0f) ang += 360.0f;
            if (ang <= (float)sweep_deg) {
                if (pts[i][0] >= 0 && pts[i][0] < 128 &&
                    pts[i][1] >= 0 && pts[i][1] <  64) {
                    SH1106_DrawPixel((uint8_t)pts[i][0],
                                     (uint8_t)pts[i][1], color);
                }
            }
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* ========================================================================== *
 *  Battery icon  (x,y = top-left of 18×8 area)
 *  bars 0–4 = fill level, charging/full from TP4057 pins
 * ========================================================================== */
void OLED_DrawBatteryIcon(uint8_t x, uint8_t y, uint8_t bars,
                           bool charging, bool full)
{
    /* Outer rectangle 16×8 */
    OLED_DrawRect(x, y, 16, 8, 1);
    /* + cap */
    OLED_FillRect((uint8_t)(x + 16), (uint8_t)(y + 2), 2, 4, 1);

    /* Fill bars — each bar 2px wide, 1px gap, 4px tall inside */
    for (uint8_t b = 0; b < 4u; b++) {
        uint8_t bx = (uint8_t)(x + 2 + b * 3);
        if (b < bars) {
            OLED_FillRect(bx, (uint8_t)(y + 2), 2, 4, 1);
        }
    }

    /* Charging bolt overlay (top-right of icon area) */
    if (charging && !full) {
        /* small lightning: 3-pixel diagonal */
        SH1106_DrawPixel((uint8_t)(x + 9), (uint8_t)(y + 1), 1);
        SH1106_DrawPixel((uint8_t)(x + 8), (uint8_t)(y + 3), 1);
        SH1106_DrawPixel((uint8_t)(x + 9), (uint8_t)(y + 3), 1);
        SH1106_DrawPixel((uint8_t)(x + 8), (uint8_t)(y + 5), 1);
    }
    if (full) {
        /* tiny check mark */
        SH1106_DrawPixel((uint8_t)(x + 8), (uint8_t)(y + 4), 1);
        SH1106_DrawPixel((uint8_t)(x + 9), (uint8_t)(y + 5), 1);
        SH1106_DrawPixel((uint8_t)(x +10), (uint8_t)(y + 3), 1);
        SH1106_DrawPixel((uint8_t)(x +11), (uint8_t)(y + 2), 1);
    }
}

/* ========================================================================== *
 *  Screen Pages — new retro design
 * ========================================================================== */

/* ── Home screen ─────────────────────────────────────────────────────────── */
void OLED_PageHome(const SoftClock_t *clk, const BatteryStatus_t *bat,
                   bool bt_enabled, bool bt_connected,
                   uint32_t steps, uint8_t hr_bpm, uint8_t spo2_pct)
{
    SH1106_Clear();

    /* ── Status bar (y=0..15, 16px tall) ─────────────────────────────
     * BT icon (12×16) | clock 1× centred | battery (18×16)
     * Clock at 1× = 8px tall → vertically centred at y=4
     * "HH:MM" = 5 chars × 6px = 30px, centred in x=12..108 → x=45
     * ─────────────────────────────────────────────────────────────── */
    if (bt_enabled) {
        OLED_DrawBitmap(0, 0, 12, 16,
                        bt_connected ? ICON_BT_ON_BMP : ICON_BT_OFF_BMP);
    }
    OLED_DrawBatteryIcon(109, 0,
        bat->bars,
        (bat->charge == BATT_CHARGING),
        (bat->charge == BATT_FULL));
    {
        char ts[8];
        snprintf(ts, sizeof(ts), "%02u:%02u", clk->hours, clk->minutes);
        OLED_DrawStr(45, 4, ts, OLED_FONT_6x8, 1);
    }

    /* ── Divider ── */
    OLED_DrawLine(0, 16, 127, 16, 1);

    /* ═══════════════════════════════════════════════════════════════
     * Three equal tiles  y=17..63 (47px)
     *  Each column is 42px wide, dividers at x=42 and x=85.
     *
     *  Col1  x=  0..41   Heart Rate
     *  Col2  x= 43..84   SpO2
     *  Col3  x= 86..127  Steps
     *
     *  Layout per tile:
     *    Icon  16×16   y=17   centred: x = col_x + 13
     *    Value 2×scale y=35   each char 12px wide, centred in 42px
     *    Label 1×scale y=56   centred in 42px
     * ═══════════════════════════════════════════════════════════════ */

    OLED_DrawLine(42, 17, 42, 63, 1);
    OLED_DrawLine(85, 17, 85, 63, 1);

/* helper macro: centre a string of known px-width inside col_start+42 */
#define TILE_VALUE(col_x, str) do { \
    uint8_t _w = (uint8_t)(strlen(str) * 12u); \
    uint8_t _x = (uint8_t)(col_x) + (_w < 42u ? (uint8_t)((42u - _w) / 2u) : 0u); \
    OLED_DrawStrScaled(_x, 35, str, 2, 1); \
} while (0)

#define TILE_LABEL(col_x, lbl) do { \
    uint8_t _w2 = (uint8_t)(sizeof(lbl) * 6u - 6u); \
    uint8_t _x2 = (uint8_t)(col_x) + (_w2 < 42u ? (uint8_t)((42u - _w2) / 2u) : 0u); \
    OLED_DrawStr(_x2, 56, lbl, OLED_FONT_6x8, 1); \
} while (0)

    /* ── Tile 1: Heart Rate ── */
    OLED_DrawBitmap(13, 17, 16, 16, MICON_HEART);
    {
        char vb[5];
        /* HR 0-250: max 3 digits (36px) fits in 42px */
        if (hr_bpm > 0u) snprintf(vb, sizeof(vb), "%u",  (unsigned)hr_bpm);
        else              snprintf(vb, sizeof(vb), "--");
        TILE_VALUE(0, vb);
    }
    TILE_LABEL(0, "bpm");

    /* ── Tile 2: SpO2 ── */
    OLED_DrawBitmap(56, 17, 16, 16, MICON_SPO2);
    {
        char vb[5];
        /* SpO2 0-100: "%" appended, max "100%" = 4 chars = 48px.
         * Drop % from value; it fits as separate label below. */
        if (spo2_pct > 0u) snprintf(vb, sizeof(vb), "%u",  (unsigned)spo2_pct);
        else                snprintf(vb, sizeof(vb), "--");
        TILE_VALUE(43, vb);
    }
    TILE_LABEL(43, "SpO2%");

    /* -- Tile 3: Steps -- */
    OLED_DrawBitmap(99, 17, 16, 16, MICON_WALKING);
    {
        char vb[12];
        /* Keep <=3 chars: 0-999 exact, >=1000 show Xk/XXk */
        if (steps < 1000UL)
            snprintf(vb, sizeof(vb), "%lu",  (unsigned long)steps);
        else
            snprintf(vb, sizeof(vb), "%luk", (unsigned long)(steps / 1000UL));
        TILE_VALUE(86, vb);
    }
    TILE_LABEL(86, "steps");

#undef TILE_VALUE
#undef TILE_LABEL

    SH1106_Flush();
}

/* ── Menu screen ─────────────────────────────────────────────────────────── */

/* ------------------------------------------------------------------ *
 *  16×16 menu icons — row-major, MSB-first (2 bytes per row, 16 rows)
 *  Bit layout: byte[row*2+0] = cols 0-7, byte[row*2+1] = cols 8-15
 *  MSB (bit 7) = leftmost pixel of the byte.
 * ------------------------------------------------------------------ */

/* Pointer to each item's icon — order matches MENU_LABELS */
static const uint8_t * const MENU_ICONS[MENU_ITEM_COUNT] = {
    MICON_HEART,
    MICON_SPO2,
    MICON_WORKOUT,
    MICON_STOPWATCH,
    MICON_STATS,
    MICON_SETTINGS,
    MICON_CONNECT,
};

static const char * const MENU_LABELS[MENU_ITEM_COUNT] = {
    "Heart Rate",
    "SpO2",
    "Workout",
    "Stopwatch",
    "Statistics",
    "Settings",
    "Connect",
};

/* ── Layout constants ───────────────────────────────────────────────────── *
 *  Screen 128×64.
 *  Left panel  : x 0..39  (40 px wide) — icons 16×16
 *  Divider     : x 40     (1 px)
 *  Right panel : x 42..127 (86 px wide) — text items
 *
 *  Header bar  : y 0..13  — "MENU" banner with slash tail
 *  Items start : y 15
 *  Row height  : (64-15) / 5 = ~9-10 px → show 5 rows at once
 *  Icon for the visible selected row is drawn in the left panel.
 * ─────────────────────────────────────────────────────────────────────── */
#define MENU_TOP_Y      4u
#define MENU_ROW_H      18u
#define MENU_ROWS_VIS   3u
#define MENU_ICON_X     8u
#define MENU_TEXT_X     30u

void OLED_PageMenu(uint8_t cursor, uint8_t scroll_offset)
{
    SH1106_Clear();

    /* Right dotted scrollbar track */
    for (uint8_t y = 4u; y < 61u; y = (uint8_t)(y + 3u)) {
        SH1106_DrawPixel(125u, y, 1);
    }
    uint8_t thumb_h = 12u;
    uint8_t thumb_y = 4u;
    if (MENU_ITEM_COUNT > 1u) {
        thumb_y = (uint8_t)(4u + ((uint32_t)(56u - thumb_h) * cursor) / (MENU_ITEM_COUNT - 1u));
    }
    OLED_FillRect(123u, thumb_y, 4u, thumb_h, 1u);

    /* 3 visible rows */
    for (uint8_t i = 0; i < MENU_ROWS_VIS; i++) {
        uint8_t item = (uint8_t)(scroll_offset + i);
        if (item >= MENU_ITEM_COUNT) break;

        uint8_t y = (uint8_t)(MENU_TOP_Y + i * MENU_ROW_H);
        uint8_t text_y = (uint8_t)(y + 5u);

        if (item == cursor) {
            OLED_DrawRect(4u, y, 118u, 17u, 1u);

            /* Left selector wedge */
            uint8_t cy = (uint8_t)(y + 8u);
            SH1106_DrawPixel(0u, cy, 1u);
            OLED_DrawLine(1u, (uint8_t)(cy - 1u), 1u, (uint8_t)(cy + 1u), 1u);
            OLED_DrawLine(2u, (uint8_t)(cy - 2u), 2u, (uint8_t)(cy + 2u), 1u);
            OLED_DrawLine(3u, (uint8_t)(cy - 3u), 3u, (uint8_t)(cy + 3u), 1u);
        }

        OLED_DrawBitmap(MENU_ICON_X, (uint8_t)(y + 1u), 16u, 16u, MENU_ICONS[item]);
        OLED_DrawStr(MENU_TEXT_X, text_y, MENU_LABELS[item], OLED_FONT_6x8, 1u);
    }

    SH1106_Flush();
}

/* ── HR Measure ──────────────────────────────────────────────────────────── */

/* Mini ECG waveform: 32 y-offsets per heartbeat cycle (4 beats across 128px).
 * Negative = upward on screen.  P-wave, QRS spike, T-wave pattern. */
static const int8_t s_ecg_beat[32] = {
    0, 0, 0, 0, -1,-2,-1, 0,   /*  flat  |  P wave          */
    0, 1,-7, 4, 1, 0, 0, 0,    /*  Q R S |  return baseline  */
   -1,-2,-2,-1, 0, 0, 0, 0,    /*  T wave                    */
    0, 0, 0, 0, 0, 0, 0, 0,    /*  flat inter-beat            */
};

/* Helper: draw the ECG trace across full width at the given baseline y.
 * scroll_offset shifts the waveform phase each frame for a moving effect. */
static void draw_ecg_trace(uint8_t base_y, uint8_t scroll_offset)
{
    for (uint8_t x = 0; x < 127u; x++) {
        uint8_t y0 = (uint8_t)((int16_t)base_y + s_ecg_beat[(x +       scroll_offset) % 32]);
        uint8_t y1 = (uint8_t)((int16_t)base_y + s_ecg_beat[(x + 1u + scroll_offset) % 32]);
        OLED_DrawLine(x, y0, (uint8_t)(x + 1u), y1, 1);
    }
}

void OLED_PageHRMeasure(MeasPhase_t phase, uint8_t progress,
                        uint8_t anim_frame, uint16_t bpm_result)
{
    SH1106_Clear();

    if (phase == MEAS_IDLE) {
        /* ── Title with decorative lines ── */
        OLED_DrawBitmap(0, 0, 16, 16, MICON_HEART);
        OLED_DrawStr(19, 4, "Heart Rate", OLED_FONT_6x8, 1);
        OLED_DrawLine(0, 17, 127, 17, 1);
        OLED_DrawStr(28, 36, "Press SELECT", OLED_FONT_6x8, 1);

    } else if (phase == MEAS_MEASURING) {
        /* ── Thick progress arc (2-px ring) with beating heart ── */
        #define HR_CX  28u
        #define HR_CY  24u
        #define HR_R   22u
        OLED_DrawArc(HR_CX, HR_CY, HR_R,     progress, 1);
        OLED_DrawArc(HR_CX, HR_CY, HR_R - 1, progress, 1);

        /* Beating heart centered inside the arc (visual center at col 7.5, row 8) */
        {
            const uint8_t *bmp = (anim_frame % 2u == 0u)
                                 ? ICON_HEART_BIG_BMP : ICON_HEART_BIG2_BMP;
            OLED_DrawBitmap(HR_CX - 8, HR_CY - 8, 24, 24, bmp);
        }

        /* ── BPM value — 3x scale, right half ── */
        {
            char vb[8];
            if (bpm_result > 0u && bpm_result <= 250u)
                snprintf(vb, sizeof(vb), "%u", (unsigned)bpm_result);
            else
                snprintf(vb, sizeof(vb), "---");

            /* Centre in the right zone (x 58..127 = 70 px) */
            uint8_t tw = (uint8_t)(strlen(vb) * 18u); /* 6*3 per char */
            uint8_t xv = (uint8_t)(58u + (70u - tw) / 2u);
            OLED_DrawStrScaled(xv, 8, vb, 3, 1);
        }
        /* "bpm" label centred below the number */
        OLED_DrawStr(82, 34, "bpm", OLED_FONT_6x8, 1);

        /* ── ECG waveform trace (scrolling animation) ── */
        draw_ecg_trace(52u, (uint8_t)((uint16_t)anim_frame * 4u));

        /* ── Thin progress bar at very bottom ── */
        OLED_DrawProgressBar(0, 61, 127, 3, progress, 100u);

    } else { /* MEAS_DONE */
        /* ── Full thick ring ── */
        OLED_DrawArc(HR_CX, HR_CY, HR_R,     100, 1);
        OLED_DrawArc(HR_CX, HR_CY, HR_R - 1, 100, 1);

        /* Static heart (visual center at col 7.5, row 8) */
        OLED_DrawBitmap(HR_CX - 8, HR_CY - 8, 24, 24, ICON_HEART_BIG_BMP);

        /* ── BPM result — 3x scale ── */
        {
            char vb[8];
            if (bpm_result > 0u && bpm_result <= 250u)
                snprintf(vb, sizeof(vb), "%u", (unsigned)bpm_result);
            else
                snprintf(vb, sizeof(vb), "---");

            uint8_t tw = (uint8_t)(strlen(vb) * 18u);
            uint8_t xv = (uint8_t)(58u + (70u - tw) / 2u);
            OLED_DrawStrScaled(xv, 8, vb, 3, 1);
        }
        OLED_DrawStr(82, 34, "bpm", OLED_FONT_6x8, 1);

        /* ── ECG trace (frozen phase) + "Complete" label ── */
        draw_ecg_trace(52u, (uint8_t)((uint16_t)anim_frame * 4u));
    }
    SH1106_Flush();
}

/* ── SpO2 Measure ─────────────────────────────────────────────────────────── */
void OLED_PageSpO2Measure(MeasPhase_t phase, uint8_t progress,
                           uint8_t spo2_result)
{
    SH1106_Clear();

    if (phase == MEAS_IDLE) {
        OLED_DrawBitmap(0, 0, 16, 16, MICON_SPO2);
        OLED_DrawStr(19, 4, "Oxygen", OLED_FONT_6x8, 1);
        OLED_DrawLine(0, 17, 127, 17, 1);
        OLED_DrawStr(28, 36, "Press SELECT", OLED_FONT_6x8, 1);

    } else if (phase == MEAS_MEASURING) {
        OLED_DrawBitmap(2, 0, 16, 16, MICON_SPO2);
        OLED_DrawStr(22, 4, "Oxygen", OLED_FONT_6x8, 1);
        OLED_DrawLine(0, 17, 127, 17, 1);
        /* Live SpO2 large — show current reading or dashes */
        {
            char _buf[8];
            if (spo2_result > 0u) {
                snprintf(_buf, sizeof(_buf), "%3u%%", (unsigned)spo2_result);
                OLED_DrawStrScaled(4, 24, _buf, 3, 1);
            } else {
                OLED_DrawStrScaled(4, 24, "---%", 3, 1);
            }
        }
        /* Thin progress bar at bottom */
        OLED_DrawProgressBar(0, 60, 127, 4, progress, 100u);

    } else { /* MEAS_DONE */
        OLED_DrawBitmap(2, 0, 16, 16, MICON_SPO2);
        OLED_DrawStr(22, 4, "Blood O2", OLED_FONT_6x8, 1);
        OLED_DrawLine(0, 17, 127, 17, 1);
        {
            char _buf[8];
            if (spo2_result > 0u) {
                snprintf(_buf, sizeof(_buf), "%3u%%", (unsigned)spo2_result);
                OLED_DrawStrScaled(4, 24, _buf, 3, 1);
            } else {
                OLED_DrawStrScaled(4, 24, "---%", 3, 1);
            }
        }
        /* Only warn when low — no other text */
        if (spo2_result > 0u && spo2_result < 95u)
            OLED_DrawStr(38, 56, "! LOW !", OLED_FONT_6x8, 1);
    }
    SH1106_Flush();
}

/* ── Workout sub-menu ─────────────────────────────────────────────────────── */
void OLED_PageWorkoutMenu(uint8_t cursor)
{
    static const char * const WORKOUT_LABELS[] = {
        "Walking", "Running", "Push-ups"
    };
    static const uint8_t * const WORKOUT_ICONS[] = {
        MICON_WALKING,   /* Walking  */
        MICON_RUNNING,   /* Running  */
        MICON_WORKOUT,   /* Push-ups */
    };

    SH1106_Clear();

    /* Right dotted scrollbar track */
    for (uint8_t y = 4u; y < 61u; y = (uint8_t)(y + 3u)) {
        SH1106_DrawPixel(125u, y, 1);
    }
    uint8_t thumb_h = 16u;
    uint8_t thumb_y = 4u;
    if ((uint8_t)WORKOUT_COUNT > 1u) {
        thumb_y = (uint8_t)(4u + ((uint32_t)(56u - thumb_h) * cursor) /
                            ((uint8_t)WORKOUT_COUNT - 1u));
    }
    OLED_FillRect(123u, thumb_y, 4u, thumb_h, 1u);

    /* 3 items — exactly fill the screen */
    for (uint8_t i = 0u; i < (uint8_t)WORKOUT_COUNT; i++) {
        uint8_t y      = (uint8_t)(MENU_TOP_Y + i * MENU_ROW_H);
        uint8_t text_y = (uint8_t)(y + 5u);

        if (i == cursor) {
            OLED_DrawRect(4u, y, 118u, 17u, 1u);
            uint8_t cy = (uint8_t)(y + 8u);
            SH1106_DrawPixel(0u, cy, 1u);
            OLED_DrawLine(1u, (uint8_t)(cy - 1u), 1u, (uint8_t)(cy + 1u), 1u);
            OLED_DrawLine(2u, (uint8_t)(cy - 2u), 2u, (uint8_t)(cy + 2u), 1u);
            OLED_DrawLine(3u, (uint8_t)(cy - 3u), 3u, (uint8_t)(cy + 3u), 1u);
        }

        OLED_DrawBitmap(MENU_ICON_X, (uint8_t)(y + 1u), 16u, 16u, WORKOUT_ICONS[i]);
        OLED_DrawStr(MENU_TEXT_X, text_y, WORKOUT_LABELS[i], OLED_FONT_6x8, 1u);
    }

    SH1106_Flush();
}

/* ── Workout ──────────────────────────────────────────────────────────────── */
void OLED_PageWorkout(WorkoutMode_t mode, bool active,
                      uint32_t reps, uint32_t elapsed_s, uint8_t hr_bpm)
{
    static const char * const MODE_NAMES[] = {
        "Walking", "Running", "Push-ups"
    };
    static const uint8_t * const MODE_ICONS[] = {
        MICON_WALKING,
        MICON_RUNNING,
        MICON_WORKOUT,
    };

    uint8_t m = ((uint8_t)mode < (uint8_t)WORKOUT_COUNT) ? (uint8_t)mode : 0u;

    SH1106_Clear();

    /* ── Status bar: icon + mode name + active indicator ── */
    OLED_DrawBitmap(0, 0, 16, 16, MODE_ICONS[m]);
    OLED_DrawStr(20, 4, MODE_NAMES[m], OLED_FONT_6x8, 1);
    OLED_DrawStr(95, 4, active ? "ACT" : "STP", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* ── Large elapsed timer centred (2x scale) ── */
    {
        char ts[20];
        uint32_t hh = elapsed_s / 3600UL;
        uint32_t mm = (elapsed_s % 3600UL) / 60UL;
        uint32_t ss = elapsed_s % 60UL;
        if (hh > 0u) {
            snprintf(ts, sizeof(ts), "%lu:%02lu:%02lu", hh, mm, ss);
        } else {
            snprintf(ts, sizeof(ts), "%02lu:%02lu", mm, ss);
        }
        uint8_t tw = (uint8_t)(strlen(ts) * 12u);  /* 6x2 per char */
        uint8_t tx = (uint8_t)((128u - tw) / 2u);
        OLED_DrawStrScaled(tx, 20, ts, 2, 1);
    }

    /* ── Horizontal divider ── */
    OLED_DrawLine(0, 38, 127, 38, 1);

    /* ── Vertical divider ── */
    OLED_DrawLine(68, 39, 68, 63, 1);

    /* ── Steps / Reps (left column x=0..67) ── */
    {
        const char *lbl = (mode == WORKOUT_PUSHUPS) ? "Reps" : "Steps";
        OLED_DrawStr(2, 40, lbl, OLED_FONT_6x8, 1);
        char rb[12];
        snprintf(rb, sizeof(rb), "%lu", (unsigned long)reps);
        OLED_DrawStr(2, 51, rb, OLED_FONT_6x8, 1);
    }

    /* ── Heart rate (right column x=70..127) — always shown ── */
    {
        OLED_DrawBitmap(70, 40, 16, 16, MICON_HEART);
        char hb[8];
        if (hr_bpm > 0u && hr_bpm <= 250u)
            snprintf(hb, sizeof(hb), "%u", (unsigned)hr_bpm);
        else
            snprintf(hb, sizeof(hb), "---");
        OLED_DrawStr(88, 40, hb, OLED_FONT_6x8, 1);
        OLED_DrawStr(88, 50, "bpm", OLED_FONT_6x8, 1);
    }

    SH1106_Flush();
}

/* ── Workout stop-confirm overlay ─────────────────────────────────────────── */
void OLED_PageWorkoutConfirm(uint8_t cursor)
{
    SH1106_Clear();

    /* Outer dialog box (100x40 centred on 128x64) */
    OLED_FillRect(14, 12, 100, 40, 0);
    OLED_DrawRect(14, 12, 100, 40, 1);

    /* Title */
    OLED_DrawStr(27, 17, "Stop training?", OLED_FONT_6x8, 1);
    OLED_DrawLine(14, 27, 113, 27, 1);

    /* YES (left) and NO (right) buttons — highlighted = filled box, inverted text */
    if (cursor == 0u) {
        /* YES selected */
        OLED_FillRect(20, 31, 38, 14, 1);
        OLED_DrawRect(70, 31, 38, 14, 1);
        OLED_DrawStr(26, 34, "YES", OLED_FONT_6x8, 0);
        OLED_DrawStr(79, 34, "NO",  OLED_FONT_6x8, 1);
    } else {
        /* NO selected */
        OLED_DrawRect(20, 31, 38, 14, 1);
        OLED_FillRect(70, 31, 38, 14, 1);
        OLED_DrawStr(26, 34, "YES", OLED_FONT_6x8, 1);
        OLED_DrawStr(79, 34, "NO",  OLED_FONT_6x8, 0);
    }

    SH1106_Flush();
}

/* ── Stopwatch ────────────────────────────────────────────────────────────── */
void OLED_PageStopwatch(uint32_t elapsed_ms, bool running)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_STOPWATCH);
    OLED_DrawStr(19, 4, "STOPWATCH", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* Decompose */
    uint32_t cs  = (elapsed_ms / 10UL) % 100UL;   /* centiseconds */
    uint32_t ss  = (elapsed_ms / 1000UL) % 60UL;
    uint32_t mm  = (elapsed_ms / 60000UL) % 60UL;
    uint32_t hh  = elapsed_ms / 3600000UL;

    /* HH:MM:SS (2×) → 8 chars × 12 = 96px → x = 16 */
    char ts[12];
    snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu", hh, mm, ss);
    OLED_DrawStrScaled(16, 19, ts, 2, 1);

    /* .cc smaller below */
    char cs_str[4];
    snprintf(cs_str, sizeof(cs_str), ".%02lu", cs);
    OLED_DrawStr(104, 24, cs_str, OLED_FONT_6x8, 1);

    /* Running indicator */
    OLED_DrawStr(50, 37, running ? "[RUN]" : "[STP]", OLED_FONT_6x8, 1);

    SH1106_Flush();
}

/* ── Statistics (rolling trend charts) ───────────────────────────────────── */
void OLED_PageStats(const WeekStats_t *stats, uint8_t view_cursor)
{
    static const char * const VIEW_NAMES[4] = { "HR", "SpO2", "WALK", "RUN" };

    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_STATS);
    OLED_DrawStr(19, 4, "STATS", OLED_FONT_6x8, 1);
    OLED_DrawStr(58, 4, VIEW_NAMES[(view_cursor < 4u) ? view_cursor : 0u], OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* Right-side mini view selector */
    for (uint8_t y = 22u; y <= 46u; y = (uint8_t)(y + 8u)) {
        SH1106_DrawPixel(124u, y, 1u);
    }
    OLED_FillRect(122u, (uint8_t)(20u + ((view_cursor < 4u ? view_cursor : 0u) * 8u)), 4u, 4u, 1u);

    const uint8_t *series = stats->trend_hr;
    uint8_t min_v = 40u, max_v = 180u;
    const char *unit = "bpm";
    switch (view_cursor) {
        case 1u: series = stats->trend_spo2; min_v = 85u; max_v = 100u; unit = "%"; break;
        case 2u: series = stats->trend_walk; min_v = 0u;  max_v = 180u; unit = "spm"; break;
        case 3u: series = stats->trend_run;  min_v = 0u;  max_v = 180u; unit = "spm"; break;
        default: break;
    }

    uint8_t count = stats->trend_count;
    if (count > STATS_TREND_POINTS) count = STATS_TREND_POINTS;

    /* Plot area */
    const uint8_t x0 = 4u, x1 = 119u;
    const uint8_t y0 = 20u, y1 = 50u;
    const uint8_t h  = (uint8_t)(y1 - y0);

    OLED_DrawRect(x0, y0, (uint8_t)(x1 - x0 + 1u), (uint8_t)(y1 - y0 + 1u), 1u);

    uint8_t latest = 0u;
    if (count > 0u) {
        uint8_t oldest = (uint8_t)((stats->trend_head + STATS_TREND_POINTS - count) % STATS_TREND_POINTS);
        uint8_t prev_x = 0u, prev_y = 0u;
        bool prev_valid = false;

        for (uint8_t i = 0u; i < count; i++) {
            uint8_t src = (uint8_t)((oldest + i) % STATS_TREND_POINTS);
            uint8_t v = series[src];

            bool valid = true;
            if ((view_cursor == 0u || view_cursor == 1u) && v == 0u) {
                valid = false;
            }

            uint8_t x = (count > 1u)
                      ? (uint8_t)(x0 + 1u + ((uint32_t)i * (uint32_t)(x1 - x0 - 2u)) / (uint32_t)(count - 1u))
                      : (uint8_t)((x0 + x1) / 2u);

            uint8_t clamped = v;
            if (clamped < min_v) clamped = min_v;
            if (clamped > max_v) clamped = max_v;
            uint8_t y = (uint8_t)(y1 - ((uint32_t)(clamped - min_v) * h) / (uint32_t)(max_v - min_v));

            if (valid && prev_valid) {
                OLED_DrawLine(prev_x, prev_y, x, y, 1u);
            }

            prev_x = x;
            prev_y = y;
            prev_valid = valid;

            if (i == (uint8_t)(count - 1u)) {
                latest = v;
            }
        }
    }

    /* Footer: latest value + controls */
    if ((view_cursor == 0u || view_cursor == 1u) && latest == 0u) {
        OLED_Printf(2, 54, OLED_FONT_6x8, 1, "Now: -- %s", unit);
    } else {
        OLED_Printf(2, 54, OLED_FONT_6x8, 1, "Now:%3u %s", (unsigned)latest, unit);
    }
    OLED_DrawStr(66, 54, "UP/DN", OLED_FONT_6x8, 1);
    SH1106_Flush();
}

/* ── Settings ─────────────────────────────────────────────────────────────── */
void OLED_PageSettings(uint8_t cursor, bool bt_en,
                        bool raise_wake, bool fall_det, uint8_t brightness)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_SETTINGS);
    OLED_DrawStr(19, 4, "SETTINGS", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    struct { const char *label; bool toggle; bool has_bool; } items[4] = {
        { "Brightness", false, false },
        { "Bluetooth",  bt_en,  true  },
        { "RaiseWake",  raise_wake, true },
        { "FallDetect", fall_det,   true },
    };

    for (uint8_t i = 0; i < 4u; i++) {
        uint8_t y = (uint8_t)(20 + i * 11);
        bool sel = (i == cursor);
        if (sel) {
            OLED_FillRect(0, (uint8_t)(y - 1), 128, 11, 1);
            OLED_DrawStr(4, y, items[i].label, OLED_FONT_6x8, 0);
        } else {
            OLED_DrawStr(4, y, items[i].label, OLED_FONT_6x8, 1);
        }
        /* Right-side value indicator */
        if (i == 0u) {
            /* Brightness bar */
            OLED_DrawRect(86, y, 38, 7, sel ? 0u : 1u);
            uint8_t bfill = (uint8_t)((uint32_t)brightness * 36u / 255u);
            if (bfill > 0u)
                OLED_FillRect(87, (uint8_t)(y + 1), bfill, 5, sel ? 0u : 1u);
        } else {
            /* Toggle checkbox */
            OLED_DrawRect(110, (uint8_t)(y + 1), 7, 7, sel ? 0u : 1u);
            if (items[i].toggle)
                OLED_FillRect(112, (uint8_t)(y + 3), 3, 3, sel ? 0u : 1u);
        }
    }
    SH1106_Flush();
}

/* ── Connect / Pairing helper ────────────────────────────────────────────── */
void OLED_PageConnect(bool bt_enabled, bool bt_connected)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_CONNECT);
    OLED_DrawStr(19, 4, "CONNECT", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    OLED_DrawStr(0, 21, "Device:", OLED_FONT_6x8, 1);
    OLED_DrawStr(42, 21, BLE_DEVICE_NAME, OLED_FONT_6x8, 1);

    OLED_DrawStr(0, 31, "State:", OLED_FONT_6x8, 1);
    OLED_DrawStr(42, 31, bt_connected ? "LINKED" : "READY", OLED_FONT_6x8, 1);

    if (!bt_enabled) {
        OLED_DrawStr(0, 43, "Bluetooth is OFF", OLED_FONT_6x8, 1);
        OLED_DrawStr(0, 53, "Enable in Settings", OLED_FONT_6x8, 1);
    } else {
        OLED_DrawRect(0, 42, 76, 12, 1);
        OLED_DrawStr(6, 45, "Pair Device", OLED_FONT_6x8, 1);
        OLED_DrawStr(80, 45, "PIN:", OLED_FONT_6x8, 1);
        OLED_DrawStr(104, 45, BLE_PAIR_PIN, OLED_FONT_6x8, 1);
        OLED_DrawStr(0, 57, bt_connected ? "Data link active" : "Open app / send cmd", OLED_FONT_6x8, 1);
    }

    SH1106_Flush();
}

/* ── Power menu popup ─────────────────────────────────────────────────────── */
void OLED_PagePowerMenu(uint8_t cursor)
{
    /* Don't clear — overlay on top of whatever was shown before */
    /* Dim background */
    OLED_FillRect(10, 14, 108, 24, 0);
    OLED_DrawRect(10, 14, 108, 24, 1);
    OLED_DrawStr(32, 16, "Power Off?", OLED_FONT_6x8, 1);
    OLED_DrawLine(10, 26, 117, 26, 1);

    /* Option 0: Sleep */
    if (cursor == 0u) {
        OLED_FillRect(12, 27, 50, 9, 1);
        OLED_DrawStr(16, 28, "Sleep", OLED_FONT_6x8, 0);
    } else {
        OLED_DrawStr(16, 28, "Sleep", OLED_FONT_6x8, 1);
    }
    /* Option 1: Cancel */
    if (cursor == 1u) {
        OLED_FillRect(66, 27, 50, 9, 1);
        OLED_DrawStr(70, 28, "Cancel", OLED_FONT_6x8, 0);
    } else {
        OLED_DrawStr(70, 28, "Cancel", OLED_FONT_6x8, 1);
    }

    SH1106_Flush();
}

/* ── Splash screen ───────────────────────────────────────────────────────── */
void OLED_PageSplash(void)
{
    SH1106_Clear();
    OLED_DrawBitmap(56, 8, 16, 16, MICON_HEART);
    OLED_DrawStr(37, 30, "PulseMate", OLED_FONT_6x8, 1);
    OLED_DrawStr(31, 42, "Health Watch", OLED_FONT_6x8, 1);
    SH1106_Flush();
}
