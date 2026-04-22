/* main.c — FreeRTOS task startup, peripheral init, battery reading */
#include "main.h"
#include "cmsis_os.h"
#include "task.h"

#include "sensor_data.h"
#include "button.h"
#include "ui_menu.h"
#include "power_manager.h"
#include "jdy31.h"
#include "mpu6050.h"
#include "max30102.h"
#include "heart_rate.h"
#include "spo2.h"
#include "step_counter.h"
#include "app_config.h"
#include "oled.h"
#include "sh1106.h"
#include <stdio.h>

/* Retarget printf ->UART1 (PA9=TX, 9600 baud) — connect CH340 RX to PA9 */
int __io_putchar(int ch)
{
#if APP_ENABLE_UART_DEBUG
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
#endif
    return ch;
}

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

osThreadId defaultTaskHandle;
osThreadId uiTaskHandle;
osThreadId sensorTaskHandle;
osThreadId bleTaskHandle;
osThreadId buttonTaskHandle;
osThreadId powerTaskHandle;
osMutexId i2cMutexHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);
void StartTask03(void const * argument);
void StartTask04(void const * argument);
void StartTask05(void const * argument);
void StartTask06(void const * argument);

/* USER CODE BEGIN PFP */
static void     Batt_VREF_ADC_Init(void);
static uint8_t  Batt_VREF_ReadBars(void);
static BattChargeState_t Batt_TP4057_ReadCharge(void);
static void     Boot_I2C_Scan(void);
static void     Boot_OLED_SelfTest(void);
static void     Debug_LogTaskStackWatermarks(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/* Battery via internal VREF (ADC1 ch17): higher raw count = lower Vdd.
 * Thresholds defined in app_config.h (BATT_VREF_*_COUNTS).
 * TP4057 CHRG (PA5) LOW = charging, STDBY (PA6) LOW = charge complete. */
static void Batt_VREF_ADC_Init(void)
{
    /* PCLK2 = 64 MHz → /6 ≈ 10.7 MHz ADC clock (must be ≤14 MHz) */
    RCC->CFGR = (RCC->CFGR & ~(3u << 14u)) | (2u << 14u);
    /* Enable ADC1 clock */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = ADC_CR2_TSVREFE | ADC_CR2_ADON;
    for (volatile uint32_t d = 0; d < 500u; d++); /* tSTAB ≥1 µs */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL);
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);
    ADC1->SQR1  = 0u;
    ADC1->SQR3  = 17u;                /* channel 17 = internal VREF */
    ADC1->SMPR1 = ADC_SMPR1_SMP17;   /* 239.5-cycle sampling */
    ADC1->CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL; /* SW trigger */

    /* TP4057 status pins PA5 (CHRG) and PA6 (STDBY) — input pull-up */
    GPIO_InitTypeDef g = {0};
    g.Pin  = TP4057_CHRG_PIN | TP4057_STDBY_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TP4057_CHRG_PORT, &g);
}

static uint8_t Batt_VREF_ReadBars(void)
{
    ADC1->SR &= ~ADC_SR_EOC;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    uint32_t deadline = HAL_GetTick() + 3u;
    while (!(ADC1->SR & ADC_SR_EOC) && (HAL_GetTick() < deadline));
    if (!(ADC1->SR & ADC_SR_EOC)) return BATT_BARS_EMPTY;
    uint32_t raw = ADC1->DR & 0x0FFFu;
    /* Higher raw = lower Vdd */
    if (raw <= BATT_VREF_IDEAL_COUNTS)  return BATT_BARS_FULL;
    if (raw <= (BATT_VREF_IDEAL_COUNTS + (BATT_VREF_LOW_COUNTS - BATT_VREF_IDEAL_COUNTS) / 3u))
                                         return BATT_BARS_HIGH;
    if (raw <= (BATT_VREF_IDEAL_COUNTS + 2u * (BATT_VREF_LOW_COUNTS - BATT_VREF_IDEAL_COUNTS) / 3u))
                                         return BATT_BARS_MED;
    if (raw <= BATT_VREF_LOW_COUNTS)     return BATT_BARS_LOW;
    return BATT_BARS_EMPTY;
}

static BattChargeState_t Batt_TP4057_ReadCharge(void)
{
    bool chrg  = (HAL_GPIO_ReadPin(TP4057_CHRG_PORT,  TP4057_CHRG_PIN)  == GPIO_PIN_RESET);
    bool stdby = (HAL_GPIO_ReadPin(TP4057_STDBY_PORT, TP4057_STDBY_PIN) == GPIO_PIN_RESET);
    if (stdby)  return BATT_FULL;
    if (chrg)   return BATT_CHARGING;
    return BATT_DISCHARGING;
}

