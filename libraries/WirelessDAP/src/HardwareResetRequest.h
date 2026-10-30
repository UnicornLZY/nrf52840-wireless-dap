#pragma once
#include <stddef.h>
#include <stdint.h>

// Recognize an explicit Cortex-M SYSRESETREQ through MEM-AP 0. Idle time,
// Disconnect and ordinary RAM/Flash writes are not download completion signals.
class HardwareResetRequest {
  uint32_t select_=0, tar_=0, csw_=0;
  bool selected_=false, addressed_=false, configured_=false;
  static uint32_t word(const uint8_t* p) {
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
  }
  bool write(uint8_t op,uint32_t value) {
    if(!(op&1)) {
      if((op&12)==8){
        if(!selected_ || (select_>>24)!=(value>>24))addressed_=configured_=false;
        select_=value;selected_=true;
      }
      return false;
    }
    if(!selected_ || (select_>>24)!=0)return false;
    uint32_t reg=(select_&0xf0)|(op&12);
    if(reg==0){csw_=value;configured_=true;return false;}
    if(reg==4){tar_=value;addressed_=true;return false;}
    if(!addressed_ || !configured_ || (csw_&7)!=2)return false;
    uint32_t address=tar_;
    if(reg>=0x10 && reg<=0x1c)address=(tar_&~15UL)+(reg-0x10);
    else if(reg!=12)return false;
    bool reset=address==0xe000ed0cUL && (value&0xffff0004UL)==0x05fa0004UL;
    if(reg==12){
      if((csw_&0x30)==0x10)tar_=(tar_&~0x3ffUL)|((tar_+4)&0x3ff);
      else if(csw_&0x30)addressed_=false;
    }
    return reset;
  }
public:
  void clear(){select_=tar_=csw_=0;selected_=addressed_=configured_=false;}
  bool observe(const uint8_t* q,size_t n,const uint8_t* r,size_t rn) {
    if(!n || !rn){clear();return false;}
    if(q[0]!=5 && q[0]!=6){if(q[0]==2 || q[0]==3 || q[0]==0x0a)clear();return false;}
    if(n<3 || rn<3 || q[1]!=0 || r[0]!=q[0]){clear();return false;}
    unsigned count=q[2];size_t pos=3;uint8_t blockOp=0;
    if(q[0]==6){
      if(n<5 || rn<4){clear();return false;}
      count|=unsigned(q[3])<<8;pos=5;blockOp=q[4];
      if((unsigned(r[1])|(unsigned(r[2])<<8))!=count || r[3]!=1){clear();return false;}
    }else if(r[1]!=count || r[2]!=1){clear();return false;}
    HardwareResetRequest next=*this;bool reset=false;
    for(unsigned i=0;i<count;i++){
      if(q[0]==5 && pos>=n){clear();return false;}
      uint8_t op=q[0]==5?q[pos++]:blockOp;
      if(op&2){if(op&0x10)pos+=4;if(pos>n){clear();return false;}continue;}
      if(pos+4>n){clear();return false;}
      uint32_t value=word(q+pos);pos+=4;
      if(!(op&0x20))reset=next.write(op,value)||reset;
    }
    *this=next;return reset;
  }
};
