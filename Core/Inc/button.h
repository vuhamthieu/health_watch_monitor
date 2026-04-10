/**
 * @file    button.h
 * @brief   Button driver — debounce, press, long-press and auto-repeat events.
 *
 * Buttons are active LOW (pulled high internally):
 *   BTN_BACK   → PA0 (EXTI0 interrupt + polled for long-press)
 *   BTN_DOWN   → PA2 (polled)
 *   BTN_UP     → PA3 (polled)
 *   BTN_SELECT → PA15 (polled)
 *
 * buttonTask polls every BTN_DEBOUNCE_MS and pushes ButtonEvent_t items
 * into xButtonEventQueue for uiTask to consume.
 */

#ifndef __BUTTON_H
#define __BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ========================================================================== *
 *  Button identifiers
 * ========================================================================== */
typedef enum {
    BTN_ID_UP     = 0,
    BTN_ID_DOWN   = 1,
    BTN_ID_SELECT = 2,
    BTN_ID_BACK   = 3,
    BTN_ID_COUNT,
} ButtonId_t;

/* ========================================================================== *
 *  Event types
 * ========================================================================== */
typedef enum {
    BTN_EVT_NONE       = 0,
    BTN_EVT_PRESS,          /**< Short press (released before LONG_PRESS_MS)  */
    BTN_EVT_LONG_PRESS,     /**< Held ≥ BTN_LONG_PRESS_MS (BACK only)        */
    BTN_EVT_REPEAT,         /**< Auto-repeat while UP/DOWN held               */
    BTN_EVT_RELEASE,        /**< Button released (after any hold)             */
} ButtonEventType_t;

/* ========================================================================== *
 *  Event structure (placed in xButtonEventQueue)
 * ========================================================================== */
typedef struct {
    ButtonId_t        id;
    ButtonEventType_t type;
    uint32_t          hold_ms;   /**< How long the button was held (ms)      */
    uint32_t          tick;      /**< FreeRTOS tick at event time             */
} ButtonEvent_t;

/* ========================================================================== *
 *  Internal state (one per button, used by button.c)
 * ========================================================================== */
typedef struct {
    bool     last_raw;       /**< Raw GPIO level from last poll              */
    bool     debounced;      /**< Debounced state (true = pressed)           */
    uint8_t  debounce_cnt;   /**< Consecutive same-state sample counter      */
    uint32_t press_tick;     /**< Tick when button first debounced as pressed*/
    uint32_t last_repeat_tick; /**< Tick of last auto-repeat event           */
    bool     long_press_sent;  /**< Long-press event already sent this press */
} ButtonState_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise the button module (zero internal state).
 *         Call once before the scheduler starts.
 */
void Button_Init(void);

/**
 * @brief  Task function — call this as buttonTask body.
 *         Polls GPIO, applies debounce, detects events, posts to queue.
 *         Never returns.
 * @param  argument  Unused (FreeRTOS task argument).
 */
void Button_Task(void const *argument);

/**
 * @brief  Non-blocking: read the current debounced state of a button.
 * @return true if button is currently pressed.
 */
bool Button_IsPressed(ButtonId_t id);

/**
 * @brief  EXTI callback for BACK button (PA0).
 *         Call from HAL_GPIO_EXTI_Callback() in stm32f1xx_it.c.
 */
void Button_EXTI_Callback(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H */
