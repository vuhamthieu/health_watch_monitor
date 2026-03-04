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

/* 16x16 retro shoe (step counter) */
static const uint8_t ICON_SHOE_BMP[32] = {
    0x00,0x00, 0x00,0x00, 0xF0,0x00, 0xF8,0x00,
    0xF8,0x01, 0xFC,0x03, 0xFC,0x07, 0xFE,0xFF,
    0xFF,0xFF, 0xFF,0xFF, 0xFE,0x7F, 0xFC,0x3F,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
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
    ICON_SHOE_BMP,
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
                   bool bt_enabled, bool bt_connected)
{
    (void)bt_enabled;
    (void)clk;   
    SH1106_Clear();

    /* ── Status bar (y = 0..9) ── */
    OLED_DrawBitmap(0, 0, 12, 16,
                    bt_connected ? ICON_BT_ON_BMP : ICON_BT_OFF_BMP);
    OLED_DrawBatteryIcon(109, 0,
        bat->bars,
        (bat->charge == BATT_CHARGING),
        (bat->charge == BATT_FULL));
    OLED_DrawLine(0, 10, 127, 10, 1);  /* separator */

    /* ── Isometric cube — STM32 CubeProgrammer logo style ────────────
     *  Vertices (centre-x = 64, all coords in pixels):
     *    Top    : (64, 12)
     *    TL     : (50, 19)   TR  : (78, 19)
     *    Center : (64, 26)
     *    BL     : (50, 28)   BR  : (78, 28)
     *    Bot    : (64, 35)
     *
     *  Shading:  Top face = solid white
     *            Left face = stipple (every other row)
     *            Right face = outline only (dark)
     * ────────────────────────────────────────────────────────────── */

    /* 1 — Fill top diamond face (solid) ─────────────────────────── */
    /*     y=12..19: half-width expands 0→14 (2px per row)           */
    /*     y=19..26: half-width contracts 14→0                       */
    for (uint8_t fy = 12u; fy <= 26u; fy++) {
        uint8_t hw = (fy <= 19u) ? (uint8_t)(2u * (fy  - 12u))
                                 : (uint8_t)(2u * (26u - fy));
        if (hw > 0u)
            OLED_DrawLine((uint8_t)(64u - hw), fy, (uint8_t)(64u + hw), fy, 1);
        else
            SH1106_DrawPixel(64, fy, 1);   /* apex (y=12) and nadir (y=26) */
    }

    /* 2 — Fill left face (stipple: even rows only) ──────────────── */
    /*     xl: TL→BL left edge (x=50, y=19..28) then BL→Bot ramp    */
    /*     xr: TL→Center diagonal (y=19..26) then Center→Bot (x=64) */
    for (uint8_t fy = 20u; fy <= 34u; fy += 2u) {
        uint8_t xl = (fy <= 28u) ? 50u : (uint8_t)(50u + 2u * (fy - 28u));
        uint8_t xr = (fy <= 26u) ? (uint8_t)(50u + 2u * (fy - 19u)) : 64u;
        if (xl < xr)
            OLED_DrawLine(xl, fy, (uint8_t)(xr - 1u), fy, 1);
    }

    /* 3 — Cube outline (9 edges) ────────────────────────────────── */
    OLED_DrawLine(64, 12, 78, 19, 1);   /* top    → TR      */
    OLED_DrawLine(78, 19, 64, 26, 1);   /* TR     → center  */
    OLED_DrawLine(64, 26, 50, 19, 1);   /* center → TL      */
    OLED_DrawLine(50, 19, 64, 12, 1);   /* TL     → top     */
    OLED_DrawLine(50, 19, 50, 28, 1);   /* TL     → BL      */
    OLED_DrawLine(50, 28, 64, 35, 1);   /* BL     → bot     */
    OLED_DrawLine(64, 35, 78, 28, 1);   /* bot    → BR      */
    OLED_DrawLine(78, 28, 78, 19, 1);   /* BR     → TR      */
    OLED_DrawLine(64, 26, 64, 35, 1);   /* center → bot     */

    /* ── "STM32" — 2× scale, centred (5 × 12 = 60 px → x = 34) ── */
    OLED_DrawStrScaled(34, 39, "STM32", 2, 1);

    /* ── "CubeProgrammer" — 1×, centred (14 × 6 = 84 px → x = 22) ─ */
    OLED_DrawStr(22, 56, "CubeProgrammer", OLED_FONT_6x8, 1);

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
};

