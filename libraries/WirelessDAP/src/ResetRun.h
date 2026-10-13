#pragma once
#include "Protocol.h"

// Narrow compatibility path for the captured Keil teardown/reset sequence.
// This does not infer flash success from Disconnect or reset on connection.
class ResetRun {
  unsigned state_=0;
  static bool write(const uint8_t* q,uint32_t address,uint32_t value){
    return q[0]==5 && q[1]==0 && q[2]==2 && q[3]==5 &&
      wd::read32(q+4)==address && q[8]==13 && wd::read32(q+9)==value;
  }
public:
  void clear(){state_=0;}
  bool observe(const uint8_t* q,const uint8_t* r){
    if(q[0]==5){
      if(r[0]!=5 || r[1]!=2 || r[2]!=1){clear();return false;}
      if(write(q,0xe000edf0,0xa05f0000)){state_=1;return false;}
      if(state_==1 && write(q,0xe000edfc,0)){state_=2;return false;}
    }
    if(q[0]==0x10 && r[0]==0x10){
      if(q[2]==0)return false; // Read-only pin query.
      if(q[2]==0x80){
        if(state_==2 && !(q[1]&0x80)){state_=3;return false;}
        if(state_==3 && (q[1]&0x80)){clear();return true;}
      }
    }
    clear();return false;
  }
};
