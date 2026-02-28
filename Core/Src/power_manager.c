/* power_manager.c - ACTIVE / DIM / SLEEP state machine
 * TODO: add HAL_PWR_EnterSTOPMode() in Power_EnterSleep for real deep sleep */

#include "power_manager.h"
#include "sensor_data.h"
#include "oled.h"
#include "sh1106.h"
#include "app_config.h"
#include "ui_menu.h"
#include "cmsis_os.h"

/* state */
static PowerState_t  s_state            = POWER_ACTIVE;
static uint32_t      s_last_activity_tick = 0;

void Power_Init(void)
{
    s_state             = POWER_ACTIVE;
    s_last_activity_tick = osKernelSysTick();
}

void Power_Task(void const *argument)
{
    (void)argument;
    Power_Init();

    for (;;)
    {
        /* Check inactivity timers */
        uint32_t now  = osKernelSysTick();
        uint32_t idle = now - s_last_activity_tick;

        if (s_state == POWER_ACTIVE && idle >= POWER_DIM_TIMEOUT_MS) {
            s_state = POWER_DIM;
            Power_ApplyState(POWER_DIM);
        }
        if (s_state == POWER_DIM && idle >= POWER_SLEEP_TIMEOUT_MS) {
            Power_EnterSleep();
        }

        /* Check event queue */
        osEvent evt = osMessageGet(xPowerEventQueue, 100);
        if (evt.status == osEventMessage) {
            PowerEvent_t event = (PowerEvent_t)evt.value.v;
            switch (event)
            {
                case POWER_EVT_USER_ACTIVITY:
                case POWER_EVT_WRIST_RAISE:
                case POWER_EVT_BT_RX:
                    if (s_state != POWER_ACTIVE) {
                        Power_WakeUp();
                    }
                    s_last_activity_tick = osKernelSysTick();
                    break;

                case POWER_EVT_SLEEP_MENU:
                    UI_ShowSleepOverlay(true);
                    break;

                case POWER_EVT_SLEEP_NOW:
                    UI_ShowSleepOverlay(false);
                    Power_EnterSleep();
                    break;

                case POWER_EVT_CANCEL_SLEEP:
                    UI_ShowSleepOverlay(false);
                    break;

                default: break;
            }
        }
    }
}

void Power_NotifyActivity(void)
{
    s_last_activity_tick = osKernelSysTick();
    if (s_state != POWER_ACTIVE) {
        Power_PostEvent(POWER_EVT_USER_ACTIVITY, false);
    }
}

void Power_PostEvent(PowerEvent_t event, bool fromISR)
{
    if (fromISR) {
        osMessagePutFromISR(xPowerEventQueue, (uint32_t)event, NULL);
    } else {
        osMessagePut(xPowerEventQueue, (uint32_t)event, 0);
    }
}

PowerState_t Power_GetState(void)
{
    return s_state;
}

void Power_EnterSleep(void)
{
    s_state = POWER_SLEEP;
    Power_ApplyState(POWER_SLEEP);
    /* TODO: optionally call HAL_PWR_EnterSTOPMode() here for deep sleep */
}

void Power_WakeUp(void)
{
    s_state = POWER_ACTIVE;
    Power_ApplyState(POWER_ACTIVE);
    UI_Invalidate();
}

void Power_ApplyState(PowerState_t state)
{
    switch (state)
    {
        case POWER_ACTIVE:
            SH1106_SetDisplayOn(true);
            SH1106_SetContrast(POWER_OLED_FULL_CONTRAST);
            LED_OFF();
            break;

        case POWER_DIM:
            SH1106_SetDisplayOn(true);
            SH1106_SetContrast((uint8_t)(POWER_OLED_FULL_CONTRAST * POWER_OLED_DIM_CONTRAST / 100));
            break;

        case POWER_SLEEP:
            SH1106_SetDisplayOn(false);
            LED_OFF();
            /* Sensor polling rate is reduced by checking Power_GetState()
             * inside sensorTask's delay logic */
            break;

        default: break;
    }
}