static const char * const MENU_LABELS[MENU_ITEM_COUNT] = {
    "Heart Rate",
    "SpO2",
    "Workout",
    "Stopwatch",
    "Statistics",
    "Settings",
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
void OLED_PageHRMeasure(MeasPhase_t phase, uint8_t progress,
                        uint8_t anim_frame, uint16_t bpm_result)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_HEART);
    OLED_DrawStr(19, 4, "HEART RATE", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    if (phase == MEAS_IDLE) {
        OLED_DrawStr(16, 28, "Place finger", OLED_FONT_6x8, 1);
        OLED_DrawStr(22, 38, "on sensor", OLED_FONT_6x8, 1);
        OLED_DrawStr(8, 54, "SELECT to start", OLED_FONT_6x8, 1);
    } else if (phase == MEAS_MEASURING) {
        /* Circular progress ring r=20, centre (64,36) */
        OLED_DrawArc(64, 36, 22, progress, 1);
        /* Beating heart at centre — 24×24 at (52,24) */
        if (anim_frame == 0u) {
            OLED_DrawBitmap(52, 24, 24, 24, ICON_HEART_BIG_BMP);
        } else {
            OLED_DrawBitmap(52, 24, 24, 24, ICON_HEART_BIG2_BMP);
        }
        OLED_Printf(46, 56, OLED_FONT_6x8, 1, "%3d%%", progress);
    } else { /* MEAS_DONE */
        /* Large BPM result */
        OLED_DrawBitmap(52, 12, 24, 24, ICON_HEART_BIG_BMP);
        if (bpm_result > 0u && bpm_result <= 250u)
            OLED_Printf(26, 40, OLED_FONT_6x8, 1, "%3d BPM", bpm_result);
        else
            OLED_DrawStr(26, 40, "--- BPM", OLED_FONT_6x8, 1);
        OLED_DrawStr(22, 54, "BACK to exit", OLED_FONT_6x8, 1);
    }
    SH1106_Flush();
}

/* ── SpO2 Measure ─────────────────────────────────────────────────────────── */
void OLED_PageSpO2Measure(MeasPhase_t phase, uint8_t progress,
                           uint8_t spo2_result)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_SPO2);
    OLED_DrawStr(19, 4, "SpO2", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    if (phase == MEAS_IDLE) {
        OLED_DrawStr(16, 28, "Place finger", OLED_FONT_6x8, 1);
        OLED_DrawStr(22, 38, "on sensor", OLED_FONT_6x8, 1);
        OLED_DrawStr(8, 54, "SELECT to start", OLED_FONT_6x8, 1);
    } else if (phase == MEAS_MEASURING) {
        OLED_DrawArc(64, 36, 22, progress, 1);
        OLED_DrawStrScaled(50, 28, "O2", 2, 1);
        OLED_Printf(46, 56, OLED_FONT_6x8, 1, "%3d%%", progress);
    } else {
        if (spo2_result > 0u)
            OLED_Printf(20, 28, OLED_FONT_6x8, 1, "SpO2: %3d%%", spo2_result);
        else
            OLED_DrawStr(20, 28, "SpO2: ---%", OLED_FONT_6x8, 1);
        if (spo2_result > 0u && spo2_result < 95u)
            OLED_DrawStr(20, 40, "!LOW!", OLED_FONT_6x8, 1);
        OLED_DrawStr(22, 54, "BACK to exit", OLED_FONT_6x8, 1);
    }
    SH1106_Flush();
}

