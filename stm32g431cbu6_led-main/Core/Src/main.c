/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RS485_HEADER       0x55AA
#define RS485_RX_BUF_SIZE  32
#define UART_TIMEOUT_MS    10000   /* 10 seconds - timeout to detect USB disconnection */
#define WATCHDOG_TIMEOUT_MS 2000   /* 2 second watchdog - reset if main loop hangs */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim15;
DMA_HandleTypeDef hdma_tim2_ch4;
DMA_HandleTypeDef hdma_tim15_ch1;

UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_uart4_rx;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */
static WS2812_t LED_channel_A;  /* TIM15_CH1 on PA2 */
static WS2812_t LED_channel_B;  /* TIM2_CH4  on PA3 */

static volatile uint32_t ms_cnt = 0;
static volatile uint8_t led_flag = 0;

static uint8_t rs485_rx_buf[RS485_RX_BUF_SIZE];
static volatile uint32_t last_uart_rx_time = 0;  /* Timestamp of last successful UART reception */
static volatile uint8_t uart_recovery_needed = 0;  /* Flag to trigger UART recovery */
static volatile uint32_t uart_packet_count = 0;  /* Debug: count received packets */
static volatile uint32_t last_main_loop_tick = 0;  /* Watchdog: last main loop tick */

#define NUM_LEDS_CH_A  200
#define NUM_LEDS_CH_B  200
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM15_Init(void);
static void MX_TIM2_Init(void);
static void MX_UART4_Init(void);
static void MX_USB_PCD_Init(void);
/* USER CODE BEGIN PFP */

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

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM6_Init();
  MX_TIM15_Init();
  MX_TIM2_Init();
  MX_UART4_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */

  /* Channel A: TIM15_CH1 (PA2) */
  WS2812_Init(&LED_channel_A, &htim15, TIM_CHANNEL_1, NUM_LEDS_CH_A);
  WS2812_SetColor(&LED_channel_A, 1, 1, 120);   /* Blue dominant */
  HAL_Delay(1);
  WS2812_Update(&LED_channel_A);

  /* Channel B: TIM2_CH4 (PA3) */
  WS2812_Init(&LED_channel_B, &htim2, TIM_CHANNEL_4, NUM_LEDS_CH_B);
  WS2812_SetColor(&LED_channel_B, 1, 1, 120);   /* Red dominant */
  HAL_Delay(1);
  WS2812_Update(&LED_channel_B);

  /* Start TIM6 in interrupt mode for 1 ms periodic tick */
  HAL_TIM_Base_Start_IT(&htim6);

  /* RS485 direction pin PA11: output push-pull, default LOW (receive mode) */
  {
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_11;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);  /* RX mode */
  }

  /* Initialize last UART RX time to current time (prevents immediate timeout) */
  last_uart_rx_time = 0;

  /* Start UART4 DMA receive with idle-line detection */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rs485_rx_buf, RS485_RX_BUF_SIZE);

  /* Initialize watchdog tick */
  last_main_loop_tick = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Refresh watchdog - this prevents auto-reset */
    last_main_loop_tick = ms_cnt;

    /* Check if main loop has stalled (watchdog trigger) */
    if ((ms_cnt - last_main_loop_tick) > WATCHDOG_TIMEOUT_MS)
    {
      /* Main loop stalled for 2 seconds - attempt emergency recovery */
      /* Stop DMA */
      HAL_UART_DMAStop(&huart4);
      
      /* Clear flags */
      __HAL_UART_CLEAR_FEFLAG(&huart4);
      __HAL_UART_CLEAR_NEFLAG(&huart4);
      __HAL_UART_CLEAR_OREFLAG(&huart4);
      __HAL_UART_CLEAR_PEFLAG(&huart4);
      
      /* Wait a bit */
      HAL_Delay(50);
      
      /* Clear buffer */
      memset(rs485_rx_buf, 0, RS485_RX_BUF_SIZE);
      
      /* Restart */
      HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rs485_rx_buf, RS485_RX_BUF_SIZE);
      
      /* Reset counters */
      last_uart_rx_time = ms_cnt;
      last_main_loop_tick = ms_cnt;
    }

    /* Only do recovery if explicitly flagged by error callback */
    /* This prevents disrupting USB CDC with unnecessary resets */
    if (uart_recovery_needed)
    {
      /* UART error detected, attempt graceful recovery */
      uart_recovery_needed = 0;
      
      /* Stop current DMA reception */
      HAL_UART_DMAStop(&huart4);
      
      /* Clear any error flags */
      __HAL_UART_CLEAR_FEFLAG(&huart4);
      __HAL_UART_CLEAR_NEFLAG(&huart4);
      __HAL_UART_CLEAR_OREFLAG(&huart4);
      __HAL_UART_CLEAR_PEFLAG(&huart4);
      
      /* Small delay before re-arming */
      HAL_Delay(10);
      
      /* Clear RX buffer */
      memset(rs485_rx_buf, 0, RS485_RX_BUF_SIZE);
      
      /* Restart DMA receive */
      HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rs485_rx_buf, RS485_RX_BUF_SIZE);
      
      /* Reset timeout counter */
      last_uart_rx_time = ms_cnt;
    }

    /* Check for extended timeout (device really disconnected) */
    if ((ms_cnt - last_uart_rx_time) > UART_TIMEOUT_MS)
    {
      /* No data for 10 seconds - assume real disconnection */
      /* Just log it but don't disrupt UART (user will check manually) */
      last_uart_rx_time = ms_cnt;  /* Reset timer to avoid repeated resets */
    }

    if (led_flag)
    {
      WS2812_Update(&LED_channel_A);
      WS2812_Update(&LED_channel_B);

      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_15);
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
      led_flag = 0;
    }
  }
  /* USER CODE END 3 */
}

