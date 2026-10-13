#pragma once
#include <stdint.h>

namespace wd {
constexpr uint32_t swdClockLimit(bool speedTest) {
  return speedTest ? 4000000UL : 3000000UL;
}
constexpr uint32_t localSwdClockLimit(bool wirelessSpeedTest) {
  return wirelessSpeedTest ? 3000000UL : swdClockLimit(false);
}
constexpr uint32_t wiredSwdClockLimit(bool wiredSlowTest) {
  return wiredSlowTest ? 2000000UL : swdClockLimit(false);
}
constexpr uint32_t wirelessSwdClockLimit(bool wirelessSpeedTest) {
  return wirelessSpeedTest ? 4000000UL : swdClockLimit(false);
}
}
