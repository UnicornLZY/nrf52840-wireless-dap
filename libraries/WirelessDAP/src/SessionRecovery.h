#pragma once
#include "Protocol.h"
namespace wd {
inline bool validHello(const Frame& f){
  return f.type==Hello && f.len>=1 && f.data[0]==LinkVersion && f.session!=0;
}
inline bool needsSessionReset(const Frame& f,bool ready,uint32_t current){
  return (!ready || f.session!=current) && f.session && f.type!=SessionReset;
}
inline bool resetRequestsOurSession(const Frame& f,uint32_t current){
  return f.type==SessionReset && f.len==4 && read32(f.data)==current;
}
}