/* ... (other functions remain the same: SystemClock_Config, MX_TIM2_Init, etc.) ... */

/* USER CODE BEGIN 4 */

/**
  * @brief  Period elapsed callback (TIM6 fires every 1 ms).
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    if (++ms_cnt % 5000 == 0)
      led_flag = 1;
  }
}

/**
  * @brief  PWM DMA transfer complete callback.
  *         Stops PWM+DMA so the WS2812 line stays low (reset).
  */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  WS2812_DMACompleteCallback(htim);
}

/**
  * @brief  UART Rx event callback (idle-line detected after DMA reception).
  *         Parses the RS485 protocol and updates LED colors with timeout recovery.
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == UART4)
  {
    /* Update last RX time - connection is alive */
    last_uart_rx_time = ms_cnt;
    
    /* Watchdog refresh */
    last_main_loop_tick = ms_cnt;

    /* Only process if we have complete packet: 0xAA 0x55 + CH + R + G + B + CRC = 7 bytes min */
    if (Size >= 7 && Size <= RS485_RX_BUF_SIZE)
    {
      /* Scan buffer for 0xAA 0x55 header - safely check boundaries */
      uint8_t start_idx = 0xFF;
      
      for (uint16_t i = 0; i <= (Size - 7); i++)
      {
        if (rs485_rx_buf[i] == 0xAA && rs485_rx_buf[i + 1] == 0x55)
        {
          start_idx = i;
          break;
        }
      }

      if (start_idx != 0xFF)
      {
        /* Packet format: [0xAA][0x55][CHANNEL][R][G][B][CHECKSUM] */
        uint8_t channel = rs485_rx_buf[start_idx + 2];
        uint8_t R = rs485_rx_buf[start_idx + 3];
        uint8_t G = rs485_rx_buf[start_idx + 4];
        uint8_t B = rs485_rx_buf[start_idx + 5];
        uint8_t received_crc = rs485_rx_buf[start_idx + 6];
        
        /* Calculate checksum: sum of [channel, R, G, B] */
        uint8_t calculated_crc = (channel + R + G + B) & 0xFF;
        
        /* Verify checksum and channel validity */
        if (calculated_crc == received_crc && (channel == 1 || channel == 2))
        {
          /* Valid packet - increment counter for diagnostics */
          uart_packet_count++;
          
          /* Apply color changes */
          WS2812_SetColor(&LED_channel_A, R, G, B);
          WS2812_Update(&LED_channel_A);
          WS2812_SetColor(&LED_channel_B, R, G, B);
          WS2812_Update(&LED_channel_B);
        }
      }
    }

    /* Clear buffer completely before re-arming */
    memset(rs485_rx_buf, 0, RS485_RX_BUF_SIZE);
    
    /* Re-arm DMA receive - MUST be done every time */
    /* If this fails, the error callback will trigger recovery */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rs485_rx_buf, RS485_RX_BUF_SIZE);
  }
}


