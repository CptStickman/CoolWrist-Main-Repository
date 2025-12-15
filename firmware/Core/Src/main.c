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
#include <stdbool.h>
#include <stdlib.h>

#include "predict.h"
#include "stm32u5xx_hal_dma_ex.h"   // prototypes for HAL_DMAEx_List_*
extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
#define EDA_USE_DMA 0   // set to 1 later when DMA works
#include "stm32u5xx_nucleo.h"         // for COM1
extern UART_HandleTypeDef hcom_uart[];



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  APP_MONITOR = 0,
  APP_PRINT   = 1,
  APP_MOTOR   = 2
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

#if !EDA_USE_DMA
static volatile uint16_t eda_soft[FS_EDA];
static volatile uint16_t eda_i = 0;
#endif

// ============ TEMP (MAX31865) acquisition ============
static uint16_t temp_ring[TEMP_RING_N];
static volatile uint16_t temp_wi = 0;
static volatile uint16_t temp_samples_this_sec = 0;
static volatile uint16_t temp_sec_count_snapshot = 0;
static volatile uint8_t  one_sec_tick = 0;             // fired by TIM7
static void TEMP_DebugPoll(void);
static uint8_t TEMP_Read_Config(void);
static uint8_t TEMP_Read_FaultStatus(void);

// ---- PPG (MAX32664 + MAX30101 via I2C1) ----
// MAX32664 uses 0xAA (write) / 0xAB (read) 8-bit address → 7-bit 0x55
// HAL expects (7-bit << 1), so we pass 0xAA.

extern I2C_HandleTypeDef hi2c1;   // PPG hub on I2C1

#define MAX32664_I2C_ADDR   (0x55u << 1)   // 7-bit 0x55 -> HAL 8-bit
#define HUB_CMD_PREFIX      0xAA
#define HUB_RSP_PREFIX      0xAB
#define HUB_FAMILY_DEVICE_MODE   0x01
#define HUB_FAMILY_OUTPUT_MODE   0x10
#define HUB_FAMILY_SENSOR        0x44
#define HUB_FAMILY_ALGO_MODE     0x52
#define HUB_FAMILY_FIFO          0x12
// CubeMX labels for MFIO / RESET:
#define PPG_MFIO_PORT   PPG_MFIO_GPIO_Port
#define PPG_MFIO_PIN    PPG_MFIO_Pin
#define PPG_RST_PORT    PPG_RESET_GPIO_Port
#define PPG_RST_PIN     PPG_RESET_Pin

//dexter define
// ---- Data Management ----
#define MAX_DATA_HOURS           1u      // Maximum hours of data to keep
#define CLEANUP_INTERVAL_MIN     30u      // Clean up every 30 minutes  
#define NODES_TO_REMOVE_MIN      30u      // Remove 30 minutes of data each cleanup
#define SECONDS_PER_HOUR         3600u
#define SECONDS_PER_MINUTE       60u
#define MAX_NODES               (MAX_DATA_HOURS * SECONDS_PER_HOUR)
#define CLEANUP_INTERVAL_SEC    (CLEANUP_INTERVAL_MIN * SECONDS_PER_MINUTE)
#define NODES_TO_REMOVE         (NODES_TO_REMOVE_MIN * SECONDS_PER_MINUTE)
// ---- UART Command Processing ----
#define UART_RX_BUFFER_SIZE 64
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
// static volatile uint8_t uart_command_ready = 0;
static volatile uint16_t uart_rx_index = 0;
void calibration();


// predict results 

int results = 0;


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
  uint16_t hr_x10;    // heart rate * 10 (e.g. 635 = 63.5 bpm)
  uint8_t  confidence;
  uint8_t  valid;     // 0 = no new sample this second
} PPG_1s_t;

typedef struct {
  uint32_t sec_index; // increments each second
  EDA_1s_t  eda;
  TEMP_1s_t temp;
  PPG_1s_t  ppg; 
} Window1s_t;
 
//dexter parts begin 


typedef struct DataNode_s{
  DataEntry currEntry;
  struct DataNode_s* nextEntry;
  int id;
  bool nodeEpisodeState;
} DataNode;


