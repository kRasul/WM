/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"

/* USER CODE BEGIN Includes */
#include "timeMgmnt.h"
#include "portsMgmnt.h"
#include "uartDataExchMgmnt.h"
#include "tm_stm32_hd44780.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan;

CRC_HandleTypeDef hcrc;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
timeStr timeConsPumpStarted;                    // �����, ����� ��������� ��� ��� ������� ����� �� �������
timeStr waterMissDetectedTime;

static uint32_t lastMilLitWentOut = 0;          // ��������� ��������� ������������ ��������, ����� ���� ��������� ��������� ������������ 

machineParameters wa;                           // ��������� ��������
filtersStr filters;                             // ��������� ��������� (�������� �� UART)
moneyStats money;                               // �������� �� ��� �����, �������� ������, �������� ����������
counters cnt = {0};


float waterPrice = 400.0;                       // ���� �����, � ��������
uint8_t outPumpNoWaterStopTime = 3;             // ������ �� ��������� ��������� ������, ���� ��� ����
uint8_t startContVolume = 15;                   // ����������� ����� ���� � ����������
uint8_t containerMinVolume = 3;                 // ����������� ����� ���� � ����������
uint8_t maxContainerVolume = 95;                // ����� ���������� � �����

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN_Init(void);
static void MX_CRC_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

void containerMgmnt() {
  wa.currentContainerVolume = (cnt.milLitContIn - cnt.milLitWentOut/* - cnt.milLitloseCounter*/) / 1000;
  if (cnt.milLitWentOut + (maxContainerVolume-5)*1000 > cnt.milLitContIn) wa.container = NOT_FULL;
  checkMagistralPressure();
  if (wa.magistralPressure == HI_PRESSURE && wa.container != FULL) {
    if (wa.mainPump == WORKING) return;
    delayMilliseconds(100);
    if (wa.magistralPressure == HI_PRESSURE) {
      MAINP_ON();
      MAINV_ON();
//      FILTV_ON();
    }
  }
  if (wa.magistralPressure == NO_PRESSURE || wa.container == FULL) {
    if (wa.mainPump == STOPPED) return;
    MAINP_OFF();
    MAINV_OFF();
//    FILTV_OFF();
  }  
}

void outPumpMgmnt() {
  static timeStr timeCheck = {0};
  static uint32_t lastMillilit = 0;
  if (wa.waterMissDetected == true) {
    if (wa.consumerPump == WORKING) CONSUMP_OFF();
    if (getTimeDiff(waterMissDetectedTime) > 500) {
      wa.waterMissDetected = false;
    }
  }      
  if (getTimeDiff(timeCheck) > 1000) {
    writeTime(&timeCheck);
    if (cnt.milLitWentOut > lastMillilit + 10) lastMillilit = cnt.milLitWentOut;      
    else 
      if (getTimeDiff(timeConsPumpStarted) > outPumpNoWaterStopTime*1000) {
        wa.currentContainerVolume = 0;
        if (wa.consumerPump == WORKING) CONSUMP_OFF();
      }
  }
}

void lcdMgmnt() {
  static timeStr refreshLCDtime = {0}; 
  if (getTimeDiff(refreshLCDtime) > 250) {
    writeTime(&refreshLCDtime);
    if (wa.machineState == NOT_READY)   printNotReady(wa.currentContainerVolume);
    if (wa.machineState == WAIT)        printWait(wa.currentContainerVolume);
    if (wa.machineState == JUST_PAID)   {
      uint32_t temp = money.sessionPaid;        // avoid undefined behavior warning
      printPaid(temp/100, (uint16_t)(((float)temp*10.0)/waterPrice));
    }
    if (wa.machineState == WORK) {
      wa.litersLeftFromSession = (uint32_t)((money.leftFromPaid*10.0)/waterPrice);
      printGiven((uint32_t)wa.litersLeftFromSession, (cnt.milLitWentOut - lastMilLitWentOut) / 100, (uint32_t) money.leftFromPaid/100);
    }
    if (wa.machineState == NO_TARE)     printLoseDetected();
    if (wa.machineState == CONFIG)      TM_HD44780_Clear();
    if (wa.machineState == WASH_FILTER) TM_HD44780_Clear();
    if (wa.machineState == SERVICE)     TM_HD44780_Clear();
    if (wa.machineState == FREE)        TM_HD44780_Clear();
  }    
} 