/**
  * @brief  UART error callback - triggers graceful recovery.
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    /* Set recovery flag - recovery will be handled in main loop */
    uart_recovery_needed = 1;
  }
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 33;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 159;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM15 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM15_Init(void)
{

  /* USER CODE BEGIN TIM15_Init 0 */

  /* USER CODE END TIM15_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM15_Init 1 */

  /* USER CODE END TIM15_Init 1 */
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 1;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 99;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM15_Init 2 */

  /* USER CODE END TIM15_Init 2 */
  HAL_TIM_MspPostInit(&htim15);

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB14 PB15 PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Period elapsed callback (TIM6 fires every 1 ms).
  */
//void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
//{
//  if (htim->Instance == TIM6)
//  {
//    if (++ms_cnt % 5000 == 0)
//      led_flag = 1;
//  }
//}
//
///**
//  * @brief  PWM DMA transfer complete callback.
//  *         Stops PWM+DMA so the WS2812 line stays low (reset).
//  */
//void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
//{
//  WS2812_DMACompleteCallback(htim);
//}

/**
  * @brief  UART Rx event callback (idle-line detected after DMA reception).
  *         Parses the RS485 protocol and updates LED colors.
  *
  *         Protocol (same as cmake project):
  *           [0:1] Header  0x55 0xAA
  *           [2]   Channel  (1 = ch_A on TIM15, 2 = ch_B on TIM2)
  *           [3]   R
  *           [4]   G
  *           [5]   B
  *           [6]   Checksum  (sum of bytes [2..5], low 8 bits)
  */


//void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
//{
//  if (huart->Instance == UART4)
//  {
//    /* Update last RX time - connection is alive */
//    last_uart_rx_time = ms_cnt;
//
//    /* Scan buffer for 0xAA 0x55 header (matches Python sender byte order) */
//    uint8_t start_idx = 0xFF;
//    for (uint16_t i = 0; i + 1 < Size; i++)
//    {
//      if (rs485_rx_buf[i] == 0xAA && rs485_rx_buf[i + 1] == 0x55)
//      {
//        start_idx = i;
//        break;
//      }
//    }
//
//    if (start_idx != 0xFF && (start_idx + 7) <= RS485_RX_BUF_SIZE)
//    {
//      /* Validate checksum: sum of bytes [2..5] */
//      uint8_t checksum = 0;
//      for (uint8_t j = start_idx + 2; j < start_idx + 6; j++)
//        checksum += rs485_rx_buf[j];
//
//      if (checksum == rs485_rx_buf[start_idx + 6])
//      {
//        uint8_t channel = rs485_rx_buf[start_idx + 2];
//        uint8_t R = rs485_rx_buf[start_idx + 3];
//        uint8_t G = rs485_rx_buf[start_idx + 4];
//        uint8_t B = rs485_rx_buf[start_idx + 5];
//
//        if (channel == 1 || channel == 2)
//        {
//          WS2812_SetColor(&LED_channel_A, R, G, B);
//          WS2812_Update(&LED_channel_A);
//          WS2812_SetColor(&LED_channel_B, R, G, B);
//          WS2812_Update(&LED_channel_B);
//        }
//      }
//    }
//
//    /* Re-arm DMA receive */
//    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rs485_rx_buf, RS485_RX_BUF_SIZE);
//  }
//}

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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