static void Boot_I2C_Scan(void)
{
  static const uint8_t expected[] = { 0x3C, 0x57, 0x68, 0x69 };
  bool seen[4] = { false, false, false, false };

  printf("[I2C] scan start\r\n");
  for (uint8_t addr = 1u; addr < 128u; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2u, 5u) == HAL_OK) {
      printf("[I2C] found 0x%02X\r\n", (unsigned)addr);
      for (uint8_t i = 0u; i < 4u; i++) {
        if (addr == expected[i]) {
          seen[i] = true;
        }
      }
    }
  }

  printf("[I2C] OLED(0x3C): %s\r\n", seen[0] ? "OK" : "MISS");
  printf("[I2C] MAX30102(0x57): %s\r\n", seen[1] ? "OK" : "MISS");
  printf("[I2C] MPU6050(0x68/0x69): %s\r\n", (seen[2] || seen[3]) ? "OK" : "MISS");
}

static void Boot_OLED_SelfTest(void)
{
  if (SH1106_Init() != SH1106_OK) {
    printf("[OLED] init fail\r\n");
    return;
  }

  SH1106_Fill(1u);
  SH1106_Flush();
  HAL_Delay(180);
  SH1106_Clear();
  SH1106_Flush();
  HAL_Delay(120);
  printf("[OLED] self-test done\r\n");
}