//dexter parts end 

static uint32_t seconds_counter = 0;
static PPG_1s_t latest_ppg = {0};
static void PPG_Init(void);
static void PPG_PollOnce(void);
static void Print_PPG_1s(const PPG_1s_t *p);

int episodeCount = 0;  // Number of consecutive checks indicating an episode
bool episodeState = 0;  // Current episode state
bool prevEpState = 0;  //The last episode state



// ============ USART COMMAND ============
#define RX_LINE_MAX   128

static uint8_t  uart_rx_byte;
static uint8_t  rx_line[RX_LINE_MAX];
static volatile uint16_t rx_len = 0;
static volatile uint8_t  rx_line_ready = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */
// ---- EDA helpers ----
static void EDA_Start(void);
static void ADC1_DoCalibration(void);
static inline uint32_t EDA_CodeTo_mV(uint16_t code);

// ---- TEMP/MAX31865 helpers ----
static void TEMP_Init_MAX31865(void);
static uint16_t TEMP_Read_RTD_Raw(void);

// -------- ppg helpers ------------
static void    PPG_Init_MAX32664(void);
static uint8_t PPG_Read_SensorHubStatus(uint8_t *status);
static uint8_t PPG_Read_FIFO_Samples(uint8_t *samples);
static void    Print_PPG_Debug(const PPG_1s_t *p);


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

// --- usart commadn --- 
static void UART_StartRx_IT(void);
// static void UART_SendText(const char *s);
// static void Send_EDA_1s_File_RAW_quick(void);

// --- Data Management ---
static void DataNode_RemoveOldest(uint32_t count);
static void DataNode_ManageMemory(void);

// --- App Communication ---
static void ProcessUARTCommand(void);
static void SendLinkedListToApp(void);
// static void StartUARTReceive(void);

static volatile uint8_t uart_cmd_ready = 0;



/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//The user's calibrated data.
DataEntry userCalibratedData;

//The linked list head for storing data entries. This'll be downloaded by the app.
DataNode* dataHead = NULL;
DataNode* dataTail = NULL;

