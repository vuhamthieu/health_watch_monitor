/* power_manager.c - ACTIVE / DIM / SLEEP state machine
 * TODO: add HAL_PWR_EnterSTOPMode() in Power_EnterSleep for real deep sleep */

#include "power_manager.h"
#include "sensor_data.h"
#include "oled.h"
#include "sh1106.h"
#include "app_config.h"
#include "ui_menu.h"
#include "cmsis_os.h"
#include "task.h"

extern osThreadId powerTaskHandle;

#define POWER_EVT_BIT_USER_ACTIVITY  (1UL << 0)
#define POWER_EVT_BIT_WRIST_RAISE    (1UL << 1)
#define POWER_EVT_BIT_BT_RX          (1UL << 2)
#define POWER_EVT_BIT_SLEEP_NOW      (1UL << 3)
#define POWER_EVT_BIT_SLEEP_MENU     (1UL << 4)
#define POWER_EVT_BIT_CANCEL_SLEEP   (1UL << 5)

static uint32_t power_event_to_bits(PowerEvent_t event)
{
    switch (event)
    {
        case POWER_EVT_USER_ACTIVITY: return POWER_EVT_BIT_USER_ACTIVITY;
        case POWER_EVT_WRIST_RAISE:   return POWER_EVT_BIT_WRIST_RAISE;
        case POWER_EVT_BT_RX:         return POWER_EVT_BIT_BT_RX;
        case POWER_EVT_SLEEP_NOW:     return POWER_EVT_BIT_SLEEP_NOW;
        case POWER_EVT_SLEEP_MENU:    return POWER_EVT_BIT_SLEEP_MENU;
        case POWER_EVT_CANCEL_SLEEP:  return POWER_EVT_BIT_CANCEL_SLEEP;
        default:                      return 0UL;
    }
}

/* state */
static PowerState_t  s_state            = POWER_ACTIVE;
static uint32_t      s_last_activity_tick = 0;

void Power_Init(void)
{
    s_state             = POWER_ACTIVE;
    s_last_activity_tick = osKernelSysTick();
    Power_ApplyState(POWER_ACTIVE);
}

void Power_Task(void const *argument)
{
    (void)argument;
    Power_Init();

    for (;;)
    {
#if POWER_DISABLE_AUTO_SLEEP
        if (s_state != POWER_ACTIVE) {
            s_state = POWER_ACTIVE;
            Power_ApplyState(POWER_ACTIVE);
        }
#else
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
#endif

        uint32_t pending = 0UL;
        if (xTaskNotifyWait(0UL, 0xFFFFFFFFUL, &pending, pdMS_TO_TICKS(100)) == pdTRUE) {
            if ((pending & (POWER_EVT_BIT_USER_ACTIVITY |
                            POWER_EVT_BIT_WRIST_RAISE |
                            POWER_EVT_BIT_BT_RX)) != 0UL)
            {
                if (s_state != POWER_ACTIVE) {
                    Power_WakeUp();
                }
                s_last_activity_tick = osKernelSysTick();
            }

            if ((pending & POWER_EVT_BIT_SLEEP_MENU) != 0UL) {
                UI_ShowSleepOverlay(true);
            }

            if ((pending & POWER_EVT_BIT_SLEEP_NOW) != 0UL) {
                UI_ShowSleepOverlay(false);
                Power_EnterSleep();
            }

            if ((pending & POWER_EVT_BIT_CANCEL_SLEEP) != 0UL) {
                UI_ShowSleepOverlay(false);
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
    const uint32_t bits = power_event_to_bits(event);
    TaskHandle_t power_task = (TaskHandle_t)powerTaskHandle;

    if (bits == 0UL || power_task == NULL) {
        return;
    }

    if (fromISR) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(power_task, bits, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        xTaskNotify(power_task, bits, eSetBits);
    }
}

PowerState_t Power_GetState(void)
{
    return s_state;
}

void Power_EnterSleep(void)
{
#if POWER_DISABLE_AUTO_SLEEP
    s_state = POWER_ACTIVE;
    Power_ApplyState(POWER_ACTIVE);
    return;
#endif
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
