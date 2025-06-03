
#ifndef __AMIR_FN_MAP_INCLUDED__
#define __AMIR_FN_MAP_INCLUDED__

#ifdef __cplusplus
  extern "C"{
#endif //__cplusplus
  
#define ___CONCAT(x, y)             x ## y
#define _CONCAT(x, y)               ___CONCAT(x, y)
#define FUNCTION_IMP(x, y, w, ...)  x _CONCAT(y, w) (__VA_ARGS__) {
#define FUNCTION_DEC(x, y, w, ...)  x _CONCAT(y, w) (__VA_ARGS__)
#define FUNCTION_CAL(y, w, ...)     _CONCAT(y, w)(__VA_ARGS__)
#define FUNCTION_END                }




#ifdef __cplusplus
  }
#endif //__cplusplus
#endif //__AMIR_FN_MAP_INCLUDED__