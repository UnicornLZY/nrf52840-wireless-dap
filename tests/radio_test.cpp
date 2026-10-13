#include <Arduino.h>
#define private public
#include "../libraries/WirelessDAP/src/EsbTransport.cpp"
#undef private
#include <cassert>
#include <random>
uint32_t mockNow=100;
MockFicr ficr;MockFicr* NRF_FICR=&ficr;
RadioRegs radio;RadioRegs* NRF_RADIO=&radio;
ClockRegs clocks;ClockRegs* NRF_CLOCK=&clocks;
RngRegs rng;RngRegs* NRF_RNG=&rng;
static void receive(EsbTransport& r,const uint8_t* packet,bool crc=true){
 memcpy(r.radioRx_,packet,packet[0]+1);radio.EVENTS_END=1;radio.CRCSTATUS=crc;radio.STATE=0;
}
static void txDone(EsbTransport& r){radio.EVENTS_END=1;radio.EVENTS_DISABLED=0;r.onRadioDisabled();}
int main(){
 EsbTransport h,t;h.bootId_=123;t.bootId_=456;h.setRole(true);t.setRole(false);
 const uint32_t expectedPlen = (WD_RADIO_MBPS == 1) ? RADIO_PCNF0_PLEN_8bit : RADIO_PCNF0_PLEN_16bit;
 const uint32_t expectedPcnf0 = (8UL << RADIO_PCNF0_LFLEN_Pos) | (expectedPlen << RADIO_PCNF0_PLEN_Pos);
 assert(radio.PCNF0==expectedPcnf0 && radio.TXPOWER==8 && !(radio.PCNF1&RADIO_PCNF1_WHITEEN_Msk));
 std::mt19937 random(52840);
 for(unsigned trial=0;trial<1000;trial++){
   uint8_t input[232],reverse[232];unsigned n=1+random()%232,m=1+random()%232;
   for(unsigned i=0;i<n;i++)input[i]=random();
   for(unsigned i=0;i<m;i++)reverse[i]=random();
   assert(t.write(reverse,m)==m);
   assert(h.write(input,n)==0 && h.phase_==EsbTransport::HostTx);
   #if WD_V3_FIX
   radio.EVENTS_END=0;h.onRadioDisabled();assert(h.phase_==EsbTransport::HostTx);assert(radio.PACKETPTR==(uintptr_t)h.radioTx_);
#endif
   uint8_t request[256];memcpy(request,h.radioTx_,256);txDone(h);
   receive(t,request);t.poll();assert(t.phase_==EsbTransport::TargetTx);
   uint8_t ack[256];memcpy(ack,t.radioTx_,256);txDone(t);
   assert(t.available()==int(n));
   // Lose up to three ACKs: retries must retain bytes/sequence and target's
   // cached reverse payload. The target UART/DAP stream is admitted only once.
   unsigned losses=random()%4;
   for(unsigned i=0;i<losses;i++){
     mockNow+=10;h.poll();assert(h.phase_==EsbTransport::Backoff);
     mockNow++;h.poll();assert(h.phase_==EsbTransport::HostTx);
     assert(!memcmp(request,h.radioTx_,request[0]+1));txDone(h);
     receive(t,request);t.poll();assert(t.available()==int(n));
     assert(!memcmp(ack,t.radioTx_,ack[0]+1));txDone(t);
   }
   // CRC failure must not close the remaining reply window.
   receive(h,ack,false);h.poll();assert(h.phase_==EsbTransport::HostRx);
   receive(h,ack);h.poll();assert(h.phase_==EsbTransport::Idle);
   assert(h.write(input,n)==n && h.available()==int(m));
   for(unsigned i=0;i<n;i++)assert(t.read()==input[i]);
   for(unsigned i=0;i<m;i++)assert(h.read()==reverse[i]);
   assert(!t.available() && !h.available());mockNow++;
 }
 // No partial admission/ACK when RX ring is full.
 uint8_t data[64]={5};while(t.rx_.free())t.rx_.push(9);
 assert(!h.write(data,64));uint8_t req[256];memcpy(req,h.radioTx_,256);txDone(h);
 receive(t,req);t.poll();assert(t.phase_==EsbTransport::Idle && t.stats_.backpressure==1);
 t.rx_.clear();receive(t,req);t.poll();assert(t.available()==64);txDone(t);
 // ACK from an earlier boot cannot acknowledge a current operation.
 uint8_t ack[256];memcpy(ack,t.radioTx_,256);write32(ack+9,h.bootId_+1);
 receive(h,ack);h.poll();assert(h.phase_==EsbTransport::HostRx);
 memcpy(ack,t.radioTx_,256);receive(h,ack);h.poll();assert(h.write(data,64)==64);
 // Radio restart preserves sequence identity. Application flush cancels only
 // the pending fragment, keeping the duplicate cache to avoid replay.
 uint16_t seq=h.txSeq_;h.setRole(true);assert(h.txSeq_==seq);
 assert(!h.write(data,64));assert(h.pendingSeq_!=seq);h.flush();assert(!h.pendingSeq_);
 assert(h.phase_==EsbTransport::Idle && !h.completedLength_);
 // Rebooted host with same 16-bit sequence has a distinct boot identity.
 t.rx_.clear();uint16_t last=t.lastPeerSeq_;uint32_t oldBoot=t.peerBoot_;
 header(req,false,oldBoot+1,last,0,0,1);req[16]=0xab;
 receive(t,req);t.poll();assert(t.available()==1 && t.read()==0xab);txDone(t);
 // Strict lengths: truncated, padded and local-owner payloads rejected.
 for(unsigned length=0;length<256;length++){
   header(req,false,1,1,0,0,5);req[0]=length;
   assert(radioPacketValid(req)==(length==RadioHeader+5));
 }
 assert(radioElapsed(4,RadioTimeMask-3)==8);
 assert(!radioDue(RadioTimeMask-3,4));assert(radioDue(4,4));
 h.flush();h.pendingSeq_=h.nextSeq();h.phase_=EsbTransport::Backoff;h.retryAt_=200;
 mockNow=67108;h.poll();assert(h.phase_==EsbTransport::Backoff);
 mockNow+=2;h.poll();assert(h.phase_==EsbTransport::HostTx);
 puts("PASS: real radio state machine, 1000 randomized bidirectional exchanges, lost ACK/CRC, duplicate suppression, backpressure, stale boot, cancellation, packet lengths");
}

