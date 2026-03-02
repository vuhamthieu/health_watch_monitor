/* ui_menu.c — screen navigation state machine + OLED render dispatch
 *
 * Screens:
 *   SCREEN_HOME        – retro homescreen (clock / shoe / heart)
 *   SCREEN_MENU        – scrollable list of 7 apps
 *   SCREEN_HR_MEASURE  – beating heart + circular progress ring
 *   SCREEN_SPO2_MEASURE
 *   SCREEN_EEG         – placeholder
 *   SCREEN_WORKOUT     – walk / run / push-up tracker
 *   SCREEN_STOPWATCH   – centisecond stopwatch
 *   SCREEN_STATS       – 7-day step bar chart
 *   SCREEN_SETTINGS    – brightness / BT / raise-to-wake / fall detect
 *   SCREEN_POWER_MENU  – sleep or cancel (BACK 5 s from home)
 */

#include "ui_menu.h"
#include "oled.h"
#include "sensor_data.h"
#include "power_manager.h"
#include "app_config.h"
#include "cmsis_os.h"

/* ── private state ──────────────────────────────────────────────────────── */
static UIState_t s_ui;
static bool      s_dirty = true;

/* Measurement timer period: 300 ms per progress tick → 30 s total */
#define MEAS_TICK_MS        300u
#define MEAS_TOTAL_TICKS    100u

/* Heart animation: toggle frame every 400 ms */
#define HEART_ANIM_MS       400u

/* ========================================================================== *
 *  Init
 * ========================================================================== */
void UI_Init(void)
{
    s_ui.screen             = SCREEN_HOME;
    s_ui.prev_screen        = SCREEN_HOME;
    s_ui.menu_cursor        = 0;
    s_ui.settings_cursor    = 0;
    s_ui.meas_phase         = MEAS_IDLE;
    s_ui.meas_progress      = 0;
    s_ui.meas_start_tick    = 0;
    s_ui.meas_bpm_result    = 0;
    s_ui.meas_spo2_result   = 0;
    s_ui.heart_anim_frame   = 0;
    s_ui.heart_anim_tick    = 0;
    s_ui.workout_mode       = WORKOUT_WALKING;
    s_ui.workout_active     = false;
    s_ui.workout_reps       = 0;
    s_ui.workout_start_tick = 0;
    s_ui.brightness         = 128u;  /* mid-level */
    s_ui.bt_enabled         = true;
    s_ui.raise_to_wake      = false;
    s_ui.fall_detect        = false;
    s_ui.power_cursor       = 1u;    /* default "Cancel" selected */
    s_ui.last_activity_tick = osKernelSysTick();
    s_dirty = true;
}

/* ========================================================================== *
 *  Task entry point
 * ========================================================================== */
void UI_Task(void const *argument)
{
    (void)argument;
    UI_Init();
    OLED_Init();
    OLED_PageSplash();
    osDelay(1500u);

    for (;;)
    {
        osEvent evt = osMessageGet(xButtonEventQueue, UI_REFRESH_PERIOD_MS);

        if (evt.status == osEventMessage) {
            ButtonEvent_t btn;
            /* Queue sends ButtonEvent_t packed as uint32_t value */
            btn = *(const ButtonEvent_t *)(uintptr_t)&evt.value.v;
            UI_HandleButtonEvent(&btn);
            s_dirty = true;
        }

        /* ── Periodic animations ── */
        uint32_t now = osKernelSysTick();

        /* Heartbeat animation while measuring */
        if (s_ui.meas_phase == MEAS_MEASURING) {
            if ((now - s_ui.heart_anim_tick) >= HEART_ANIM_MS) {
                s_ui.heart_anim_frame ^= 1u;
                s_ui.heart_anim_tick   = now;
                s_dirty = true;
            }
            /* Advance progress ring */
            if ((now - s_ui.meas_start_tick) >=
                (uint32_t)s_ui.meas_progress * MEAS_TICK_MS) {
                if (s_ui.meas_progress < 100u) {
                    s_ui.meas_progress++;
                    s_dirty = true;
                } else {
                    /* Measurement "done" — grab latest reading */
                    HeartData_t hd;
                    Sensor_Data_GetHeart(&hd);
                    s_ui.meas_phase     = MEAS_DONE;
                    s_ui.meas_bpm_result = (hd.hr_status == SENSOR_OK) ?
                                           hd.bpm : 0u;
                    s_ui.meas_spo2_result = (hd.spo2_status == SENSOR_OK) ?
                                            hd.spo2 : 0u;
                    s_dirty = true;
                }
            }
        }

        /* Stopwatch tick */
        if (gSharedData.stopwatch.running) {
            s_dirty = true;  /* force redraw every cycle while running */
        }

        if (s_dirty) {
            UI_Render();
            s_dirty = false;
        }
    }
}

