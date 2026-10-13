#pragma once
#include "Protocol.h"
struct PacketStats {
  uint32_t commands=0, blocks=0, writeWords=0, readWords=0;
  uint32_t largeBlocks=0, maxWrite=0, maxRead=0;
  void request(const uint8_t* q){
    if(q[0]>=0x80)return;
    commands++;
    if(q[0]!=6)return;
    const uint32_t words=q[2]|uint32_t(q[3])<<8;
    blocks++;if(words>15)largeBlocks++;
    if(q[4]&2){readWords+=words;if(words>maxRead)maxRead=words;}
    else{writeWords+=words;if(words>maxWrite)maxWrite=words;}
  }
  void encode(uint8_t* out,size_t packetSize)const{
    memset(out,0,64);out[0]=0x83;out[1]=1;out[2]=packetSize;out[3]=wd::LinkVersion;
    const uint32_t fields[]={commands,blocks,writeWords,readWords,largeBlocks,maxWrite,maxRead};
    for(unsigned i=0;i<7;i++)wd::write32(out+4+i*4,fields[i]);
  }
};

