
#ifndef __SPI_RTOS_DRIVER_H__
#define __SPI_RTOS_DRIVER_H__

#ifdef __cplusplus
  extern "C"{
#endif //__cplusplus

#include "stdint.h"
#include "stdbool.h"

void SRD_Driver_Init (void);
bool SRD_SPI_TransmitDMA (const void* pSrc, uint16_t txNumber, uint32_t timeout_ms); 
bool SRD_SPI_ReceiveDMA (void* pDst, uint16_t rcvNumber, uint32_t timeout_ms);  
bool SRD_SPI_TransmitReceiveDMA (void* pSrc, void* pDst, uint16_t len, uint32_t timeout_ms);
bool SRD_SPI_TransmitReceivePolling (void* pSrc, void* pDst, uint16_t len, uint32_t timeout_ms);


#ifdef __cplusplus
  };
#endif //__cplusplus
#endif //__SPI_RTOS_DRIVER_H__