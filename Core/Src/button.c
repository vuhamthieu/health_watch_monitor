/* button.c - debounce + press / long-press / auto-repeat for all 4 buttons
 * polled every 1 ms tick in Button_Task */

#include "button.h"
#include "sensor_data.h"
#include "power_manager.h"
#include "cmsis_os.h"
#include <string.h>

/* state */
static ButtonState_t s_state[BTN_ID_COUNT];

/* GPIO lookup tables */
static GPIO_TypeDef * const s_port[BTN_ID_COUNT] = {
    BTN_UP_PORT, BTN_DOWN_PORT, BTN_SELECT_PORT, BTN_BACK_PORT
};
static const uint16_t s_pin[BTN_ID_COUNT] = {
    BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN, BTN_BACK_PIN
};

void Button_Init(void)
{
    memset(s_state, 0, sizeof(s_state));
}

/* active LOW: GPIO_PIN_RESET = pressed */
static bool read_raw(ButtonId_t id)
{
    return (HAL_GPIO_ReadPin(s_port[id], s_pin[id]) == GPIO_PIN_RESET);
}

static void post_event(ButtonId_t id, ButtonEventType_t type, uint32_t hold_ms)
{
    ButtonEvent_t evt = {
        .id      = id,
        .type    = type,
        .hold_ms = hold_ms,
        .tick    = osKernelSysTick(),
    };
    (void)xQueueSend(xButtonEventQueue, &evt, 0u);
}

void Button_Task(void const *argument)
{
    (void)argument;
    Button_Init();

    for (;;)
    {
        uint32_t now = osKernelSysTick();

        for (uint8_t i = 0; i < BTN_ID_COUNT; i++)
        {
            bool raw = read_raw((ButtonId_t)i);
            ButtonState_t *st = &s_state[i];

            /* ------- Debounce ------- */
            if (raw == st->last_raw) {
                if (st->debounce_cnt < BTN_DEBOUNCE_MS) st->debounce_cnt++;
            } else {
                st->debounce_cnt = 0;
                st->last_raw = raw;
            }

            bool prev_debounced = st->debounced;
            if (st->debounce_cnt >= BTN_DEBOUNCE_MS) {
                st->debounced = raw;
            }

            /* ------- Rising edge: button pressed ------- */
            if (st->debounced && !prev_debounced) {
                st->press_tick        = now;
                st->last_repeat_tick  = now;
                st->long_press_sent   = false;
                /* Wake the power manager */
                Power_PostEvent(POWER_EVT_USER_ACTIVITY, false);
            }

            /* ------- While held ------- */
            if (st->debounced) {
                uint32_t hold_ms = now - st->press_tick;

                /* Long-press (BACK only) */
                if (i == BTN_ID_BACK && !st->long_press_sent &&
                    hold_ms >= BTN_LONG_PRESS_MS)
                {
                    post_event((ButtonId_t)i, BTN_EVT_LONG_PRESS, hold_ms);
                    st->long_press_sent = true;
                }

                /* Auto-repeat (UP / DOWN only) */
                if ((i == BTN_ID_UP || i == BTN_ID_DOWN) &&
                    hold_ms >= BTN_REPEAT_DELAY_MS)
                {
                    if ((now - st->last_repeat_tick) >= BTN_REPEAT_INTERVAL_MS) {
                        post_event((ButtonId_t)i, BTN_EVT_REPEAT, hold_ms);
                        st->last_repeat_tick = now;
                    }
                }
            }

            /* ------- Falling edge: button released ------- */
            if (!st->debounced && prev_debounced) {
                uint32_t hold_ms = now - st->press_tick;
                if (!st->long_press_sent) {
                    /* Short press */
                    post_event((ButtonId_t)i, BTN_EVT_PRESS, hold_ms);
                } else {
                    post_event((ButtonId_t)i, BTN_EVT_RELEASE, hold_ms);
                }
            }
        }

        osDelay(1); /* Poll every 1 ms tick */
    }
}

bool Button_IsPressed(ButtonId_t id)
{
    if (id >= BTN_ID_COUNT) return false;
    return s_state[id].debounced;
}

void Button_EXTI_Callback(void)
{
    /* PA0 EXTI0 fires on BACK button falling edge.
     * The buttonTask will handle debounce — nothing extra needed here
     * unless you want immediate wake from STM32 STOP mode. */
    Power_PostEvent(POWER_EVT_USER_ACTIVITY, true);
}
