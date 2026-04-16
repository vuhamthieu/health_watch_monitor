/* ui_menu.c — screen navigation state machine + OLED render dispatch
 *
 * Screens:
 *   SCREEN_HOME        – retro homescreen (clock / shoe / heart)
 *   SCREEN_MENU        – scrollable list of 6 apps
 *   SCREEN_HR_MEASURE  – beating heart + circular progress ring
 *   SCREEN_SPO2_MEASURE
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
#include "jdy31.h"
#include <stdio.h>

/* ── private state ──────────────────────────────────────────────────────── */
static UIState_t s_ui;
static bool      s_dirty = true;
static uint32_t  s_btn_latency_sum_ms = 0u;
static uint32_t  s_btn_latency_samples = 0u;
static uint32_t  s_btn_latency_max_ms = 0u;
static UBaseType_t s_btn_q_peak = 0u;

/* Measurement timer period: 300 ms per progress tick → 30 s total */
#define MEAS_TICK_MS        300u
#define MEAS_TOTAL_TICKS    100u

/* Heart animation: toggle frame every 400 ms */
#define HEART_ANIM_MS       400u

/* Fall alert overlay visibility duration */
#define FALL_ALERT_SHOW_MS  8000u

static uint32_t s_fall_alert_until_tick = 0u;

/* ========================================================================== *
 *  Init
 * ========================================================================== */
