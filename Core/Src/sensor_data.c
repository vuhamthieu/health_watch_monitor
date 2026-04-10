/* sensor_data.c - shared data struct + all FreeRTOS IPC handles (mutex, queues) */

#include "sensor_data.h"
#include "button.h"
#include "jdy31.h"
#include "power_manager.h"
#include <string.h>

/* IPC handles */
osMutexId    xSensorDataMutex;
QueueHandle_t xButtonEventQueue;
osMessageQId xBleQueue;

SharedData_t gSharedData;

#define DATA_MUTEX_TIMEOUT_MS  10u

void Sensor_Data_Init(void)
{
    memset(&gSharedData, 0, sizeof(gSharedData));

    /* Mark all sensor statuses as INIT (no data yet) */
    gSharedData.heart.hr_status   = SENSOR_INIT;
    gSharedData.heart.spo2_status = SENSOR_INIT;
    gSharedData.motion.status     = SENSOR_INIT;
    gSharedData.motion.fall_detected = false;
    gSharedData.motion.last_fall_tick = 0u;
    gSharedData.settings.bluetooth_enabled = true;
    gSharedData.settings.raise_to_wake = false;
    gSharedData.settings.fall_detect = false;
    gSharedData.settings.ppg_force_active = false;
    gSharedData.ble.enabled       = true;
    gSharedData.workout.active = false;
    gSharedData.workout.mode = 0u;
    gSharedData.workout.start_total_steps = 0u;
    gSharedData.workout.session_steps = 0u;
    gSharedData.workout.pushup_reps = 0u;
    gSharedData.battery.bars      = BATT_BARS_FULL;
    gSharedData.battery.charge    = BATT_UNKNOWN;

    /* Create FreeRTOS objects */
    osMutexDef(SensorDataMutex);
    xSensorDataMutex = osMutexCreate(osMutex(SensorDataMutex));
    xButtonEventQueue = xQueueCreate(8u, sizeof(ButtonEvent_t));

    osMessageQDef(BleQueue, 16, BlePacket_t);
    xBleQueue = osMessageCreate(osMessageQ(BleQueue), NULL);
}

/* heart data get/set - all mutex protected */
bool Sensor_Data_GetHeart(HeartData_t *out)
{
    if (osMutexWait(xSensorDataMutex, DATA_MUTEX_TIMEOUT_MS) != osOK) return false;
    *out = gSharedData.heart;
    osMutexRelease(xSensorDataMutex);
    return true;
}

bool Sensor_Data_SetHeart(const HeartData_t *in)
{
    if (osMutexWait(xSensorDataMutex, DATA_MUTEX_TIMEOUT_MS) != osOK) return false;
    gSharedData.heart = *in;
    osMutexRelease(xSensorDataMutex);
    return true;
}

bool Sensor_Data_GetMotion(MotionData_t *out)
{
    if (osMutexWait(xSensorDataMutex, DATA_MUTEX_TIMEOUT_MS) != osOK) return false;
    *out = gSharedData.motion;
    osMutexRelease(xSensorDataMutex);
    return true;
}

bool Sensor_Data_SetMotion(const MotionData_t *in)
{
    if (osMutexWait(xSensorDataMutex, DATA_MUTEX_TIMEOUT_MS) != osOK) return false;
    gSharedData.motion = *in;
    osMutexRelease(xSensorDataMutex);
    return true;
}

void Sensor_Data_SetClock(uint8_t h, uint8_t m, uint8_t s)
{
    /* Atomic write — no mutex needed for 3 bytes on Cortex-M3 word access */
    gSharedData.clock.hours   = h;
    gSharedData.clock.minutes = m;
    gSharedData.clock.seconds = s;
}

SoftClock_t Sensor_Data_GetClock(void)
{
    return gSharedData.clock;
}
