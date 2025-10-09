/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "gpdma.h"
#include "i2c.h"
#include "icache.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  APP_MONITOR = 0,
  APP_PRINT   = 1
} AppState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ---- EDA (ADC1) ----
#define FS_EDA        200u
#define BUF_SAMPLES  (2u * FS_EDA)   // ping-pong: 400 half-words

// ---- TEMP (MAX31865) ----
#define TEMP_RING_N   64u            // gotta check with the sensor


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
__IO uint32_t BspButtonState = BUTTON_RELEASED;

/* USER CODE BEGIN PV */

// ---- EDA acquisition (ADC @ 200 Hz) ----

static uint16_t eda_buf[BUF_SAMPLES];             // DMA target
static volatile const uint16_t *eda_win_ptr = NULL; // points to the half that just finished
static volatile uint32_t eda_seconds = 0;         // simple 1s counter

// ============ TEMP (MAX31865) acquisition ============
static uint16_t temp_ring[TEMP_RING_N];
static volatile uint16_t temp_wi = 0;
static volatile uint16_t temp_samples_this_sec = 0;
static volatile uint16_t temp_sec_count_snapshot = 0;
static volatile uint8_t  one_sec_tick = 0;             // fired by TIM7

// ============ FSM ============
static AppState app_state = APP_MONITOR;

// ============ 1s window structs ============
typedef struct {
  uint16_t samples[FS_EDA]; // 200 samples @ 200 Hz
  uint16_t n;               // always 200 when ready
} EDA_1s_t;

typedef struct {
  uint16_t raw[TEMP_RING_N]; // raw 16-bit RTD regs (bit0=fault, code=raw>>1)
  uint16_t n;                // how many DRDY samples happened in last second
} TEMP_1s_t;

typedef struct {
  uint32_t sec_index; // increments each second
  EDA_1s_t  eda;
  TEMP_1s_t temp;
} Window1s_t;

static uint32_t seconds_counter = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */
// ---- EDA helpers ----
static void EDA_Start(void);
static void ADC1_DoCalibration(void);

// ---- TEMP/MAX31865 helpers ----
static void TEMP_Init_MAX31865(void);
static uint16_t TEMP_Read_RTD_Raw(void);

// ---- GPIO helpers (CS) ----
static inline void TEMP_CS_Low(void);
static inline void TEMP_CS_High(void);

// ---- Printing & Predict stub ----
static void Print_EDA_Window(const EDA_1s_t *w);
static void Print_TEMP_Window(const TEMP_1s_t *w);
static void Predict(const Window1s_t *win);
static void BuildWindow_1s(Window1s_t *dst);

// --- PWN ---
static void PWM1_SetFrequency(uint32_t hz);
static void PWM1_SetDuty(float duty_percent);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_ICACHE_Init();
  MX_TIM6_Init();
  MX_I2C1_Init();
  MX_TIM7_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
	
	// Start periodic timers
  HAL_TIM_Base_Start_IT(&htim7); // 1 Hz boundary
  // Start EDA pipeline (ADC1 + DMA + TIM6 200 Hz)
  EDA_Start();
  // Configure MAX31865 (VBIAS+AutoConv, 50 Hz filter)
  TEMP_Init_MAX31865();
  printf("Init OK (FSM starts in MONITOR)\r\n");

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // start PA8 PWM

  PWM1_SetDuty(25.0f);     // 25% duty at 1 kHz
  // PWM1_SetDuty(60.0f);
  //PWM1_SetFrequency(5000);  // switch to 5 kHz, keeps duty


  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN BSP */

  /* Infinite loop */
  /* -- Sample board code to switch on leds ---- */
	
  BSP_LED_On(LED_GREEN);
  BSP_LED_On(LED_BLUE);
  BSP_LED_On(LED_RED);
  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* -- Sample board code for User push-button in interrupt mode ---- */
    if (BspButtonState == BUTTON_PRESSED)
    {
      /* Update button state */
      BspButtonState = BUTTON_RELEASED;
      /* -- Sample board code to toggle leds ---- */
      BSP_LED_Toggle(LED_GREEN);
      BSP_LED_Toggle(LED_BLUE);
      BSP_LED_Toggle(LED_RED);
      /* ..... Perform your action ..... */
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		switch (app_state)
    {
      case APP_MONITOR:
        // stay here collecting data; jump to PRINT on each 1s tick
        if (one_sec_tick) {
          one_sec_tick = 0;
          app_state = APP_PRINT;
        }
        break;

      case APP_PRINT:
      {
        // Build the combined 1s window from latest EDA half-buffer and TEMP ring snapshot
        Window1s_t win;
        BuildWindow_1s(&win);

        // Print both windows (EDA first, then TEMP)
        Print_EDA_Window(&win.eda);
        Print_TEMP_Window(&win.temp);

        // Call ML/prediction (stub)
        Predict(&win);

        // advance time and go back to monitoring
        seconds_counter++;
        app_state = APP_MONITOR;
      } break;

      default: app_state = APP_MONITOR; break;
    }


  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 8;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** LSCO configuration
  */
  HAL_RCCEx_EnableLSCO(RCC_LSCOSOURCE_LSE);
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddIO2();

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/* USER CODE BEGIN 4 */
/**************  EDA (ADC1 + TIM6 + DMA)  *****************/
static void ADC1_DoCalibration(void)
{
#if defined(ADC_CALIB_OFFSET_LINEARITY)
  (void)HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
#elif defined(ADC_CALIB_OFFSET)
  (void)HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET,          ADC_SINGLE_ENDED);
#else
  (void)HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
#endif
}

