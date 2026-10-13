#pragma once
#include <stdint.h>
#ifndef WD_V3_FIX
#define WD_V3_FIX 0
#endif
#ifndef WD_RADIO_MBPS
#define WD_RADIO_MBPS 2
#endif
#ifndef WD_RADIO_CHANNEL
#define WD_RADIO_CHANNEL 40
#endif
static_assert(WD_RADIO_MBPS == 1 || WD_RADIO_MBPS == 2, "Nordic proprietary PHY must be 1 or 2 Mbps");
static_assert(WD_RADIO_CHANNEL >= 0 && WD_RADIO_CHANNEL <= 80, "RF channel must be 2400..2480 MHz");
namespace wd {
// Adafruit micros() is DWT->CYCCNT / 64 and wraps at 2^26 us, not 2^32.
// Mask every subtraction, including retry deadlines, across the 67 s wrap.
constexpr uint32_t RadioTimeMask = 0x03ffffffUL;
inline uint32_t radioElapsed(uint32_t now,uint32_t then){return (now-then)&RadioTimeMask;}
inline bool radioDue(uint32_t now,uint32_t deadline){return radioElapsed(now,deadline)<0x02000000UL;}
constexpr uint8_t RadioHeader = 15;
constexpr uint8_t RadioPayload = 232;
constexpr uint32_t RadioAirtimeUs = (1+5+1+RadioHeader+RadioPayload+2)*8/WD_RADIO_MBPS;
constexpr uint32_t RadioTxTimeoutUs = RadioAirtimeUs + 1500;
constexpr uint32_t RadioAckTimeoutUs = RadioAirtimeUs + 5000;
inline bool radioPacketValid(const uint8_t* p) {
  const uint16_t magic = uint16_t(p[1]) | uint16_t(p[2]) << 8;
  return p[0] >= RadioHeader && p[0] <= RadioHeader+RadioPayload &&
    (magic == 0xe552 || magic == 0xe553) && p[15] <= RadioPayload &&
    p[0] == RadioHeader+p[15] && (magic != 0xe553 || p[15] == 0);
}
}
