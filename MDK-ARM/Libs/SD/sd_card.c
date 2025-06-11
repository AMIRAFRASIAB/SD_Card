
#include "fn_map.h"
#include "sd_card.h"
#include "spi_rtos_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "fatfs.h"
#include "sd_card_config.h"
#include "limits.h"
#include "string.h"
#include "ctype.h"


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
static char fileName[16] = "Log.txt";
static uint16_t index[2];
static bool bufferReady[2] = {false, false};
static uint8_t active = 0;
static TaskHandle_t hTaskSD = NULL;
static SemaphoreHandle_t hMutex = NULL;
static TimerHandle_t hTim = NULL;
static FATFS   fs;
static FIL     file;
static FRESULT res;
static UINT    bw;
static uint64_t total_bytes = 0; 
static uint64_t free_bytes  = 0;  
static uint8_t  __initFlag = 0;
static bool synchReady = false;
/*------------------------------------------------------------*/
typedef enum {
  SD_TaskCMD_Write      = 1UL << 0,
  SD_TaskCMD_Synch      = 1UL << 1,
  SD_TaskCMD_FileSwitch = 1UL << 2,
} SD_TaskCMD_e;
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
void __sd_synchTimCallback (TimerHandle_t xTimer) {
  xTaskNotify(hTaskSD, SD_TaskCMD_Synch, eSetBits);
}
/*------------------------------------------------------------*/
static bool __sd_deleteOldestFile (void) {
  DIR dir;
  FILINFO fno;
  FRESULT res;
  char oldestFile[16] = {0};
  res = f_opendir(&dir, "/");
  bool result = false;
  if (res != FR_OK) {
    LOG_ERROR("SD :: Failed to open root directory (%d)", res);
    return false;
  }
  while (1) {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK || fno.fname[0] == 0) {
      break;
    }
    if (!(fno.fattrib & AM_DIR) && strlen(fno.fname) < (sizeof(oldestFile) - 1) && isdigit(fno.fname[0]) && isdigit(fno.fname[7])) {
      if (oldestFile[0] == 0 || strcmp(fno.fname, oldestFile) < 0) {
        strcpy(oldestFile, fno.fname);
      }
    }
  }
  f_closedir(&dir);
  if (oldestFile[0]) {
    res = f_unlink(oldestFile);
    if (res == FR_OK) {
      result = true;
      LOG_WARNING("SD :: Deleted oldest log file: %s", oldestFile);
    } 
    else {
      LOG_ERROR("SD :: Failed to delete %s (%d)", oldestFile, res);
    }
  } 
  else {
    LOG_INFO("SD :: No log files found to delete");
  }
  return result;
}
/*------------------------------------------------------------*/
static bool __sd_remount (uint32_t tryCount, uint32_t delayMs) {
  FRESULT res;
  for (uint32_t i = 0; i < tryCount; i++) {
    f_mount(NULL, "", 0);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    extern Disk_drvTypeDef disk;
    disk.is_initialized[0] = 0;
    disk_initialize(0);             // Re-init hardware
    res = f_mount(&fs, "", 1);
    if (res == FR_OK) {
      LOG_WARNING("SD :: Re-mount successful on attempt %d", i + 1);
      __sd_UpdateSpaceParams();
      uint32_t freeMB = (free_bytes) / (1024 * 1024);
      if (freeMB <= SD_CARD_MIN_SPACE_THRESHOLD_IN_MB) {
        if (!__sd_deleteOldestFile()) {
          return false;
        }
      }
      res = f_open(&file, fileName, FA_WRITE | FA_OPEN_ALWAYS);
      if (res == FR_OK) {
        f_lseek(&file, f_size(&file));
        return true;
      } 
      else {
        LOG_ERROR("SD :: Re-mount succeeded but file open failed");
      }
    }
  }
  return false;
}

/*------------------------------------------------------------*/
/* Function Imp */
/*------------------------------------------------------------*/
void serviceSD (void* const pvParameters) {
  __sd_remount(ULONG_MAX, 1000);
  uint32_t notifVal = 0;
  bool mountNeed = false;
  while (1) {
    xTaskNotifyWait(0, ULONG_MAX, &notifVal, portMAX_DELAY);
    /* Synchronization */
    if ((notifVal & SD_TaskCMD_Synch) && synchReady) {
      synchReady = false;
      f_sync(&file);
    }
    /* Writting operation */
    if (notifVal & SD_TaskCMD_Write) {
      for (uint8_t i = 0; i < 2; i++) {
        if (bufferReady[i]) {
          res = f_write(&file, txBuf[i], index[i], &bw);
          if (res == FR_OK && bw == index[i]) {
            index[i] = 0;
            bufferReady[i] = false;
            synchReady = true;
          } 
          else {
            mountNeed = true;
            LOG_ERROR("SD :: Write failed on buffer %d", i);
          }
        }
      }
    }
    /* Re-mounting ... */
    if (mountNeed == true) {
      mountNeed = false;
      __sd_remount(ULONG_MAX, 1000);
    }
    /* File Switching */
    if (notifVal & SD_TaskCMD_FileSwitch) {
      f_sync(&file);  
      f_close(&file); 
      res = f_open(&file, fileName, FA_WRITE | FA_OPEN_ALWAYS);
      if (res == FR_OK) {
        f_lseek(&file, f_size(&file));
        LOG_TRACE("SD :: Switched to file: %s",fileName);
      } 
      else {
        mountNeed = true;
        LOG_ERROR("SD :: Failed to open new file: %s", fileName);
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
static BaseType_t (* const __sd_giveMutexFn[2]) (SemaphoreHandle_t) = {
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
static BaseType_t (* const __sd_takeMutexFn[2]) (SemaphoreHandle_t) = {
  __sd_takeMutexFromThread, // Thread mode
  __sd_takeMutexFromISR     // ISR mode
};
/*------------------------------------------------------------*/
/* Give Notify APIs */
static void __sd_notifyTaskFromThread (TaskHandle_t task, uint32_t notify) {
  xTaskNotify(task, notify, eSetBits);
}
static void __sd_notifyTaskFromISR (TaskHandle_t task, uint32_t notify) {
  xTaskNotifyFromISR(task, notify, eSetBits, NULL);
}
static void (* const __sd_notifyFn[2])(TaskHandle_t, uint32_t) = {
  __sd_notifyTaskFromThread, // [0] Thread mode
  __sd_notifyTaskFromISR     // [1] ISR mode
};
/*------------------------------------------------------------*/
bool sd_write (const char* pSRC, uint16_t len, TickType_t tickToWait) {
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
      __sd_notifyFn[context](hTaskSD, SD_TaskCMD_Write);
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
  status = status && (hTim = xTimerCreate("SD_Sync_Tim", SD_CARD_SYNCH_TIME_MS, pdTRUE, NULL, &__sd_synchTimCallback)) != NULL;
  status = status && (xTimerStart(hTim, portMAX_DELAY) == pdTRUE);
  if (!status) {
    LOG_ERROR("SD :: Initialization failed");
  }
  __initFlag = status;
  return status;
}
/*------------------------------------------------------------*/
void sd_synchForce (void) {
  synchReady = true;
  __sd_notifyFn[__get_IPSR() != 0](hTaskSD, SD_TaskCMD_Synch);
}
/*------------------------------------------------------------*/
void sd_swtichToNewFile (const char* pSTR) {
  memset(fileName, 0x00, sizeof(fileName));
  memcpy(fileName, pSTR, sizeof(fileName) - 1);
  __sd_notifyFn[__get_IPSR() != 0](hTaskSD, SD_TaskCMD_FileSwitch);
}
/*------------------------------------------------------------*/
