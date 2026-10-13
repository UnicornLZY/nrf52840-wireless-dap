#pragma once
#include <stdint.h>
#include <stddef.h>

// Tracks MEM-AP writes so the flash-algorithm resume write can be retried only
// when the command stream explicitly targets Cortex-M DHCSR with C_DEBUGEN=1
// and C_HALT=0. Repeating that write is idempotent and does not reset the core.
class DapRunKick {
  uint32_t select_=0;
  uint32_t tar_=0;
  uint32_t csw_=0;

  bool apWrite(uint8_t request,uint32_t value) {
    if(!(request&1U)) {
      if((request&0x0cU)==0x08U)select_=value;
      return false;
    }
    if((select_>>24)!=0)return false; // This compatibility path is MEM-AP 0.
    const uint32_t address=(select_&0xf0U)|(request&0x0cU);
    if(address==0x00U){csw_=value;return false;}
    if(address==0x04U){tar_=value;return false;}
    if(address==0x0cU){
      const bool resume=tar_==0xe000edf0UL && value==0xa05f0001UL;
      if(csw_&0x30U)tar_+=4U;
      return resume;
    }
    if(address>=0x10U && address<=0x1cU){
      return tar_+(address-0x10U)==0xe000edf0UL && value==0xa05f0001UL;
    }
    return false;
  }

public:
  void clear(){select_=tar_=csw_=0;}

  // Returns true only after the complete CMSIS-DAP write command succeeded.
  bool observe(const uint8_t* request,const uint8_t* response){
    if(request[0]==0x05U){
      const unsigned count=request[2];
      if(response[0]!=0x05U || response[1]!=count || response[2]!=1U)return false;
      size_t offset=3;
      bool resume=false;
      for(unsigned i=0;i<count;i++){
        if(offset>=64)return false;
        const uint8_t transfer=request[offset++];
        if(transfer&2U){
          if(transfer&0x10U)offset+=4; // Value-match operand.
        } else {
          if(offset+4>64)return false;
          const uint32_t value=uint32_t(request[offset])|
            (uint32_t(request[offset+1])<<8)|(uint32_t(request[offset+2])<<16)|
            (uint32_t(request[offset+3])<<24);
          offset+=4;
          if(!(transfer&0x20U))resume=apWrite(transfer,value)||resume;
        }
      }
      return resume;
    }
    if(request[0]==0x06U){
      const unsigned count=request[2]|(unsigned(request[3])<<8);
      const uint8_t transfer=request[4];
      if(response[0]!=0x06U || response[1]!=uint8_t(count) ||
         response[2]!=uint8_t(count>>8) || response[3]!=1U || (transfer&2U))return false;
      bool resume=false;
      size_t offset=5;
      for(unsigned i=0;i<count;i++){
        if(offset+4>64)return false;
        const uint32_t value=uint32_t(request[offset])|
          (uint32_t(request[offset+1])<<8)|(uint32_t(request[offset+2])<<16)|
          (uint32_t(request[offset+3])<<24);
        offset+=4;
        resume=apWrite(transfer,value)||resume;
      }
      return resume;
    }
    return false;
  }
};
