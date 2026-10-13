#pragma once
#include <stdint.h>

namespace wd {
// Validate framing before an ACK is allowed to alter USB ownership or queues.
inline bool validEsbReply(uint8_t length,uint16_t magic,uint8_t count){
  if(count>240 || unsigned(length)!=7u+count)return false;
  if(magic==0xe543)return count==0; // Ownership notifications carry no data.
  return magic==0xe542;
}
}