/* ── Workout ──────────────────────────────────────────────────────────────── */
void OLED_PageWorkout(WorkoutMode_t mode, bool active,
                      uint32_t reps, uint32_t elapsed_s)
{
    static const char * const MODE_NAMES[] = {
        "Walking", "Running", "Push-ups"
    };
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_WORKOUT);
    OLED_DrawStr(19, 4, "WORKOUT", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* Mode name */
    uint8_t m = (mode < WORKOUT_PUSHUPS) ? (uint8_t)mode : (uint8_t)WORKOUT_PUSHUPS;
    OLED_DrawStr(0, 19, MODE_NAMES[m], OLED_FONT_6x8, 1);

    /* Status */
    OLED_DrawStr(90, 19, active ? "  RUN" : " STOP", OLED_FONT_6x8, 1);

    /* Reps / steps */
    if (mode == WORKOUT_PUSHUPS)
        OLED_Printf(0, 30, OLED_FONT_6x8, 1, "Reps: %lu", (unsigned long)reps);
    else
        OLED_Printf(0, 30, OLED_FONT_6x8, 1, "Steps:%lu", (unsigned long)reps);

    /* Elapsed HH:MM:SS */
    uint32_t hh = elapsed_s / 3600UL;
    uint32_t mm = (elapsed_s % 3600UL) / 60UL;
    uint32_t ss = elapsed_s % 60UL;
    OLED_Printf(0, 41, OLED_FONT_6x8, 1, "Time:%02lu:%02lu:%02lu", hh, mm, ss);

    /* Hints */
    OLED_DrawLine(0, 50, 127, 50, 1);
    OLED_DrawStr(0,  53, "UP=mode", OLED_FONT_6x8, 1);
    OLED_DrawStr(72, 53, "SEL=start", OLED_FONT_6x8, 1);
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

    /* Button hints */
    OLED_DrawLine(0, 48, 127, 48, 1);
    OLED_DrawStr(0,  51, "UP=start/stop", OLED_FONT_6x8, 1);
    OLED_DrawStr(0,  58, "DOWN=reset", OLED_FONT_6x8, 1);
    SH1106_Flush();
}

/* ── Statistics ────────────────────────────────────────────────────────────── */
void OLED_PageStats(const WeekStats_t *stats)
{
    SH1106_Clear();
    OLED_DrawBitmap(0, 0, 16, 16, MICON_STATS);
    OLED_DrawStr(19, 4, "STATS (7d)", OLED_FONT_6x8, 1);
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* Find max steps for scaling */
    uint32_t max_steps = 1UL;
    for (uint8_t d = 0; d < STATS_DAYS; d++) {
        if (stats->daily_steps[d] > max_steps)
            max_steps = stats->daily_steps[d];
    }

    /* 7 bars, each 14px wide with 4px gap → total = 7*14+6*4 = 122 → x_start=3 */
    static const char * const DAY_ABBR[7] = {
        "Mo","Tu","We","Th","Fr","Sa","Su"
    };
    uint8_t max_bar_h = 30u; /* pixels available for bars: y=18 to y=48 */
    for (uint8_t d = 0; d < STATS_DAYS; d++) {
        uint8_t bar_h = (uint8_t)(
            ((uint32_t)stats->daily_steps[d] * max_bar_h) / max_steps);
        uint8_t bx  = (uint8_t)(3 + d * 18);
        uint8_t by  = (uint8_t)(18 + max_bar_h - bar_h);
        /* Highlight today */
        bool is_today = (d == stats->day_index);
        if (bar_h > 0u)
            OLED_FillRect(bx, by, 14, bar_h, 1);
        if (is_today)
            OLED_DrawRect(bx, (uint8_t)(18), 14, max_bar_h, 1); /* outline today */
        OLED_DrawStr(bx, 51, DAY_ABBR[d], OLED_FONT_6x8, 1);
    }
    /* Baseline */
    OLED_DrawLine(0, 49, 127, 49, 1);
    /* Today avg HR */
    if (stats->daily_hr_avg[stats->day_index] > 0u) {
        OLED_Printf(0, 57, OLED_FONT_6x8, 1, "HR avg:%3d",
                    (int)stats->daily_hr_avg[stats->day_index]);
    }
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

/* ── Power menu popup ─────────────────────────────────────────────────────── */
void OLED_PagePowerMenu(uint8_t cursor)
{
    /* Don't clear — overlay on top of whatever was shown before */
    /* Dim background */
    OLED_FillRect(10, 14, 108, 36, 0);
    OLED_DrawRect(10, 14, 108, 36, 1);
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

    /* Hint row */
    OLED_DrawLine(10, 38, 117, 38, 1);
    OLED_DrawStr(16, 40, "UP/DWN=nav SEL=ok", OLED_FONT_6x8, 1);

    SH1106_Flush();
}

/* ── Splash screen ───────────────────────────────────────────────────────── */
void OLED_PageSplash(void)
{
    SH1106_Clear();
    /* "HealthWatch" 11 chars × 12 = 132 > 128 → use 1× (66px) centred */
    OLED_DrawStr(31, 20, "HealthWatch", OLED_FONT_6x8, 1);
    OLED_DrawStr(52, 32, "v2.0", OLED_FONT_6x8, 1);
    /* Small heart icon */
    OLED_DrawBitmap(56, 42, 16, 16, ICON_HEART_SMALL_BMP);
    SH1106_Flush();
}