// static uint32_t seconds_counter = 0;
static uint32_t total_nodes = 0;  // Track total number of nodes
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
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
	
	// Start periodic timers
  HAL_TIM_Base_Start_IT(&htim7); // 1 Hz boundary
  // Start EDA pipeline (ADC1 + DMA + TIM6 200 Hz)
  EDA_Start();
  // Configure MAX31865 (VBIAS+AutoConv, 50 Hz filter)
  TEMP_Init_MAX31865();
  PPG_Init();

  // printf("Init OK (FSM starts in MONITOR)\r\n");

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // start PA8 PWM
  PWM1_SetDuty(25.0f);     // 25% duty at 1 kHz
  // PWM1_SetDuty(60.0f);
  PWM1_SetFrequency(5000);  // switch to 5 kHz, keeps duty
  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);


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
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);


  /* USER CODE BEGIN BSP */
  printf("COM port is working now\r\n");
  UART_StartRx_IT();

  /* Infinite loop */
  /* -- Sample board code to switch on leds ---- */
	
  BSP_LED_On(LED_GREEN);
  BSP_LED_On(LED_BLUE);
  BSP_LED_On(LED_RED);
        
  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_Delay(5000);
  calibration();
  while (1)
  {

    if (uart_cmd_ready) {
      uart_cmd_ready = 0;
      printf("isr get called!\n");
      ProcessUARTCommand();   // uses uart_rx_buffer
    }

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

      // APP_CALIBRATION happen here once the botton preessed
      int calIndx = 0;
      float skinCond[10];
      int sum_eda = 0; 

      // float temp[10];
      // int heartRate[10];

      while(calIndx < 10) {
        if(one_sec_tick) {
          Window1s_t win;
          // PPG_PollOnce();
          BuildWindow_1s(&win);

          one_sec_tick = 0;
          for(int j = 0; j< 200; j++) {
            sum_eda += win.eda.samples[j];
          }
          skinCond[calIndx] = sum_eda/200; // worng,change to average later 
          // temp[calIndx] = win.temp.n;
          // heartRate[calIndx] = 75; //PLACEHOLDER value until HR sensor is integrated.
          calIndx++;
        }
      }

        //Calculate averages
        float skinCondSum = 0;
        // float tempSum = 0;
        // int heartRateSum = 0;
        for(int i = 0; i < 10; i++) {
          skinCondSum += skinCond[i];
        }
        userCalibratedData.skinCond = skinCondSum / 10;
        printf("%f\n", userCalibratedData.skinCond);
        HAL_Delay(100);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // TEMP_Init_MAX31865();
    // HAL_Delay(100);
    // uint8_t cfg = TEMP_Read_Config();
    // uint8_t fs  = TEMP_Read_FaultStatus();
    // uint16_t rtd_raw = TEMP_Read_RTD_Raw();
    // printf("MAX31865 CFG=0x%02X, FAULT=0x%02X, RTD_RAW=0x%04X (code15=%u, faultbit=%u)\r\n",
    //       cfg, fs, rtd_raw,
    //       (unsigned)(rtd_raw >> 1),
    //       (unsigned)(rtd_raw & 1u));
          
		switch (app_state)
    {
      case APP_MONITOR:
        // stay here collecting data; jump to PRINT on each 1s tick
        if (one_sec_tick) {
          one_sec_tick = 0;
          printf("Time: %d. Monitor state!!\r\n", seconds_counter);
          app_state = APP_PRINT;
        }
        break;

      case APP_PRINT:
      {
        // Build the combined 1s window from latest EDA half-buffer and TEMP ring snapshot
        // PPG_PollOnce();

        Window1s_t win;
        BuildWindow_1s(&win);

        // Print both windows (EDA first, then TEMP)
        printf("Time: %d. Predict state!!\r\n", seconds_counter);
        Print_EDA_Window(&win.eda);
        // Print_TEMP_Window(&win.temp);
        // Print_PPG_1s(&win.ppg);

        // Call ML/prediction (stub)
        // Predict(&win);
        // results = predict();

        DataEntry currEntry;
        // currEntry.temp = 0;
        int sum = 0;
        for(int i = 0; i < 200; i++) {
          sum += win.eda.samples[i];
        }
        currEntry.skinCond = sum / 200;
        // printf("%f", currEntry.skinCond);
        // currEntry.heartRate = win.ppg.hr_x10; // PLACEHOLDER value until HR sensor is integrated.

        episodeCount = deterministicAlgorithm(currEntry, episodeState, episodeCount, userCalibratedData);
        // printf("%d\n", episodeCount);
        if (episodeCount >= 5) {
            // Episode started
            episodeState = 1;
            printf("Episode started!\n");
        } else if (episodeCount < 3 && episodeState) {
            // Episode ended
            episodeState = 0;
            printf("Episode ended!\n");
        }


        //Create a new node, then add it to the linked list
        DataNode* newNode = (DataNode*)malloc(sizeof(DataNode));
        if (newNode != NULL) {
          newNode->currEntry = currEntry;
          newNode->id = seconds_counter;
          newNode->nextEntry = NULL;
          newNode->nodeEpisodeState = episodeState;

          if (dataHead == NULL) {
            dataHead = newNode;
            dataTail = newNode;
          } else {
            dataTail->nextEntry = newNode;
            dataTail = newNode;
          }
          total_nodes++;
          DataNode_ManageMemory();
        }


        // advance time and go back to monitoring
        seconds_counter++;
        app_state = (episodeState != prevEpState) ? APP_MOTOR: APP_MONITOR;
        prevEpState = episodeState;
      } break;

      case APP_MOTOR:
      {
        //pwm enabale 
        // printf("buzz\n");
        if(app_state == episodeState){
          HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // start PA8 PWM
          PWM1_SetDuty(60.0f);
          HAL_Delay(500);
          HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        }else if(app_state != episodeState){
          HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // start PA8 PWM
          PWM1_SetDuty(60.0f);
          HAL_Delay(1000);
          HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        }
        //pwm disable
        app_state = APP_MONITOR;
      }

      default: app_state = APP_MONITOR; break;
    }

    // if (rx_line_ready) {
    //   // make a local copy (avoid using volatile buffer while print)
    //   char cmd[RX_LINE_MAX];
    //   uint16_t len = rx_len;
    //   if (len >= RX_LINE_MAX) len = RX_LINE_MAX-1;
    //   memcpy(cmd, rx_line, len);
    //   cmd[len] = 0;

    //   // reset RX line state
    //   rx_len = 0;
    //   rx_line_ready = 0;

    //   if (strcmp(cmd, "GET_FILE") == 0) {
    //     Send_EDA_1s_File_RAW_quick();
    //   } else {
    //     UART_SendText("ERR unknown_cmd\n");
    //   }
    // }

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

// static void EDA_Start(void)
// {
//   ADC1_DoCalibration();
//   HAL_ADC_Start_DMA(&hadc1, (uint32_t*)eda_buf, BUF_SAMPLES);  // circular, 400 half-words
//   HAL_TIM_Base_Start(&htim6);                                  // 200 Hz TRGO
// }

static void EDA_Start(void)
{
  ADC1_DoCalibration();
#if EDA_USE_DMA
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)eda_buf, BUF_SAMPLES); // 400 half-words
#else
  // Arm ADC to convert on TIM6 TRGO and raise EOC interrupt each conversion
  HAL_ADC_Start_IT(&hadc1);
