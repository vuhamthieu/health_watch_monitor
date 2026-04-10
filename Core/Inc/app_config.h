/* app_config.h - all hardware config: pins, I2C addresses, timeouts
 * change these to match your board wiring */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* clock / system */
#define APP_SYSCLK_MHZ          64u     /**< System clock (MHz)               */
#define APP_TICK_RATE_HZ        1000u   /**< FreeRTOS tick (configTICK_RATE_HZ)*/

/* buttons - active LOW, internal pull-up */
#define BTN_BACK_PORT           GPIOA
#define BTN_BACK_PIN            GPIO_PIN_0   /**< PA0 – WKUP, EXTI0            */

#define BTN_DOWN_PORT           GPIOA
#define BTN_DOWN_PIN            GPIO_PIN_2   /**< PA2 – polled                 */

#define BTN_UP_PORT             GPIOA
#define BTN_UP_PIN              GPIO_PIN_3   /**< PA3 – polled                 */

#define BTN_SELECT_PORT         GPIOA
#define BTN_SELECT_PIN          GPIO_PIN_15  /**< PA15 – polled                */

/* Button timing (milliseconds) */
#define BTN_DEBOUNCE_MS         20u     /**< Debounce filter window            */
#define BTN_LONG_PRESS_MS       2000u   /**< Long-press: back button menu      */
#define BTN_POWER_PRESS_MS      5000u   /**< 5 s hold BACK on homescreen = power menu */
#define BTN_REPEAT_DELAY_MS     500u    /**< Auto-repeat initial delay         */
#define BTN_REPEAT_INTERVAL_MS  150u    /**< Auto-repeat interval              */

/* LED - PC13 active LOW */
#define LED_PORT                GPIOC
#define LED_PIN                 GPIO_PIN_13  /**< PC13 – active LOW            */
#define LED_ON()                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET)
#define LED_OFF()               HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET)
#define LED_TOGGLE()            HAL_GPIO_TogglePin(LED_PORT, LED_PIN)

/* I2C - bus handle + sensor addresses */
extern I2C_HandleTypeDef hi2c1;
#define APP_I2C_HANDLE          hi2c1

#define OLED_I2C_ADDR           (0x3C << 1) /**< SH1106 – SA0 = GND → 0x3C   */
/* If SA0 pin is tied HIGH, use: #define OLED_I2C_ADDR (0x3D << 1) */

#define MPU6050_I2C_ADDR        (0x68 << 1) /**< AD0 = GND → 0x68            */
/* If AD0 pin is tied HIGH, use: #define MPU6050_I2C_ADDR (0x69 << 1) */

#define MAX30102_I2C_ADDR       (0x57 << 1) /**< Fixed address                */

#define I2C_TIMEOUT_MS          100u    /**< HAL I2C timeout                  */

/* UART1 - JDY-31 BT */
extern UART_HandleTypeDef huart1;
#define BLE_UART_HANDLE         huart1
#define BLE_UART_BAUD           9600u   /**< JDY-31 default baud rate         */
#define BLE_RX_BUF_SIZE         128u    /**< UART RX ring buffer size         */
#define BLE_TX_BUF_SIZE         128u    /**< UART TX buffer size              */
#define BLE_PACKET_INTERVAL_MS  5000u   /**< Periodic data send interval      */
#define BLE_DEVICE_NAME         "HealthWatch"
#define BLE_PAIR_PIN            "1234"  /**< JDY-31 default pairing PIN        */

/* Debug logging over UART1 (shared with JDY-31). Keep OFF in normal BLE use. */
#define APP_ENABLE_UART_DEBUG   0u

/* OLED */
#define OLED_WIDTH              128u
#define OLED_HEIGHT             64u
#define OLED_PAGES              8u      /**< 64 / 8 = 8 pages                 */

/* MPU-6050 */
#define MPU6050_SAMPLE_RATE_HZ  100u    /**< Accelerometer sampling rate      */
#define MPU6050_ACCEL_FSR       2u      /**< Full-scale range: ±2g            */
#define MPU6050_GYRO_FSR        250u    /**< Full-scale range: ±250 deg/s     */

/* PPG background policy (MAX30102)
 * PASSIVE: collect for WINDOW_MS, then cooldown for PERIOD_MS.
 * ACTIVE: continuous collection while workout is active. */
#define PPG_PASSIVE_PERIOD_MS  180000u /**< 3 min cooldown between passive windows */
#define PPG_PASSIVE_WINDOW_MS   20000u /**< 20 s passive measurement window         */
#define PPG_ACTIVE_PERIOD_MS     1000u /**< reserved for active-mode tuning         */

/* Wake gesture: magnitude of acc change above this = wrist raise */
#define MPU6050_WAKE_THRESH     0.5f    /**< g (above 1g baseline)            */
#define MPU6050_WAKE_COUNT      3u      /**< Consecutive detections required  */

