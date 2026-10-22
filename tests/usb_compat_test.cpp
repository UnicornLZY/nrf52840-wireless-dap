#include <cstdio>
#define WD_USB_COMPAT 1
#include "../libraries/WirelessDAP/src/UsbDapResponse.h"
#include "../libraries/WirelessDAP/src/UsbDapTxState.h"
#include <cassert>
#include <vector>
struct Endpoint {
 UsbDapTxState state;std::vector<std::vector<uint8_t>> packets;
 bool mounted(){return true;}
 size_t available(){return state.isBusy()?0:64;}
 size_t write(const uint8_t* p,size_t n){assert(!state.isBusy());state.begin(n);packets.emplace_back(p,p+n);return n;}
 void flush(){}
};
int main(){
 Endpoint ep;uint8_t p[64]={};
 for(unsigned i=0;i<20000;i++){
   const size_t n=1+i%64;p[0]=i%251;
   assert(sendDapUsbPacket(ep,p,n));assert(ep.packets.back().size()==n);
   assert(!sendDapUsbPacket(ep,p,2));ep.state.complete(n);
   assert(!ep.state.isBusy());
 }
 ep.state.begin(64);ep.state.clear();assert(!ep.state.isBusy());
 puts("PASS USB compatibility: actual response lengths, no merging, immediate completion serialization, 20000 completion schedules");
}

