
#include "spi_rtos_driver.h"
#include "sd_card_config.h"
#include "ff_gen_drv.h"
#include "user_diskio.h"

#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_gpio.h"

#include "FreeRTOS.h"
#include "semphr.h"

#if SD_CARD_LOGGER_ENABLE != NO
  #include "serial_debugger.h"
#else
  #define LOG_TRACE(...)      /* Empty */
  #define LOG_INFO(...)       /* Empty */
  #define LOG_WARNING(...)    /* Empty */
  #define LOG_ERROR(...)      /* Empty */
  #define LOG_FATAL(...)      /* Empty */
#endif  

//-----------------------------------------------------------------------
// Private Objects 
//-----------------------------------------------------------------------
static SemaphoreHandle_t hSemaphore = NULL;
static char USERPath[4];
//-----------------------------------------------------------------------
// Private APIs 
//-----------------------------------------------------------------------
static void __SRD_GPIO_Init (void) {
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
  __DSB();
  /**
    * PA5  ------> SPI1_SCK
    * PA6  ------> SPI1_MISO
    * PA7  ------> SPI1_MOSI
    * PB0  ------> SPI1_CS 
   */
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_5, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_6, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_0, LL_GPIO_SPEED_FREQ_MEDIUM);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_5, LL_GPIO_AF_5);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_6, LL_GPIO_AF_5);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_5);
  LL_GPIO_LockPin(GPIOA, LL_GPIO_PIN_5);
  LL_GPIO_LockPin(GPIOA, LL_GPIO_PIN_6);
  LL_GPIO_LockPin(GPIOA, LL_GPIO_PIN_7);
  LL_GPIO_LockPin(GPIOB, LL_GPIO_PIN_0);
}
//-----------------------------------------------------------------------
static bool __SRD_DMA_Init (void) {
  uint32_t startTick = xTaskGetTickCount();
  uint32_t timeout = 100;
  uint32_t elapsed;
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
  __DSB();
  /* Tx => DMA_2 Stream_5 Channel_3 */
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
  while (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_5)) {
    elapsed = xTaskGetTickCount() - timeout;
    if (elapsed >= timeout) {
      return false;
    }
    taskYIELD();
  }
  timeout -= elapsed;
  LL_DMA_ClearFlag_DME5(DMA2);
  LL_DMA_ClearFlag_FE5(DMA2);
  LL_DMA_ClearFlag_HT5(DMA2);
  LL_DMA_ClearFlag_TC5(DMA2);
  LL_DMA_ClearFlag_TE5(DMA2);
  DMA2_Stream5->CR = 0UL;
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_5, LL_DMA_CHANNEL_3);
  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_5, LL_DMA_PRIORITY_MEDIUM);
  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_5, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_5, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_5, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_5, LL_SPI_DMA_GetRegAddr(SPI1));
  LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
  NVIC_SetPriority(DMA2_Stream5_IRQn, DMA2_STREAM5_FOR_SPI_SD_TX_IRQ_PRIORITY);
  NVIC_EnableIRQ(DMA2_Stream5_IRQn);
  /* Rx => DMA_2 Stream_0 Channel_3 */
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
  while (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_0)) {
    elapsed = xTaskGetTickCount() - timeout;
    if (elapsed >= timeout) {
      return false;
    }
    taskYIELD();
  }
  LL_DMA_ClearFlag_DME0(DMA2);
  LL_DMA_ClearFlag_FE0(DMA2);
  LL_DMA_ClearFlag_HT0(DMA2);
  LL_DMA_ClearFlag_TC0(DMA2);
  LL_DMA_ClearFlag_TE0(DMA2);
  DMA2_Stream0->CR = 0UL;
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_0, LL_DMA_CHANNEL_3);
  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_0, LL_DMA_PRIORITY_MEDIUM);
  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_0, LL_SPI_DMA_GetRegAddr(SPI1));
  LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_0);
  NVIC_SetPriority(DMA2_Stream0_IRQn, DMA2_STREAM0_FOR_SPI_SD_RX_IRQ_PRIORITY);
  NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  return true;
}
//-----------------------------------------------------------------------
void DMA2_Stream5_IRQHandler (void) {
  LL_DMA_ClearFlag_TC5(DMA2);
  xSemaphoreGiveFromISR(hSemaphore, NULL);
}
//-----------------------------------------------------------------------
void DMA2_Stream0_IRQHandler (void) {
  LL_DMA_ClearFlag_TC0(DMA2);
  xSemaphoreGiveFromISR(hSemaphore, NULL);
}
//-----------------------------------------------------------------------
static bool __SRD_SPI_Init (void) {
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
  __DSB();
  LL_SPI_Disable(SPI1);
  LL_SPI_InitTypeDef spi1 = {
    .BitOrder = LL_SPI_MSB_FIRST,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16, /* SPI Clock = 4.5 MHz */
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 10,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .Mode = LL_SPI_MODE_MASTER,
    .NSS = LL_SPI_NSS_SOFT,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
  };
  if (LL_SPI_Init(SPI1, &spi1) != SUCCESS) {
    return false;
  }
  hSemaphore = xSemaphoreCreateBinary();
  if (hSemaphore == NULL) {
    return false;
  }
  LL_SPI_EnableDMAReq_RX(SPI1);
  LL_SPI_EnableDMAReq_TX(SPI1);
  LL_SPI_Enable(SPI1);
  return true;
}
//-----------------------------------------------------------------------
// Public APIs 
//-----------------------------------------------------------------------
/**
 * @brief  Initialize the SD SPI driver.
 *
 * This function initializes the required DMA and SPI peripherals used by the SD card SPI driver.
 * It must be called before any other driver functions.
 *
 * @retval true  Initialization succeeded.
 * @retval false Initialization failed due to DMA or SPI setup error.
 */
