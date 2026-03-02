/**
 * @file    sensor_data.h
 * @brief   Shared sensor data structures and FreeRTOS IPC object declarations.
 *          All tasks read/write sensor data through these structures and queues.
 *
 * USAGE:
 *   - Call Sensor_Data_Init() once before starting the scheduler.
 *   - sensorTask writes under xSensorDataMutex.
 *   - uiTask and bleTask read under xSensorDataMutex.
 */

#ifndef __SENSOR_DATA_H
#define __SENSOR_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"
#include "app_config.h"

/* ========================================================================== *
 *  Sensor validity / status flags
 * ========================================================================== */
typedef enum {
    SENSOR_OK       = 0,   /**< Data is valid                                 */
    SENSOR_NO_FINGER,      /**< MAX30102: finger not detected                 */
    SENSOR_FAULT,          /**< I2C/driver error; data unreliable             */
    SENSOR_INIT,           /**< Not yet measured                              */
} SensorStatus_t;

/* ========================================================================== *
 *  Heart-rate & SpO2 (MAX30102)
 * ========================================================================== */
typedef struct {
    uint16_t        bpm;            /**< Heart rate in BPM                    */
    uint8_t         spo2;           /**< SpO2 in % (0–100)                    */
    SensorStatus_t  hr_status;
    SensorStatus_t  spo2_status;
    bool            hr_alert;       /**< Tachy/brady alert active             */
    bool            spo2_alert;     /**< Low SpO2 alert active                */
    uint32_t        last_update_tick; /**< xTaskGetTickCount() at last update */
} HeartData_t;

/* ========================================================================== *
 *  Motion & step counter (MPU-6050)
 * ========================================================================== */
typedef enum {
    ACTIVITY_UNKNOWN    = 0,
    ACTIVITY_STATIONARY,
    ACTIVITY_WALKING,
    ACTIVITY_RUNNING,
} ActivityState_t;

typedef struct {
    /* Raw IMU */
    float           accel_x;        /**< g                                   */
    float           accel_y;
    float           accel_z;
    float           gyro_x;         /**< deg/s                               */
    float           gyro_y;
    float           gyro_z;
    /* Derived */
    uint32_t        steps;          /**< Daily step count                    */
    float           distance_m;     /**< Estimated distance in metres        */
    float           calories_kcal;  /**< Estimated calories                  */
    ActivityState_t activity;
    SensorStatus_t  status;
    uint32_t        last_update_tick;
} MotionData_t;

/* ========================================================================== *
 *  Software clock (no external RTC)
 * ========================================================================== */
typedef struct {
    uint8_t hours;          /**< 0–23                                        */
    uint8_t minutes;        /**< 0–59                                        */
    uint8_t seconds;        /**< 0–59                                        */
} SoftClock_t;

/* ========================================================================== *
 *  Battery charge state (from TP4057 status pins)
 * ========================================================================== */
typedef enum {
    BATT_UNKNOWN    = 0,   /**< Pins not yet read                            */
    BATT_DISCHARGING,      /**< Normal use, no charger connected             */
    BATT_CHARGING,         /**< CHRG pin LOW = charge in progress            */
    BATT_FULL,             /**< STDBY pin LOW = charge complete              */
} BattChargeState_t;

typedef struct {
    uint8_t           bars;        /**< 0–4 bars derived from VREF ADC        */
    BattChargeState_t charge;      /**< Charging / full / discharging         */
} BatteryStatus_t;

/* ========================================================================== *
 *  Stopwatch
 * ========================================================================== */
typedef struct {
    bool      running;      /**< Timer is counting                           */
    uint32_t  elapsed_ms;   /**< Total elapsed time in ms                    */
    uint32_t  start_tick;   /**< xTaskGetTickCount() when last started       */
} StopwatchData_t;

/* ========================================================================== *
 *  7-day statistics (ring buffer, resets every Monday)
 * ========================================================================== */
#include "app_config.h"
typedef struct {
    uint32_t  daily_steps[STATS_DAYS];  /**< Steps per day [0]=oldest        */
    uint16_t  daily_hr_avg[STATS_DAYS]; /**< Avg BPM per day                 */
    uint8_t   day_index;                /**< Current day slot (0–6)          */
    uint8_t   days_recorded;            /**< How many days have valid data    */
} WeekStats_t;

/* ========================================================================== *
 *  Bluetooth / connectivity status
 * ========================================================================== */
typedef struct {
    bool    connected;      /**< JDY-31 connection state                     */
    uint8_t buffered_count; /**< Packets buffered while disconnected         */
} BleStatus_t;

/* ========================================================================== *
 *  Master shared data block
 * ========================================================================== */
typedef struct {
    HeartData_t     heart;
    MotionData_t    motion;
    SoftClock_t     clock;
    BleStatus_t     ble;
    BatteryStatus_t battery;
    StopwatchData_t stopwatch;
    WeekStats_t     stats;
} SharedData_t;

/* ========================================================================== *
 *  FreeRTOS IPC Object Declarations
 *  (defined in sensor_data.c, created before scheduler starts)
 * ========================================================================== */

/** Mutex protecting SharedData_t gSharedData */
extern osMutexId xSensorDataMutex;

/** Mutex protecting the shared I2C1 bus (MPU-6050 + MAX30102) */
extern osMutexId i2cMutexHandle;

/** Queue: buttonTask → uiTask, powerTask.  Item = ButtonEvent_t (button.h) */
extern osMessageQId xButtonEventQueue;

/** Queue: sensorTask → bleTask.  Item = BlePacket_t (jdy31.h) */
extern osMessageQId xBleQueue;

/** Queue: buttonTask / sensorTask → powerTask.  Item = PowerEvent_t (power_manager.h) */
extern osMessageQId xPowerEventQueue;

/* ========================================================================== *
 *  Global data instance (lock with xSensorDataMutex before access)
 * ========================================================================== */
extern SharedData_t gSharedData;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Create all IPC objects and zero-initialise shared data.
 *         Call BEFORE osKernelStart().
 */
void Sensor_Data_Init(void);

/**
 * @brief  Safely copy a snapshot of heart data (acquires mutex internally).
 * @param  out  Pointer to caller-allocated HeartData_t to fill.
 * @return true if mutex acquired and data copied, false on timeout.
 */
bool Sensor_Data_GetHeart(HeartData_t *out);

/**
 * @brief  Safely copy a snapshot of motion data.
 */
bool Sensor_Data_GetMotion(MotionData_t *out);

/**
 * @brief  Safely update heart data (called by sensorTask).
 */
bool Sensor_Data_SetHeart(const HeartData_t *in);

/**
 * @brief  Safely update motion data (called by sensorTask).
 */
bool Sensor_Data_SetMotion(const MotionData_t *in);

/**
 * @brief  Update software clock (called by defaultTask / bleTask SYNC_TIME).
 */
void Sensor_Data_SetClock(uint8_t h, uint8_t m, uint8_t s);

/**
 * @brief  Get current software clock (thread-safe, atomic read).
 */
SoftClock_t Sensor_Data_GetClock(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_DATA_H */
