# fn_map.h Usage Example

This is a simple usage example showing how to declare, implement, and call functions using the macros provided in `fn_map.h`:

```c
/* 
  FUNCTION_DEC(void, __p, rint, void);
  FUNCTION_IMP(void, __p, rint, void)
    printf("Hello AMIR\n");
  FUNCTION_END

  #define print(...)    FUNCTION_CAL(__p, rint, __VA_ARGS__)

  int main (void) {
    print();
    return 0;
  }
*/
```