/* Step counter thresholds */
#define STEP_ACCEL_THRESHOLD    1.05f   /**< g — lower = more sensitive wrist waving  */
#define STEP_MIN_INTERVAL_MS    300u    /**< Min time between steps (200 bpm max)     */
#define STEP_STRIDE_LENGTH_M    0.75f   /**< Default stride length in metres  */
#define STEP_CALORIE_PER_STEP   0.04f   /**< kcal per step (approximate)      */
#define STEP_DAILY_GOAL         10000u  /**< Daily step goal                  */

/* MAX30102 */
#define MAX30102_SAMPLE_RATE_HZ 100u    /**< Sensor sample rate setting       */
#define MAX30102_FIFO_DEPTH     32u
#define MAX30102_IR_MIN_VALID   50000u  /**< Min IR value = finger present    */
#define HR_UPDATE_PERIOD_MS     1000u   /**< Heart rate output update period  */
#define SPO2_UPDATE_PERIOD_MS   2000u   /**< SpO2 output update period        */
#define HR_VALID_MIN            40u     /**< Min valid BPM                    */
#define HR_VALID_MAX            200u    /**< Max valid BPM                    */
#define SPO2_VALID_MIN          80u     /**< Min valid SpO2 %                 */
#define HR_TACHY_THRESHOLD      120u    /**< Tachycardia alert threshold      */
#define HR_BRADY_THRESHOLD      50u     /**< Bradycardia alert threshold      */
#define SPO2_LOW_THRESHOLD      95u     /**< Low SpO2 alert threshold         */

/* power */
#define POWER_DIM_TIMEOUT_MS    15000u  /**< Inactivity → dim display         */
#define POWER_SLEEP_TIMEOUT_MS  60000u  /**< Inactivity → sleep mode          */
#define POWER_OLED_DIM_CONTRAST 50u     /**< Display contrast in dim mode     */
#define POWER_OLED_FULL_CONTRAST 0xCF   /**< Display contrast in active mode  */
/* Diagnostic mode: keep OLED always on while debugging display issues. */
#define POWER_DISABLE_AUTO_SLEEP 0u

/* UI */
#define UI_REFRESH_PERIOD_MS    100u    /**< OLED refresh rate (10 Hz max)    */

/* Battery — VREF-based estimation (no external pin needed)
 * ADC1 channel 17 = internal VREF (1.20 V typical on STM32F103).
 * We compare VREF counts to VDD to infer VDD; if VDD is low the battery is low.
 * LiPo typical: 4.2 V (full) → 3.0 V (empty). Board has 3.3 V LDO, so VDD
 * stays at 3.3 V until LDO drops out ~3.5 V → VDD starts to sag.
 * These thresholds are approximate — tune after measuring your specific board. */
#define BATT_VREF_IDEAL_COUNTS  1550u  /**< VREF counts when VDD = 3.3 V (nom) */
#define BATT_VREF_LOW_COUNTS    1700u  /**< VREF counts when VDD ≈ 3.1 V (low) */
#define BATT_VREF_CRIT_COUNTS   1850u  /**< VREF counts when VDD ≈ 2.85 V (crit)*/

/* TP4057 status pins — active LOW, input with pull-up
 * Connect TP4057 CHRG pin → PA5, STDBY pin → PA6 */
#define TP4057_CHRG_PORT        GPIOA
#define TP4057_CHRG_PIN         GPIO_PIN_5   /**< PA5 – LOW = charging         */
#define TP4057_STDBY_PORT       GPIOA
#define TP4057_STDBY_PIN        GPIO_PIN_6   /**< PA6 – LOW = charge complete  */

/* Battery bar icon levels */
#define BATT_BARS_FULL          4u
#define BATT_BARS_HIGH          3u
#define BATT_BARS_MED           2u
#define BATT_BARS_LOW           1u
#define BATT_BARS_EMPTY         0u

/* Workout / statistics */
#define STATS_DAYS              7u      /**< Keep 7 days of daily step data   */
#define STATS_HR_SAMPLES        24u     /**< Keep 24 hourly HR averages       */
#define WORKOUT_PUSHUP_THRESH   1.8f    /**< g threshold for push-up detect   */
#define WORKOUT_RUN_STEP_HZ     2.5f    /**< Steps/s threshold: walk→run      */

/* Stopwatch */
#define STOPWATCH_TIMER_PERIOD_MS  10u  /**< Stopwatch tick resolution (10 ms)*/

/* IWDG */
#define IWDG_TIMEOUT_MS         2000u   /**< IWDG window — must feed < 2s     */

#ifdef __cplusplus
}
#endif

#endif /* __APP_CONFIG_H */