#endif
  HAL_TIM_Base_Start(&htim6);  // 200 Hz TRGO
}




// // DMA HT/TC each represent a full 1s EDA window (200 samples)
// void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
// {
//   if (hadc == &hadc1) { eda_win_ptr = &eda_buf[0]; }
// }
// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
// {
//   if (hadc == &hadc1) { eda_win_ptr = &eda_buf[FS_EDA]; }
// }


// void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc){
//   if (hadc == &hadc1) { eda_win_ptr = &eda_buf[0]; printf("[HT]\r\n"); }
// }
// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
//   if (hadc == &hadc1) { eda_win_ptr = &eda_buf[FS_EDA]; printf("[TC]\r\n"); }
// }


void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
#if EDA_USE_DMA
  if (hadc == &hadc1) eda_win_ptr = &eda_buf[0];
#endif
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
#if EDA_USE_DMA
  if (hadc == &hadc1) eda_win_ptr = &eda_buf[FS_EDA];
#else
  if (hadc == &hadc1) {
    // One sample ready after each TIM6 trigger
    uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    if (eda_i < FS_EDA) eda_soft[eda_i++] = v;
    if (eda_i >= FS_EDA) {                // full 1-s window
      eda_win_ptr = (const uint16_t*)eda_soft;  // hand to BuildWindow_1s()
      eda_i = 0;
    }
  }
#endif
}


/**************  TEMP (MAX31865 on SPI1)  *****************/
// Use CubeMX labels from gpio.h
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
  if (GPIO_Pin == TEMP_DRDY_Pin) {
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
  // TEMP_CS_High();
}
static void SPI1_ReadN(uint8_t addr_read, uint8_t *rx, uint16_t n)
{
  TEMP_CS_Low();
  HAL_SPI_Transmit(&hspi1, &addr_read, 1, 10);
  HAL_SPI_Receive(&hspi1, rx, n, 10);
  // TEMP_CS_High();
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

  //ppg
  dst->ppg = latest_ppg;
}

// static void Print_EDA_Window(const EDA_1s_t *w)
// {
//   printf("EDA sec=%lu, N=%u, RAW=",
//          (unsigned long)seconds_counter, (unsigned)w->n);
//   for (uint16_t i = 0; i < w->n; ++i) {
//     printf("%u", (unsigned)w->samples[i]);
//     if (i + 1u < w->n) printf(",");
//   }
//   printf("\r\n");
// }