/* ========================================================================== *
 *  Helper — go back to previous screen
 * ========================================================================== */
static void go_back(void)
{
    s_ui.screen = s_ui.prev_screen;
    /* Reset measurement state when leaving those screens */
    s_ui.meas_phase    = MEAS_IDLE;
    s_ui.meas_progress = 0;
}

/* ========================================================================== *
 *  Button event handler
 * ========================================================================== */
void UI_HandleButtonEvent(const ButtonEvent_t *evt)
{
    if (!evt) return;

    s_ui.last_activity_tick = osKernelSysTick();
    Power_NotifyActivity();

    /* ── Power menu intercepts all input ─── */
    if (s_ui.screen == SCREEN_POWER_MENU) {
        if (evt->type == BTN_EVT_PRESS) {
            switch (evt->id) {
                case BTN_ID_UP:
                case BTN_ID_DOWN:
                    s_ui.power_cursor = (s_ui.power_cursor == 0u) ? 1u : 0u;
                    break;
                case BTN_ID_SELECT:
                    if (s_ui.power_cursor == 0u) {
                        Power_PostEvent(POWER_EVT_SLEEP_NOW, false);
                    } else {
                        s_ui.screen = s_ui.prev_screen;
                    }
                    break;
                case BTN_ID_BACK:
                    s_ui.screen = s_ui.prev_screen;
                    break;
                default: break;
            }
        }
        return;
    }

    /* ── BACK long press (5 s) from home → power menu ─── */
    if (evt->id == BTN_ID_BACK && evt->type == BTN_EVT_LONG_PRESS) {
        s_ui.prev_screen  = s_ui.screen;
        s_ui.screen       = SCREEN_POWER_MENU;
        s_ui.power_cursor = 1u; /* default Cancel */
        return;
    }

    /* ── Screen-specific handling ─── */
    switch (s_ui.screen)
    {
        /* ── Home ─────────────────────────────────────────────────── */
        case SCREEN_HOME:
            if (evt->type == BTN_EVT_PRESS && evt->id == BTN_ID_SELECT) {
                s_ui.prev_screen = SCREEN_HOME;
                s_ui.screen      = SCREEN_MENU;
                s_ui.menu_cursor = 0;
            }
            break;

        /* ── Menu ─────────────────────────────────────────────────── */
        case SCREEN_MENU:
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    if (s_ui.menu_cursor > 0u) s_ui.menu_cursor--;
                } else if (evt->id == BTN_ID_DOWN) {
                    if (s_ui.menu_cursor < MENU_ITEM_COUNT - 1u) s_ui.menu_cursor++;
                } else if (evt->id == BTN_ID_SELECT) {
                    /* Map cursor → screen */
                    static const ScreenId_t SCREEN_MAP[MENU_ITEM_COUNT] = {
                        SCREEN_HR_MEASURE,
                        SCREEN_SPO2_MEASURE,
                        SCREEN_EEG,
                        SCREEN_WORKOUT,
                        SCREEN_STOPWATCH,
                        SCREEN_STATS,
                        SCREEN_SETTINGS,
                    };
                    s_ui.prev_screen = SCREEN_MENU;
                    s_ui.screen      = SCREEN_MAP[s_ui.menu_cursor];
                    /* Reset measurement state on entry */
                    if (s_ui.screen == SCREEN_HR_MEASURE ||
                        s_ui.screen == SCREEN_SPO2_MEASURE) {
                        s_ui.meas_phase    = MEAS_IDLE;
                        s_ui.meas_progress = 0;
                    }
                } else if (evt->id == BTN_ID_BACK && evt->type == BTN_EVT_PRESS) {
                    s_ui.screen = SCREEN_HOME;
                }
            }
            break;

        /* ── HR Measure ───────────────────────────────────────────── */
        case SCREEN_HR_MEASURE:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_SELECT && s_ui.meas_phase == MEAS_IDLE) {
                    s_ui.meas_phase      = MEAS_MEASURING;
                    s_ui.meas_progress   = 0;
                    s_ui.meas_start_tick = osKernelSysTick();
                    s_ui.heart_anim_tick = s_ui.meas_start_tick;
                    s_ui.heart_anim_frame = 0;
                } else if (evt->id == BTN_ID_BACK) {
                    go_back();
                }
            }
            break;

        /* ── SpO2 Measure ─────────────────────────────────────────── */
        case SCREEN_SPO2_MEASURE:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_SELECT && s_ui.meas_phase == MEAS_IDLE) {
                    s_ui.meas_phase      = MEAS_MEASURING;
                    s_ui.meas_progress   = 0;
                    s_ui.meas_start_tick = osKernelSysTick();
                } else if (evt->id == BTN_ID_BACK) {
                    go_back();
                }
            }
            break;

        /* ── EEG placeholder ──────────────────────────────────────── */
        case SCREEN_EEG:
            if (evt->type == BTN_EVT_PRESS && evt->id == BTN_ID_BACK)
                go_back();
            break;

        /* ── Workout ──────────────────────────────────────────────── */
        case SCREEN_WORKOUT:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_UP) {
                    /* Cycle mode only when stopped */
                    if (!s_ui.workout_active) {
                        s_ui.workout_mode = (WorkoutMode_t)(
                            ((uint8_t)s_ui.workout_mode + 1u) % (uint8_t)WORKOUT_COUNT);
                        s_ui.workout_reps = 0;
                    }
                } else if (evt->id == BTN_ID_SELECT) {
                    if (!s_ui.workout_active) {
                        s_ui.workout_active     = true;
                        s_ui.workout_reps       = 0;
                        s_ui.workout_start_tick = osKernelSysTick();
                    } else {
                        s_ui.workout_active = false;
                    }
                } else if (evt->id == BTN_ID_BACK) {
                    s_ui.workout_active = false;
                    go_back();
                }
            }
            break;

        /* ── Stopwatch ────────────────────────────────────────────── */
        case SCREEN_STOPWATCH:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_UP) {
                    /* Start / Stop */
                    osMutexWait(xSensorDataMutex, osWaitForever);
                    if (!gSharedData.stopwatch.running) {
                        gSharedData.stopwatch.running    = true;
                        gSharedData.stopwatch.start_tick =
                            osKernelSysTick() - gSharedData.stopwatch.elapsed_ms;
                    } else {
                        uint32_t now2 = osKernelSysTick();
                        gSharedData.stopwatch.elapsed_ms =
                            now2 - gSharedData.stopwatch.start_tick;
                        gSharedData.stopwatch.running = false;
                    }
                    osMutexRelease(xSensorDataMutex);
                } else if (evt->id == BTN_ID_DOWN) {
                    /* Reset */
                    osMutexWait(xSensorDataMutex, osWaitForever);
                    gSharedData.stopwatch.running    = false;
                    gSharedData.stopwatch.elapsed_ms = 0;
                    gSharedData.stopwatch.start_tick = 0;
                    osMutexRelease(xSensorDataMutex);
                } else if (evt->id == BTN_ID_BACK) {
                    /* Stop and go back */
                    osMutexWait(xSensorDataMutex, osWaitForever);
                    if (gSharedData.stopwatch.running) {
                        gSharedData.stopwatch.elapsed_ms =
                            osKernelSysTick() - gSharedData.stopwatch.start_tick;
                        gSharedData.stopwatch.running = false;
                    }
                    osMutexRelease(xSensorDataMutex);
                    go_back();
                }
            }
            break;

        /* ── Stats ────────────────────────────────────────────────── */
        case SCREEN_STATS:
            if (evt->type == BTN_EVT_PRESS && evt->id == BTN_ID_BACK)
                go_back();
            break;

        /* ── Settings ─────────────────────────────────────────────── */
        case SCREEN_SETTINGS:
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    if (s_ui.settings_cursor > 0u) s_ui.settings_cursor--;
                } else if (evt->id == BTN_ID_DOWN) {
                    if (s_ui.settings_cursor < SETTING_COUNT - 1u)
                        s_ui.settings_cursor++;
                } else if (evt->id == BTN_ID_SELECT) {
                    switch (s_ui.settings_cursor) {
                        case SETTING_BRIGHTNESS:
                            s_ui.brightness = (s_ui.brightness >= 200u) ?
                                               50u : (uint8_t)(s_ui.brightness + 50u);
                            /* TODO: apply SH1106 contrast command */
                            break;
                        case SETTING_BLUETOOTH:
                            s_ui.bt_enabled = !s_ui.bt_enabled;
                            break;
                        case SETTING_RAISE_TO_WAKE:
                            s_ui.raise_to_wake = !s_ui.raise_to_wake;
                            break;
                        case SETTING_FALL_DETECT:
                            s_ui.fall_detect = !s_ui.fall_detect;
                            break;
                        default: break;
                    }
                } else if (evt->id == BTN_ID_BACK) {
                    go_back();
                }
            }
            break;

        default: break;
    }
}