void UI_Init(void)
{
    s_ui.screen             = SCREEN_HOME;
    s_ui.prev_screen        = SCREEN_HOME;
    s_ui.menu_cursor        = 0;
    s_ui.settings_cursor    = 0;
    s_ui.stats_cursor       = 0;
    s_ui.meas_phase         = MEAS_IDLE;
    s_ui.meas_progress      = 0;
    s_ui.meas_start_tick    = 0;
    s_ui.meas_bpm_result    = 0;
    s_ui.meas_spo2_result   = 0;
    s_ui.home_last_bpm      = 0;
    s_ui.home_last_spo2     = 0;
    s_ui.heart_anim_frame   = 0;
    s_ui.heart_anim_tick    = 0;
    s_ui.workout_mode       = WORKOUT_WALKING;
    s_ui.workout_cursor      = 0u;
    s_ui.workout_confirm_cursor = 1u;  /* default NO */
    s_ui.workout_active     = false;
    s_ui.workout_reps       = 0;
    s_ui.workout_start_tick = 0;
    s_ui.brightness         = 128u;  /* mid-level */
    s_ui.bt_enabled         = gSharedData.settings.bluetooth_enabled;
    s_ui.raise_to_wake      = gSharedData.settings.raise_to_wake;
    s_ui.fall_detect        = gSharedData.settings.fall_detect;
    s_ui.power_cursor       = 1u;    /* default "Cancel" selected */
    s_ui.last_activity_tick = osKernelSysTick();
    s_dirty = true;
    s_btn_latency_sum_ms = 0u;
    s_btn_latency_samples = 0u;
    s_btn_latency_max_ms = 0u;
    s_btn_q_peak = 0u;
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
#if APP_ENABLE_UART_DEBUG 
    uint32_t last_perf_log_tick = osKernelSysTick();
#endif

    for (;;)
    {
        ButtonEvent_t btn;
        if (xQueueReceive(xButtonEventQueue, &btn,
                          pdMS_TO_TICKS(UI_REFRESH_PERIOD_MS)) == pdTRUE) {
            uint32_t now_tick = osKernelSysTick();
            uint32_t latency_ms = now_tick - btn.tick;
            s_btn_latency_sum_ms += latency_ms;
            s_btn_latency_samples++;
            if (latency_ms > s_btn_latency_max_ms) {
                s_btn_latency_max_ms = latency_ms;
            }

            UBaseType_t q_cur = uxQueueMessagesWaiting(xButtonEventQueue);
            if (q_cur > s_btn_q_peak) {
                s_btn_q_peak = q_cur;
            }

            UI_HandleButtonEvent(&btn);
            s_dirty = true;
        }

#if APP_ENABLE_UART_DEBUG
        uint32_t now_perf = osKernelSysTick();
        if ((now_perf - last_perf_log_tick) >= 10000u) {
            UBaseType_t q_cur = uxQueueMessagesWaiting(xButtonEventQueue);
            uint32_t avg_ms = (s_btn_latency_samples > 0u)
                            ? (s_btn_latency_sum_ms / s_btn_latency_samples)
                            : 0u;
            printf("[UIQ] lat_avg=%lums lat_max=%lums q_cur=%lu q_peak=%lu samples=%lu\r\n",
                   (unsigned long)avg_ms,
                   (unsigned long)s_btn_latency_max_ms,
                   (unsigned long)q_cur,
                   (unsigned long)s_btn_q_peak,
                   (unsigned long)s_btn_latency_samples);
            last_perf_log_tick = now_perf;
        }
#endif

        /* ── Periodic animations ── */
        uint32_t now = osKernelSysTick();
        bool ppg_force_active =
            (s_ui.screen == SCREEN_HR_MEASURE || s_ui.screen == SCREEN_SPO2_MEASURE);
        if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
            if (gSharedData.motion.fall_detected) {
                gSharedData.motion.fall_detected = false;
                s_fall_alert_until_tick = now + FALL_ALERT_SHOW_MS;
                s_dirty = true;
            }
            gSharedData.settings.ppg_force_active = ppg_force_active;
            osMutexRelease(xSensorDataMutex);
        }

        /* Heartbeat animation while measuring */
        if (s_ui.meas_phase == MEAS_MEASURING) {
            if ((now - s_ui.heart_anim_tick) >= HEART_ANIM_MS) {
                s_ui.heart_anim_frame++;   /* counts up; used for ECG scroll and beat frame (even/odd) */
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
                    if (s_ui.screen == SCREEN_HR_MEASURE && s_ui.meas_bpm_result > 0u) {
                        s_ui.home_last_bpm = s_ui.meas_bpm_result;
                    }
                    if (s_ui.screen == SCREEN_SPO2_MEASURE && s_ui.meas_spo2_result > 0u) {
                        s_ui.home_last_spo2 = s_ui.meas_spo2_result;
                    }
                    s_dirty = true;
                }
            }
        }

        /* Stopwatch tick */
        if (gSharedData.stopwatch.running) {
            s_dirty = true;  /* force redraw every cycle while running */
        }

        /* Workout timer tick */
        if (s_ui.workout_active) {
            s_dirty = true;  /* force real-time redraw while workout is active */
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

    /* ── Fall alert overlay is modal ──
     * If the overlay is visible, consume input and dismiss it on the first
     * button action. This prevents BACK from navigating screens while the
     * overlay timer is still running (which feels like it needs multiple
     * presses to close).
     */
    if ((int32_t)(s_fall_alert_until_tick - osKernelSysTick()) > 0) {
        if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_LONG_PRESS) {
            s_fall_alert_until_tick = 0u;
            s_dirty = true;
            return;
        }
    }

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

    /* ── BACK long press: only from home → power menu ── */
    if (evt->id == BTN_ID_BACK && evt->type == BTN_EVT_LONG_PRESS
        && s_ui.screen == SCREEN_HOME) {
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
                        SCREEN_WORKOUT_MENU,
                        SCREEN_STOPWATCH,
                        SCREEN_STATS,
                        SCREEN_SETTINGS,
                        SCREEN_CONNECT,
                    };
                    s_ui.prev_screen = SCREEN_MENU;
                    s_ui.screen      = SCREEN_MAP[s_ui.menu_cursor];
                    /* Reset state on entry */
                    if (s_ui.screen == SCREEN_HR_MEASURE ||
                        s_ui.screen == SCREEN_SPO2_MEASURE) {
                        s_ui.meas_phase    = MEAS_IDLE;
                        s_ui.meas_progress = 0;
                    }
                    if (s_ui.screen == SCREEN_WORKOUT_MENU) {
                        s_ui.workout_cursor = 0u;
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

        /* ── Workout Sub-menu ─────────────────────────────────────── */
        case SCREEN_WORKOUT_MENU:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_BACK) {
                    go_back();
                } else if (evt->id == BTN_ID_SELECT) {
                    /* Pick mode, switch to active training immediately */
                    s_ui.workout_mode       = (WorkoutMode_t)s_ui.workout_cursor;
                    s_ui.prev_screen        = SCREEN_WORKOUT_MENU;
                    s_ui.screen             = SCREEN_WORKOUT;
                    s_ui.workout_active     = true;
                    s_ui.workout_reps       = 0;
                    s_ui.workout_start_tick = osKernelSysTick();

                    if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                        gSharedData.workout.active = true;
                        gSharedData.workout.mode = (uint8_t)s_ui.workout_mode;
                        gSharedData.workout.start_total_steps = gSharedData.motion.steps;
                        gSharedData.workout.session_steps = 0u;
                        gSharedData.workout.pushup_reps = 0u;
                        osMutexRelease(xSensorDataMutex);
                    }
                }
            }
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    if (s_ui.workout_cursor > 0u) s_ui.workout_cursor--;
                } else if (evt->id == BTN_ID_DOWN) {
                    if (s_ui.workout_cursor < (uint8_t)(WORKOUT_COUNT - 1u))
                        s_ui.workout_cursor++;
                }
            }
            break;

        /* ── Workout ──────────────────────────────────────────────── */
        case SCREEN_WORKOUT:
            if (evt->type == BTN_EVT_PRESS && evt->id == BTN_ID_BACK) {
                if (s_ui.workout_active) {
                    /* Show stop-confirm dialog */
                    s_ui.workout_confirm_cursor = 1u;  /* default NO */
                    s_ui.prev_screen = SCREEN_WORKOUT;
                    s_ui.screen      = SCREEN_WORKOUT_CONFIRM;
                } else {
                    go_back();  /* already stopped, just navigate back */
                }
            }
            break;

        /* ── Stop-confirm dialog ──────────────────────────────────────── */
        case SCREEN_WORKOUT_CONFIRM:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_UP || evt->id == BTN_ID_DOWN) {
                    /* Toggle between YES(0) and NO(1) */
                    s_ui.workout_confirm_cursor ^= 1u;
                } else if (evt->id == BTN_ID_SELECT) {
                    if (s_ui.workout_confirm_cursor == 0u) {
                        /* YES — stop workout, go back to workout menu */
                        s_ui.workout_active = false;
                        if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                            gSharedData.workout.active = false;
                            osMutexRelease(xSensorDataMutex);
                        }
                        s_ui.screen         = SCREEN_WORKOUT_MENU;
                        s_ui.prev_screen    = SCREEN_MENU;
                    } else {
                        /* NO — resume workout */
                        s_ui.screen = SCREEN_WORKOUT;
                    }
                } else if (evt->id == BTN_ID_BACK) {
                    /* BACK = dismiss = NO */
                    s_ui.screen = SCREEN_WORKOUT;
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
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    if (s_ui.stats_cursor > 0u) s_ui.stats_cursor--;
                } else if (evt->id == BTN_ID_DOWN) {
                    if (s_ui.stats_cursor < 3u) s_ui.stats_cursor++;
                } else if (evt->id == BTN_ID_BACK) {
                    go_back();
                }
            }
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
                            if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                                gSharedData.settings.bluetooth_enabled = s_ui.bt_enabled;
                                gSharedData.ble.enabled = s_ui.bt_enabled;
                                if (!s_ui.bt_enabled) {
                                    gSharedData.ble.connected = false;
                                }
                                osMutexRelease(xSensorDataMutex);
                            }
                            break;
                        case SETTING_RAISE_TO_WAKE:
                            s_ui.raise_to_wake = !s_ui.raise_to_wake;
                            if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                                gSharedData.settings.raise_to_wake = s_ui.raise_to_wake;
                                osMutexRelease(xSensorDataMutex);
                            }
                            break;
                        case SETTING_FALL_DETECT:
                            s_ui.fall_detect = !s_ui.fall_detect;
                            if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                                gSharedData.settings.fall_detect = s_ui.fall_detect;
                                osMutexRelease(xSensorDataMutex);
                            }
                            break;
                        default: break;
                    }
                } else if (evt->id == BTN_ID_BACK) {
                    go_back();
                }
            }
            break;

        /* ── Connect ─────────────────────────────────────────────── */
        case SCREEN_CONNECT:
            if (evt->type == BTN_EVT_PRESS) {
                if (evt->id == BTN_ID_BACK) {
                    go_back();
                } else if (evt->id == BTN_ID_SELECT) {
                    if (s_ui.bt_enabled) {
                        JDY31_SendStr("PAIR_DEVICE\r\n");
                    }
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
    /* Do not flush to the OLED while the display is sleeping — avoids
     * I2C bus contention that would prevent Power_WakeUp from
     * sending the display-on command (0xAF) successfully. */
    if (Power_GetState() == POWER_SLEEP) return;
    /* Snapshot shared data under mutex */
    HeartData_t  heart;
    MotionData_t motion;
    SoftClock_t  clk;
    BatteryStatus_t bat;
    WeekStats_t  stats;
    StopwatchData_t sw;
    WorkoutSession_t workout;

    Sensor_Data_GetHeart(&heart);
    Sensor_Data_GetMotion(&motion);
    clk = Sensor_Data_GetClock();

    if (heart.hr_status == SENSOR_OK && heart.bpm > 0u) {
        s_ui.home_last_bpm = heart.bpm;
    }
    if (heart.spo2_status == SENSOR_OK && heart.spo2 > 0u) {
        s_ui.home_last_spo2 = heart.spo2;
    }

    osMutexWait(xSensorDataMutex, osWaitForever);
    bat   = gSharedData.battery;
    stats = gSharedData.stats;
    sw    = gSharedData.stopwatch;
    workout = gSharedData.workout;
    /* If stopwatch is running, compute live elapsed */
    if (sw.running) {
        sw.elapsed_ms = osKernelSysTick() - sw.start_tick;
    }
    osMutexRelease(xSensorDataMutex);

    switch (s_ui.screen)
    {
        case SCREEN_HOME:
        {
            uint8_t home_bpm  = (heart.hr_status == SENSOR_OK && heart.bpm > 0u)
                                ? (uint8_t)heart.bpm
                                : (uint8_t)s_ui.home_last_bpm;
            uint8_t home_spo2 = (heart.spo2_status == SENSOR_OK && heart.spo2 > 0u)
                                ? heart.spo2
                                : s_ui.home_last_spo2;
            OLED_PageHome(
                &clk, &bat,
                s_ui.bt_enabled, gSharedData.ble.connected,
                motion.steps,
                home_bpm,
                home_spo2);
            break;
        }

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

        case SCREEN_WORKOUT_MENU:
            OLED_PageWorkoutMenu(s_ui.workout_cursor);
            break;

        case SCREEN_WORKOUT:
        {
            uint32_t elapsed_s = 0u;
            if (s_ui.workout_active) {
                elapsed_s = (osKernelSysTick() - s_ui.workout_start_tick) / 1000u;
            }
            uint32_t reps = (s_ui.workout_mode == WORKOUT_PUSHUPS) ?
                             workout.pushup_reps : workout.session_steps;
            OLED_PageWorkout(s_ui.workout_mode, s_ui.workout_active,
                             reps, elapsed_s, heart.bpm);
            break;
        }

        case SCREEN_WORKOUT_CONFIRM:
            OLED_PageWorkoutConfirm(s_ui.workout_confirm_cursor);
            break;

        case SCREEN_STOPWATCH:
            OLED_PageStopwatch(sw.elapsed_ms, sw.running);
            break;

        case SCREEN_STATS:
            OLED_PageStats(&stats, s_ui.stats_cursor);
            break;

        case SCREEN_SETTINGS:
            OLED_PageSettings(
                s_ui.settings_cursor,
                s_ui.bt_enabled,
                s_ui.raise_to_wake,
                s_ui.fall_detect,
                s_ui.brightness);
            break;

        case SCREEN_CONNECT:
            OLED_PageConnect(s_ui.bt_enabled, gSharedData.ble.connected);
            break;

        case SCREEN_POWER_MENU:
            /* Render underlying screen first, then overlay.
             * Avoid UI_SetScreen here (it toggles dirty flag) and avoid
             * recursive power-menu rendering if prev_screen is also POWER_MENU. */
            if (s_ui.prev_screen != SCREEN_POWER_MENU) {
                ScreenId_t saved = s_ui.screen;
                s_ui.screen = s_ui.prev_screen;
                UI_Render();
                s_ui.screen = saved;
            }
            OLED_PagePowerMenu(s_ui.power_cursor);
            break;

        default: break;
    }

    if ((int32_t)(s_fall_alert_until_tick - osKernelSysTick()) > 0) {
        OLED_DrawFallAlertOverlay();
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
        if (s_ui.screen == SCREEN_POWER_MENU) {
            s_dirty = true;
            return;
        }
        s_ui.prev_screen  = s_ui.screen;
        s_ui.screen       = SCREEN_POWER_MENU;
        s_ui.power_cursor = 1u;
    } else {
        s_ui.screen = s_ui.prev_screen;
    }
    s_dirty = true;
}
