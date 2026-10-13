#pragma once
#include "Protocol.h"

#ifndef WD_SOFT_RESET_VERIFY
#define WD_SOFT_RESET_VERIFY 0
#endif

namespace {
constexpr uint32_t DhcsrResetStatus = 1UL << 25;

inline bool softResetDelay(uint32_t (*process)(const uint8_t*,uint8_t*),
                           uint16_t microseconds) {
  uint8_t request[3]={0x09,static_cast<uint8_t>(microseconds),
                      static_cast<uint8_t>(microseconds>>8)};
  uint8_t response[64]={};
  process(request,response);
  return response[0]==0x09 && response[1]==0;
}

inline bool readCortexMDhcsr(uint32_t (*process)(const uint8_t*,uint8_t*),
                             uint32_t& value) {
  // DP SELECT, MEM-AP CSW/TAR, then one posted DRW read. DAP_ProcessCommand
  // drains the posted value through DP RDBUFF and returns it at response[3].
  uint8_t request[64]={0x05,0,4,
    0x08,0,0,0,0,
    0x01,0x52,0x00,0x00,0x23,
    0x05,0xf0,0xed,0x00,0xe0,
    0x0f};
  uint8_t response[64]={};
  process(request,response);
  if(response[0]!=0x05 || response[1]!=4 || response[2]!=1)return false;
  value=wd::read32(response+3);
  return true;
}
}

// Cortex-M on MEM-AP 0 (the project's MSPM0G3507). Invoked only by an explicit
// DAP_ResetTarget request, never by USB/RF connection or DAP_Disconnect.
inline bool resetCortexMViaSwd(uint32_t (*process)(const uint8_t*,uint8_t*)){
#if WD_SOFT_RESET_VERIFY
  uint8_t setup[64]={0x05,0,3,
    0x08,0,0,0,0,
    0x01,0x52,0x00,0x00,0x23,
    0x05,0x0c,0xed,0x00,0xe0};
  uint8_t setupResponse[64]={};
  process(setup,setupResponse);
  if(setupResponse[0]!=0x05 || setupResponse[1]!=3 || setupResponse[2]!=1)return false;

  // SYSRESETREQ can reset the target before SWD finishes returning the final
  // write ACK. Do not interpret that ACK alone as proof; confirm the reset via
  // DHCSR.S_RESET_ST below.
  uint8_t resetRequest[8]={0x05,0,1,0x0d,0x04,0x00,0xfa,0x05};
  uint8_t resetResponse[64]={};
  process(resetRequest,resetResponse);
  // CMSIS-Pack ResetSystem uses a 20 ms recovery interval, then polls
  // DHCSR.S_RESET_ST until it clears. This gives the target's reset and
  // debug port time to recover without using the unconnected nRESET pin.
  if(!softResetDelay(process,20000))return false;
  bool resetObserved=false;
  for(unsigned elapsedUs=0;elapsedUs<=500000;elapsedUs+=1000){
    uint32_t dhcsr=0;
    if(readCortexMDhcsr(process,dhcsr)){
      if(dhcsr&DhcsrResetStatus)resetObserved=true;
      else if(resetObserved)return true;
    }
    if(elapsedUs<500000 && !softResetDelay(process,1000))return false;
  }
  return false;
#else
  uint8_t request[64]={0x05,0,4},response[64]={};
  const uint8_t reg[4]={0x08,0x01,0x05,0x0d};
  const uint32_t value[4]={0,0x23000052,0xe000ed0c,0x05fa0004};
  for(unsigned i=0;i<4;i++){
    request[3+i*5]=reg[i];wd::write32(request+4+i*5,value[i]);
  }
  process(request,response);
  return response[0]==0x05 && response[1]==4 && response[2]==1;
#endif
}
