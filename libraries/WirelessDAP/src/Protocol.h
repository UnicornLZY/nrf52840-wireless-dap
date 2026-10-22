#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace wd {
#ifndef WD_DAP240
#define WD_DAP240 0
#endif
#ifndef WD_DAP128
#define WD_DAP128 0
#endif
constexpr uint8_t LinkVersion=WD_DAP240?14:(WD_DAP128?5:4);
inline uint16_t nextDapSequence(uint16_t seq){return seq==0xffff?1:uint16_t(seq+1);}
constexpr size_t PayloadMax = WD_DAP240?240:(WD_DAP128?128:64);
constexpr size_t RawMax = PayloadMax + 10;
constexpr size_t WireMax = RawMax * 2 + 2;
enum Type : uint8_t { Hello=1, HelloAck, Ping, Pong, DapRequest, DapResponse,
                      UartData, UartAck, Abort, UartConfig, UartConfigAck, SessionReset };
struct Frame {
  uint32_t epoch; // Internal worker generation; not serialized on radio.
  uint8_t type;
  uint32_t session;
  uint16_t seq;
  uint8_t len;
  uint8_t data[PayloadMax];
};
inline uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc=0xffff;
  while(n--) { crc ^= uint16_t(*p++)<<8; for(int b=0;b<8;b++) crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1; }
  return crc;
}
inline uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
inline void write32(uint8_t* p,uint32_t v) { for(int i=0;i<4;i++) p[i]=v>>(i*8); }
inline size_t encode(const Frame& f,uint8_t* out) {
  if(f.len>PayloadMax) return 0;
  uint8_t raw[RawMax]; raw[0]=f.type; write32(raw+1,f.session);
  raw[5]=f.seq; raw[6]=f.seq>>8; raw[7]=f.len;
  memcpy(raw+8,f.data,f.len);
  uint16_t c=crc16(raw,f.len+8); raw[f.len+8]=c; raw[f.len+9]=c>>8;
  size_t n=0; out[n++]=0x7e;
  for(size_t i=0;i<size_t(f.len)+10;i++) {
    if(raw[i]==0x7e || raw[i]==0x7d) { out[n++]=0x7d; out[n++]=raw[i]^0x20; }
    else out[n++]=raw[i];
  }
  out[n++]=0x7e; return n;
}
class Parser {
  uint8_t raw[RawMax]; size_t used=0; bool escape=false,bad=false;
public:
  uint32_t errors=0;
  void reset() { used=0; escape=false; bad=false; }
  bool feed(uint8_t ch,Frame& f) {
    if(ch==0x7e) {
      bool ok=!bad && !escape && used>=10 && raw[7]<=PayloadMax && used==size_t(raw[7])+10;
      if(ok) ok=crc16(raw,used-2)==uint16_t(raw[used-2]|uint16_t(raw[used-1])<<8);
      if(ok) { f.type=raw[0]; f.session=read32(raw+1); f.seq=raw[5]|uint16_t(raw[6])<<8; f.len=raw[7]; memcpy(f.data,raw+8,f.len); }
      else if(used || bad || escape) errors++;
      reset(); return ok;
    }
    if(bad) return false;
    if(escape) { ch^=0x20; escape=false; }
    else if(ch==0x7d) { escape=true; return false; }
    if(used==RawMax) bad=true; else raw[used++]=ch;
    return false;
  }
};
template<size_t N> class Ring {
  uint8_t bytes[N]; size_t head=0,tail=0,count_=0;
public:
  uint32_t dropped=0;
  size_t size() const { return count_; }
  size_t free() const { return N-count_; }
  void clear() { head=tail=count_=0; }
  bool push(uint8_t b) { if(count_==N){dropped++;return false;} bytes[head]=b;head=(head+1)%N;count_++;return true; }
  uint8_t pop() { uint8_t b=bytes[tail];tail=(tail+1)%N;count_--;return b; }
  uint8_t peek() const {return bytes[tail];}
};
}
