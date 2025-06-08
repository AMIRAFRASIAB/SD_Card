
#include "main.h"
#include "fatfs.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "spi_rtos_driver.h"
#include "stdio.h"
#include "stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "sd_card.h"

#define __DEBUGGER_H_INCLUDED__



FATFS fs;
FIL file;
FRESULT res;
UINT bw;

char txBuf[120];

void uart_log(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

void SystemClock_Config(void);

void serviceDummy (void* const pvParameters) {
  
  while (1) {
    uint32_t cnt = 5000000;
    while (cnt-- > 0);
    vTaskDelay(1);
  }
}

char str[100];
void service_SD (void* const pvParameters) {
  uint32_t cnt = 0;
  sd_init();
  while (1) {
    snprintf((char*)str, sizeof(str), "This is a test %d\n", cnt++);
    if (sd_write(str, strlen(str), 100)) {
      uart_log("Write ok\n");
    }
    else {
      uart_log("Write failed\n");
    }
    vTaskDelay(100);
  }
}

int main (void) {

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
//  MX_SPI1_Init();
  MX_USART2_UART_Init();
  SRD_Driver_Init();
  MX_FATFS_Init();
  
  xTaskCreate(&service_SD, "SD", 512, NULL, 2, NULL);
  xTaskCreate(&serviceDummy, "D1", 64, NULL, 0, NULL);
  xTaskCreate(&serviceDummy, "D2", 64, NULL, 0, NULL);
  vTaskStartScheduler();                     
  
  

  uint32_t cnt = 0;
  while (1) {
    
      while (1) {
        __NOP();
      }
    }
  
  
}


void SystemClock_Config (void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}


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
