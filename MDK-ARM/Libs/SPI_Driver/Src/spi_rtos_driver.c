
#include "spi_rtos_driver.h"

#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_spi.h"


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
}
//-----------------------------------------------------------------------
static void __SRD_SPI_Init (void) {
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
  LL_SPI_Init(SPI1, &spi1);
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
bool SRD_SPI_TransmitDMA (void* pSrc, uint16_t txNumber, uint32_t timeout_ms) {
  LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
  LL_DMA_ClearFlag_TC5(DMA2);
  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_5, (uint32_t)pSrc);
  LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, txNumber);
  LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);
  while (LL_DMA_GetDataLength(DMA2, LL_DMA_STREAM_5) != 0) {
    __NOP();
  }
  return true;
}
//-----------------------------------------------------------------------
bool SRD_SPI_ReceiveDMA (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms) {
  
}
//-----------------------------------------------------------------------
bool SRD_SPI_TransmitReceiveDMA (void* pSrc, void* pDst, uint16_t len, uint32_t timeout_ms) {
  
}
//-----------------------------------------------------------------------
