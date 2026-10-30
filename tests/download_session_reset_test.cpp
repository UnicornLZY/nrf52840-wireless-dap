#include "DownloadSessionReset.h"
#include <cassert>
#include <cstdio>
int main(){
  DownloadSessionReset policy;
  uint8_t block[13]={6,0,2,0,13,1,2,3,4,5,6,7,8},br[4]={6,2,0,1};
  uint8_t disc[1]={3},dr[2]={3,0};
  auto end=[&](){return policy.observe(disc,1,dr,2);};
  auto data=[&](){return policy.observe(block,sizeof(block),br,4);};
  assert(!end());assert(!data());assert(end());assert(!end());
  block[4]=15;assert(!data());assert(!end());block[4]=13;
  block[4]=8;assert(!data());assert(!end());block[4]=13;
  assert(!data());br[3]=4;assert(!data());br[3]=1;
  assert(!data());assert(!end()); // Failure latches until the session ends.
  assert(!data());assert(end());
  assert(!data());uint8_t req[8]={5,0,1,13},rsp[3]={5,0,8};
  assert(!policy.observe(req,8,rsp,3));assert(!end());
  assert(!data());policy.clear();assert(!end()); // Lost epoch/link abandons it.
  assert(!data());policy.resetIssued();assert(!end());
  assert(!data());uint8_t reset[1]={0x0a},rr[3]={0x0a,0,1};
  assert(!policy.observe(reset,1,rr,3));assert(!end());
  assert(!data());uint8_t connect[2]={2,1},cr[2]={2,1};
  assert(!policy.observe(connect,2,cr,2));assert(!end());
  assert(!data());dr[1]=0xff;assert(!end());dr[1]=0;
  // No reset request appears anywhere in the successful completion case.
  assert(!data());uint8_t read[5]={6,0,2,0,15};
  assert(!policy.observe(read,5,br,4));assert(end());
  puts("PASS auto download reset: bulk write + Disconnect without Reset and Run; single pulse, reads/errors/link epoch/reset exclusions");
}
