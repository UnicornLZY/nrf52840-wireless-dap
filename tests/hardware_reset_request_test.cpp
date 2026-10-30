#include "HardwareResetRequest.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static void put(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=uint8_t(v>>(8*i));}
int main(){
  HardwareResetRequest tracker;
  uint8_t q[240]={5,0,4,8,0,0,0,0,1,0x12,0,0,0x23,5,0x0c,0xed,0,0xe0,13,4,0,0xfa,5};
  uint8_t r[4]={5,4,1,0};
  assert(tracker.observe(q,sizeof(q),r,3));
  q[19]=0;assert(!tracker.observe(q,sizeof(q),r,3));q[19]=4;
  q[22]=0;assert(!tracker.observe(q,sizeof(q),r,3));q[22]=5;
  r[2]=4;assert(!tracker.observe(q,sizeof(q),r,3));r[2]=1;
  assert(tracker.observe(q,sizeof(q),r,3));
  assert(!tracker.observe(q,21,r,3)); // Truncated reset write cancels state.
  uint8_t write[8]={5,0,1,13,4,0,0xfa,5},wr[3]={5,1,1};
  assert(!tracker.observe(write,sizeof(write),wr,3));
  assert(tracker.observe(q,sizeof(q),r,3));
  uint8_t disconnect[1]={3},dr[2]={3,0};
  assert(!tracker.observe(disconnect,1,dr,2));
  assert(!tracker.observe(write,sizeof(write),wr,3));
  // Packed, unaligned and wrong-AP writes cannot identify AIRCR.
  q[9]=0x10;assert(!tracker.observe(q,sizeof(q),r,3));q[9]=0x12;
  q[7]=1;assert(!tracker.observe(q,sizeof(q),r,3));q[7]=0;
  // Track >64-byte TransferBlock writes with single increment and TAR wrap.
  uint8_t setup[18]={5,0,3,8,0,0,0,0,1,0x12,0,0,0x23,5};
  put(setup+14,0xe000ecc0);uint8_t sr[3]={5,3,1};
  assert(!tracker.observe(setup,sizeof(setup),sr,3));
  uint8_t block[240]={6,0,20,0,13},br[4]={6,20,0,1};
  put(block+5+19*4,0x05fa0004);
  assert(tracker.observe(block,sizeof(block),br,4));
  // Banked data register addressing rounds TAR down to a 16-byte boundary.
  put(setup+14,0xe000ed0f);tracker.clear();
  assert(!tracker.observe(setup,sizeof(setup),sr,3));
  uint8_t bank[8]={5,0,1,8,0x10,0,0,0};
  assert(!tracker.observe(bank,sizeof(bank),wr,3));
  write[3]=13;assert(tracker.observe(write,sizeof(write),wr,3));
  // Debug RAM, disconnect, idle and failures never request hardware reset.
  uint8_t info[2]={0,0xff},ir[4]={0,2,240,0};
  assert(!tracker.observe(info,2,ir,4));
  puts("PASS hardware reset request: explicit AIRCR, bounds, failures, AP/size filtering, block and banked writes");
}
