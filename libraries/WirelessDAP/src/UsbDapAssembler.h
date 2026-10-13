#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TinyUSB reports each 64-byte Bulk OUT transaction separately. CMSIS-DAP's
// advertised 128-byte logical command spans two such transactions. A short
// transaction (including ZLP) ends a shorter command.
class UsbDapAssembler {
public:
  void reset(){used_=0;}
  bool feed(const uint8_t* bytes,size_t n,uint8_t out[128]){
    if(n>64 || used_+n>128){reset();return false;}
    if(n)memcpy(partial_+used_,bytes,n);
    used_+=n;
    if(used_!=128 && n==64)return false;
    if(!used_)return false;
    memset(out,0,128);memcpy(out,partial_,used_);reset();return true;
  }
private:
  uint8_t partial_[128]={};
  size_t used_=0;
};