void prepareToTransition (){
  if (wa.machineState == NOT_READY){
    INHIBIT_DIS();
  }
  else INHIBIT_EN();
  
  if (wa.machineState == WAIT){    
    INHIBIT_EN();
    CONSUMP_OFF();
    wa.machineState = WAIT;
  }
  
  if (wa.machineState == WORK) {
    if (wa.lastMachineState == JUST_PAID) lastMilLitWentOut = cnt.milLitWentOut;
    disableButtonsForTime();
    disableSensorsForTime();  
    CONSUMP_ON();
  }
  
  if (wa.machineState == NO_TARE) {
    writeTime(&waterMissDetectedTime);
    if (wa.consumerPump == WORKING) {
      disableButtonsForTime();  
      disableSensorsForTime();  
      CONSUMP_OFF();
    }
  }
  wa.lastMachineState = wa.machineState; 
}
                                                  
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  MX_ADC1_Init();
  MX_CAN_Init();
  MX_CRC_Init();
  MX_SPI2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  initUART();
  TM_HD44780_Init(16, 2, 1000);
  wa.machineState = WAIT;
  wa.lastMachineState = FREE;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  //input10Counter = 0;
  //out10Counter = 0;
  cnt.milLitContIn = startContVolume * 1000;
#ifdef DEBUG_PCB_MODE
  setUpCheckGPIO();
  clrAll();
  while(1) {
    static timeStr lastTime = {0};
    uartDataExchMgmnt();
    if (getTimeDiff(lastTime) > TIME_BETWEEN_PORT_CHANGES) {      
      writeTime(&lastTime);
      setupIn();
      setupGPIOOut();
      setupULN2003Out();
      setupMOSFET();
      setupPwrOut();
      setupW26();
    }    
    if (isAdminUpButtonPressed()) {
      printAdminUp();
      disableButtonsForTime();  
      clrServUpButton();
    }
    if (isAdminDownButtonPressed()) {
      printAdminDown();
      disableButtonsForTime();  
      clrServDownButton();
    }
    if (isAdminLeftButtonPressed()) {
      printAdminLeft();
      disableButtonsForTime();  
      clrServLeftButton();
    }
    if (isAdminRightButtonPressed()) {
      printAdminRight();
      disableButtonsForTime();  
      clrServRightButton();
    }
  }
