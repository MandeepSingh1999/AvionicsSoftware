/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ReadAccelGyroMagnetism.h"
#include "ReadBarometer.h"
#include "ReadCombustionChamberPressure.h"
#include "ReadGps.h"
#include "ReadOxidizerTankPressure.h"
#include "MonitorForEmergencyShutoff.h"
#include "EngineControl.h"
#include "ParachutesControl.h"
#include "LogData.h"
#include "TransmitData.h"
#include "AbortPhase.h"
#include "Data.h"
#include "FlightPhase.h"
#include "ValveControl.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;

CRC_HandleTypeDef hcrc;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_uart4_rx;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
static osThreadId readAccelGyroMagnetismTaskHandle;
static osThreadId readBarometerTaskHandle;
static osThreadId readCombustionChamberPressureTaskHandle;
static osThreadId readGpsTaskHandle;
static osThreadId readOxidizerTankPressureTaskHandle;
// Controls that will perform actions
static osThreadId monitorForEmergencyShutoffTaskHandle;
static osThreadId engineControlTaskHandle;
static osThreadId parachutesControlTaskHandle;
// Storing data
static osThreadId logDataTaskHandle;
static osThreadId transmitDataTaskHandle;
// Special abort thread
static osThreadId abortPhaseTaskHandle;

static const uint8_t LAUNCH_CMD_BYTE = 0x20;
static const uint8_t ARM_CMD_BYTE = 0x21;
static const uint8_t ABORT_CMD_BYTE = 0x2F;
static const uint8_t RESET_AVIONICS_CMD_BYTE = 0x4F;
static const uint8_t HEARTBEAT_BYTE = 0x46;
static const uint8_t OPEN_INJECTION_VALVE = 0x2A;
static const uint8_t CLOSE_INJECTION_VALVE = 0x2B;

uint8_t launchSystemsRxChar = 0;
uint8_t launchCmdReceived = 0;
uint8_t abortCmdReceived = 0;
uint8_t resetAvionicsCmdReceived = 0;

const int32_t HEARTBEAT_TIMEOUT = 3 * 60 * 1000; // 3 minutes
int32_t heartbeatTimer = 0; // Initalized to HEARTBEAT_TIMEOUT in MonitorForEmergencyShutoff thread

static const int FLIGHT_PHASE_DISPLAY_FREQ = 1000;
static const int FLIGHT_PHASE_BLINK_FREQ = 100;

