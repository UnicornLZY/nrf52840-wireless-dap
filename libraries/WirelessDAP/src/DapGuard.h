#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "Protocol.h"
#ifndef WD_DAP128
#define WD_DAP128 0
#endif
static constexpr size_t DapPacketSize=WD_DAP128?128:64;
// Check all variable-length requests BEFORE entering the upstream fixed-buffer parser.
// Atomic, JTAG, SWO and DAP-UART commands are not advertised or accepted.
inline bool validDap(const uint8_t* q) {
  size_t in=0,out=0;
  switch(q[0]) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x08: case 0x09: case 0x0a: case 0x13: return true;
    case 0x10: return wd::read32(q+3)<=3000000; // CMSIS-DAP SWJ_Pins maximum wait
    case 0x11: return wd::read32(q+1)>=50000; // avoid unbounded slow transfers
    case 0x12: return true; // at most 256 bits (32 bytes)
    case 0x05:
      in=3;out=3;
      for(unsigned i=0;i<q[2];i++) {
        if(in>=DapPacketSize) return false;
        uint8_t r=q[in++];
        if(r&0xc0) return false;
        if(r&2) { if(r&0x10) in+=4; else out+=4; }
        else in+=4;
        if(in>DapPacketSize || out>DapPacketSize) return false;
      }
      return true;
    case 0x06: {
      unsigned n=q[2]|unsigned(q[3])<<8;
      return (q[4]&0xf0)==0 && n<=((q[4]&2)?
        (DapPacketSize-4)/4:(DapPacketSize-5)/4);
    }
    case 0x1d:
      in=2;out=2;
      for(unsigned i=0;i<q[1];i++) {
        if(in>=DapPacketSize) return false;
        uint8_t r=q[in++]; unsigned bits=r&63; if(!bits) bits=64;
        if(r&0x40) return false;
        if(r&0x80) out+=(bits+7)/8; else in+=(bits+7)/8;
        if(in>DapPacketSize || out>DapPacketSize) return false;
      }
      return true;
    default:return false;
  }
}
inline void dapError(uint8_t cmd,uint8_t* out) {
  memset(out,0,DapPacketSize); out[0]=cmd;
  if(cmd==0x05) out[2]=8; // DAP_TRANSFER_ERROR, zero completed
  else if(cmd==0x06) out[3]=8;
  else if(cmd==0x02 || cmd==0x00) out[1]=0;
  else out[1]=0xff;
}

inline uint8_t dapErrorLength(uint8_t cmd){return cmd==5?3:(cmd==6?4:(cmd==0x0a?3:2));}
inline bool validDapResponse(const uint8_t* q,const uint8_t* r,size_t n){
  if(n==1 && q[0]==9 && r[0]==9)return true;
  if(n<2 || r[0]!=q[0])return false;
  if(q[0]==5){
    if(n<3 || r[1]>q[2])return false;
    size_t in=3,required=3;
    for(unsigned i=0;i<r[1];i++){
      if(in>=DapPacketSize)return false;
      uint8_t transfer=q[in++];
      if((transfer&2) && !(transfer&0x10))required+=4;
      if(!(transfer&2) || (transfer&0x10))in+=4;
      if(in>DapPacketSize)return false;
    }
    return n>=required;
  }
  if(q[0]==6){
    if(n<4)return false;
    unsigned count=unsigned(r[1])|(unsigned(r[2])<<8);
    if(count>(unsigned(q[2])|(unsigned(q[3])<<8)))return false;
    return n>=4+((q[4]&2)?count*4:0);
  }
  return true;
}