static void EDA_Start(void)
{
  ADC1_DoCalibration();
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)eda_buf, BUF_SAMPLES);  // circular, 400 half-words
  HAL_TIM_Base_Start(&htim6);                                  // 200 Hz TRGO
}

// DMA HT/TC each represent a full 1s EDA window (200 samples)
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc == &hadc1) { eda_win_ptr = &eda_buf[0]; }
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc == &hadc1) { eda_win_ptr = &eda_buf[FS_EDA]; }
}

/**************  TEMP (MAX31865 on SPI1)  *****************/
// Use your CubeMX labels from gpio.h
static inline void TEMP_CS_Low(void)  { HAL_GPIO_WritePin(TEMP_CS_GPIO_Port, TEMP_CS_Pin, GPIO_PIN_RESET); }
static inline void TEMP_CS_High(void) { HAL_GPIO_WritePin(TEMP_CS_GPIO_Port, TEMP_CS_Pin, GPIO_PIN_SET); }

// 1 Hz boundary (TIM7) → snapshot temperature count and signal FSM
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM7) {
    temp_sec_count_snapshot = temp_samples_this_sec;  // capture
    temp_samples_this_sec = 0;                         // reset for next second
    one_sec_tick = 1;                                  // drive FSM
  }
}

// DRDY falling → read one RTD sample and push to ring
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == MAX_DRDY_Pin) {
    uint16_t raw = TEMP_Read_RTD_Raw();
    temp_ring[temp_wi] = raw;
    temp_wi = (uint16_t)((temp_wi + 1) % TEMP_RING_N);
    temp_samples_this_sec++;
  }
}

// MAX31865 basic driver
#define MAX31865_REG_CFG_READ    0x00u
#define MAX31865_REG_CFG_WRITE   0x80u
#define MAX31865_REG_RTD_MSB     0x01u   // then 0x02 LSB (auto-increment)

static void SPI1_Write1(uint8_t addr_write, uint8_t data)
{
  TEMP_CS_Low();
  HAL_SPI_Transmit(&hspi1, &addr_write, 1, 10);
  HAL_SPI_Transmit(&hspi1, &data,       1, 10);
  TEMP_CS_High();
}
static void SPI1_ReadN(uint8_t addr_read, uint8_t *rx, uint16_t n)
{
  TEMP_CS_Low();
  HAL_SPI_Transmit(&hspi1, &addr_read, 1, 10);
  HAL_SPI_Receive(&hspi1, rx, n, 10);
  TEMP_CS_High();
}

// Config: VBIAS=1, AutoConv=1, 3-wire=0 (2/4-wire), FaultClr=1, 50Hz=1 -> 0xC3
static void TEMP_Init_MAX31865(void)
{
  const uint8_t cfg = 0xC3u;
  SPI1_Write1(MAX31865_REG_CFG_WRITE, cfg);
}

static uint16_t TEMP_Read_RTD_Raw(void)
{
  uint8_t rx[2] = {0,0};
  SPI1_ReadN(MAX31865_REG_RTD_MSB, rx, 2);
  return (uint16_t)((rx[0] << 8) | rx[1]);   // bit0=fault, 15b code = raw>>1
}

