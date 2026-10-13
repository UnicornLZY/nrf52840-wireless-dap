#pragma once
#include <stdint.h>
#include <string.h>
#include "Protocol.h"

// Worker-owned counters. The snapshot command runs on the same SWD worker,
// so no lock is needed and measurements survive USB/RF ownership switches.
struct DapTiming {
  struct Bucket {
    uint32_t commands=0, us=0, blocks=0, blockUs=0, words=0, maxUs=0;
  } local,wireless;
  void record(bool isLocal,uint8_t command,uint32_t elapsedUs,uint16_t wordsDone){
    Bucket& b=isLocal?local:wireless;
    b.commands++;b.us+=elapsedUs;
    if(elapsedUs>b.maxUs)b.maxUs=elapsedUs;
    if(command==0x06){b.blocks++;b.blockUs+=elapsedUs;b.words+=wordsDone;}
  }
  void encode(uint8_t out[64]) const {
    memset(out,0,64);out[0]=0x81;out[1]=1;
    const Bucket* buckets[2]={&local,&wireless};
    for(unsigned i=0;i<2;i++){
      const Bucket& b=*buckets[i];uint8_t* p=out+2+i*24;
      wd::write32(p,b.commands);wd::write32(p+4,b.us);
      wd::write32(p+8,b.blocks);wd::write32(p+12,b.blockUs);
      wd::write32(p+16,b.words);wd::write32(p+20,b.maxUs);
    }
  }
};
