/* ui_menu.h - screen IDs, UI state machine, and API
 *
 * Screen hierarchy:
 *   SCREEN_HOME
 *     → SELECT → SCREEN_MENU (scrollable list)
 *         [0] Heart Rate     → SCREEN_HR_MEASURE
 *         [1] SpO2           → SCREEN_SPO2_MEASURE
 *         [2] Workout        → SCREEN_WORKOUT
 *         [3] Stopwatch      → SCREEN_STOPWATCH
 *         [4] Statistics     → SCREEN_STATS
 *         [5] Settings       → SCREEN_SETTINGS
 *     BACK (5 s on home)     → SCREEN_POWER_MENU
 */

#ifndef __UI_MENU_H
#define __UI_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "button.h"
#include "sensor_data.h"

/* ========================================================================== *
 *  Screen IDs
 * ========================================================================== */
typedef enum {
    SCREEN_HOME = 0,        /* retro homescreen                              */
    SCREEN_MENU,            /* main scrollable menu                          */
    SCREEN_HR_MEASURE,      /* beating heart + progress ring                 */
    SCREEN_SPO2_MEASURE,    /* SpO2 measurement                              */
    SCREEN_WORKOUT,         /* walk / run / push-up mode                     */
    SCREEN_STOPWATCH,       /* stopwatch                                     */
    SCREEN_STATS,           /* 7-day bar chart                               */
    SCREEN_SETTINGS,        /* settings list                                 */
    SCREEN_POWER_MENU,      /* sleep / cancel (BACK 5 s on home)             */
    SCREEN_COUNT,
} ScreenId_t;

/* ========================================================================== *
 *  Menu / settings items
 * ========================================================================== */
#define MENU_ITEM_COUNT     6u

typedef enum {
    SETTING_BRIGHTNESS = 0,
    SETTING_BLUETOOTH,
    SETTING_RAISE_TO_WAKE,
    SETTING_FALL_DETECT,
    SETTING_COUNT,
} SettingItem_t;

typedef enum {
    WORKOUT_WALKING = 0,
    WORKOUT_RUNNING,
    WORKOUT_PUSHUPS,
    WORKOUT_COUNT,
} WorkoutMode_t;

/* ========================================================================== *
 *  Measurement phase (used for HR & SpO2 screens)
 * ========================================================================== */
typedef enum {
    MEAS_IDLE = 0,
    MEAS_MEASURING,         /* progress ring filling                         */
    MEAS_DONE,              /* result is ready, show it                      */
} MeasPhase_t;

/* ========================================================================== *
 *  UI state (single global, owned by uiTask)
 * ========================================================================== */
typedef struct {
    ScreenId_t    screen;
    ScreenId_t    prev_screen;       /* for BACK navigation                   */

    /* Menu / settings scrolling */
    uint8_t       menu_cursor;       /* highlighted row in SCREEN_MENU        */
    uint8_t       settings_cursor;   /* highlighted row in SCREEN_SETTINGS    */

    /* HR / SpO2 measurement state */
    MeasPhase_t   meas_phase;
    uint8_t       meas_progress;     /* 0-100: arc fill %                     */
    uint32_t      meas_start_tick;
    uint16_t      meas_bpm_result;
    uint8_t       meas_spo2_result;
    uint8_t       heart_anim_frame;  /* 0 or 1 for beat animation             */
    uint32_t      heart_anim_tick;

    /* Workout */
    WorkoutMode_t workout_mode;
    bool          workout_active;
    uint32_t      workout_reps;      /* steps or push-up reps                 */
    uint32_t      workout_start_tick;

    /* Settings values */
    uint8_t       brightness;        /* 0-100                                 */
    bool          bt_enabled;
    bool          raise_to_wake;
    bool          fall_detect;

    /* Power menu cursor */
    uint8_t       power_cursor;      /* 0=Sleep 1=Cancel                      */

    /* Last user input tick */
    uint32_t      last_activity_tick;
} UIState_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */
void UI_Init(void);
void UI_Task(void const *argument);
void UI_HandleButtonEvent(const ButtonEvent_t *evt);
void UI_Render(void);
void UI_Invalidate(void);

ScreenId_t UI_GetCurrentScreen(void);
void       UI_SetScreen(ScreenId_t screen);
void       UI_ShowSleepOverlay(bool show);   /* power_manager compatibility   */

#ifdef __cplusplus
}
#endif

#endif /* __UI_MENU_H */
