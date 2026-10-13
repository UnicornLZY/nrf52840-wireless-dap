#pragma once
#include "Protocol.h"
namespace wd {
struct UartSettings {
  uint32_t baud;
  uint8_t stop,parity,bits;
  UartSettings(uint32_t rate=115200,uint8_t s=0,uint8_t p=0,uint8_t b=8)
    :baud(rate),stop(s),parity(p),bits(b){}
  bool operator==(const UartSettings& o)const{
    return baud==o.baud && stop==o.stop && parity==o.parity && bits==o.bits;
  }
  bool operator!=(const UartSettings& o)const{return !(*this==o);}
  bool valid()const{
    if(bits!=8 || (stop!=0 && stop!=2) || (parity!=0 && parity!=2))return false;
    const uint32_t rates[]={1200,2400,4800,9600,14400,19200,28800,31250,38400,
      56000,57600,76800,115200,230400,250000,460800,921600,1000000};
    for(uint32_t rate:rates)if(baud==rate)return true;
    return false;
  }
  void encode(uint8_t* p)const{write32(p,baud);p[4]=stop;p[5]=parity;p[6]=bits;}
  static UartSettings decode(const uint8_t* p){return UartSettings(read32(p),p[4],p[5],p[6]);}
};
}