static void Debug_LogTaskStackWatermarks(void)
{
#if APP_ENABLE_UART_DEBUG
  const UBaseType_t min_words = uxTaskGetStackHighWaterMark(NULL);
  const UBaseType_t ui_words = uxTaskGetStackHighWaterMark((TaskHandle_t)uiTaskHandle);
  const UBaseType_t sensor_words = uxTaskGetStackHighWaterMark((TaskHandle_t)sensorTaskHandle);
  const UBaseType_t ble_words = uxTaskGetStackHighWaterMark((TaskHandle_t)bleTaskHandle);
  const UBaseType_t button_words = uxTaskGetStackHighWaterMark((TaskHandle_t)buttonTaskHandle);
  const UBaseType_t power_words = uxTaskGetStackHighWaterMark((TaskHandle_t)powerTaskHandle);

  printf("[STACK] default=%luB ui=%luB sensor=%luB ble=%luB button=%luB power=%luB\r\n",
       (unsigned long)(min_words * sizeof(StackType_t)),
       (unsigned long)(ui_words * sizeof(StackType_t)),
       (unsigned long)(sensor_words * sizeof(StackType_t)),
       (unsigned long)(ble_words * sizeof(StackType_t)),
       (unsigned long)(button_words * sizeof(StackType_t)),
       (unsigned long)(power_words * sizeof(StackType_t)));
#endif
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  Boot_I2C_Scan();
  Boot_OLED_SelfTest();
  Batt_VREF_ADC_Init();
  Sensor_Data_Init();
  /* USER CODE END 2 */

  osMutexDef(i2cMutex);
  i2cMutexHandle = osMutexCreate(osMutex(i2cMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */
  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */
  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */
  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal,      0, 256);
  defaultTaskHandle  = osThreadCreate(osThread(defaultTask),  NULL);
  osThreadDef(uiTask,      StartTask02,      osPriorityNormal,      0, 512);
  uiTaskHandle       = osThreadCreate(osThread(uiTask),       NULL);
  osThreadDef(sensorTask,  StartTask03,      osPriorityAboveNormal, 0, 512);
  sensorTaskHandle   = osThreadCreate(osThread(sensorTask),   NULL);
  osThreadDef(bleTask,     StartTask04,      osPriorityBelowNormal, 0, 512);
  bleTaskHandle      = osThreadCreate(osThread(bleTask),      NULL);
  osThreadDef(buttonTask,  StartTask05,      osPriorityNormal,      0, 256);
  buttonTaskHandle   = osThreadCreate(osThread(buttonTask),   NULL);
  osThreadDef(powerTask,   StartTask06,      osPriorityHigh,        0, 256);
  powerTaskHandle    = osThreadCreate(osThread(powerTask),    NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */

  osKernelStart();

  while (1) {}
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}

static void MX_I2C1_Init(void)
{
  /* USER CODE BEGIN I2C1_Init 0 */
  /* USER CODE END I2C1_Init 0 */
  /* USER CODE BEGIN I2C1_Init 1 */
  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;  /* Standard mode — improves bus robustness */
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
  /* USER CODE BEGIN I2C1_Init 2 */
  /* USER CODE END I2C1_Init 2 */
}

static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */
  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BTN_BACK_PIN) {
        Button_EXTI_Callback(); /* wake from sleep / notify power manager */
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        JDY31_UartRxCplt();
    }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  (void)argument;
  uint8_t  half    = 0;
  uint16_t bat_cnt = 0;
  uint8_t  stack_cnt = 0;
  for(;;)
  {
    LED_TOGGLE();
    if (++half >= 2u) {
      half = 0;
      /* tick software clock every 1 s */
      SoftClock_t c = Sensor_Data_GetClock();
      if (++c.seconds >= 60u) { c.seconds = 0;
        if (++c.minutes >= 60u) { c.minutes = 0;
          if (++c.hours >= 24u) c.hours = 0;
        }
      }
      Sensor_Data_SetClock(c.hours, c.minutes, c.seconds);
      /* read battery every 30 s */
      if (++bat_cnt >= 30u) {
        bat_cnt = 0;
        uint8_t          bars   = Batt_VREF_ReadBars();
        BattChargeState_t charge = Batt_TP4057_ReadCharge();
        if (osMutexWait(xSensorDataMutex, 5) == osOK) {
          gSharedData.battery.bars   = bars;
          gSharedData.battery.charge = charge;
          osMutexRelease(xSensorDataMutex);
        }
      }

      if (++stack_cnt >= 10u) {
        stack_cnt = 0;
        Debug_LogTaskStackWatermarks();
      }
    }
    osDelay(500);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask02 */
/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  UI_Task(argument);
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/* USER CODE END Header_StartTask03 */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StarItTask03 */
  (void)argument;
  TickType_t last_wake = xTaskGetTickCount();
  enum PPG_MODE_OFF;
  enum PPG_MODE_PASSIVE;
  enum PPG_MODE_ACTIVE;

  /* Wait for UI task to finish OLED init and show splash */
  osDelay(2000);

  /* Init sensors — brief mutex grabs, no long holds */
  osMutexWait(i2cMutexHandle, osWaitForever);
  MPU6050_Status_t  mpu_status = MPU6050_Init();
  osMutexRelease(i2cMutexHandle);

  osDelay(5);

  osMutexWait(i2cMutexHandle, osWaitForever);
  MAX30102_Status_t max_status = MAX30102_Init();
  if (max_status == MAX30102_OK) {
      MAX30102_FlushFIFO();
  }
  osMutexRelease(i2cMutexHandle);

  printf("[BOOT] MPU6050: %s (addr=0x%02X, whoami=0x%02X)\r\n",
      mpu_status == MPU6050_OK      ? "OK" :
      mpu_status == MPU6050_ERR_I2C ? "NOT FOUND on 0x68 or 0x69" : "ERR_WHOAMI",
      (unsigned)MPU6050_GetAddr(),
      (unsigned)MPU6050_GetWhoAmI());
  printf("[BOOT] MAX30102: %s\r\n",
      max_status == MAX30102_OK         ? "OK" :
      max_status == MAX30102_ERR_I2C    ? "NOT FOUND (check wiring)" : "ERR_PARTID");
  if (max_status == MAX30102_OK) {
      osMutexWait(i2cMutexHandle, osWaitForever);
      MAX30102_DumpRegs(); /* print mode/LED/FIFO registers to verify config */
      osMutexRelease(i2cMutexHandle);
  }

  /* Skip MPU calibration at boot — avoids holding I2C mutex for 1s.
   * Small accel bias is acceptable for step counting. */

  bool mpu_ok = (mpu_status == MPU6050_OK);
  bool max_ok = (max_status == MAX30102_OK);

  HR_Reset();
  SpO2_Reset();
  StepCounter_Reset();

  MAX30102_Sample_t fifo_buf[16];
  uint8_t  fifo_cnt  = 0;
  uint32_t loop_cnt  = 0u;
  uint32_t max_empty = 0u; /* consecutive empty FIFO reads — triggers reinit */
  uint32_t trend_tick = osKernelSysTick();
  uint32_t trend_last_steps = 0u;

  bool raise_wake_enabled = false;
  bool fall_detect_enabled = false;
  uint8_t wake_count = 0u;
  float prev_accel_mag = 1.0f;

  uint8_t fall_phase = 0u;      /* 0=idle, 1=freefall seen, 2=impact seen */
  uint8_t freefall_count = 0u;  /* consecutive samples below freefall threshold */
  uint32_t freefall_tick = 0u;
  uint32_t impact_tick = 0u;
  uint32_t still_start_tick = 0u;
  uint32_t last_fall_tick = 0u;
  bool pushup_peak = false;
  uint32_t last_pushup_tick = 0u;
  typedef enum {
    PPG_MODE_OFF = 0,
    PPG_MODE_PASSIVE,
    PPG_MODE_ACTIVE,
  } PpgMode_t;

  PpgMode_t ppg_mode = PPG_MODE_PASSIVE;
  PpgMode_t ppg_prev_mode = PPG_MODE_OFF;
  bool ppg_collect_enabled = false;
  bool ppg_workout_active = false;
  bool ppg_ui_active = false;
  uint32_t ppg_next_start_tick = osKernelSysTick();
  uint32_t ppg_window_end_tick = 0u;

  for (;;)
  {
      if (mpu_ok) {
          MPU6050_Data_t imu;
          osMutexWait(i2cMutexHandle, osWaitForever);
          bool imu_ok = (MPU6050_Read(&imu) == MPU6050_OK);
          osMutexRelease(i2cMutexHandle);

          if (imu_ok) {
            if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
              raise_wake_enabled = gSharedData.settings.raise_to_wake;
              fall_detect_enabled = gSharedData.settings.fall_detect;
              osMutexRelease(xSensorDataMutex);
            }

              StepCounter_Update(imu.accel_g[0], imu.accel_g[1], imu.accel_g[2]);

              /* Workout session updates (separate counters from home daily steps). */
              {
                uint32_t total_steps = StepCounter_GetSteps();
                uint32_t now_tick = osKernelSysTick();
                const float PUSHUP_RELEASE_G = 1.10f;
                const uint32_t PUSHUP_MIN_INTERVAL_MS = 450u;

                if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                  if (gSharedData.workout.active) {
                    uint8_t mode = gSharedData.workout.mode;
                    if (mode == (uint8_t)WORKOUT_PUSHUPS) {
                      if (!pushup_peak &&
                          imu.accel_magnitude >= WORKOUT_PUSHUP_THRESH &&
                          (now_tick - last_pushup_tick) >= PUSHUP_MIN_INTERVAL_MS) {
                        pushup_peak = true;
                      }
                      if (pushup_peak && imu.accel_magnitude <= PUSHUP_RELEASE_G) {
                        pushup_peak = false;
                        gSharedData.workout.pushup_reps++;
                        last_pushup_tick = now_tick;
                      }
                    } else {
                      pushup_peak = false;
                      uint32_t base = gSharedData.workout.start_total_steps;
                      gSharedData.workout.session_steps =
                          (total_steps >= base) ? (total_steps - base) : 0u;
                    }
                  } else {
                    pushup_peak = false;
                  }
                  osMutexRelease(xSensorDataMutex);
                }
              }

            /* Raise-to-wake: detect a wrist-raise movement while display is sleeping. */
            {
              const float WAKE_DELTA_MIN = (MPU6050_WAKE_THRESH * 0.55f);
              const float WAKE_AXIS_Y_MIN = 0.20f;
              const float WAKE_AXIS_Z_MIN = 0.65f;

              float mag = imu.accel_magnitude;
              float delta = mag - prev_accel_mag;
              float ay = imu.accel_g[1];
              float az = imu.accel_g[2];
              if (delta < 0.0f) delta = -delta;
              if (ay < 0.0f) ay = -ay;
              if (az < 0.0f) az = -az;
              prev_accel_mag = mag;

                if (raise_wake_enabled && Power_GetState() == POWER_SLEEP) {
                  if ((delta >= WAKE_DELTA_MIN) && ((ay >= WAKE_AXIS_Y_MIN) || (az >= WAKE_AXIS_Z_MIN))) {
                    if (wake_count < 255u) wake_count++;
                } else {
                    if (wake_count > 0u) wake_count--;
                }

                if (wake_count >= MPU6050_WAKE_COUNT) {
                  wake_count = 0u;
                  Power_PostEvent(POWER_EVT_WRIST_RAISE, false);
                }
              } else {
                wake_count = 0u;
              }
            }

            /* Conservative fall detection (to reduce false positives):
             * free-fall (<0.45g) -> impact (>2.2g) within 0.9s -> stillness around 1g for 1.2s. */
            {
              const float FALL_FREEFALL_G = 0.45f;
              const float FALL_IMPACT_G = 2.20f;
              const float FALL_STILL_BAND_G = 0.18f;
              const uint8_t FALL_FREEFALL_MIN_COUNT = 3u; /* reject single-sample glitches */
              const uint32_t FREEFALL_TO_IMPACT_MS = 900u;
              const uint32_t STILLNESS_CONFIRM_MS = 1200u;
              const uint32_t IMPACT_TIMEOUT_MS = 4000u;
              const uint32_t FALL_COOLDOWN_MS = 10000u;

              uint32_t now_tick = osKernelSysTick();
              float mag = imu.accel_magnitude;
              float dev_from_1g = mag - 1.0f;
              if (dev_from_1g < 0.0f) dev_from_1g = -dev_from_1g;

              if (!fall_detect_enabled) {
                fall_phase = 0u;
                still_start_tick = 0u;
                freefall_count = 0u;
              } else if ((now_tick - last_fall_tick) >= FALL_COOLDOWN_MS) {
                if (fall_phase == 0u) {
                  if (mag < FALL_FREEFALL_G) {
                    if (freefall_count == 0u) {
                      freefall_tick = now_tick;
                    }
                    if (freefall_count < 255u) {
                      freefall_count++;
                    }
                    if (freefall_count >= FALL_FREEFALL_MIN_COUNT) {
                      fall_phase = 1u;
                      /* freefall_tick remains the time freefall started (1st low-g sample) */
                    }
                  } else {
                    freefall_count = 0u;
                  }
                } else if (fall_phase == 1u) {
                  if ((now_tick - freefall_tick) > FREEFALL_TO_IMPACT_MS) {
                    fall_phase = 0u;
                    freefall_count = 0u;
                  } else if (mag > FALL_IMPACT_G) {
                    fall_phase = 2u;
                    impact_tick = now_tick;
                    still_start_tick = 0u;
                    freefall_count = 0u;
                  }
                } else {
                  if ((now_tick - impact_tick) > IMPACT_TIMEOUT_MS) {
                    fall_phase = 0u;
                    still_start_tick = 0u;
                    freefall_count = 0u;
                  } else if (dev_from_1g <= FALL_STILL_BAND_G) {
                    if (still_start_tick == 0u) {
                      still_start_tick = now_tick;
                    } else if ((now_tick - still_start_tick) >= STILLNESS_CONFIRM_MS) {
                      fall_phase = 0u;
                      still_start_tick = 0u;
                      freefall_count = 0u;
                      last_fall_tick = now_tick;
                      if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                        gSharedData.motion.fall_detected = true;
                        gSharedData.motion.last_fall_tick = now_tick;
                        osMutexRelease(xSensorDataMutex);
                      }
                      Power_PostEvent(POWER_EVT_WRIST_RAISE, false);
                    }
                  } else {
                    still_start_tick = 0u;
                  }
                }
              }
            }

              if ((loop_cnt % 10u) == 0u) {
                  int activity = StepCounter_GetActivityState();
                  if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                      gSharedData.motion.steps         = StepCounter_GetSteps();
                      gSharedData.motion.distance_m    = StepCounter_GetDistance();
                      gSharedData.motion.calories_kcal = StepCounter_GetCalories();
                      gSharedData.motion.activity      = (ActivityState_t)activity;
                      gSharedData.motion.status        = SENSOR_OK;
                gSharedData.motion.last_update_tick = osKernelSysTick();
                      osMutexRelease(xSensorDataMutex);
                  }
              }
          } else {
              if (gSharedData.motion.status != SENSOR_FAULT) {
                  if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                      gSharedData.motion.status = SENSOR_FAULT;
                      osMutexRelease(xSensorDataMutex);
                  }
              }
          }
      }

          if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
            ppg_workout_active = gSharedData.workout.active;
            ppg_ui_active = gSharedData.settings.ppg_force_active;
            osMutexRelease(xSensorDataMutex);
          }

        {
          uint32_t now_tick = osKernelSysTick();
          if (Power_GetState() == POWER_SLEEP) {
            ppg_mode = PPG_MODE_OFF;
            } else if (ppg_workout_active || ppg_ui_active) {
            ppg_mode = PPG_MODE_ACTIVE;
          } else {
            ppg_mode = PPG_MODE_PASSIVE;
          }

          if (ppg_mode != ppg_prev_mode) {
            if (ppg_mode == PPG_MODE_OFF) {
              ppg_collect_enabled = false;
              ppg_window_end_tick = 0u;
            } else if (ppg_mode == PPG_MODE_ACTIVE) {
              ppg_collect_enabled = true;
              osMutexWait(i2cMutexHandle, osWaitForever);
              MAX30102_FlushFIFO();
              osMutexRelease(i2cMutexHandle);
              HR_Reset();
              SpO2_Reset();
            } else {
              ppg_collect_enabled = false;
              ppg_next_start_tick = now_tick;
              ppg_window_end_tick = 0u;
            }
            ppg_prev_mode = ppg_mode;
          }

          if (ppg_mode == PPG_MODE_PASSIVE) {
            if (!ppg_collect_enabled && (now_tick >= ppg_next_start_tick)) {
              ppg_collect_enabled = true;
              ppg_window_end_tick = now_tick + PPG_PASSIVE_WINDOW_MS;
              osMutexWait(i2cMutexHandle, osWaitForever);
              MAX30102_FlushFIFO();
              osMutexRelease(i2cMutexHandle);
              HR_Reset();
              SpO2_Reset();
            }

            if (ppg_collect_enabled && (now_tick >= ppg_window_end_tick)) {
              ppg_collect_enabled = false;
              ppg_next_start_tick = now_tick + PPG_PASSIVE_PERIOD_MS;
            }
          }

          if (ppg_mode == PPG_MODE_OFF) {
            ppg_collect_enabled = false;
            ppg_window_end_tick = 0u;
          }
        }

        if (max_ok && ppg_collect_enabled) {
          osMutexWait(i2cMutexHandle, osWaitForever);
          MAX30102_Status_t fs = MAX30102_ReadFIFO(fifo_buf, 16, &fifo_cnt);
          osMutexRelease(i2cMutexHandle);

          /* Print raw FIFO data every 2 s (200 loops) to diagnose IR=0 */
          if ((loop_cnt % 200u) == 1u) {
              osMutexWait(i2cMutexHandle, osWaitForever);
              MAX30102_DumpRegs();
              osMutexRelease(i2cMutexHandle);
              printf("[MAX-FIFO] fs=%u cnt=%u ir0=%lu red0=%lu\r\n",
                  (unsigned)fs, (unsigned)fifo_cnt,
                  fifo_cnt ? (unsigned long)fifo_buf[0].ir  : 0UL,
                  fifo_cnt ? (unsigned long)fifo_buf[0].red : 0UL);
          }

          if (fs == MAX30102_OK && fifo_cnt > 0) {
              max_empty = 0u;
              for (uint8_t s = 0; s < fifo_cnt; s++) {
                  HR_AddSample(fifo_buf[s].ir);
                  SpO2_AddSample(fifo_buf[s].red, fifo_buf[s].ir);
              }
              if ((loop_cnt % 50u) == 0u) {
                  uint16_t bpm  = 0;
                  uint8_t  spo2 = 0;
                  bool hr_valid   = HR_GetBPM(&bpm);
                  bool spo2_valid = SpO2_GetValue(&spo2);
                  bool finger     = HR_FingerPresent();
                  bool tach, brad;
                  HR_GetAlertStatus(&tach, &brad);

                    if (!finger && ppg_mode == PPG_MODE_PASSIVE) {
                      ppg_collect_enabled = false;
                      ppg_next_start_tick = osKernelSysTick() + (PPG_PASSIVE_PERIOD_MS * 2u);
                    }

                  if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                      gSharedData.heart.bpm         = hr_valid   ? bpm  : 0u;
                      gSharedData.heart.spo2        = spo2_valid ? spo2 : 0u;
                      gSharedData.heart.hr_status   = finger ? (hr_valid   ? SENSOR_OK : SENSOR_INIT) : SENSOR_NO_FINGER;
                      gSharedData.heart.spo2_status = finger ? (spo2_valid ? SENSOR_OK : SENSOR_INIT) : SENSOR_NO_FINGER;
                      gSharedData.heart.hr_alert    = tach || brad;
                      gSharedData.heart.spo2_alert  = SpO2_IsLowAlert();
                      osMutexRelease(xSensorDataMutex);
                  }
              }
          } else {
              /* FIFO empty or I2C error: chip may be in power-down, reinit after 5s */
              if (++max_empty >= 500u) {
                  max_empty = 0u;
                  printf("[MAX] FIFO stuck, reinit...\r\n");
                  osMutexWait(i2cMutexHandle, osWaitForever);
                  max_status = MAX30102_Init();
                  if (max_status == MAX30102_OK) MAX30102_FlushFIFO();
                  MAX30102_DumpRegs();
                  osMutexRelease(i2cMutexHandle);
                  max_ok = (max_status == MAX30102_OK);
              }
          }
              } else if (!ppg_collect_enabled) {
                max_empty = 0u;
      }

      loop_cnt++;

        /* Every 1 s: push rolling trend points for statistics page */
        {
          uint32_t now = osKernelSysTick();
          if ((now - trend_tick) >= 1000u) {
            trend_tick = now;

            uint32_t steps_now = StepCounter_GetSteps();
            uint32_t step_delta = (steps_now >= trend_last_steps) ?
                      (steps_now - trend_last_steps) : 0u;
            trend_last_steps = steps_now;

            int act = StepCounter_GetActivityState();
            uint32_t spm = step_delta * 60u;
            if (spm > 255u) spm = 255u;

            if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
              uint8_t idx = gSharedData.stats.trend_head;

              gSharedData.stats.trend_hr[idx] =
                (gSharedData.heart.hr_status == SENSOR_OK && gSharedData.heart.bpm > 0u)
                ? (uint8_t)((gSharedData.heart.bpm > 255u) ? 255u : gSharedData.heart.bpm)
                : 0u;

              gSharedData.stats.trend_spo2[idx] =
                (gSharedData.heart.spo2_status == SENSOR_OK && gSharedData.heart.spo2 > 0u)
                ? gSharedData.heart.spo2 : 0u;

              gSharedData.stats.trend_walk[idx] = (act == ACTIVITY_WALKING) ? (uint8_t)spm : 0u;
              gSharedData.stats.trend_run[idx]  = (act == ACTIVITY_RUNNING) ? (uint8_t)spm : 0u;

              idx = (uint8_t)((idx + 1u) % STATS_TREND_POINTS);
              gSharedData.stats.trend_head = idx;
              if (gSharedData.stats.trend_count < STATS_TREND_POINTS) {
                gSharedData.stats.trend_count++;
              }

              osMutexRelease(xSensorDataMutex);
            }
          }
        }

      /* Every 5 s: print live sensor values over UART for debugging */
      if ((loop_cnt % 500u) == 0u) {
          if (mpu_ok) {
              MPU6050_Data_t d;
              osMutexWait(i2cMutexHandle, osWaitForever);
              MPU6050_Read(&d);
              osMutexRelease(i2cMutexHandle);
              printf("[MPU] ax=%d ay=%d az=%d steps=%lu (mg)\r\n",
                  (int)(d.accel_g[0]*1000), (int)(d.accel_g[1]*1000), (int)(d.accel_g[2]*1000),
                  (unsigned long)StepCounter_GetSteps());
          } else {
              printf("[MPU] not connected\r\n");
          }
          if (max_ok) {
              printf("[MAX] HR=%u bpm  SpO2=%u%%  finger=%s  IR=%lu\r\n",
                  (unsigned)gSharedData.heart.bpm,
                  (unsigned)gSharedData.heart.spo2,
                  HR_FingerPresent() ? "YES" : "NO",
                  (unsigned long)HR_GetLastIR());
          } else {
              printf("[MAX] not connected\r\n");
          }
      }
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10)); /* 100 Hz */
  }
  /* USER CODE END StartTask03 */
}