static void Print_EDA_Window(const EDA_1s_t *w)
{
  printf("EDA sec=%lu, N=%u, mV=",
         (unsigned long)seconds_counter, (unsigned)w->n);

  for (uint16_t i = 0; i < w->n; ++i) {
    uint32_t mv = EDA_CodeTo_mV(w->samples[i]);
    printf("%lu", (unsigned long)mv);
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

static inline uint32_t EDA_CodeTo_mV(uint16_t code)
{
  return  (uint32_t)code * 3300 / 4095;
}

static void UART_StartRx_IT(void) {
  HAL_UART_Receive_IT(&hcom_uart[COM1], &uart_rx_byte, 1);
}

// //uart part
// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
// {
//   if (huart == &hcom_uart[COM1]) {
//     uint8_t b = uart_rx_byte;

//     if (!rx_line_ready) {
//       if (b == '\n') {
//         rx_line[ (rx_len < RX_LINE_MAX) ? rx_len : (RX_LINE_MAX-1) ] = 0;
//         rx_line_ready = 1;
//       } else if (b != '\r') { // ignore CR
//         if (rx_len < RX_LINE_MAX-1) {
//           rx_line[rx_len++] = b;
//         } else {
//           // overflow: reset line
//           rx_len = 0;
//         }
//       }
//     }
//     // re-arm for next byte
//     HAL_UART_Receive_IT(&hcom_uart[COM1], &uart_rx_byte, 1);
//   }
// }


// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
// {
//   if (huart == &hcom_uart[COM1]) {      // use COM1, not huart1
//     uint8_t b = uart_rx_byte;

//     // simple line-based parser: collect until \r or \n
//     if (b == '\r' || b == '\n') {
//       // end of command
//       uart_rx_buffer[uart_rx_index] = '\0';   // null-terminate
//       uart_cmd_ready = 1;                    // signal main loop
//       uart_rx_index = 0;                     // reset for next command
//     } else {
//       if (uart_rx_index < UART_RX_BUFFER_SIZE - 1) {
//         uart_rx_buffer[uart_rx_index++] = b;
//       } else {
//         // overflow: reset line
//         uart_rx_index = 0;
//       }
//     }

//     // re-arm for next byte
//     HAL_UART_Receive_IT(&hcom_uart[COM1], &uart_rx_byte, 1);
//   }
// }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == hcom_uart[COM1].Instance) {
    uint8_t b = uart_rx_byte;

    // DEBUG: echo every byte so we know ISR fires
    HAL_UART_Transmit(&hcom_uart[COM1], &b, 1, 100);

    if (b == '\r' || b == '\n') {
      uart_rx_buffer[uart_rx_index] = '\0';
      uart_cmd_ready = 1;
      uart_rx_index = 0;
    } else {
      if (uart_rx_index < UART_RX_BUFFER_SIZE - 1) {
        uart_rx_buffer[uart_rx_index++] = b;
      } else {
        uart_rx_index = 0; // overflow reset
      }
    }

    HAL_UART_Receive_IT(&hcom_uart[COM1], &uart_rx_byte, 1);
  }
}




// static void Send_EDA_1s_File_RAW_quick(void)
// {
//   // Build latest 1-second window
//   Window1s_t win;
//   BuildWindow_1s(&win);

//   // Stream just the CSV lines (millivolts), no headers/trailers
//   for (uint16_t i = 0; i < win.eda.n; ++i) {
//     uint32_t mv = EDA_CodeTo_mV(win.eda.samples[i]);   // you already have EDA_CodeTo_mV()
//     char line[16];
//     int n = snprintf(line, sizeof(line), "%lu\n", (unsigned long)mv);
//     HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t*)line, (uint16_t)n, 200);
//   }

//   // IMPORTANT: give his script time to hit its 5s timeout and close the file
//   HAL_Delay(6000);  // 6 seconds
// }

// static void UART_SendText(const char *s)
// {
//   HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t*)s, (uint16_t)strlen(s), 200);
// }


// -=-=-=-=-=-=- get download -=-=-=-=-=-=-=-=-=-=- 

