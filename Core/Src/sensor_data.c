/* sensor_data.c - shared data struct + all FreeRTOS IPC handles (mutex, queues) */

#include "sensor_data.h"
#include "button.h"
#include "jdy31.h"
#include "power_manager.h"
#include <string.h>

/* IPC handles */
osMutexId    xSensorDataMutex;
osMessageQId xButtonEventQueue;
osMessageQId xBleQueue;
osMessageQId xPowerEventQueue;

SharedData_t gSharedData;

#define DATA_MUTEX_TIMEOUT_MS  10u

void Sensor_Data_Init(void)
{
    memset(&gSharedData, 0, sizeof(gSharedData));

    /* Mark all sensor statuses as INIT (no data yet) */
    gSharedData.heart.hr_status   = SENSOR_INIT;
    gSharedData.heart.spo2_status = SENSOR_INIT;
    gSharedData.motion.status     = SENSOR_INIT;

    /* Create FreeRTOS objects */
    osMutexDef(SensorDataMutex);
    xSensorDataMutex = osMutexCreate(osMutex(SensorDataMutex));

    /* TODO: Use the queue sizes that fit your usage pattern.
     *       ButtonEvent_t  = ~16 bytes
     *       BlePacket_t    = ~12 bytes
     *       PowerEvent_t   = 4 bytes
     */
    osMessageQDef(ButtonEventQueue, 8, ButtonEvent_t);
    xButtonEventQueue = osMessageCreate(osMessageQ(ButtonEventQueue), NULL);

    osMessageQDef(BleQueue, 16, BlePacket_t);
    xBleQueue = osMessageCreate(osMessageQ(BleQueue), NULL);

    osMessageQDef(PowerEventQueue, 4, PowerEvent_t);
    xPowerEventQueue = osMessageCreate(osMessageQ(PowerEventQueue), NULL);
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
