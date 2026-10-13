#pragma once
#include <stdint.h>
#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline
struct MockGpio { uint32_t IN,OUTSET,OUTCLR,DIRSET,DIRCLR,PIN_CNF[32]; };
struct MockDwt { uint32_t CYCCNT,CTRL; };
struct MockCoreDebug { uint32_t DEMCR; };
extern MockGpio* NRF_P0;
extern MockDwt* DWT;
extern MockCoreDebug* CoreDebug;
#define CoreDebug_DEMCR_TRCENA_Msk (1u<<24)
#define DWT_CTRL_CYCCNTENA_Msk 1u