bool SRD_Driver_Init (void) {
  LOG_TRACE("SD SPI Driver :: Initializing...");
  FATFS_LinkDriver(&USER_Driver, USERPath);
  __SRD_GPIO_Init ();
  if (!__SRD_DMA_Init()) {
    LOG_ERROR("SD SPI Driver :: DMA initialization failed");
    return false;
  }
  if (!__SRD_SPI_Init()) {
    LOG_ERROR("SD SPI Driver :: SPI initialization failed");
    return false;
  }
  LOG_TRACE("SD SPI Driver :: SPI driver initialized successfully");
  return true;
}
//-----------------------------------------------------------------------
/**
 * @brief Transmit data over SPI using DMA with a timeout.
 * 
 * Sends a specified number of bytes over SPI using DMA, blocking until the
 * transmission completes or the timeout expires.
 * 
 * @param[in] pSrc       Pointer to the buffer containing data to transmit.
 * @param[in] txNumber   Number of bytes to transmit.
 * @param[in] timeout_ms Timeout in milliseconds to wait for the operation to complete.
 * 
 * @return true if the transmission completed successfully within the timeout,
 *         false if a timeout occurred or SPI was busy for too long.
 */
bool SRD_SPI_TransmitDMA (const void* pSrc, uint16_t txNumber, uint32_t timeout_ms) {
  uint32_t startTick = xTaskGetTickCount();
  uint32_t elapsedTime;
  while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    elapsedTime = xTaskGetTickCount() - startTick;
    if (elapsedTime >= timeout_ms) {
      return false;
    }
    taskYIELD();
  }
  timeout_ms -= elapsedTime;
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
  LL_DMA_ClearFlag_TC5(DMA2);
  LL_DMA_ClearFlag_FE5(DMA2);
  LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
  LL_DMA_DisableIT_TC(DMA2, LL_DMA_STREAM_0);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_5, (uint32_t)pSrc);
  LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, txNumber);
  LL_SPI_Enable(SPI1);
  LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);
  xSemaphoreTake(hSemaphore, 0);
  if (xSemaphoreTake(hSemaphore, timeout_ms) != pdTRUE) {
    return false;
  }
  return true;
}
//-----------------------------------------------------------------------
/**
 * @brief Receive data over SPI using DMA with a timeout.
 * 
 * This function receives a specified number of bytes via SPI using DMA,
 * blocking the calling task until the transfer completes or the timeout expires.
 * 
 * @param[out] pDst       Pointer to the buffer where received data will be stored.
 * @param[in]  rcvNumber  Number of bytes to receive.
 * @param[in]  timeout_ms Timeout in milliseconds to wait for the operation to complete.
 * 
 * @return true if the data was successfully received within the timeout period,
 *         false if the operation timed out or SPI was busy for too long.
 */