/* ========================================================================== *
 *  Render dispatch
 * ========================================================================== */
void UI_Render(void)
{
    /* Snapshot shared data under mutex */
    HeartData_t  heart;
    MotionData_t motion;
    SoftClock_t  clk;
    BatteryStatus_t bat;
    WeekStats_t  stats;
    StopwatchData_t sw;

    Sensor_Data_GetHeart(&heart);
    Sensor_Data_GetMotion(&motion);
    clk = Sensor_Data_GetClock();

    osMutexWait(xSensorDataMutex, osWaitForever);
    bat   = gSharedData.battery;
    stats = gSharedData.stats;
    sw    = gSharedData.stopwatch;
    /* If stopwatch is running, compute live elapsed */
    if (sw.running) {
        sw.elapsed_ms = osKernelSysTick() - sw.start_tick;
    }
    osMutexRelease(xSensorDataMutex);

    switch (s_ui.screen)
    {
        case SCREEN_HOME:
            OLED_PageHome(
                &clk, &bat,
                s_ui.bt_enabled, gSharedData.ble.connected);
            break;

        case SCREEN_MENU:
        {
            uint8_t offset = 0u;
            if (s_ui.menu_cursor == 0u) {
                offset = 0u;
            } else if (s_ui.menu_cursor >= (MENU_ITEM_COUNT - 1u)) {
                offset = (uint8_t)(MENU_ITEM_COUNT - 3u);
            } else {
                offset = (uint8_t)(s_ui.menu_cursor - 1u);
            }
            OLED_PageMenu(s_ui.menu_cursor, offset);
            break;
        }

        case SCREEN_HR_MEASURE:
            OLED_PageHRMeasure(
                s_ui.meas_phase,
                s_ui.meas_progress,
                s_ui.heart_anim_frame,
                s_ui.meas_bpm_result);
            break;

        case SCREEN_SPO2_MEASURE:
            OLED_PageSpO2Measure(
                s_ui.meas_phase,
                s_ui.meas_progress,
                s_ui.meas_spo2_result);
            break;

        case SCREEN_EEG:
            OLED_PageEEG();
            break;

        case SCREEN_WORKOUT:
        {
            uint32_t elapsed_s = 0u;
            if (s_ui.workout_active) {
                elapsed_s = (osKernelSysTick() - s_ui.workout_start_tick) / 1000u;
            }
            /* For walking/running use step count; push-ups use rep counter */
            uint32_t reps = (s_ui.workout_mode == WORKOUT_PUSHUPS) ?
                             s_ui.workout_reps : motion.steps;
            OLED_PageWorkout(s_ui.workout_mode, s_ui.workout_active,
                             reps, elapsed_s);
            break;
        }

        case SCREEN_STOPWATCH:
            OLED_PageStopwatch(sw.elapsed_ms, sw.running);
            break;

        case SCREEN_STATS:
            OLED_PageStats(&stats);
            break;

        case SCREEN_SETTINGS:
            OLED_PageSettings(
                s_ui.settings_cursor,
                s_ui.bt_enabled,
                s_ui.raise_to_wake,
                s_ui.fall_detect,
                s_ui.brightness);
            break;

        case SCREEN_POWER_MENU:
            /* Render underlying screen first, then overlay */
            UI_SetScreen(s_ui.prev_screen);
            UI_Render();
            UI_SetScreen(SCREEN_POWER_MENU);
            OLED_PagePowerMenu(s_ui.power_cursor);
            return; /* OLED_PagePowerMenu already calls SH1106_Flush */

        default: break;
    }
}

/* ========================================================================== *
 *  Misc API
 * ========================================================================== */
void       UI_Invalidate(void)            { s_dirty = true; }
ScreenId_t UI_GetCurrentScreen(void)      { return s_ui.screen; }
void       UI_SetScreen(ScreenId_t s)     { s_ui.screen = s; s_dirty = true; }

/* Legacy compat — power_manager may call this */
void UI_ShowSleepOverlay(bool show)
{
    if (show) {
        s_ui.prev_screen  = s_ui.screen;
        s_ui.screen       = SCREEN_POWER_MENU;
        s_ui.power_cursor = 1u;
    } else {
        s_ui.screen = s_ui.prev_screen;
    }
    s_dirty = true;
}
