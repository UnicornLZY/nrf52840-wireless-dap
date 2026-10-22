#pragma once
#include "nrf.h"
#include <stdint.h>
#include <string.h>
#define CPU_CLOCK 64000000U
#define IO_PORT_WRITE_CYCLES 2U
#define DAP_SWD 1
#define DAP_JTAG 0
#define DAP_JTAG_DEV_CNT 0
#define DAP_DEFAULT_PORT 1U
#define DAP_DEFAULT_SWJ_CLOCK 4000000U
#ifndef WD_DAP240
#define WD_DAP240 0
#endif
#ifndef WD_DAP128
#define WD_DAP128 0
#endif
#define DAP_PACKET_SIZE (WD_DAP240 ? 240U : (WD_DAP128 ? 128U : 64U))
#define DAP_PACKET_COUNT 4U
#define DAP_FW_V1
#define SWO_UART 0
#define SWO_MANCHESTER 0
#define SWO_STREAM 0
#define TIMESTAMP_CLOCK 64000000U
#define DAP_UART 0
#define DAP_UART_USB_COM_PORT 0
#define TARGET_FIXED 0
#ifndef WD_ONE_ROW_WIRING_TEST
#define WD_ONE_ROW_WIRING_TEST 0
#endif
#ifndef WD_ONE_ROW_OUTER_RESET_TEST
#define WD_ONE_ROW_OUTER_RESET_TEST 0
#endif
#ifndef WD_ONE_ROW_RESET_P113_TEST
#define WD_ONE_ROW_RESET_P113_TEST 0
#endif
#if WD_ONE_ROW_RESET_P113_TEST
#define WD_CLK 31U
#define WD_DIO 29U
#define WD_RST 13U
#define WD_RST_PORT NRF_P1
#elif WD_ONE_ROW_OUTER_RESET_TEST
// Keep the frequently used pins contiguous and move optional target reset
// to the outermost exposed GPIO on the VCC-side row.
#define WD_CLK 31U
#define WD_DIO 29U
#define WD_RST 9U
#define WD_RST_PORT NRF_P0
#elif WD_ONE_ROW_WIRING_TEST
// ProMicro NRF52840 VCC-side header row. Keep all SWD outputs together.
#define WD_CLK 31U
#define WD_DIO 29U
#define WD_RST 2U
#define WD_RST_PORT NRF_P0
#else
#define WD_CLK 17U
#define WD_DIO 20U
#define WD_RST 22U
#define WD_RST_PORT NRF_P0
#endif
__STATIC_INLINE uint8_t DAP_GetVendorString(char* s){strcpy(s,"ARM");return 4;}
__STATIC_INLINE uint8_t DAP_GetProductString(char* s){strcpy(s,"CMSIS-DAP v2");return 13;}
__STATIC_INLINE uint8_t DAP_GetSerNumString(char* s){(void)s;return 0;}
__STATIC_INLINE uint8_t DAP_GetTargetDeviceVendorString(char* s){(void)s;return 0;}
__STATIC_INLINE uint8_t DAP_GetTargetDeviceNameString(char* s){(void)s;return 0;}
__STATIC_INLINE uint8_t DAP_GetTargetBoardVendorString(char* s){(void)s;return 0;}
__STATIC_INLINE uint8_t DAP_GetTargetBoardNameString(char* s){(void)s;return 0;}
__STATIC_INLINE uint8_t DAP_GetProductFirmwareVersionString(char* s){strcpy(s,"0.6.0");return 6;}
__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void){return (NRF_P0->IN>>WD_CLK)&1;}
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void){NRF_P0->OUTSET=1UL<<WD_CLK;}
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void){NRF_P0->OUTCLR=1UL<<WD_CLK;}
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void){return (NRF_P0->IN>>WD_DIO)&1;}
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void){NRF_P0->OUTSET=1UL<<WD_DIO;}
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void){NRF_P0->OUTCLR=1UL<<WD_DIO;}
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void){return PIN_SWDIO_TMS_IN();}
__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit){if(bit&1) PIN_SWDIO_TMS_SET(); else PIN_SWDIO_TMS_CLR();}
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void){NRF_P0->DIRSET=1UL<<WD_DIO;}
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void){NRF_P0->DIRCLR=1UL<<WD_DIO;}
__STATIC_FORCEINLINE uint32_t PIN_nRESET_IN(void){return (WD_RST_PORT->IN>>WD_RST)&1;}
// Open drain through direction control. Never drive the target reset high.
__STATIC_FORCEINLINE void PIN_nRESET_OUT(uint32_t b){WD_RST_PORT->OUTCLR=1UL<<WD_RST;if(b) WD_RST_PORT->DIRCLR=1UL<<WD_RST;else WD_RST_PORT->DIRSET=1UL<<WD_RST;}
__STATIC_INLINE void PORT_OFF(void){NRF_P0->PIN_CNF[WD_CLK]=0;NRF_P0->PIN_CNF[WD_DIO]=0;WD_RST_PORT->PIN_CNF[WD_RST]=(3UL<<2);}
__STATIC_INLINE void PORT_SWD_SETUP(void){
  NRF_P0->OUTSET=(1UL<<WD_CLK)|(1UL<<WD_DIO);
  NRF_P0->PIN_CNF[WD_CLK]=1;NRF_P0->PIN_CNF[WD_DIO]=1;
  // Input connected, weak pull-up. An unconnected NRST must read released,
  // not float low and make Keil wait forever for reset deassertion.
  WD_RST_PORT->PIN_CNF[WD_RST]=(3UL<<2);PIN_nRESET_OUT(1);
}
__STATIC_INLINE void PORT_JTAG_SETUP(void){}
__STATIC_INLINE uint32_t PIN_TDI_IN(void){return 0;}
__STATIC_INLINE void PIN_TDI_OUT(uint32_t b){(void)b;}
__STATIC_INLINE uint32_t PIN_TDO_IN(void){return 0;}
__STATIC_INLINE uint32_t PIN_nTRST_IN(void){return 1;}
__STATIC_INLINE void PIN_nTRST_OUT(uint32_t b){(void)b;}
__STATIC_INLINE void LED_CONNECTED_OUT(uint32_t b){(void)b;}
__STATIC_INLINE void LED_RUNNING_OUT(uint32_t b){(void)b;}
__STATIC_INLINE uint32_t TIMESTAMP_GET(void){return DWT->CYCCNT;}
__STATIC_INLINE void DAP_SETUP(void){
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  PORT_OFF();
}
__STATIC_INLINE uint8_t RESET_TARGET(void){
  // No board-specific reset sequence. Report Execute=0 so the debugger can
  // perform its selected reset via SWD (SYSRESETREQ) or SWJ_Pins (nRESET).
  // Claiming Execute=1 after an unconnected nRESET pulse hides a failed reset.
  return 0;
}