/**************  Window build / Print / Predict  **********/
static void BuildWindow_1s(Window1s_t *dst)
{
  dst->sec_index = seconds_counter;

  // ---- EDA ----
  const uint16_t *p = (const uint16_t *)eda_win_ptr;
  if (p != NULL) {
    memcpy(dst->eda.samples, p, sizeof(dst->eda.samples));
    dst->eda.n = FS_EDA;
    // release pointer for next DMA half; we prefer latest ready half
    eda_win_ptr = NULL;
  } else {
    // if no new EDA half exactly at tick, keep zeros/old count=0
    memset(dst->eda.samples, 0, sizeof(dst->eda.samples));
    dst->eda.n = 0;
  }

  // ---- TEMP ----
  uint16_t n = temp_sec_count_snapshot;
  if (n > TEMP_RING_N) n = TEMP_RING_N;
  // copy last n samples from ring
  uint16_t start = ( (temp_wi + TEMP_RING_N) - n ) % TEMP_RING_N;
  for (uint16_t i = 0; i < n; ++i) {
    dst->temp.raw[i] = temp_ring[(start + i) % TEMP_RING_N];
  }
  dst->temp.n = n;
}

static void Print_EDA_Window(const EDA_1s_t *w)
{
  printf("EDA sec=%lu, N=%u, RAW=",
         (unsigned long)seconds_counter, (unsigned)w->n);
  for (uint16_t i = 0; i < w->n; ++i) {
    printf("%u", (unsigned)w->samples[i]);
    if (i + 1u < w->n) printf(",");
  }
  printf("\r\n");
}

static void Print_TEMP_Window(const TEMP_1s_t *w)
{
  printf("TEMP sec=%lu, N=%u, RAW=",
         (unsigned long)seconds_counter, (unsigned)w->n);
  for (uint16_t i = 0; i < w->n; ++i) {
    printf("%u", (unsigned)w->raw[i]);
    if (i + 1u < w->n) printf(",");
  }
  printf("\r\n");
}

// I use this as the temp for maek sure it went through this function, will be swap with the actul one
static void Predict(const Window1s_t *win)
{
  // just print quick stats to show it ran
  if (win->eda.n) {
    uint16_t min = 0xFFFF, max = 0;
    uint32_t sum = 0;
    for (uint16_t i = 0; i < win->eda.n; ++i) {
      uint16_t v = win->eda.samples[i];
      if (v < min) min = v;
      if (v > max) max = v;
      sum += v;
    }
    printf("EDA stats: min=%u max=%u avg=%u\r\n",
           (unsigned)min, (unsigned)max, (unsigned)(sum / win->eda.n));
  }
  if (win->temp.n) {
    printf("TEMP last raw=0x%04X (code15=%u fault=%u)\r\n",
           (unsigned)win->temp.raw[win->temp.n-1],
           (unsigned)(win->temp.raw[win->temp.n-1] >> 1),
           (unsigned)(win->temp.raw[win->temp.n-1] & 1u));
  }
}

// Set duty cycle (0.0 .. 100.0 %) on TIM1_CH1 (PA8)
static void PWM1_SetDuty(float duty_percent)
{
    if (duty_percent < 0.0f)  duty_percent = 0.0f;
    if (duty_percent > 100.0f) duty_percent = 100.0f;

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);     // current ARR
    uint32_t ccr = (uint32_t)((duty_percent / 100.0f) * (arr + 1u));
    if (ccr > arr) ccr = arr;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);   // CCR1
}

// Change PWM frequency (keeps current duty ratio as best as possible)
static void PWM1_SetFrequency(uint32_t hz)
{
    if (hz == 0u) hz = 1u;
    // TIM1 clock ≈ 160 MHz rn
    uint32_t timclk = 160000000u;
    uint32_t psc = 159u;                                  // fixed prescaler (÷160)
    uint32_t arr = (timclk / ((psc + 1u) * hz)) - 1u;
    if ((int32_t)arr < 0) arr = 0;

    // Preserve duty ratio while changing frequency
    uint32_t old_arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    uint32_t old_ccr = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1);
    float duty = (old_arr + 1u) ? (100.0f * (float)old_ccr / (float)(old_arr + 1u)) : 0.0f;

    __HAL_TIM_SET_PRESCALER(&htim1, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
    __HAL_TIM_SET_COUNTER(&htim1, 0);

    PWM1_SetDuty(duty);                                   // re-apply duty
}



/* USER CODE END 4 */

/**
  * @brief  BSP Push Button callback
  * @param  Button Specifies the pressed button
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_USER)
  {
    BspButtonState = BUTTON_PRESSED;
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
