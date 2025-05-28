
#include "spi_rtos_driver.h"

#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_spi.h"

#include "FreeRTOS.h"
#include "semphr.h"



#define DMA2_STREAM5_FOR_SPI_SD_TX_IRQ_PRIORITY   6
#define DMA2_STREAM0_FOR_SPI_SD_RX_IRQ_PRIORITY   6
//-----------------------------------------------------------------------
// Private Objects 
//-----------------------------------------------------------------------
static SemaphoreHandle_t hSemaphore = NULL;

//-----------------------------------------------------------------------
// Private APIs 
//-----------------------------------------------------------------------
static void __SRD_DMA_Init (void) {
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
  __DSB();
  /* Tx => DMA_2 Stream_5 Channel_3 */
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
  LL_DMA_ClearFlag_DME5(DMA2);
  LL_DMA_ClearFlag_FE5(DMA2);
  LL_DMA_ClearFlag_HT5(DMA2);
  LL_DMA_ClearFlag_TC5(DMA2);
  LL_DMA_ClearFlag_TE5(DMA2);
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
  LL_DMA_ClearFlag_DME0(DMA2);
  LL_DMA_ClearFlag_FE0(DMA2);
  LL_DMA_ClearFlag_HT0(DMA2);
  LL_DMA_ClearFlag_TC0(DMA2);
  LL_DMA_ClearFlag_TE0(DMA2);
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
static void __SRD_SPI_Init (void) {
//  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
//  __DSB();
//  LL_SPI_Disable(SPI1);
//  LL_SPI_InitTypeDef spi1 = {
//    .BitOrder = LL_SPI_MSB_FIRST,
//    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16, /* SPI Clock = 4.5 MHz */
//    .ClockPhase = LL_SPI_PHASE_1EDGE,
//    .ClockPolarity = LL_SPI_POLARITY_LOW,
//    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
//    .CRCPoly = 10,
//    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
//    .Mode = LL_SPI_MODE_MASTER,
//    .NSS = LL_SPI_NSS_SOFT,
//    .TransferDirection = LL_SPI_FULL_DUPLEX,
//  };
//  LL_SPI_Init(SPI1, &spi1);
  hSemaphore = xSemaphoreCreateBinary();
  LL_SPI_Disable(SPI1);
  LL_SPI_EnableDMAReq_RX(SPI1);
  LL_SPI_EnableDMAReq_TX(SPI1);
  LL_SPI_Enable(SPI1);
}
//-----------------------------------------------------------------------
// Public APIs 
//-----------------------------------------------------------------------
void SRD_Driver_Init (void) {
  static bool __initFlag = false;
  if (!__initFlag) {
    __initFlag = true;
    __SRD_DMA_Init();
    __SRD_SPI_Init();
  }
}
//-----------------------------------------------------------------------
bool SRD_SPI_TransmitPolling (const void* pSrc, uint16_t txNumber, uint32_t timeout_ms) {
  LL_DMA_DisableIT_TC(DMA2, LL_DMA_STREAM_0);
  LL_DMA_DisableIT_TC(DMA2, LL_DMA_STREAM_5);
  volatile uint32_t initial_tick = xTaskGetTickCount();
  uint8_t dummy = 0;
  const uint8_t* buff = pSrc;
  LL_SPI_Enable(SPI1);
  while (txNumber > 0) {
    if (LL_SPI_IsActiveFlag_TXE(SPI1)) {
      LL_SPI_TransmitData8(SPI1, *buff++);
      txNumber--;
    }
    else if ((initial_tick + timeout_ms) < xTaskGetTickCount()) {
      return false;
    }
  }
  while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    if ((initial_tick + timeout_ms) < xTaskGetTickCount()) {
      return false;
    }
  }
  //clear over run status bit
  dummy = LL_SPI_ReceiveData8(SPI1);
  dummy = LL_SPI_ReadReg(SPI1, SR);
  return true;
}
//-----------------------------------------------------------------------
bool SRD_SPI_TransmitDMA (const void* pSrc, uint16_t txNumber, uint32_t timeout_ms) {
  uint32_t __now = xTaskGetTickCount();
  uint32_t __elapsed;
  while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    __elapsed = xTaskGetTickCount() - __now;
    if (__elapsed >= timeout_ms) {
      return false;
    }
    taskYIELD();
  }
  timeout_ms -= __elapsed;
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
bool SRD_SPI_ReceiveDMA (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms) {
  uint32_t __now = xTaskGetTickCount();
  uint32_t __elapsed;
  while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    __elapsed = xTaskGetTickCount() - __now;
    if (__elapsed >= timeout_ms) {
      return false;
    }
    taskYIELD();
  }
  timeout_ms -= __elapsed;
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
bool SRD_SPI_ReceivePolling (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms) {
  bool txAllowed = true;
  volatile uint32_t initial_tick = HAL_GetTick();
  uint8_t dummy = 0xFF;
  uint8_t* dst = pDst;
  while (rcvNumber > 0) {
    if ((txAllowed) && (LL_SPI_IsActiveFlag_TXE(SPI1))) {
      LL_SPI_TransmitData8(SPI1, 0xFF);
      txAllowed = false;
    }
    if (LL_SPI_IsActiveFlag_RXNE(SPI1)) {
      *dst++ = LL_SPI_ReceiveData8(SPI1);
      txAllowed = true;
      rcvNumber--;
    }
    else if ((initial_tick + timeout_ms) < HAL_GetTick()) {
      return false;
    }
  }
   while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    if ((initial_tick + timeout_ms) < HAL_GetTick()) {
      return false;
    }
  }
  //clear over run status bit
  dummy = LL_SPI_ReceiveData8(SPI1);
  dummy = LL_SPI_ReadReg(SPI1, SR);
  return true;
}
//-----------------------------------------------------------------------
bool SRD_SPI_TransmitReceivePolling (void* pSrc, void* pDst, uint16_t len, uint32_t timeout_ms) {
  bool txAllowed = true;
  volatile uint32_t initial_tick = HAL_GetTick();
  while (len > 0) {
    if ((txAllowed) && (LL_SPI_IsActiveFlag_TXE(SPI1))) {
      LL_SPI_TransmitData8(SPI1, *(uint8_t*)pSrc++);
      txAllowed = false;
    }
    if (LL_SPI_IsActiveFlag_RXNE(SPI1)) {
      *(uint8_t*)pDst++ = LL_SPI_ReceiveData8(SPI1);
      txAllowed = true;
      len--;
    }
    else if ((initial_tick + timeout_ms) < HAL_GetTick()) {
      return false;
    }
  }
   while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
    if ((initial_tick + timeout_ms) < HAL_GetTick()) {
      return false;
    }
  }
  //clear over run status bit
  uint8_t dummy;
  dummy = LL_SPI_ReceiveData8(SPI1);
  dummy = LL_SPI_ReadReg(SPI1, SR);
  return true;
}
//-----------------------------------------------------------------------

