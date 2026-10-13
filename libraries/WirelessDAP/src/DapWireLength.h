#pragma once
#include <stddef.h>
#include <stdint.h>

// Return the number of request bytes consumed by the high-volume CMSIS-DAP
// Transfer commands. The USB-facing packet remains fixed-size; only the
// private radio frame omits zero padding. Unknown commands keep the full size.
inline size_t dapWireRequestLength(const uint8_t* request, size_t packetSize) {
  if (!request || packetSize == 0) return packetSize;
  switch (request[0]) {
    case 0x00: case 0x01: case 0x02: case 0x13: return packetSize>=2?2:packetSize;
    case 0x03: case 0x0a: return 1;
    case 0x04: case 0x08: return packetSize>=6?6:packetSize;
    case 0x09: return packetSize>=3?3:packetSize;
    case 0x10: return packetSize>=7?7:packetSize;
    case 0x11: return packetSize>=5?5:packetSize;
    case 0x12: {
      if(packetSize<2)return packetSize;
      const size_t bytes=2+((request[1]?request[1]:256)+7)/8;
      return bytes<=packetSize?bytes:packetSize;
    }
    case 0x05: { // DAP_Transfer
      if (packetSize < 3) return packetSize;
      size_t input = 3;
      const uint8_t count = request[2];
      for (uint8_t i = 0; i < count; ++i) {
        if (input >= packetSize) return packetSize;
        const uint8_t transfer = request[input++];
        if ((transfer & 0x02) == 0 || (transfer & 0x10) != 0) input += 4;
        if (input > packetSize) return packetSize;
      }
      return input;
    }
    case 0x06: { // DAP_TransferBlock
      if (packetSize < 5) return packetSize;
      const size_t count = size_t(request[2]) | (size_t(request[3]) << 8);
      const size_t input = 5 + ((request[4] & 0x02) ? 0 : count * 4);
      return input <= packetSize ? input : packetSize;
    }
    default:
      return packetSize;
  }
}
