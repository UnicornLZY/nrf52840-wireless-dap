#include "EsbTransport.h"
#include "nrf.h"
#include <string.h>
using namespace wd;
static EsbTransport* activeRadio=nullptr;
static uint32_t radioNow(){return micros()&RadioTimeMask;}
extern "C" void RADIO_IRQHandler(){
  if(NRF_RADIO->EVENTS_DISABLED){
    NRF_RADIO->EVENTS_DISABLED=0;
    if(activeRadio)activeRadio->onRadioDisabled();
  }
}
// IRQ only switches DMA direction. The main task owns all byte queues and
// protocol state. PTX must enter RX before the peer's ACK, without a task tick.
void EsbTransport::onRadioDisabled(){
  if((phase_==HostTx || phase_==TargetTx)
#if WD_V3_FIX
     && NRF_RADIO->EVENTS_END
#endif
     ){
    const bool host=phase_==HostTx;
    NRF_RADIO->PACKETPTR=(uintptr_t)radioRx_;
    NRF_RADIO->EVENTS_END=0;
    phase_=host?HostRx:Idle;phaseAt_=radioNow();
    NRF_RADIO->TASKS_RXEN=1;
  }
}
static uint16_t get16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
static void put16(uint8_t* p,uint16_t n){p[0]=n;p[1]=n>>8;}
static void header(uint8_t* p,bool local,uint32_t boot,uint16_t seq,
                   uint32_t echoBoot,uint16_t echo,uint8_t count){
  p[0]=RadioHeader+count;put16(p+1,local?0xe553:0xe552);
  write32(p+3,boot);put16(p+7,seq);write32(p+9,echoBoot);
  put16(p+13,echo);p[15]=count;
}
void EsbTransport::configure(){
  NVIC_DisableIRQ(RADIO_IRQn);
  NRF_CLOCK->EVENTS_HFCLKSTARTED=0;NRF_CLOCK->TASKS_HFCLKSTART=1;
  uint32_t start=radioNow();
  const uint32_t mask=CLOCK_HFCLKSTAT_SRC_Msk|CLOCK_HFCLKSTAT_STATE_Msk;
  while((NRF_CLOCK->HFCLKSTAT&mask)!=mask && radioElapsed(radioNow(),start)<10000){}
  NRF_RADIO->POWER=1;
  NRF_RADIO->MODE=WD_RADIO_MBPS==1?RADIO_MODE_MODE_Nrf_1Mbit:RADIO_MODE_MODE_Nrf_2Mbit;
  NRF_RADIO->TXPOWER=RADIO_TXPOWER_TXPOWER_Pos8dBm;
  NRF_RADIO->FREQUENCY=WD_RADIO_CHANNEL;
  NRF_RADIO->MODECNF0=RADIO_MODECNF0_RU_Fast;
  // Nordic 2 Mbit proprietary mode uses a 16-bit preamble (PS 6.20); 1 Mbit uses 8-bit.
  const uint32_t plen = (WD_RADIO_MBPS == 1) ? RADIO_PCNF0_PLEN_8bit : RADIO_PCNF0_PLEN_16bit;
  NRF_RADIO->PCNF0=(8UL<<RADIO_PCNF0_LFLEN_Pos)|(plen<<RADIO_PCNF0_PLEN_Pos);
  NRF_RADIO->PCNF1=(250UL<<RADIO_PCNF1_MAXLEN_Pos)|(4UL<<RADIO_PCNF1_BALEN_Pos);
  // Whitening is disabled for Nordic proprietary ESB mode.
  NRF_RADIO->BASE0=0x7a6d5100UL;NRF_RADIO->PREFIX0=0xc3;
  NRF_RADIO->TXADDRESS=0;NRF_RADIO->RXADDRESSES=1;
  NRF_RADIO->CRCCNF=RADIO_CRCCNF_LEN_Two;
  NRF_RADIO->CRCINIT=0xffff;NRF_RADIO->CRCPOLY=0x11021;
  NRF_RADIO->SHORTS=RADIO_SHORTS_READY_START_Msk|RADIO_SHORTS_END_DISABLE_Msk|
    RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
  NRF_RADIO->PACKETPTR=(uintptr_t)radioRx_;
  NRF_RADIO->EVENTS_END=NRF_RADIO->EVENTS_DISABLED=0;
  NRF_RADIO->INTENCLR=0xffffffffUL;
  NRF_RADIO->INTENSET=RADIO_INTENSET_DISABLED_Msk;
  NVIC_SetPriority(RADIO_IRQn,2);NVIC_ClearPendingIRQ(RADIO_IRQn);NVIC_EnableIRQ(RADIO_IRQn);
}
bool EsbTransport::waitDisabled(uint32_t timeoutUs){
  uint32_t start=radioNow();
  while(NRF_RADIO->STATE!=RADIO_STATE_STATE_Disabled && radioElapsed(radioNow(),start)<timeoutUs){}
  return NRF_RADIO->STATE==RADIO_STATE_STATE_Disabled;
}
void EsbTransport::receiveStart(){
  NVIC_DisableIRQ(RADIO_IRQn);
#if WD_V3_FIX
  if(NRF_RADIO->STATE!=RADIO_STATE_STATE_Disabled)
#endif
  NRF_RADIO->TASKS_DISABLE=1;
  if(!waitDisabled(100))return;
  NRF_RADIO->PACKETPTR=(uintptr_t)radioRx_;
  NRF_RADIO->EVENTS_END=NRF_RADIO->EVENTS_DISABLED=0;
  NVIC_ClearPendingIRQ(RADIO_IRQn);NVIC_EnableIRQ(RADIO_IRQn);
  NRF_RADIO->TASKS_RXEN=1;
}
void EsbTransport::begin(bool host){
  activeRadio=this;
  // A new power cycle must not collide with the peer's cached sequence 1.
  // RNG is used only at boot; SoftDevice is never started by this firmware.
  NRF_RNG->CONFIG=1;NRF_RNG->EVENTS_VALRDY=0;NRF_RNG->TASKS_START=1;
  uint32_t seed=0,start=radioNow();unsigned bytes=0;
  while(bytes<4 && radioElapsed(radioNow(),start)<20000){
    if(NRF_RNG->EVENTS_VALRDY){seed=(seed<<8)|NRF_RNG->VALUE;bytes++;NRF_RNG->EVENTS_VALRDY=0;}
  }
  NRF_RNG->TASKS_STOP=1;
  bootId_=seed^NRF_FICR->DEVICEID[0]^micros();if(!bootId_)bootId_=1;
  setRole(host);
}
void EsbTransport::suspend(){
  NVIC_DisableIRQ(RADIO_IRQn);
#if WD_V3_FIX
  if(NRF_RADIO->STATE!=RADIO_STATE_STATE_Disabled)
#endif
  NRF_RADIO->TASKS_DISABLE=1;
  waitDisabled(100);NRF_RADIO->EVENTS_END=0;phase_=Idle;
}
void EsbTransport::setRole(bool host){
  suspend();host_=host;localOwner_=peerLocal_=false;
  pendingSeq_=0;pendingLength_=completedLength_=0;lastAckSize_=0;
  lastPeerSeq_=0;peerBoot_=0;lastRx_=lastPoll_=0;
  rx_.clear();tx_.clear();configure();if(!host_)receiveStart();
  // Preserve txSeq_ and bootId_: a radio restart is NOT a new radio identity.
}
void EsbTransport::flush(){
  // Never discard an in-flight DMA buffer while the radio owns it.
  suspend();tx_.clear();pendingSeq_=0;
  pendingLength_=completedLength_=0;
  // Keep cached ACK/duplicate identity until the peer advances. Cancelling
  // the application stream must not cause retransmitted bytes to be executed.
  if(!host_)receiveStart();
}
void EsbTransport::reset(){
  flush();rx_.clear();lastRx_=0;peerLocal_=false;
}
bool EsbTransport::connected()const{return lastRx_ && uint32_t(millis()-lastRx_)<1500;}
int EsbTransport::read(){return rx_.size()?rx_.pop():-1;}
void EsbTransport::startExchange(){
  NVIC_DisableIRQ(RADIO_IRQn);
#if WD_V3_FIX
  if(NRF_RADIO->STATE!=RADIO_STATE_STATE_Disabled)
#endif
  NRF_RADIO->TASKS_DISABLE=1;
  if(!waitDisabled(100)){retry();return;}
  header(radioTx_,false,bootId_,pendingSeq_,peerBoot_,lastPeerSeq_,pendingLength_);
  if(pendingLength_)memcpy(radioTx_+16,pendingData_,pendingLength_);
  NRF_RADIO->PACKETPTR=(uintptr_t)radioTx_;
  NRF_RADIO->EVENTS_END=NRF_RADIO->EVENTS_DISABLED=0;
  phase_=HostTx;phaseAt_=radioNow();
  NVIC_ClearPendingIRQ(RADIO_IRQn);NVIC_EnableIRQ(RADIO_IRQn);
  NRF_RADIO->TASKS_TXEN=1;stats_.tx++;
}
void EsbTransport::retry(){
  NVIC_DisableIRQ(RADIO_IRQn);
#if WD_V3_FIX
  if(NRF_RADIO->STATE!=RADIO_STATE_STATE_Disabled)
#endif
  NRF_RADIO->TASKS_DISABLE=1;
  waitDisabled(100);
  stats_.retries++;phase_=Backoff;
  // Break repeated collisions without occupying the CPU between attempts.
  retryAt_=(radioNow()+200+((stats_.retries*137+bootId_)&511))&RadioTimeMask;
}
bool EsbTransport::acceptReply(){
  if(!radioPacketValid(radioRx_) || read32(radioRx_+9)!=bootId_ || get16(radioRx_+13)!=pendingSeq_)return false;
  const bool local=get16(radioRx_+1)==0xe553;
  uint32_t boot=read32(radioRx_+3);uint16_t seq=get16(radioRx_+7);uint8_t n=radioRx_[15];
  if(!local && (boot!=peerBoot_ || seq!=lastPeerSeq_)){
    if(rx_.free()<n){stats_.backpressure++;return false;}
    for(unsigned i=0;i<n;i++)rx_.push(radioRx_[16+i]);
    peerBoot_=boot;lastPeerSeq_=seq;stats_.received++;
  }
  peerLocal_=local;lastRx_=millis();stats_.rssi=NRF_RADIO->RSSISAMPLE;
  completedLength_=local?0:pendingLength_;
  pendingSeq_=0;phase_=Idle;lastPoll_=radioNow();
  if(local){pendingLength_=0;rx_.clear();}
  return true;
}
void EsbTransport::targetService(){
  if(phase_==TargetTx){
    if(phase_==TargetTx && radioElapsed(radioNow(),phaseAt_)>RadioTxTimeoutUs){stats_.timeouts++;phase_=Idle;receiveStart();}
    return;
  }
  if(!NRF_RADIO->EVENTS_END)return;
  NRF_RADIO->EVENTS_END=0;
  if(NRF_RADIO->CRCSTATUS!=1){stats_.crcErrors++;receiveStart();return;}
  if(!radioPacketValid(radioRx_) || get16(radioRx_+1)!=0xe552){receiveStart();return;}
  const uint32_t boot=read32(radioRx_+3);const uint16_t seq=get16(radioRx_+7);
  const uint8_t count=radioRx_[15];
  if(!boot || !seq){receiveStart();return;}
  if(localOwner_){
    header(lastAck_,true,bootId_,nextSeq(),boot,seq,0);lastAckSize_=16;
  } else if(boot!=peerBoot_ || seq!=lastPeerSeq_){
    if(rx_.free()<count){stats_.backpressure++;receiveStart();return;}
    for(unsigned i=0;i<count;i++)rx_.push(radioRx_[16+i]);
    peerBoot_=boot;lastPeerSeq_=seq;stats_.received++;
    uint8_t out=0;while(out<RadioPayload && tx_.size())lastAck_[16+out++]=tx_.pop();
    header(lastAck_,false,bootId_,nextSeq(),boot,seq,out);lastAckSize_=16+out;
  } else stats_.duplicates++;
  if(!lastAckSize_ || !waitDisabled(100)){receiveStart();return;}
  NVIC_DisableIRQ(RADIO_IRQn);
  memcpy(radioTx_,lastAck_,lastAckSize_);
  NRF_RADIO->PACKETPTR=(uintptr_t)radioTx_;
  NRF_RADIO->EVENTS_END=NRF_RADIO->EVENTS_DISABLED=0;
  phase_=TargetTx;phaseAt_=radioNow();
  NVIC_ClearPendingIRQ(RADIO_IRQn);NVIC_EnableIRQ(RADIO_IRQn);
  NRF_RADIO->TASKS_TXEN=1;
  lastRx_=millis();stats_.rssi=NRF_RADIO->RSSISAMPLE;stats_.tx++;
}
void EsbTransport::poll(bool dapBusy,bool outboundPending){
  const uint32_t mask=CLOCK_HFCLKSTAT_STATE_Msk|CLOCK_HFCLKSTAT_SRC_Msk;
  if((NRF_CLOCK->HFCLKSTAT&mask)!=mask){
    suspend();configure();stats_.clockRestarts++;
    if(!host_)receiveStart();
    else if(pendingSeq_)retry();
  }
  if(!host_){
    targetService();
    // Recover an unexpectedly disabled receiver even with no peer traffic.
    if(phase_==Idle && !NRF_RADIO->EVENTS_END && NRF_RADIO->STATE==RADIO_STATE_STATE_Disabled)
      receiveStart();
    return;
  }
  if(phase_==HostTx){
    if(phase_==HostTx && radioElapsed(radioNow(),phaseAt_)>RadioTxTimeoutUs){stats_.timeouts++;retry();}
    return;
  }
  if(phase_==HostRx){
    if(NRF_RADIO->EVENTS_END){
      if(NRF_RADIO->CRCSTATUS==1){if(acceptReply())return;}
      else stats_.crcErrors++;
      receiveStart(); // A corrupt/foreign packet does not consume the ACK window.
    }
    if(radioElapsed(radioNow(),phaseAt_)>RadioAckTimeoutUs){stats_.timeouts++;retry();}
    return;
  }
  if(phase_==Backoff){if(radioDue(radioNow(),retryAt_))startExchange();return;}
  // write() owns the completed fragment until the caller advances its offset.
  if(completedLength_)return;
  if(outboundPending && connected())return;
#ifndef WD_DAP_POLL_INTERVAL_US
#define WD_DAP_POLL_INTERVAL_US (WD_RADIO_MBPS == 2 ? 80U : 150U)
#endif
  if(radioElapsed(radioNow(),lastPoll_)>=(dapBusy?WD_DAP_POLL_INTERVAL_US:(connected()?500U:100U))){
    pendingSeq_=nextSeq();pendingLength_=0;startExchange();
  }
}
size_t EsbTransport::write(const uint8_t* data,size_t length){
  if(!length)return 0;
  if(!host_){size_t n=0;while(n<length && tx_.free())tx_.push(data[n++]);return n;}
  if(completedLength_){
    const uint8_t n=completedLength_;
    if(length<n || memcmp(data,pendingData_,n))return 0;
    completedLength_=pendingLength_=0;return n;
  }
  if(phase_!=Idle || pendingSeq_)return 0;
  pendingLength_=length>RadioPayload?RadioPayload:(uint8_t)length;
  memcpy(pendingData_,data,pendingLength_);pendingSeq_=nextSeq();startExchange();return 0;
}
