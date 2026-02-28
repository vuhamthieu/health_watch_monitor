/* ui_menu.c - screen navigation state machine + OLED render dispatch
 * TODO: implement settings mutations (brightness, BT toggle, step reset) */

#include "ui_menu.h"
#include "oled.h"
#include "sensor_data.h"
#include "power_manager.h"
#include "cmsis_os.h"

static const char *SETTINGS_ITEMS[SETTING_COUNT] = {
    "Brightness",
    "Bluetooth",
    "Reset Steps",
    "Sleep Timer",
    "About",
};

/* state */
static UIState_t s_ui;
static bool s_dirty = true;

void UI_Init(void)
{
    s_ui.current_screen    = SCREEN_MAIN;
    s_ui.overlay           = 0;
    s_ui.settings_cursor   = 0;
    s_ui.sleep_cursor      = 0;
    s_ui.brightness        = 80;
    s_ui.bt_enabled        = true;
    s_ui.sleep_timer_ms    = POWER_SLEEP_TIMEOUT_MS;
    s_ui.last_activity_tick = 0;
    s_dirty = true;
}

void UI_Task(void const *argument)
{
    (void)argument;
    UI_Init();
    OLED_Init();
    OLED_PageSplash();
    osDelay(1500); /* Show splash for 1.5 s */

    for (;;)
    {
        /* Consume one button event (with timeout so we refresh at UI_REFRESH_PERIOD_MS) */
        osEvent evt = osMessageGet(xButtonEventQueue, UI_REFRESH_PERIOD_MS);

        if (evt.status == osEventMessage) {
            /* TODO: Properly unpack ButtonEvent_t from message value.
             *       Switch to pointer-based queue when implementing properly. */
            ButtonEvent_t btn_evt;
            /* Placeholder: btn_evt = *(ButtonEvent_t*)&evt.value.v; */
            (void)btn_evt;
            UI_HandleButtonEvent(&btn_evt);
            s_dirty = true;
        }

        if (s_dirty) {
            UI_Render();
            s_dirty = false;
        }
    }
}

void UI_HandleButtonEvent(const ButtonEvent_t *evt)
{
    if (!evt) return;

    /* Always reset inactivity on any input */
    s_ui.last_activity_tick = osKernelSysTick();
    Power_NotifyActivity();

    /* --- Sleep overlay active: only UP/DOWN/SELECT/BACK matter --- */
    if (s_ui.overlay == OVERLAY_SLEEP_OPTIONS)
    {
        switch (evt->id) {
            case BTN_ID_UP:
            case BTN_ID_DOWN:
                s_ui.sleep_cursor = (s_ui.sleep_cursor == 0) ? 1 : 0;
                break;
            case BTN_ID_SELECT:
                if (s_ui.sleep_cursor == 0) {
                    Power_PostEvent(POWER_EVT_SLEEP_NOW, false);
                } else {
                    UI_ShowSleepOverlay(false);
                }
                break;
            case BTN_ID_BACK:
                if (evt->type == BTN_EVT_PRESS) {
                    UI_ShowSleepOverlay(false);
                }
                break;
            default: break;
        }
        return;
    }

    /* --- BACK long press: show sleep overlay (from any screen) --- */
    if (evt->id == BTN_ID_BACK && evt->type == BTN_EVT_LONG_PRESS) {
        UI_ShowSleepOverlay(true);
        return;
    }

    /* --- Normal navigation --- */
    switch (s_ui.current_screen)
    {
        case SCREEN_MAIN:
        case SCREEN_HEART_RATE:
        case SCREEN_SPO2:
        case SCREEN_STEPS:
        case SCREEN_BT_STATUS:
        {
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    /* Cycle screens upward */
                    if (s_ui.current_screen == SCREEN_MAIN)
                        s_ui.current_screen = SCREEN_SETTINGS;
                    else
                        s_ui.current_screen = (ScreenId_t)(s_ui.current_screen - 1);
                }
                else if (evt->id == BTN_ID_DOWN) {
                    s_ui.current_screen = (ScreenId_t)(
                        (s_ui.current_screen + 1) % SCREEN_SETTINGS + 0);
                    /* Wrap: 0..SCREEN_BT_STATUS cycle */
                    if (s_ui.current_screen > SCREEN_BT_STATUS)
                        s_ui.current_screen = SCREEN_MAIN;
                }
                else if (evt->id == BTN_ID_SELECT) {
                    if (s_ui.current_screen == SCREEN_STEPS) {
                        /* TODO: show "Reset?" confirm dialog */
                    }
                    s_ui.current_screen = SCREEN_SETTINGS;
                }
            }
            break;
        }

        case SCREEN_SETTINGS:
        {
            if (evt->type == BTN_EVT_PRESS || evt->type == BTN_EVT_REPEAT) {
                if (evt->id == BTN_ID_UP) {
                    if (s_ui.settings_cursor > 0) s_ui.settings_cursor--;
                }
                else if (evt->id == BTN_ID_DOWN) {
                    if (s_ui.settings_cursor < SETTING_COUNT - 1) s_ui.settings_cursor++;
                }
                else if (evt->id == BTN_ID_SELECT) {
                    /* TODO: enter setting sub-pages */
                }
                else if (evt->id == BTN_ID_BACK && evt->type == BTN_EVT_PRESS) {
                    s_ui.current_screen = SCREEN_MAIN;
                }
            }
            break;
        }

        default: break;
    }
}

