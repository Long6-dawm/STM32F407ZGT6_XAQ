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
#include "adc.h"
#include "arm_math_types.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dsp_analyzer.h"
#include "usart.h"
#include "screen_app.h"
#include "screen_config.h"
#include "wave_render.h"
#include <string.h>
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

/* USER CODE BEGIN PV */
uint16_t AD_Value[1024];
float32_t adc_value[1024];
float32_t top1_freq = 0.0f, top1_amp = 0.0f;
float32_t top2_freq = 0.0f, top2_amp = 0.0f;
float32_t top3_freq = 0.0f, top3_amp = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t s_screen_sequence = 0u;
static uint32_t s_last_measure_ms = 0u;
static float s_fund_sm = 0.0f;

static void BuildScreenFrame(ScreenDataFrame *frame)
{
  float f_fund = 0.0f, amp_fund = 0.0f;
  float freq[3] = {top1_freq, top2_freq, top3_freq};
  float amp[3]  = {top1_amp,  top2_amp,  top3_amp};
  uint8_t i;

  memset(frame, 0, sizeof(*frame));
  frame->magic = SCREEN_FRAME_MAGIC;
  frame->version = SCREEN_FRAME_VERSION;
  frame->byte_len = sizeof(*frame);
  frame->sequence = ++s_screen_sequence;

  /* 基波 = 三个频率中最小且非零者 (含谐波时 top1 未必是基波) */
  for (i = 0u; i < 3u; i++)
  {
    if ((freq[i] > 0.0f) && ((f_fund == 0.0f) || (freq[i] < f_fund)))
    {
      f_fund = freq[i];
      amp_fund = amp[i];
    }
  }

  /* 基频 EMA 平滑: 稳定 period_pts, 消除波形伸缩/相位漂移 */
  if (f_fund > 0.0f)
  {
    if (s_fund_sm <= 0.0f)
    {
      s_fund_sm = f_fund;
    }
    else
    {
      s_fund_sm = 0.7f * s_fund_sm + 0.3f * f_fund;
    }
    f_fund = s_fund_sm;
  }

  frame->frequency_mhz = (uint32_t)(f_fund * 1000.0f);
  frame->vpp_uv = (uint32_t)(amp_fund * 2000.0f);
  frame->vrms_uv = (uint32_t)(amp_fund * 1000.0f / 1.41421356f);

  /* 谐波列表按频率升序 (冒泡排序, 忽略无效项) */
  for (i = 0u; i < 3u; i++)
  {
    uint8_t j;
    for (j = 0u; j < (3u - i - 1u); j++)
    {
      if ((freq[j] > freq[j + 1u]) && (freq[j] > 0.0f) && (freq[j + 1u] > 0.0f))
      {
        float tf = freq[j]; freq[j] = freq[j + 1u]; freq[j + 1u] = tf;
        float ta = amp[j];  amp[j]  = amp[j + 1u];  amp[j + 1u]  = ta;
      }
    }
  }
  for (i = 0u; i < 3u; i++)
  {
    frame->harmonic_freq_mhz[i] = (uint32_t)(freq[i] * 1000.0f);
    frame->harmonic_rms_uv[i] = (uint32_t)(amp[i] * 1000.0f / 1.41421356f);
  }

  frame->mode = (uint8_t)ScreenApp_GetDisplayMode();
  frame->wave_periods = (uint8_t)ScreenApp_GetWavePeriods();
  frame->wave_count = WaveRender_Build(AD_Value, 1024u, f_fund,
                                       (uint8_t)ScreenApp_GetWavePeriods(), frame->wave);
  DSP_Analyzer_GetSpectrum(frame->fft, &frame->fft_count);
}

static void CaptureAndAnalyze(void)
{
  uint32_t i;

  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)AD_Value, 1024);
  HAL_Delay(3);

  for (i = 0u; i < 1024u; i++)
  {
    adc_value[i] = (float32_t)AD_Value[i];
  }

  DSP_Analyzer_Process(adc_value,
                       &top1_freq, &top1_amp,
                       &top2_freq, &top2_amp,
                       &top3_freq, &top3_amp);
  top1_amp = top1_amp / 4.01f;
  top2_amp = top2_amp / 4.01f;
  top3_amp = top3_amp / 4.01f;
}
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
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start(&htim3);

  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)AD_Value, 1024);

  HAL_Delay(500);

  for(uint32_t i = 0; i < 1024; i++)
  {
      adc_value[i] = (float32_t)AD_Value[i];
  }

  DSP_Analyzer_Process(adc_value,
                          &top1_freq, &top1_amp,
                          &top2_freq, &top2_amp,
                          &top3_freq, &top3_amp);
  //幅度修正
  top1_amp = top1_amp /4.01f;
  top2_amp = top2_amp /4.01f;
  top3_amp = top3_amp /4.01f;

  ScreenApp_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    ScreenDataFrame frame;
    uint32_t now = HAL_GetTick();

    ScreenApp_Task();

    if ((now - s_last_measure_ms) >= SCREEN_REFRESH_INTERVAL_MS)
    {
      s_last_measure_ms = now;
      CaptureAndAnalyze();
      BuildScreenFrame(&frame);
      ScreenApp_SetFrame(&frame);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
