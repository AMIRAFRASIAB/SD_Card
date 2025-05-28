
#ifndef __SPI_RTOS_DRIVER_H__
#define __SPI_RTOS_DRIVER_H__

#ifdef __cplusplus
  extern "C"{
#endif //__cplusplus

#include "stdint.h"
#include "stdbool.h"


#define DMA2_STREAM5_FOR_SPI_SD_TX_IRQ_PRIORITY   6
#define DMA2_STREAM0_FOR_SPI_SD_RX_IRQ_PRIORITY   6  
#define SRD_SPI_CS_GPIO                           GPIOB
#define SRD_SPI_CS_PIN                            LL_GPIO_PIN_0
    
    
    
bool SRD_Driver_Init (void);
bool SRD_SPI_TransmitDMA (const void* pSrc, uint16_t txNumber, uint32_t timeout_ms); 
bool SRD_SPI_ReceiveDMA (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms);  
bool SRD_SPI_TransmitReceiveDMA (void* pSrc, void* pDst, uint16_t len, uint32_t timeout_ms);
bool SRD_SPI_TransmitReceivePolling (const uint8_t* pSRC, uint8_t* pDst, uint16_t len, uint32_t timeout_ms);
void SRD_ClockChangeToSlow (void);
void SRD_ClockChangeToFast (void);

#ifdef __cplusplus
  };
#endif //__cplusplus
#endif //__SPI_RTOS_DRIVER_H__