#endif
  while (1) {}
  {
////// MANAGE STUFF    
  //  containerMgmnt();
    uartDataExchMgmnt();
    lcdMgmnt();
    if (wa.machineState == WAIT) {
    }
    if (wa.machineState == NOT_READY) {
    }
    if (wa.machineState == WORK) {
      if (wa.waterMissDetected == true){
        wa.machineState = NO_TARE;
        prepareToTransition();
      }
      outPumpMgmnt();
      money.leftFromPaid = money.sessionPaid - (((double)cnt.milLitWentOut - (double)lastMilLitWentOut) / 1000.0) * waterPrice;      
      if (money.leftFromPaid <= 0) {
        wa.machineState = WAIT;
        prepareToTransition();
        money.totalPaid += money.sessionPaid - (uint32_t)money.leftFromPaid;
        money.sessionPaid = 0;
        money.leftFromPaid = 0.0;
      }
    }

////// TRANSITIONS MANAGMENT
    if (money.sessionPaid > 0 && wa.machineState == WAIT){
      wa.machineState = JUST_PAID;
      prepareToTransition();
    }
    if (wa.currentContainerVolume < containerMinVolume && wa.machineState == WAIT) {
      wa.machineState = NOT_READY;
      prepareToTransition();
    }
    if (wa.currentContainerVolume >= containerMinVolume && wa.machineState == NOT_READY) {
      wa.machineState = WAIT;
      prepareToTransition();
    }    
////// BUTTONS PROCESSING        
    if (isUserButtonPressed()) {
      if (wa.machineState == JUST_PAID) {
        wa.machineState = WORK;
        prepareToTransition();
      }
      if (wa.machineState == WORK) {
        disableSensorsForTime();
        if (wa.consumerPump == WORKING) CONSUMP_OFF();
        else CONSUMP_ON();
      }
      if (wa.machineState == NO_TARE && getTimeDiff(waterMissDetectedTime) > 1000) {
        wa.machineState = WORK;
        prepareToTransition();
      } 
      disableButtonsForTime();  
      clrUserButton();
    } 
    if (isAdminUpButtonPressed()) {
      
      disableButtonsForTime();  
      clrServUpButton();
    }
    if (isAdminDownButtonPressed()) {
      
      disableButtonsForTime();  
      clrServDownButton();
    }
    if (isAdminLeftButtonPressed()) {
      
      disableButtonsForTime();  
      clrServLeftButton();
    }
    if (isAdminRightButtonPressed()) {
      
      disableButtonsForTime();  
      clrServRightButton();
    }
    
    
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Common config 
    */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* CAN init function */
static void MX_CAN_Init(void)
{

  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 16;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SJW = CAN_SJW_1TQ;
  hcan.Init.BS1 = CAN_BS1_1TQ;
  hcan.Init.BS2 = CAN_BS2_1TQ;
  hcan.Init.TTCM = DISABLE;
  hcan.Init.ABOM = DISABLE;
  hcan.Init.AWUM = DISABLE;
  hcan.Init.NART = DISABLE;
  hcan.Init.RFLM = DISABLE;
  hcan.Init.TXFP = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* CRC init function */
static void MX_CRC_Init(void)
{

  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* SPI2 init function */
static void MX_SPI2_Init(void)
{

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* TIM3 init function */
static void MX_TIM3_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 47;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART3 init function */
static void MX_USART3_UART_Init(void)
{

  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_D7_Pin|LCD_D6_Pin|LCD_D5_Pin|LCD_D4_Pin 
                          |OUT3_Pin|OUT4_Pin|PWR5_Pin|PWR4_Pin 
                          |PWR3_Pin|PWR2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_E_Pin|LCD_RS_Pin|OUT1_Pin|OUT2_Pin 
                          |PWR1_Pin|W26_DO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, OUT5_Pin|OUT6_Pin|OUT7_Pin|NINT_TEMP3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, OUT8_Pin|OUT9_Pin|OUT10_Pin|OUT11_Pin 
                          |OUT12_Pin|OUT13_Pin|OUT14_Pin|MOSFET1_Pin 
                          |MOSFET2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO7_Pin|GPIO6_Pin|GPIO5_Pin|GPIO4_Pin 
                          |GPIO3_Pin|GPIO2_Pin|GPIO1_Pin|PWR6_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : INT_IN4_Pin INT_IN3_Pin INT_IN2_Pin INT_IN1_Pin */
  GPIO_InitStruct.Pin = INT_IN4_Pin|INT_IN3_Pin|INT_IN2_Pin|INT_IN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : SERV_BUT4_Pin INT_IN6_Pin INT_IN5_Pin */
  GPIO_InitStruct.Pin = SERV_BUT4_Pin|INT_IN6_Pin|INT_IN5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : SERV_BUT3_Pin SERV_BUT2_Pin SERV_BUT1_Pin W26_DI_Pin 
                           INT_TEMP2_Pin INT_TEMP1_Pin */
  GPIO_InitStruct.Pin = SERV_BUT3_Pin|SERV_BUT2_Pin|SERV_BUT1_Pin|W26_DI_Pin 
                          |INT_TEMP2_Pin|INT_TEMP1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D7_Pin LCD_D6_Pin LCD_D5_Pin LCD_D4_Pin 
                           OUT3_Pin OUT4_Pin PWR5_Pin PWR4_Pin 
                           PWR3_Pin PWR2_Pin */
  GPIO_InitStruct.Pin = LCD_D7_Pin|LCD_D6_Pin|LCD_D5_Pin|LCD_D4_Pin 
                          |OUT3_Pin|OUT4_Pin|PWR5_Pin|PWR4_Pin 
                          |PWR3_Pin|PWR2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_E_Pin LCD_RS_Pin OUT1_Pin OUT2_Pin 
                           PWR1_Pin W26_DO_Pin */
  GPIO_InitStruct.Pin = LCD_E_Pin|LCD_RS_Pin|OUT1_Pin|OUT2_Pin 
                          |PWR1_Pin|W26_DO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : OUT5_Pin OUT6_Pin OUT7_Pin NINT_TEMP3_Pin */
  GPIO_InitStruct.Pin = OUT5_Pin|OUT6_Pin|OUT7_Pin|NINT_TEMP3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : OUT8_Pin OUT9_Pin OUT10_Pin OUT11_Pin 
                           OUT12_Pin OUT13_Pin OUT14_Pin MOSFET1_Pin 
                           MOSFET2_Pin */
  GPIO_InitStruct.Pin = OUT8_Pin|OUT9_Pin|OUT10_Pin|OUT11_Pin 
                          |OUT12_Pin|OUT13_Pin|OUT14_Pin|MOSFET1_Pin 
                          |MOSFET2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : GPIO7_Pin GPIO6_Pin GPIO5_Pin GPIO4_Pin 
                           GPIO3_Pin GPIO2_Pin GPIO1_Pin PWR6_Pin */
  GPIO_InitStruct.Pin = GPIO7_Pin|GPIO6_Pin|GPIO5_Pin|GPIO4_Pin 
                          |GPIO3_Pin|GPIO2_Pin|GPIO1_Pin|PWR6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : NINT_IN21_Pin NINT_IN20_Pin NINT_IN19_Pin NINT_IN18_Pin 
                           NINT_IN17_Pin NINT_IN16_Pin NINT_IN15_Pin NINT_IN14_Pin */
  GPIO_InitStruct.Pin = NINT_IN21_Pin|NINT_IN20_Pin|NINT_IN19_Pin|NINT_IN18_Pin 
                          |NINT_IN17_Pin|NINT_IN16_Pin|NINT_IN15_Pin|NINT_IN14_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : NINT_IN13_Pin NINT_IN12_Pin NINT_IN11_Pin NINT_IN10_Pin */
  GPIO_InitStruct.Pin = NINT_IN13_Pin|NINT_IN12_Pin|NINT_IN11_Pin|NINT_IN10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : INT_IN9_Pin INT_IN8_Pin INT_IN7_Pin */
  GPIO_InitStruct.Pin = INT_IN9_Pin|INT_IN8_Pin|INT_IN7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */

void setUpCheckGPIO() {
  GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = INT_IN4_Pin|INT_IN3_Pin|INT_IN2_Pin|INT_IN1_Pin|OUT8_Pin|
                        OUT9_Pin|OUT10_Pin|OUT11_Pin|OUT12_Pin|OUT13_Pin|OUT14_Pin|
                        MOSFET1_Pin|MOSFET2_Pin|INT_IN6_Pin|INT_IN5_Pin;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = W26_DI_Pin|INT_TEMP2_Pin|INT_TEMP1_Pin|LCD_D7_Pin|LCD_D6_Pin|LCD_D5_Pin|
                        LCD_D4_Pin|OUT3_Pin|OUT4_Pin|PWR5_Pin|PWR4_Pin|PWR3_Pin|PWR2_Pin|
                        INT_TEMP2_Pin|INT_TEMP1_Pin;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_E_Pin|LCD_RS_Pin|OUT1_Pin|OUT2_Pin|PWR1_Pin|W26_DO_Pin;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = OUT5_Pin|OUT6_Pin|OUT7_Pin|NINT_TEMP3_Pin|NINT_IN13_Pin|NINT_IN12_Pin|
                        NINT_IN11_Pin|NINT_IN10_Pin|INT_IN9_Pin|INT_IN8_Pin|INT_IN7_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO7_Pin|GPIO6_Pin|GPIO5_Pin|GPIO4_Pin|GPIO3_Pin|GPIO2_Pin|
                        GPIO1_Pin|PWR6_Pin|NINT_IN21_Pin|NINT_IN20_Pin|NINT_IN19_Pin|
                        NINT_IN18_Pin|NINT_IN17_Pin|NINT_IN16_Pin|NINT_IN15_Pin|NINT_IN14_Pin;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Pin = SERV_BUT4_Pin;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = SERV_BUT3_Pin|SERV_BUT2_Pin|SERV_BUT1_Pin;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void setupGPIOOut() {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(GPIO1_GPIO_Port, GPIO1_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(GPIO2_GPIO_Port, GPIO2_Pin, state);
              break;
    case 2:   HAL_GPIO_WritePin(GPIO3_GPIO_Port, GPIO3_Pin, state);
              break;
    case 3:   HAL_GPIO_WritePin(GPIO4_GPIO_Port, GPIO4_Pin, state);
              break;
    case 4:   HAL_GPIO_WritePin(GPIO5_GPIO_Port, GPIO5_Pin, state);
              break;
    case 5:   HAL_GPIO_WritePin(GPIO6_GPIO_Port, GPIO6_Pin, state);
              break;
    case 6:   HAL_GPIO_WritePin(GPIO7_GPIO_Port, GPIO7_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 6) num = 0;
    state = GPIO_PIN_SET;
  }
}


void setupMOSFET() {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(MOSFET1_GPIO_Port, MOSFET1_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(MOSFET2_GPIO_Port, MOSFET2_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 1) num = 0;
    state = GPIO_PIN_SET;
  }
}

void setupW26() {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(W26_DO_GPIO_Port, W26_DO_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(W26_DI_GPIO_Port, W26_DI_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 1) num = 0;
    state = GPIO_PIN_SET;
  }
}

void setupIn() {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(INT_IN1_GPIO_Port, INT_IN1_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(INT_IN2_GPIO_Port, INT_IN2_Pin, state);
              break;
    case 2:   HAL_GPIO_WritePin(INT_IN3_GPIO_Port, INT_IN3_Pin, state);
              break;
    case 3:   HAL_GPIO_WritePin(INT_IN4_GPIO_Port, INT_IN4_Pin, state);
              break;
    case 4:   HAL_GPIO_WritePin(INT_IN5_GPIO_Port, INT_IN5_Pin, state);
              break;
    case 5:   HAL_GPIO_WritePin(INT_IN6_GPIO_Port, INT_IN6_Pin, state);
              break;
    case 6:   HAL_GPIO_WritePin(INT_IN7_GPIO_Port, INT_IN7_Pin, state);
              break;
    case 7:   HAL_GPIO_WritePin(INT_IN8_GPIO_Port, INT_IN8_Pin, state);
              break;
    case 8:   HAL_GPIO_WritePin(INT_IN9_GPIO_Port, INT_IN9_Pin, state);
              break;
    case 9:   HAL_GPIO_WritePin(NINT_IN10_GPIO_Port, NINT_IN10_Pin, state);
              break;
    case 10:  HAL_GPIO_WritePin(NINT_IN11_GPIO_Port, NINT_IN11_Pin, state);
              break;
    case 11:  HAL_GPIO_WritePin(NINT_IN12_GPIO_Port, NINT_IN12_Pin, state);
              break;
    case 12:  HAL_GPIO_WritePin(NINT_IN13_GPIO_Port, NINT_IN13_Pin, state);
              break;
    case 13:  HAL_GPIO_WritePin(NINT_IN14_GPIO_Port, NINT_IN14_Pin, state);
              break;
    case 14:  HAL_GPIO_WritePin(NINT_IN15_GPIO_Port, NINT_IN15_Pin, state);
              break;
    case 15:  HAL_GPIO_WritePin(NINT_IN16_GPIO_Port, NINT_IN16_Pin, state);
              break;
    case 16:  HAL_GPIO_WritePin(NINT_IN17_GPIO_Port, NINT_IN17_Pin, state);
              break;
    case 17:  HAL_GPIO_WritePin(NINT_IN18_GPIO_Port, NINT_IN18_Pin, state);
              break;
    case 18:  HAL_GPIO_WritePin(NINT_IN19_GPIO_Port, NINT_IN19_Pin, state);
              break;
    case 19:  HAL_GPIO_WritePin(NINT_IN20_GPIO_Port, NINT_IN20_Pin, state);
              break;
    case 20:  HAL_GPIO_WritePin(NINT_IN21_GPIO_Port, NINT_IN21_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 20) num = 0;
    state = GPIO_PIN_SET;
  }
}

void setupULN2003Out () {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, state);
              break;
    case 2:   HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, state);
              break;
    case 3:   HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, state);
              break;
    case 4:   HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, state);
              break;
    case 5:   HAL_GPIO_WritePin(OUT6_GPIO_Port, OUT6_Pin, state);
              break;
    case 6:   HAL_GPIO_WritePin(OUT7_GPIO_Port, OUT7_Pin, state);
              break;
    case 7:   HAL_GPIO_WritePin(OUT8_GPIO_Port, OUT8_Pin, state);
              break;
    case 8:   HAL_GPIO_WritePin(OUT9_GPIO_Port, OUT9_Pin, state);
              break;
    case 9:   HAL_GPIO_WritePin(OUT10_GPIO_Port, OUT10_Pin, state);
              break;
    case 10:  HAL_GPIO_WritePin(OUT11_GPIO_Port, OUT11_Pin, state);
              break;
    case 11:  HAL_GPIO_WritePin(OUT12_GPIO_Port, OUT12_Pin, state);
              break;
    case 12:  HAL_GPIO_WritePin(OUT13_GPIO_Port, OUT13_Pin, state);
              break;
    case 13:  HAL_GPIO_WritePin(OUT14_GPIO_Port, OUT14_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 13) num = 0;
    state = GPIO_PIN_SET;
  }
}

void setupPwrOut() {
  static uint8_t num = 0;
  static GPIO_PinState state = GPIO_PIN_SET;
  switch (num) {
    case 0:   HAL_GPIO_WritePin(PWR1_GPIO_Port, PWR1_Pin, state);
              break;
    case 1:   HAL_GPIO_WritePin(PWR2_GPIO_Port, PWR2_Pin, state);
              break;
    case 2:   HAL_GPIO_WritePin(PWR3_GPIO_Port, PWR3_Pin, state);
              break;
    case 3:   HAL_GPIO_WritePin(PWR4_GPIO_Port, PWR4_Pin, state);
              break;
    case 4:   HAL_GPIO_WritePin(PWR5_GPIO_Port, PWR5_Pin, state);
              break;
    case 5:   HAL_GPIO_WritePin(PWR6_GPIO_Port, PWR6_Pin, state);
              break;
  }
  if (state == GPIO_PIN_SET) {
    state = GPIO_PIN_RESET;
  }
  else {
    if (++num > 5) num = 0;
    state = GPIO_PIN_SET;
  }
}

void clrAll() {
  HAL_GPIO_WritePin(GPIOC, OUT3_Pin|OUT4_Pin|PWR5_Pin|PWR4_Pin|PWR3_Pin|PWR2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, OUT1_Pin|OUT2_Pin|PWR1_Pin|W26_DO_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, OUT5_Pin|OUT6_Pin|OUT7_Pin|NINT_TEMP3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, OUT8_Pin|OUT9_Pin|OUT10_Pin|OUT11_Pin|OUT12_Pin|OUT13_Pin|OUT14_Pin|MOSFET1_Pin|MOSFET2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO7_Pin|GPIO6_Pin|GPIO5_Pin|GPIO4_Pin|GPIO3_Pin|GPIO2_Pin|GPIO1_Pin|PWR6_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(W26_DI_GPIO_Port, W26_DI_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN1_GPIO_Port, INT_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN2_GPIO_Port, INT_IN2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN3_GPIO_Port, INT_IN3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN4_GPIO_Port, INT_IN4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN5_GPIO_Port, INT_IN5_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN6_GPIO_Port, INT_IN6_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN7_GPIO_Port, INT_IN7_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN8_GPIO_Port, INT_IN8_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INT_IN9_GPIO_Port, INT_IN9_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN10_GPIO_Port, NINT_IN10_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN11_GPIO_Port, NINT_IN11_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN12_GPIO_Port, NINT_IN12_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN13_GPIO_Port, NINT_IN13_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN14_GPIO_Port, NINT_IN14_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN15_GPIO_Port, NINT_IN15_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN16_GPIO_Port, NINT_IN16_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN17_GPIO_Port, NINT_IN17_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN18_GPIO_Port, NINT_IN18_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN19_GPIO_Port, NINT_IN19_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN20_GPIO_Port, NINT_IN20_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINT_IN21_GPIO_Port, NINT_IN21_Pin, GPIO_PIN_RESET);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