void UI_Render(void)
{
    HeartData_t  heart;
    MotionData_t motion;
    SoftClock_t  clk;

    Sensor_Data_GetHeart(&heart);
    Sensor_Data_GetMotion(&motion);
    clk = Sensor_Data_GetClock();

    /* Determine what to show based on overlay or screen */
    switch (s_ui.current_screen)
    {
        case SCREEN_MAIN:
            OLED_PageMain(
                heart.hr_status == SENSOR_OK ? heart.bpm : 0,
                heart.spo2_status == SENSOR_OK ? heart.spo2 : 0,
                motion.steps,
                clk.hours, clk.minutes,
                gSharedData.ble.connected,
                heart.hr_alert, heart.spo2_alert);
            break;

        case SCREEN_HEART_RATE:
        {
            const char *status = (heart.hr_status == SENSOR_NO_FINGER) ? "No finger"
                               : (heart.hr_status == SENSOR_FAULT)     ? "Sensor error"
                               : "";
            OLED_PageHeartRate(
                heart.hr_status == SENSOR_OK ? heart.bpm : 0,
                heart.hr_alert, status);
            break;
        }

        case SCREEN_SPO2:
        {
            const char *status = (heart.spo2_status == SENSOR_NO_FINGER) ? "No finger"
                               : (heart.spo2_status == SENSOR_FAULT)     ? "Sensor error"
                               : "";
            OLED_PageSpO2(
                heart.spo2_status == SENSOR_OK ? heart.spo2 : 0,
                heart.spo2_alert, status);
            break;
        }

        case SCREEN_STEPS:
            OLED_PageSteps(motion.steps, motion.distance_m,
                           motion.calories_kcal, STEP_DAILY_GOAL);
            break;

        case SCREEN_BT_STATUS:
            OLED_PageBluetooth(gSharedData.ble.connected, BLE_DEVICE_NAME);
            break;

        case SCREEN_SETTINGS:
            OLED_PageSettings(SETTINGS_ITEMS, SETTING_COUNT, s_ui.settings_cursor);
            break;

        default: break;
    }

    /* Draw sleep overlay on top if active */
    if (s_ui.overlay == OVERLAY_SLEEP_OPTIONS) {
        OLED_PageSleepOptions(s_ui.sleep_cursor);
    }
}

void UI_Invalidate(void)       { s_dirty = true; }
ScreenId_t UI_GetCurrentScreen(void) { return s_ui.current_screen; }
void UI_SetScreen(ScreenId_t s) { s_ui.current_screen = s; s_dirty = true; }

void UI_ShowSleepOverlay(bool show)
{
    s_ui.overlay      = show ? OVERLAY_SLEEP_OPTIONS : 0;
    s_ui.sleep_cursor = 0;
    s_dirty = true;
}
