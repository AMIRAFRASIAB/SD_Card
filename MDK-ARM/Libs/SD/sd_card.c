
#include "fn_map.h"
#include "sd_card.h"
#include "spi_rtos_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "fatfs.h"
#include "sd_card_config.h"


#if SD_CARD_LOGGER_ENABLE != NO
  #include "serial_debugger.h"
#else
  #define LOG_TRACE(...)      /* Empty */
  #define LOG_INFO(...)       /* Empty */
  #define LOG_WARNING(...)    /* Empty */
  #define LOG_ERROR(...)      /* Empty */
  #define LOG_FATAL(...)      /* Empty */
#endif  

/*------------------------------------------------------------*/
/* Private Objects */
/*------------------------------------------------------------*/
static uint8_t txBuf[2][512];
static uint16_t index[2];
static bool bufferReady[2] = {false, false};
static uint8_t active = 0;
static TaskHandle_t hTaskSD = NULL;
static SemaphoreHandle_t hMutex = NULL;
static FATFS   fs;
static FIL     file;
static FRESULT res;
static UINT    bw;
static uint32_t total_bytes = 0; 
static uint32_t free_bytes  = 0;  
static uint8_t  __initFlag = 0;
/*------------------------------------------------------------*/
/* Function Dec */
/*------------------------------------------------------------*/



/*------------------------------------------------------------*/
/* Priveate Functions */
/*------------------------------------------------------------*/
bool __sd_UpdateSpaceParams (void) {
  bool result = false;
  FATFS* fs;
  unsigned long free_clusters, total_clusters;
  unsigned long sector_size = 512;
  if (f_getfree("", &free_clusters, &fs) == FR_OK) {
    total_clusters = fs->n_fatent - 2;
    unsigned long sectors_per_cluster = fs->csize;
    total_bytes = (uint64_t)total_clusters * sectors_per_cluster * sector_size;
    free_bytes  = (uint64_t)free_clusters  * sectors_per_cluster * sector_size;
    result = true;
  }
  return result;
}
/*------------------------------------------------------------*/



/*------------------------------------------------------------*/
/* Function Imp */
/*------------------------------------------------------------*/
void serviceSD (void* const pvParameters) {
  res = f_mount(&fs, "", 1);
  if (res == FR_OK) {
    LOG_TRACE("SD :: Card mounted successfully");
    __sd_UpdateSpaceParams();
    LOG_TRACE("SD :: Total size: %d MB, Free space: %d MB", total_bytes / (1024 * 1024), free_bytes / (1024 * 1024));
    res = f_open(&file, "LOG.txt", FA_WRITE | FA_OPEN_ALWAYS );
    if (res == FR_OK) {
      f_lseek(&file, f_size(&file));
    }
  }
  else {
    LOG_ERROR("SD :: Failed to mount the card");
    vTaskSuspend(NULL);
  }
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (uint8_t i = 0; i < 2; i++) {
      if (bufferReady[i]) {
        res = f_write(&file, txBuf[i], index[i], &bw);
        if (res == FR_OK && bw == index[i]) {
          f_sync(&file);
          index[i] = 0;
          bufferReady[i] = false;
        } 
        else {
          LOG_ERROR("SD :: Write failed on buffer %d", i);
        }
      }
    }
  }
}
/*------------------------------------------------------------*/
/* Give Mutex APIs */
static BaseType_t __sd_giveMutexFromThread (SemaphoreHandle_t handle) {
  return xSemaphoreGive(handle);
}
static BaseType_t __sd_giveMutexFromISR (SemaphoreHandle_t handle) {
  return xSemaphoreGiveFromISR(handle, NULL);
}
static BaseType_t (*__sd_giveMutexFn[2]) (SemaphoreHandle_t) = {
  __sd_giveMutexFromThread, // Thread mode
  __sd_giveMutexFromISR     // ISR mode
};
/*------------------------------------------------------------*/
/* Take Mutex APIs */
static BaseType_t __sd_takeMutexFromThread (SemaphoreHandle_t handle) {
  return xSemaphoreTake(handle, 0);
}
static BaseType_t __sd_takeMutexFromISR (SemaphoreHandle_t handle) {
  return xSemaphoreTakeFromISR(handle, NULL);
}
static BaseType_t (*__sd_takeMutexFn[2]) (SemaphoreHandle_t) = {
  __sd_takeMutexFromThread, // Thread mode
  __sd_takeMutexFromISR     // ISR mode
};
/*------------------------------------------------------------*/
/* Give Notify APIs */
static void __sd_notifyTaskFromThread (TaskHandle_t task) {
  xTaskNotifyGive(task);
}
static void __sd_notifyTaskFromISR (TaskHandle_t task) {
  vTaskNotifyGiveFromISR(task, NULL);
}
static void (*__sd_notifyFn[2])(TaskHandle_t) = {
  __sd_notifyTaskFromThread, // [0] Thread mode
  __sd_notifyTaskFromISR     // [1] ISR mode
};
/*------------------------------------------------------------*/
bool sd_write(const char* pSRC, uint16_t len, TickType_t tickToWait) {
  /* Context: 0 -> Thread | 1 -> ISR */
  if (!__initFlag) return false;
  if (len > SD_CARD_TX_BUFFER_LEN) return false;
  uint8_t context = (__get_IPSR() != 0);
  if (__sd_takeMutexFn[context](hMutex) == pdTRUE) {
    uint16_t freeSpace = SD_CARD_TX_BUFFER_LEN - index[active];
    uint16_t remainBytes = 0;
  
    if (freeSpace < len) {
      remainBytes = len - freeSpace;
      len = freeSpace;
    }
    memcpy(txBuf[active] + index[active], pSRC, len);
    index[active] += len;
    if (index[active] >= SD_CARD_TX_BUFFER_LEN) {
      bufferReady[active] = true;
      __sd_notifyFn[context](hTaskSD);
      uint8_t next = active ^ 1;
      if (!bufferReady[next]) {
        active = next;
        if (remainBytes != 0) {
          memcpy(txBuf[active], pSRC + len, remainBytes);
          index[active] = remainBytes;
        }
      } 
      else if (remainBytes != 0) {
        __sd_giveMutexFn[context](hMutex);
        return false;
      }
    }
    __sd_giveMutexFn[context](hMutex);
    return true;
  }
  return false;
}
/*------------------------------------------------------------*/
bool sd_init (void) {
  bool status = true;
  LOG_TRACE("SD :: Initializing...");
  status = status && SRD_Driver_Init();
  status = status && xTaskCreate(&serviceSD, "SD Card", SD_CARD_STACK_SIZE, NULL, SD_CARD_TASK_PRIORITY, &hTaskSD) == pdTRUE;
  status = status && (hMutex = xSemaphoreCreateMutex()) != NULL;
  if (!status) {
    LOG_ERROR("SD :: Initialization failed");
  }
  __initFlag = status;
  return status;
}


