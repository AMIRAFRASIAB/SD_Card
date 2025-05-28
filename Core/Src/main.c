
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

void PrintCardSize(void) {
    FATFS *fs;
    DWORD free_clusters, total_clusters;
    uint32_t sector_size = 512; // SD cards use 512-byte sectors
    char buf[100];

    if (f_getfree("", &free_clusters, &fs) == FR_OK) {
        total_clusters = fs->n_fatent - 2;  // Total clusters (n_fatent includes 2 reserved)
        DWORD sectors_per_cluster = fs->csize;

        uint64_t total_bytes = (uint64_t)total_clusters * sectors_per_cluster * sector_size;
        uint64_t free_bytes  = (uint64_t)free_clusters  * sectors_per_cluster * sector_size;

        snprintf(buf, sizeof(buf),
                 "SD Card Size: %lu MB, Free: %lu MB\r\n",
                 (uint32_t)(total_bytes / (1024 * 1024)),
                 (uint32_t)(free_bytes / (1024 * 1024)));

        HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
    } else {
        HAL_UART_Transmit(&huart2, (uint8_t*)"Failed to get SD info\r\n", 24, HAL_MAX_DELAY);
    }
}


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
    vTaskDelay(50);
    
  }
}


void service_SD (void* const pvParameters) {
  uart_log("Hello\n");
  // Mount SD Card
  res = f_mount(&fs, "", 1);
//  PrintCardSize();
  if (res == FR_OK) {
    uart_log("SD card mounted successfully.\r\n");
    // Open or create log.txt for writing
    res = f_open(&file, "log3.txt", FA_WRITE | FA_OPEN_ALWAYS);
    if (res == FR_OK) {
      // Move file pointer to the end for appending
      f_lseek(&file, f_size(&file));
      char log_entry[] = "Write to sd card\n";
      res = f_write(&file, log_entry, strlen(log_entry), &bw);
      if (res == FR_OK && bw == strlen(log_entry)) {
        uart_log("Data written to log.txt\r\n");
      } 
      else {
        uart_log("Failed to write data\r\n");
      }
//        f_close(&file);
    } 
    else {
      uart_log("Failed to open log.txt\r\n");
    }
//      f_mount(NULL, "", 1); // Unmount SD
  } 
  else {
    uart_log("Failed to mount SD card\r\n");
  }
  uint32_t cnt = 0;
  while (1) {
    snprintf(txBuf, sizeof(txBuf), "this is a test message for sd card %d\n", cnt++);
    vTaskDelay(10);
    res = f_write(&file, txBuf, strlen(txBuf), &bw);
    if (res == FR_OK && bw == strlen(txBuf)) {
      f_sync(&file);
      uart_log("Data written to log.txt\r\n");
    }
    else {
      uart_log("Failed to write data\r\n");
    }
    if (cnt == 1000) {
      f_mount(NULL, "", 1);
      uart_log("Eject SD Card\r\n");
      vTaskSuspend(NULL);
    }
  }
}

int main (void) {

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  SRD_Driver_Init();
  MX_FATFS_Init();
  
  xTaskCreate(&service_SD, "SD", 512, NULL, 1, NULL);
//  xTaskCreate(&serviceDummy, "D1", 64, NULL, 2, NULL);
//  xTaskCreate(&serviceDummy, "D2", 64, NULL, 2, NULL);
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
