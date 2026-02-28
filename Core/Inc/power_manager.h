/**
 * @file    power_manager.h
 * @brief   Power state machine for the health watch.
 *
 * Power States:
 *   POWER_ACTIVE   → Full sensors + full display brightness.
 *   POWER_DIM      → After POWER_DIM_TIMEOUT_MS inactivity: reduced contrast.
 *   POWER_SLEEP    → After POWER_SLEEP_TIMEOUT_MS inactivity: OLED off,
 *                    sensor polling rate reduced.
 *
 * Wake triggers:
 *   - Any button press (via xButtonEventQueue / xPowerEventQueue)
 *   - Wrist-raise gesture (MPU-6050, via xPowerEventQueue)
 *   - BT incoming data (via xPowerEventQueue)
 *
 * BACK long-press → sends POWER_EVT_SLEEP_MENU to self to show sleep overlay.
 */

#ifndef __POWER_MANAGER_H
#define __POWER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"

/* ========================================================================== *
 *  Power states
 * ========================================================================== */
typedef enum {
    POWER_ACTIVE = 0,
    POWER_DIM,
    POWER_SLEEP,
} PowerState_t;

/* ========================================================================== *
 *  Power events (placed in xPowerEventQueue)
 * ========================================================================== */
typedef enum {
    POWER_EVT_USER_ACTIVITY = 0,  /**< Any button press / touch             */
    POWER_EVT_WRIST_RAISE,        /**< MPU detected wrist-raise gesture     */
    POWER_EVT_BT_RX,              /**< Bluetooth data received              */
    POWER_EVT_SLEEP_NOW,          /**< "Sleep Now" selected in overlay      */
    POWER_EVT_SLEEP_MENU,         /**< BACK long-press → show sleep menu    */
    POWER_EVT_CANCEL_SLEEP,       /**< "Cancel" selected in overlay         */
} PowerEvent_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise the power manager (defaults to POWER_ACTIVE).
 */
void Power_Init(void);

/**
 * @brief  powerTask body — monitors inactivity timers, handles power events.
 *         Never returns.
 */
void Power_Task(void const *argument);

/**
 * @brief  Notify the power manager of user activity (resets inactivity timer).
 *         Safe to call from any task or ISR.
 */
void Power_NotifyActivity(void);

/**
 * @brief  Notify the power manager of a specific event.
 *         Safe to call from any task.
 * @param  event  Event type.
 * @param  fromISR  Pass pdTRUE if calling from an ISR.
 */
void Power_PostEvent(PowerEvent_t event, bool fromISR);

/**
 * @brief  Get the current power state.
 */
PowerState_t Power_GetState(void);

/**
 * @brief  Execute transition to POWER_SLEEP (called internally by powerTask).
 */
void Power_EnterSleep(void);

/**
 * @brief  Execute transition to POWER_ACTIVE (called internally by powerTask).
 */
void Power_WakeUp(void);

/**
 * @brief  Apply current power state to peripherals (OLED contrast, sensor rate).
 *         Called by powerTask after any state change.
 */
void Power_ApplyState(PowerState_t state);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MANAGER_H */