/* USER CODE BEGIN Header_StartTask04 */
/* USER CODE END Header_StartTask04 */
void StartTask04(void const * argument)
{
  /* USER CODE BEGIN StartTask04 */
  (void)argument;
  TickType_t last_wake = xTaskGetTickCount();
  if (JDY31_Init() != JDY31_OK) {
    printf("[BLE] JDY31 init failed\r\n");
  } else {
    printf("[BLE] JDY31 init OK\r\n");
  }

  bool prev_ble_enabled = true;
  if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
    prev_ble_enabled = gSharedData.ble.enabled;
    osMutexRelease(xSensorDataMutex);
  }
  if (prev_ble_enabled) {
    JDY31_SendStr("HW BLE READY\r\n");
  }

  uint32_t last_pkt_tick = osKernelSysTick();
  bool last_conn = false;
  char line[64];

  for(;;)
  {
      bool ble_enabled = true;
      if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
        ble_enabled = gSharedData.ble.enabled;
        osMutexRelease(xSensorDataMutex);
      }

      /* Apply state transitions to the JDY module (best-effort). */
      if (ble_enabled != prev_ble_enabled) {
        (void)JDY31_SetEnabled(ble_enabled);
        if (ble_enabled) {
          JDY31_SendStr("HW BLE READY\r\n");
        }
        prev_ble_enabled = ble_enabled;
      }

      if (!ble_enabled) {
        if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
          gSharedData.ble.connected = false;
          gSharedData.ble.buffered_count = 0u;
          osMutexRelease(xSensorDataMutex);
        }
        last_conn = false;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
        continue;
      }

    bool connected = JDY31_IsConnected();
    if (connected != last_conn) {
      printf("[BLE] %s\r\n", connected ? "CONNECTED" : "DISCONNECTED");
      last_conn = connected;
    }

    if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
      gSharedData.ble.connected = connected;
      gSharedData.ble.buffered_count = 0u;
      osMutexRelease(xSensorDataMutex);
    }

    while (JDY31_ReadLine(line, sizeof(line)) > 0u) {
      BleCommand_t cmd;
      BleCommandType_t type = JDY31_ParseCommand(line, &cmd);

      switch (type) {
      case BLE_CMD_SYNC_TIME: {
        uint32_t t = cmd.param % 86400u;
        uint8_t h = (uint8_t)(t / 3600u);
        uint8_t m = (uint8_t)((t % 3600u) / 60u);
        uint8_t s = (uint8_t)(t % 60u);
        Sensor_Data_SetClock(h, m, s);
        JDY31_SendStr("OK\r\n");
        break;
      }
      case BLE_CMD_RESET_STEPS:
        StepCounter_Reset();
        if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
          gSharedData.motion.steps = 0u;
          gSharedData.motion.distance_m = 0.0f;
          gSharedData.motion.calories_kcal = 0.0f;
          osMutexRelease(xSensorDataMutex);
        }
        JDY31_SendStr("OK\r\n");
        break;
      case BLE_CMD_GET_DATA:
      {
        BlePacket_t pkt = {0};
        SoftClock_t clk = Sensor_Data_GetClock();
        if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
          pkt.bpm   = gSharedData.heart.bpm;
          pkt.spo2  = gSharedData.heart.spo2;
          pkt.steps = gSharedData.motion.steps;
          osMutexRelease(xSensorDataMutex);
        }
        pkt.hh = clk.hours;
        pkt.mm = clk.minutes;
        JDY31_SendPacket(&pkt);
        break;
      }
      case BLE_CMD_UNKNOWN:
        JDY31_SendStr("ERR:UNKNOWN_CMD\r\n");
        break;
      default:
        break;
      }
    }

    uint32_t now = osKernelSysTick();
    /* Send periodic packets whenever Bluetooth feature is enabled.
     * Relying on RX activity to infer 'connected' can stop updates if the phone
     * app is passive (receives only). Sending anyway is safe; if no phone is
     * connected, the module will just drop/ignore the UART payload.
     */
    if (((now - last_pkt_tick) >= BLE_PACKET_INTERVAL_MS)) {
      BlePacket_t pkt = {0};
      SoftClock_t clk = Sensor_Data_GetClock();
      if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
        pkt.bpm   = gSharedData.heart.bpm;
        pkt.spo2  = gSharedData.heart.spo2;
        pkt.steps = gSharedData.motion.steps;
        osMutexRelease(xSensorDataMutex);
      }
      pkt.hh = clk.hours;
      pkt.mm = clk.minutes;

      JDY31_SendPacket(&pkt);
      last_pkt_tick = now;
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
  }
  /* USER CODE END StartTask04 */
}

/* USER CODE BEGIN Header_StartTask05 */
/* USER CODE END Header_StartTask05 */
void StartTask05(void const * argument)
{
  /* USER CODE BEGIN StartTask05 */
  Button_Task(argument);
  /* USER CODE END StartTask05 */
}

/* USER CODE BEGIN Header_StartTask06 */
/* USER CODE END Header_StartTask06 */
void StartTask06(void const * argument)
{
  /* USER CODE BEGIN StartTask06 */
  Power_Task(argument);
  /* USER CODE END StartTask06 */
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) { HAL_IncTick(); }
  /* USER CODE BEGIN Callback 1 */
  /* USER CODE END Callback 1 */
}

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file; (void)line;
  /* USER CODE END 6 */
}
#endif