static void ProcessUARTCommand(void)
{
  // uart_rx_buffer already null-terminated by ISR
  printf("Received command: %s\r\n", (char*)uart_rx_buffer);

  // NOTE: command is case-sensitive right now:
  // you must type GET_LINKED_LIST (all caps) in Tera Term
  if (strstr((char*)uart_rx_buffer, "GET_LINKED_LIST") != NULL) {
    SendLinkedListToApp();
  } else {
    printf("Unknown command received\r\n");
  }
}


// Send linked list data to app in parseable format
static void SendLinkedListToApp(void)
{
  if (total_nodes == 0) {
    printf("No data available\r\n");
    printf("END_OF_LIST\r\n");
    return;
  }
  
  printf("Sending %lu linked list entries...\r\n", (unsigned long)total_nodes);
  
  DataNode* current = dataHead;
  uint32_t sent_count = 0;
  
  while (current != NULL) {
    // Send data in format expected by SerialConnection.py parser
    // Format: timestamp:SECONDS,temp:VALUE,hr:VALUE,sc:VALUE,status:Normal
    printf("timestamp:%f,sc:%.2f,status:%d\r\n",
           current->id / 100.0,
           //current->currEntry.temp,
           //current->currEntry.heartRate,
           current->currEntry.skinCond,
           current->nodeEpisodeState ? 1 : 0);
    
    current = current->nextEntry;
    sent_count++;
    
    // Small delay to prevent overwhelming the UART
    HAL_Delay(10);
  }
  
  printf("END_OF_LIST\r\n");
  printf("Sent %lu entries successfully\r\n", (unsigned long)sent_count);
}


// temp stuff

static void TEMP_DebugPoll(void)
{
  static uint32_t last_ms = 0;
  uint32_t now = HAL_GetTick();
  if (now - last_ms >= 500) {   // every 500 ms
    last_ms = now;
    uint16_t raw = TEMP_Read_RTD_Raw();
    printf("TEMP POLL raw=0x%04X code15=%u fault=%u\r\n",
           (unsigned)raw,
           (unsigned)(raw >> 1),
           (unsigned)(raw & 1u));
  }
}


static uint8_t TEMP_Read_Config(void)
{
    uint8_t cfg = 0;
    SPI1_ReadN(MAX31865_REG_CFG_READ, &cfg, 1);
    return cfg;
}

static uint8_t TEMP_Read_FaultStatus(void)
{
    uint8_t fs = 0;
    // Fault status register is at 0x07 (read)
    SPI1_ReadN(0x07u, &fs, 1);
    return fs;
}

// --------------------------PPG ----------------------
// static HAL_StatusTypeDef Hub_WriteCmd(uint8_t family, uint8_t index,
//                                       const uint8_t *payload, uint8_t plen)
// {
//   uint8_t tx[3 + 8];     // we only ever send <=5 data bytes here
//   if (plen > 8) return HAL_ERROR;

//   tx[0] = HUB_CMD_PREFIX;
//   tx[1] = family;
//   tx[2] = index;
//   if (plen && payload) {
//     memcpy(&tx[3], payload, plen);
//   }

//   HAL_StatusTypeDef st =
//       HAL_I2C_Master_Transmit(&hi2c1, MAX32664_I2C_ADDR, tx, 3 + plen, 100);
//   if (st != HAL_OK) return st;

//   // Read back 2-byte status header (0xAB, status)
//   uint8_t rsp[2];
//   st = HAL_I2C_Master_Receive(&hi2c1, MAX32664_I2C_ADDR, rsp, 2, 100);
//   if (st != HAL_OK) return st;
//   if (rsp[0] != HUB_RSP_PREFIX || rsp[1] != 0x00) return HAL_ERROR;  // non-zero = error
//   return HAL_OK;
// }

// static HAL_StatusTypeDef Hub_ReadCmd(uint8_t family, uint8_t index,
//                                      uint8_t *rx, uint8_t rlen)
// {
//   uint8_t tx[3] = { HUB_CMD_PREFIX, family, index };

//   HAL_StatusTypeDef st =
//       HAL_I2C_Master_Transmit(&hi2c1, MAX32664_I2C_ADDR, tx, 3, 100);
//   if (st != HAL_OK) return st;