char dma_rx_buffer[NMEA_MAX_LENGTH + 1] = {0};
GpsData* gpsData;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_UART4_Init(void);
static void MX_ADC3_Init(void);
static void MX_CRC_Init(void);
void StartDefaultTask(void const* argument);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

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
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_SPI1_Init();
    MX_SPI3_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_UART4_Init();
    MX_ADC3_Init();
    MX_CRC_Init();
    /* USER CODE BEGIN 2 */
    // Data primitive structs
    AccelGyroMagnetismData* accelGyroMagnetismData =
        malloc(sizeof(AccelGyroMagnetismData));
    BarometerData* barometerData =
        malloc(sizeof(BarometerData));
    CombustionChamberPressureData* combustionChamberPressureData =
        malloc(sizeof(CombustionChamberPressureData));
    gpsData =
        calloc(1, sizeof(GpsData));
    OxidizerTankPressureData* oxidizerTankPressureData =
        malloc(sizeof(OxidizerTankPressureData));

    osMutexDef(ACCEL_GYRO_MAGNETISM_DATA_MUTEX);
    accelGyroMagnetismData->mutex_ = osMutexCreate(osMutex(ACCEL_GYRO_MAGNETISM_DATA_MUTEX));
    accelGyroMagnetismData->accelX_ = -1;
    accelGyroMagnetismData->accelY_ = -2;
    accelGyroMagnetismData->accelZ_ = -3;
    accelGyroMagnetismData->gyroX_ = -4;
    accelGyroMagnetismData->gyroY_ = -5;
    accelGyroMagnetismData->gyroZ_ = -6;
    accelGyroMagnetismData->magnetoX_ = -7;
    accelGyroMagnetismData->magnetoY_ = -8;
    accelGyroMagnetismData->magnetoZ_ = -9;

    osMutexDef(BAROMETER_DATA_MUTEX);
    barometerData->mutex_ = osMutexCreate(osMutex(BAROMETER_DATA_MUTEX));
    barometerData->pressure_ = -10;
    barometerData->temperature_ = -11;

    osMutexDef(COMBUSTION_CHAMBER_PRESSURE_DATA_MUTEX);
    combustionChamberPressureData->mutex_ = osMutexCreate(osMutex(COMBUSTION_CHAMBER_PRESSURE_DATA_MUTEX));
    combustionChamberPressureData->pressure_ = -12;

    osMutexDef(GPS_DATA_MUTEX);
    gpsData->mutex_ = osMutexCreate(osMutex(GPS_DATA_MUTEX));

    osMutexDef(OXIDIZER_TANK_PRESSURE_DATA_MUTEX);
    oxidizerTankPressureData->mutex_ = osMutexCreate(osMutex(OXIDIZER_TANK_PRESSURE_DATA_MUTEX));
    oxidizerTankPressureData->pressure_ = -17;

    // Data containers
    AllData* allData =
        malloc(sizeof(AllData));
    allData->accelGyroMagnetismData_ = accelGyroMagnetismData;
    allData->barometerData_ = barometerData;
    allData->combustionChamberPressureData_ = combustionChamberPressureData;
    allData->gpsData_ = gpsData;
    allData->oxidizerTankPressureData_ = oxidizerTankPressureData;

    ParachutesControlData* parachutesControlData =
        malloc(sizeof(ParachutesControlData));
    parachutesControlData->accelGyroMagnetismData_ = accelGyroMagnetismData;
    parachutesControlData->barometerData_ = barometerData;
    /* USER CODE END 2 */

    /* USER CODE BEGIN RTOS_MUTEX */
    osMutexDef(FLIGHT_PHASE_MUTEX);
    flightPhaseMutex = osMutexCreate(osMutex(FLIGHT_PHASE_MUTEX));
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    HAL_UART_Receive_IT(&huart2, &launchSystemsRxChar, 1);

    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* definition and creation of defaultTask */
    osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */

    osThreadDef(
        readAccelGyroMagnetismThread,
        readAccelGyroMagnetismTask,
        osPriorityNormal,
        1,
        configMINIMAL_STACK_SIZE
    );
    readAccelGyroMagnetismTaskHandle =
        osThreadCreate(osThread(readAccelGyroMagnetismThread), accelGyroMagnetismData);

    osThreadDef(
        readBarometerThread,
        readBarometerTask,
        osPriorityNormal,
        1,
        configMINIMAL_STACK_SIZE
    );
    readBarometerTaskHandle =
        osThreadCreate(osThread(readBarometerThread), barometerData);

    osThreadDef(
        readCombustionChamberPressureThread,
        readCombustionChamberPressureTask,
        osPriorityAboveNormal,
        1,
        configMINIMAL_STACK_SIZE
    );
    readCombustionChamberPressureTaskHandle =
        osThreadCreate(osThread(readCombustionChamberPressureThread), combustionChamberPressureData);

    osThreadDef(
        readGpsThread,
        readGpsTask,
        osPriorityBelowNormal,
        1,
        configMINIMAL_STACK_SIZE
    );
    readGpsTaskHandle =
        osThreadCreate(osThread(readGpsThread), gpsData);

    osThreadDef(
        readOxidizerTankPressureThread,
        readOxidizerTankPressureTask,
        osPriorityAboveNormal,
        1,
        configMINIMAL_STACK_SIZE
    );
    readOxidizerTankPressureTaskHandle =
        osThreadCreate(osThread(readOxidizerTankPressureThread), oxidizerTankPressureData);

    osThreadDef(
        monitorForEmergencyShutoffThread,
        monitorForEmergencyShutoffTask,
        osPriorityHigh,
        1,
        configMINIMAL_STACK_SIZE
    );
    monitorForEmergencyShutoffTaskHandle =
        osThreadCreate(osThread(monitorForEmergencyShutoffThread), accelGyroMagnetismData);

    osThreadDef(
        engineControlThread,
        engineControlTask,
        osPriorityNormal,
        1,
        configMINIMAL_STACK_SIZE * 2
    );
    engineControlTaskHandle =
        osThreadCreate(osThread(engineControlThread), oxidizerTankPressureData);

    osThreadDef(
        parachutesControlThread,
        parachutesControlTask,
        osPriorityAboveNormal,
        1,
        configMINIMAL_STACK_SIZE * 2
    );
    parachutesControlTaskHandle =
        osThreadCreate(osThread(parachutesControlThread), parachutesControlData);

    osThreadDef(
        logDataThread,
        logDataTask,
        osPriorityNormal,
        1,
        configMINIMAL_STACK_SIZE * 3
    );
    logDataTaskHandle =
        osThreadCreate(osThread(logDataThread), allData);

    osThreadDef(
        transmitDataThread,
        transmitDataTask,
        osPriorityNormal,
        1,
        configMINIMAL_STACK_SIZE * 3
    );
    transmitDataTaskHandle =
        osThreadCreate(osThread(transmitDataThread), allData);

    osThreadDef(
        abortPhaseThread,
        abortPhaseTask,
        osPriorityHigh,
        1,
        configMINIMAL_STACK_SIZE
    );
    abortPhaseTaskHandle =
        osThreadCreate(osThread(abortPhaseThread), NULL);
    /* USER CODE END RTOS_THREADS */

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }

    free(accelGyroMagnetismData);
    free(barometerData);
    free(combustionChamberPressureData);
    free(gpsData);
    free(oxidizerTankPressureData);
    free(allData);
    free(parachutesControlData);
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
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /** Initializes the CPU, AHB and APB busses clocks
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB busses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV8;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

    /* USER CODE BEGIN ADC1_Init 0 */

    /* USER CODE END ADC1_Init 0 */

    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */
    /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
    */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
    */
    sConfig.Channel = ADC_CHANNEL_8;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN ADC1_Init 2 */

    /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

    /* USER CODE BEGIN ADC2_Init 0 */

    /* USER CODE END ADC2_Init 0 */

    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC2_Init 1 */

    /* USER CODE END ADC2_Init 1 */
    /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
    */
    hadc2.Instance = ADC2;
    hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc2.Init.Resolution = ADC_RESOLUTION_12B;
    hadc2.Init.ScanConvMode = DISABLE;
    hadc2.Init.ContinuousConvMode = ENABLE;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc2.Init.NbrOfConversion = 1;
    hadc2.Init.DMAContinuousRequests = DISABLE;
    hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;

    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
    */
    sConfig.Channel = ADC_CHANNEL_9;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN ADC2_Init 2 */

    /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

    /* USER CODE BEGIN ADC3_Init 0 */

    /* USER CODE END ADC3_Init 0 */

    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC3_Init 1 */

    /* USER CODE END ADC3_Init 1 */
    /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
    */
    hadc3.Instance = ADC3;
    hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc3.Init.Resolution = ADC_RESOLUTION_12B;
    hadc3.Init.ScanConvMode = DISABLE;
    hadc3.Init.ContinuousConvMode = ENABLE;
    hadc3.Init.DiscontinuousConvMode = DISABLE;
    hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc3.Init.NbrOfConversion = 1;
    hadc3.Init.DMAContinuousRequests = DISABLE;
    hadc3.Init.EOCSelection = ADC_EOC_SEQ_CONV;

    if (HAL_ADC_Init(&hadc3) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
    */
    sConfig.Channel = ADC_CHANNEL_10;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN ADC3_Init 2 */

    /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

    /* USER CODE BEGIN CRC_Init 0 */

    /* USER CODE END CRC_Init 0 */

    /* USER CODE BEGIN CRC_Init 1 */

    /* USER CODE END CRC_Init 1 */
    hcrc.Instance = CRC;

    if (HAL_CRC_Init(&hcrc) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN CRC_Init 2 */

    /* USER CODE END CRC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

    /* USER CODE BEGIN SPI1_Init 0 */

    /* USER CODE END SPI1_Init 0 */

    /* USER CODE BEGIN SPI1_Init 1 */

    /* USER CODE END SPI1_Init 1 */
    /* SPI1 parameter configuration*/
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN SPI1_Init 2 */

    /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

    /* USER CODE BEGIN SPI2_Init 0 */

    /* USER CODE END SPI2_Init 0 */

    /* USER CODE BEGIN SPI2_Init 1 */

    /* USER CODE END SPI2_Init 1 */
    /* SPI2 parameter configuration*/
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN SPI2_Init 2 */

    /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

    /* USER CODE BEGIN SPI3_Init 0 */

    /* USER CODE END SPI3_Init 0 */

    /* USER CODE BEGIN SPI3_Init 1 */

    /* USER CODE END SPI3_Init 1 */
    /* SPI3 parameter configuration*/
    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi3.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi3) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN SPI3_Init 2 */

    /* USER CODE END SPI3_Init 2 */

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
    huart4.Init.BaudRate = 9600;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN UART4_Init 2 */

    /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
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

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

    /* USER CODE BEGIN USART2_Init 0 */

    /* USER CODE END USART2_Init 0 */

    /* USER CODE BEGIN USART2_Init 1 */

    /* USER CODE END USART2_Init 1 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN USART2_Init 2 */

    /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Stream2_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, IMU_CS_Pin | PROPULSION_3_VALVE_Pin | INJECTION_VALVE_Pin | SD1_CS_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, MAG_CS_Pin | LED1_Pin | BARO_CS_Pin | MUX_POWER_TEMP_Pin
                      | MAIN_PARACHUTE_Pin | DROGUE_PARACHUTE_TEMP_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, LED2_Pin | ACCEL_CS_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LOWER_VENT_VALVE_GPIO_Port, LOWER_VENT_VALVE_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : PC13 PC14 PC15 PC1
                             PC2 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_1
                          | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pin : LAUNCH_Pin */
    GPIO_InitStruct.Pin = LAUNCH_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LAUNCH_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : IMU_CS_Pin PROPULSION_3_VALVE_Pin INJECTION_VALVE_Pin SD1_CS_Pin */
    GPIO_InitStruct.Pin = IMU_CS_Pin | PROPULSION_3_VALVE_Pin | INJECTION_VALVE_Pin | SD1_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : MAG_CS_Pin LED1_Pin BARO_CS_Pin MUX_POWER_TEMP_Pin
                             MAIN_PARACHUTE_Pin DROGUE_PARACHUTE_TEMP_Pin */
    GPIO_InitStruct.Pin = MAG_CS_Pin | LED1_Pin | BARO_CS_Pin | MUX_POWER_TEMP_Pin
                          | MAIN_PARACHUTE_Pin | DROGUE_PARACHUTE_TEMP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : LED2_Pin ACCEL_CS_Pin */
    GPIO_InitStruct.Pin = LED2_Pin | ACCEL_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : PB10 PB11 PB4 PB5
                             PB8 PB9 */
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_4 | GPIO_PIN_5
                          | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : PA8 PA9 PA10 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin : LOWER_VENT_VALVE_Pin */
    GPIO_InitStruct.Pin = LOWER_VENT_VALVE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LOWER_VENT_VALVE_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART2)
    {
        if (launchSystemsRxChar == LAUNCH_CMD_BYTE)
        {
            if (ARM == getCurrentFlightPhase())
            {
                launchCmdReceived++;
            }
        }
        else if (launchSystemsRxChar == ARM_CMD_BYTE)
        {
            if (PRELAUNCH == getCurrentFlightPhase())
            {
                newFlightPhase(ARM);
            }
        }
        else if (launchSystemsRxChar == ABORT_CMD_BYTE)
        {
            abortCmdReceived = 1;
        }
        else if (launchSystemsRxChar == RESET_AVIONICS_CMD_BYTE)
        {
            resetAvionicsCmdReceived = 1;
        }
        else if (launchSystemsRxChar == HEARTBEAT_BYTE)
        {
            heartbeatTimer = HEARTBEAT_TIMEOUT;
        }
        else if (launchSystemsRxChar == OPEN_INJECTION_VALVE)
        {
            if (IS_ABORT_PHASE)
            {
                openInjectionValve();
            }
        }
        else if (launchSystemsRxChar == CLOSE_INJECTION_VALVE)
        {
            if (IS_ABORT_PHASE)
            {
                closeInjectionValve();
            }
        }
    }
    else if (huart->Instance == UART4)
    {
        static char rx_buffer[NMEA_MAX_LENGTH + 1];
        static int rx_index = 0;
        static int gpggaDetected = 0;
        char message[6] = "$GPGGA";

        for (int i = 0; i < NMEA_MAX_LENGTH + 1; i++)
        {
            char rx = dma_rx_buffer[i]; // Read 1 character

            if ((rx == '\r') || (rx == '\n')) // End of line character has been reached
            {
                if (rx_index != 0 && rx_buffer[0] == '$') // Check that buffer has content and that the message is valid
                {
                    rx_buffer[rx_index++] = 0;

                    if (osMutexWait(gpsData->mutex_, 0) == osOK)
                    {
                        memcpy(&gpsData->buffer_, &rx_buffer, rx_index); // Copy to gps data buffer from rx_buffer
                        gpsData->parseFlag_ = 1; // Data in gps data buffer is ready to be parsed
                    }

                    osMutexRelease(gpsData->mutex_);

                    // Reset back to initial values
                    rx_index = 0;
                    gpggaDetected = 0;
                }
            }
            else
            {
                if ((rx == '$') || (rx_index == NMEA_MAX_LENGTH)) // If start character received or end of rx buffer reached
                {
                    // Reset back to initial values
                    rx_index = 0;
                    gpggaDetected = 0;
                    rx_buffer[rx_index++] = rx;
                }
                else if (gpggaDetected == 0)
                {
                    if (rx_index >= 6) // If the first 6 characters follow $GPGGA, set gpggaDetected to 1
                    {
                        gpggaDetected = 1;
                        rx_buffer[rx_index++] = rx; // Contents of the rx_buffer will be $GPGGA at this point
                    }
                    else if (rx == message[rx_index]) // Check if the first 6 characters follow $GPGGA
                    {
                        rx_buffer[rx_index++] = rx;
                    }
                    else
                    {
                        // If any of the first 6 characters does not follow $GPGGA, reset to initial values
                        rx_index = 0;
                        gpggaDetected = 0;
                    }
                }
                else
                {
                    rx_buffer[rx_index++] = rx; // Copy received characters to rx_buffer
                }
            }
        }

        HAL_UART_Receive_DMA(&huart4, (uint8_t*) &dma_rx_buffer, NMEA_MAX_LENGTH + 1);
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const* argument)
{
    /* USER CODE BEGIN 5 */
    /* Infinite loop */
    HAL_GPIO_WritePin(MUX_POWER_TEMP_GPIO_Port, MUX_POWER_TEMP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);

    for (;;)
    {
        osDelay(FLIGHT_PHASE_DISPLAY_FREQ);

        // blink once for PRELAUNCH phase
        // blink twice for BURN phase
        // blink 3 times for COAST phase
        // blink 4 times for DROGUE_DESCENT phase
        // blink 5 times for MAIN_DESCENT phase
        for (int i = -1; i < getCurrentFlightPhase(); i++)
        {
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
            osDelay(FLIGHT_PHASE_BLINK_FREQ);
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
            osDelay(FLIGHT_PHASE_BLINK_FREQ);
        }
    }

    /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM1)
    {
        HAL_IncTick();
    }

    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
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
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
      ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
