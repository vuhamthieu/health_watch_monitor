/* main.c — FreeRTOS task startup, peripheral init, battery reading */
#include "main.h"
#include "cmsis_os.h"

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
#include <stdio.h>

/* Retarget printf → UART1 (PA9=TX, 9600 baud) — connect CH340 RX to PA9 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
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
  hi2c1.Init.ClockSpeed = 100000;
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
  /* USER CODE BEGIN StartTask03 */
  (void)argument;

  /* Init sensors under I2C mutex */
  osMutexWait(i2cMutexHandle, osWaitForever);
  MPU6050_Status_t  mpu_status = MPU6050_Init();
  MAX30102_Status_t max_status = MAX30102_Init();
  if (max_status == MAX30102_OK) {
      MAX30102_SetLEDCurrent(0x3F, 0x3F); /* ~12 mA — visible red glow */
      MAX30102_FlushFIFO();
  }
  osMutexRelease(i2cMutexHandle);

  /* Report sensor init results over UART (connect CH340 to PA9 at 9600 baud) */
  printf("[BOOT] MPU6050 init: %s\r\n",
      mpu_status == MPU6050_OK        ? "OK" :
      mpu_status == MPU6050_ERR_I2C   ? "ERR_I2C (check wiring/pullups)" :
                                        "ERR_WHOAMI (wrong addr? AD0 pin?)");
  printf("[BOOT] MAX30102 init: %s\r\n",
      max_status == MAX30102_OK         ? "OK" :
      max_status == MAX30102_ERR_I2C    ? "ERR_I2C (check wiring/pullups)" :
                                          "ERR_PARTID (chip not found)");

  /* Calibrate MPU — device should be flat and still at boot */
  if (mpu_status == MPU6050_OK) {
      osMutexWait(i2cMutexHandle, osWaitForever);
      MPU6050_Calibrate();
      osMutexRelease(i2cMutexHandle);
  }

  HR_Reset();
  SpO2_Reset();
  StepCounter_Reset();

  bool mpu_ok = (mpu_status == MPU6050_OK);
  bool max_ok = (max_status == MAX30102_OK);
  MAX30102_Sample_t fifo_buf[8];
  uint8_t  fifo_cnt = 0;
  uint32_t loop_cnt = 0u;

  for (;;)
  {
      if (mpu_ok) {
          MPU6050_Data_t imu;
          osMutexWait(i2cMutexHandle, osWaitForever);
          bool imu_ok = (MPU6050_Read(&imu) == MPU6050_OK);
          osMutexRelease(i2cMutexHandle);

          if (imu_ok) {
              StepCounter_Update(imu.accel_g[0], imu.accel_g[1], imu.accel_g[2]);
              if ((loop_cnt % 10u) == 0u) {
                  int activity = StepCounter_GetActivityState();
                  if (osMutexWait(xSensorDataMutex, 5u) == osOK) {
                      gSharedData.motion.steps         = StepCounter_GetSteps();
                      gSharedData.motion.distance_m    = StepCounter_GetDistance();
                      gSharedData.motion.calories_kcal = StepCounter_GetCalories();
                      gSharedData.motion.activity      = (ActivityState_t)activity;
                      gSharedData.motion.status        = SENSOR_OK;
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

      if (max_ok) {
          osMutexWait(i2cMutexHandle, osWaitForever);
          MAX30102_Status_t fs = MAX30102_ReadFIFO(fifo_buf, 8, &fifo_cnt);
          osMutexRelease(i2cMutexHandle);

          if (fs == MAX30102_OK) {
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
          }
      }

      loop_cnt++;
      /* Every 5 s: print live sensor values over UART for debugging */
      if ((loop_cnt % 500u) == 0u) {
          if (mpu_ok) {
              MPU6050_Data_t d;
              osMutexWait(i2cMutexHandle, osWaitForever);
              MPU6050_Read(&d);
              osMutexRelease(i2cMutexHandle);
              printf("[MPU] ax=%.2f ay=%.2f az=%.2f steps=%lu\r\n",
                  (double)d.accel_g[0], (double)d.accel_g[1], (double)d.accel_g[2],
                  (unsigned long)StepCounter_GetSteps());
          } else {
              printf("[MPU] not connected\r\n");
          }
          if (max_ok) {
              printf("[MAX] HR=%u bpm  SpO2=%u%%  finger=%s\r\n",
                  (unsigned)gSharedData.heart.bpm,
                  (unsigned)gSharedData.heart.spo2,
                  HR_FingerPresent() ? "YES" : "NO");
          } else {
              printf("[MAX] not connected\r\n");
          }
      }
      osDelay(10); /* 100 Hz */
  }
  /* USER CODE END StartTask03 */
}

/* USER CODE BEGIN Header_StartTask04 */
/* USER CODE END Header_StartTask04 */
void StartTask04(void const * argument)
{
  /* USER CODE BEGIN StartTask04 */
  (void)argument;
  JDY31_Init();
  for(;;) { osDelay(100); }
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