//   // Response header + payload
//   uint8_t buf[2 + 32];          // enough for small payloads
//   if (rlen > 32) return HAL_ERROR;

//   st = HAL_I2C_Master_Receive(&hi2c1, MAX32664_I2C_ADDR, buf, 2 + rlen, 100);
//   if (st != HAL_OK) return st;
//   if (buf[0] != HUB_RSP_PREFIX || buf[1] != 0x00) return HAL_ERROR;

//   memcpy(rx, &buf[2], rlen);
//   return HAL_OK;
// }


static HAL_StatusTypeDef Hub_WriteCmd(uint8_t family, uint8_t index,
                                      const uint8_t *payload, uint8_t plen)
{
  uint8_t tx[2 + 8];
  if (plen > 8) return HAL_ERROR;

  tx[0] = family;
  tx[1] = index;
  if (plen && payload) memcpy(&tx[2], payload, plen);

  HAL_StatusTypeDef st =
      HAL_I2C_Master_Transmit(&hi2c1, MAX32664_I2C_ADDR,
                              tx, 2 + plen, 100);
  if (st != HAL_OK) return st;

  // Wait >60 µs
  HAL_Delay(1);

  uint8_t status = 0xFF;
  st = HAL_I2C_Master_Receive(&hi2c1, MAX32664_I2C_ADDR,
                              &status, 1, 100);
  if (st != HAL_OK) return st;

  return (status == 0x00) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef Hub_ReadCmd(uint8_t family, uint8_t index,
                                     uint8_t *rx, uint8_t rlen)
{
  uint8_t tx[2] = { family, index };

  HAL_StatusTypeDef st =
      HAL_I2C_Master_Transmit(&hi2c1, MAX32664_I2C_ADDR, tx, 2, 100);
  if (st != HAL_OK) return st;

  HAL_Delay(1);

  uint8_t buf[1 + 32];
  if (rlen > 32) return HAL_ERROR;

  st = HAL_I2C_Master_Receive(&hi2c1, MAX32664_I2C_ADDR,
                              buf, 1 + rlen, 100);
  if (st != HAL_OK) return st;

  uint8_t status = buf[0];
  if (status != 0x00) return HAL_ERROR;

  memcpy(rx, &buf[1], rlen);
  return HAL_OK;
}




static void PPG_ResetToAppMode(void)
{
  // MFIO high -> application mode (per Maxim guides)
  HAL_GPIO_WritePin(PPG_MFIO_PORT, PPG_MFIO_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(PPG_RST_PORT,  PPG_RST_PIN,  GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(PPG_RST_PORT,  PPG_RST_PIN,  GPIO_PIN_SET);
  HAL_Delay(200);   // allow hub to boot
}

static void PPG_Init(void)
{
  PPG_ResetToAppMode();

  // (Optional but nice) ensure we’re in application mode, not bootloader:
  uint8_t mode;
  if (Hub_ReadCmd(HUB_FAMILY_DEVICE_MODE, 0x00, &mode, 1) == HAL_OK) {
    // mode == 0x00 => application
  }

  // 1) Output mode: algorithm data only
  //    family 0x10, index 0x00, data=0x02  (Algorithm Data) 
  uint8_t out_mode = 0x02;
  Hub_WriteCmd(HUB_FAMILY_OUTPUT_MODE, 0x00, &out_mode, 1);
  HAL_Delay(20);

  // 2) Enable MAX30101 sensor (family 0x44, index 0x03, data=0x01) 
  uint8_t on = 0x01;
  Hub_WriteCmd(HUB_FAMILY_SENSOR, 0x03, &on, 1);
  HAL_Delay(20);

  // 3) Enable MaximFast algorithm mode 1 (HR+SpO2) (family 0x52, index 0x02) 
  uint8_t algo_mode = 0x01;  // Mode 1
  Hub_WriteCmd(HUB_FAMILY_ALGO_MODE, 0x02, &algo_mode, 1);
  HAL_Delay(120);            // algorithm start-up per table
}


static void PPG_PollOnce(void)
{
  uint8_t count = 0;
  if (Hub_ReadCmd(HUB_FAMILY_FIFO, 0x00, &count, 1) != HAL_OK) {
    return;
  }
  if (count == 0) {
    latest_ppg.valid = 0;
    return;
  }

  uint8_t sample[6];
  if (Hub_ReadCmd(HUB_FAMILY_FIFO, 0x01, sample, sizeof(sample)) != HAL_OK) {
    latest_ppg.valid = 0;
    return;
  }

  uint16_t hr_raw   = ((uint16_t)sample[0] << 8) | sample[1];
  uint8_t  conf     = sample[2];
  // uint16_t spo2_raw = ((uint16_t)sample[3] << 8) | sample[4];
  // uint8_t  status   = sample[5];

  latest_ppg.hr_x10     = hr_raw;   // assume Q1 with 1 decimal place
  latest_ppg.confidence = conf;
  latest_ppg.valid      = 1;
}

static void Print_PPG_1s(const PPG_1s_t *p)
{
  printf("PPG sec=%lu, ", (unsigned long)seconds_counter);

  if (!p->valid) {
    printf("HR=NA\r\n");
    return;
  }

  uint16_t bpm_int  = p->hr_x10 / 10u;
  uint16_t bpm_frac = p->hr_x10 % 10u;

  printf("HR=%u.%u bpm, conf=%u\r\n",
         (unsigned)bpm_int,
         (unsigned)bpm_frac,
         (unsigned)p->confidence);
}




// -=-=-=-=-=-=- linked list helper -=-=-=- 
// Remove the oldest 'count' nodes from the linked list
static void DataNode_RemoveOldest(uint32_t count)
{
  for (uint32_t i = 0; i < count && dataHead != NULL; i++) {
    DataNode* nodeToRemove = dataHead;
    dataHead = dataHead->nextEntry;
    
    // If we removed the last node, update tail
    if (dataHead == NULL) {
      dataTail = NULL;
    }
    
    free(nodeToRemove);
    total_nodes--;
  }
}

// Manage memory by removing old data when limits are reached
static void DataNode_ManageMemory(void)
{
  // Only start managing memory after we reach 10 hours of data
  if (total_nodes >= MAX_NODES) {
    
    // Remove the oldest 30 minutes of data to make room
    printf("Memory limit reached (%lu nodes): Removing %lu oldest nodes\r\n", 
           (unsigned long)total_nodes, (unsigned long)NODES_TO_REMOVE);
    
    DataNode_RemoveOldest(NODES_TO_REMOVE);
    
    printf("Memory cleanup complete: %lu nodes remaining\r\n", 
           (unsigned long)total_nodes);
  }
  
  // Emergency cleanup if we somehow exceed maximum by a large margin
  if (total_nodes > (MAX_NODES + 100)) {
    uint32_t excess = total_nodes - MAX_NODES;
    printf("Emergency cleanup: Removing %lu excess nodes\r\n", (unsigned long)excess);
    DataNode_RemoveOldest(excess);
  }
}
// -=-=-=-== linked list helper end -=-=-=-=-=-=-=-=- 

void calibration(){
  int calIndx = 0;
      float skinCond[10];
      int sum_eda = 0; 

      // float temp[10];
      // int heartRate[10];

      while(calIndx < 10) {
        if(one_sec_tick) {
          Window1s_t win;
          // PPG_PollOnce();
          BuildWindow_1s(&win);

          one_sec_tick = 0;
          for(int j = 0; j< 200; j++) {
            sum_eda += win.eda.samples[j];
          }
          skinCond[calIndx] = sum_eda/200; // worng,change to average later 
          // temp[calIndx] = win.temp.n;
          // heartRate[calIndx] = 75; //PLACEHOLDER value until HR sensor is integrated.
          calIndx++;
        }
      }

        //Calculate averages
        float skinCondSum = 0;
        // float tempSum = 0;
        // int heartRateSum = 0;
        for(int i = 0; i < 10; i++) {
          skinCondSum += skinCond[i];
        }
        userCalibratedData.skinCond = skinCondSum / 10;
        printf("%f\n", userCalibratedData.skinCond);
        HAL_Delay(100);
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