bool SRD_SPI_ReceiveDMA (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms) {
  uint32_t startTick = xTaskGetTickCount();
  uint32_t elapsedTime;
  while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    elapsedTime = xTaskGetTickCount() - startTick;
    if (elapsedTime >= timeout_ms) { // this line prevent timeout bad subtraction
      return false;
    }
    taskYIELD();
  }
  timeout_ms -= elapsedTime;
  static const uint8_t DUMMY = 0xFF;
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
  LL_DMA_ClearFlag_TC0(DMA2);
  LL_DMA_ClearFlag_TC5(DMA2);
  LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_0);
  LL_DMA_DisableIT_TC(DMA2, LL_DMA_STREAM_5);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_NOINCREMENT);
  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_5, (uint32_t)(&DUMMY));
  LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, rcvNumber);
  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_0, (uint32_t)pDst);
  LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_0, rcvNumber);
  LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_0);
  LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);
  xSemaphoreTake(hSemaphore, 0);
  if (xSemaphoreTake(hSemaphore, timeout_ms) != pdTRUE) {
    return false;
  }
  return true;
}
//-----------------------------------------------------------------------
/**
 * @brief Exchanges data over SPI using polling with a timeout.
 *
 * Sends and receives a fixed number of bytes over SPI in full-duplex mode.
 * The function blocks until all data is transferred or the timeout expires.
 *
 * @param pSRC        Pointer to the buffer containing data to send.
 * @param pDst        Pointer to the buffer where received data will be stored.
 * @param len         Number of bytes to exchange.
 * @param timeout_ms  Maximum time in milliseconds to complete the transfer.
 *
 * @return true if the transfer completed successfully, false if a timeout occurred.
 */
bool SRD_SPI_TransmitReceivePolling (const uint8_t* pSRC, uint8_t* pDst, uint16_t len, uint32_t timeout_ms) {
  bool txAllowed = true;
  uint32_t startTick = xTaskGetTickCount();
  while (len > 0) {
    if ((txAllowed) && (LL_SPI_IsActiveFlag_TXE(SPI1))) {
      LL_SPI_TransmitData8(SPI1, *pSRC++);
      txAllowed = false;
    }
    if (LL_SPI_IsActiveFlag_RXNE(SPI1)) {
      *pDst++ = LL_SPI_ReceiveData8(SPI1);
      txAllowed = true;
      len--;
    }
    if (xTaskGetTickCount() - startTick >= timeout_ms) {
      return false;
    }
    taskYIELD();
  }
   while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    if (xTaskGetTickCount() - startTick >= timeout_ms) {
      return false;
    }
    taskYIELD();
  }
  if (LL_SPI_IsActiveFlag_RXNE(SPI1)) {
    (void)LL_SPI_ReceiveData8(SPI1);
  }
  (void)LL_SPI_ReadReg(SPI1, SR);
  return true;
}
//-----------------------------------------------------------------------
void SRD_ClockChangeToSlow (void) {
  bool spiEnableStatus = false;
  if (LL_SPI_IsEnabled(SPI1)) {
    spiEnableStatus = true;
    LL_SPI_Disable(SPI1);
    __DSB();
  }
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV256);
  if (spiEnableStatus) {
    LL_SPI_Enable(SPI1);
    __DSB();
  }
}
//-----------------------------------------------------------------------
void SRD_ClockChangeToFast (void) {
  bool spiEnableStatus = false;
  if (LL_SPI_IsEnabled(SPI1)) {
    spiEnableStatus = true;
    LL_SPI_Disable(SPI1);
    __DSB();
  }
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV8);
  if (spiEnableStatus) {
    LL_SPI_Enable(SPI1);
    __DSB();
  }
}
//-----------------------------------------------------------------------
