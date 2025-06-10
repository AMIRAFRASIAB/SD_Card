
#ifndef __SD_CARD_H_INCLUDED__
#define __SD_CARD_H_INCLUDED__

#ifdef __cplusplus
  extern "C" {
#endif //__cplusplus  

#include <stdint.h>
#include <stdbool.h>
#include "fn_map.h"
#include "FreeRTOS.h"


bool sd_init (void);
bool sd_write (const char* pSRC, uint16_t len, TickType_t tickToWait);
void sd_swtichToNewFile (const char* pSTR);
void sd_synchForce (void);
    
#ifdef __cplusplus
  }
#endif //__cplusplus  
#endif //__SD_CARD_H_INCLUDED__
