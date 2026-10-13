#pragma once
#include <stdint.h>

namespace wd {
enum BoardPreference : uint8_t { AutoRole=0, TargetRole=1, HostRole=2 };

// Both boards run the exact same image. These FICR IDs bind the existing
// wired target board and computer board to their proven physical wiring.
// Unrecognized boards keep the experimental automatic election policy.
inline BoardPreference boardPreference(uint32_t deviceIdLow,uint32_t deviceIdHigh){
  (void)deviceIdLow;(void)deviceIdHigh;
  return AutoRole;
}
}
