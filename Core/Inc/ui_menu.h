/**
 * @file    ui_menu.h
 * @brief   UI / menu state machine for the OLED display.
 *          Driven by ButtonEvent_t items consumed from xButtonEventQueue.
 *
 * Screen hierarchy:
 *   SCREEN_MAIN
 *     → SCREEN_HEART_RATE  (UP/DOWN from main)
 *     → SCREEN_SPO2
 *     → SCREEN_STEPS
 *     → SCREEN_BT_STATUS
 *     → SCREEN_SETTINGS
 *         → SETTINGS_BRIGHTNESS
 *         → SETTINGS_BT_TOGGLE
 *         → SETTINGS_RESET_STEPS
 *         → SETTINGS_SLEEP_TIMER
 *         → SETTINGS_ABOUT
 *   OVERLAY_SLEEP_OPTIONS  (BACK long-press from any screen)
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
    /* Main screens — cycled with UP/DOWN from main */
    SCREEN_MAIN         = 0,
    SCREEN_HEART_RATE,
    SCREEN_SPO2,
    SCREEN_STEPS,
    SCREEN_BT_STATUS,
    SCREEN_SETTINGS,
    SCREEN_COUNT,

    /* Overlays (drawn on top of the current screen) */
    OVERLAY_SLEEP_OPTIONS = 0x80,
} ScreenId_t;

/* ========================================================================== *
 *  Settings sub-menu items
 * ========================================================================== */
typedef enum {
    SETTING_BRIGHTNESS  = 0,
    SETTING_BT_TOGGLE,
    SETTING_RESET_STEPS,
    SETTING_SLEEP_TIMER,
    SETTING_ABOUT,
    SETTING_COUNT,
} SettingItem_t;

/* ========================================================================== *
 *  UI state
 * ========================================================================== */
typedef struct {
    ScreenId_t   current_screen;
    ScreenId_t   overlay;           /**< Active overlay (0 = none)           */

    /* Settings cursor */
    uint8_t      settings_cursor;   /**< Highlighted item in settings menu   */

    /* Sleep options overlay cursor */
    uint8_t      sleep_cursor;      /**< 0 = "Sleep Now", 1 = "Cancel"       */

    /* User settings values */
    uint8_t      brightness;        /**< 0–100 %                            */
    bool         bt_enabled;
    uint32_t     sleep_timer_ms;    /**< 0 = never sleep                     */

    uint32_t     last_activity_tick; /**< FreeRTOS tick of last user input   */
} UIState_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise UI state to defaults.
 */
void UI_Init(void);

/**
 * @brief  Task function — uiTask body.
 *         Reads button events, updates state, re-renders OLED.
 *         Never returns.
 */
void UI_Task(void const *argument);

/**
 * @brief  Process a single button event and update UI state accordingly.
 *         Separated for unit-testing without FreeRTOS.
 */
void UI_HandleButtonEvent(const ButtonEvent_t *evt);

/**
 * @brief  Render the current screen to the OLED framebuffer and flush.
 *         Reads from gSharedData (acquires xSensorDataMutex internally).
 */
void UI_Render(void);

/**
 * @brief  Force a full redraw on the next UI_Render() call.
 */
void UI_Invalidate(void);

/**
 * @brief  Get the currently active screen ID.
 */
ScreenId_t UI_GetCurrentScreen(void);

/**
 * @brief  Programmatically navigate to a screen (e.g. from power manager).
 */
void UI_SetScreen(ScreenId_t screen);

/**
 * @brief  Show or hide the sleep options overlay.
 */
void UI_ShowSleepOverlay(bool show);

#ifdef __cplusplus
}
#endif

#endif /* __UI_MENU_H */
