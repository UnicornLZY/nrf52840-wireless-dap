#pragma once
#include <Arduino.h>
#include "Protocol.h"
#include "RadioPolicy.h"

// Stop-and-wait ESB-style link. poll() advances hardware events; no RF wait
// loop may block USB/UART/SWD for an ACK or a batch of retries.
class EsbTransport {
public:
  struct Stats {
    uint32_t tx=0, retries=0, received=0, duplicates=0;
    uint32_t crcErrors=0, timeouts=0, backpressure=0, clockRestarts=0;
    uint8_t rssi=0;
  };
  void onRadioDisabled();
  void begin(bool host);
  void setRole(bool host);
  void poll(bool dapBusy=false,bool outboundPending=false);
  bool connected() const;
  void reset();
  void suspend();
  void setLocalOwner(bool local){localOwner_=local;}
  bool peerLocal() const{return peerLocal_ && connected();}
  int available() const{return (int)rx_.size();}
  int read();
  size_t write(const uint8_t* data,size_t length);
  void flush();
  uint16_t mtu() const{return wd::RadioPayload;}
  uint32_t bootId() const{return bootId_;}
  const Stats& stats() const{return stats_;}
  void setPairCode(uint32_t code){pairCode_=code;}
  void setChannel(uint8_t ch){channel_=ch;updateHopChannels();}
  void setTxPower(int8_t power){txPower_=power;}
  uint32_t pairCode() const{return pairCode_;}
  uint8_t channel() const{return channel_;}
  int8_t txPower() const{return txPower_;}
  uint8_t currentChannel() const{return channel_;}
  uint8_t currentHopIndex() const{return currentHopIdx_;}
  uint32_t hopCount() const{return hopCount_;}
  const uint8_t* hopChannels() const{return hopChannels_;}
  bool afhEnabled() const{return afhEnabled_;}
  void setAfhEnabled(bool en){afhEnabled_=en;}
  void advanceHopChannel();
private:
  uint32_t pairCode_=123456;
  uint8_t channel_=WD_RADIO_CHANNEL;
  int8_t txPower_=8;
  bool afhEnabled_=true;
  uint8_t hopChannels_[4]={WD_RADIO_CHANNEL,75,50,25};
  uint8_t currentHopIdx_=0;
  uint32_t consecutiveRetries_=0;
  uint32_t lastTargetHopAt_=0;
  uint32_t hopCount_=0;
  void updateHopChannels();
  enum Phase:uint8_t{Idle,HostTx,HostRx,Backoff,TargetTx};
  volatile Phase phase_=Idle;
  bool host_=false,localOwner_=false,peerLocal_=false;
  uint32_t bootId_=1,peerBoot_=0,retryAt_=0;
  volatile uint32_t phaseAt_=0;
  uint16_t txSeq_=0,lastPeerSeq_=0,pendingSeq_=0;
  uint8_t pendingLength_=0,completedLength_=0;
  uint8_t pendingData_[wd::RadioPayload]={};
  uint32_t lastRx_=0,lastPoll_=0;
  wd::Ring<4096> rx_,tx_;
  alignas(4) uint8_t radioTx_[256]={},radioRx_[256]={};
  uint8_t lastAck_[256]={};
  uint16_t lastAckSize_=0;
  Stats stats_;
  void configure();
  void receiveStart();
  void startExchange();
  void retry();
  void targetService();
  bool acceptReply();
  bool waitDisabled(uint32_t timeoutUs);
  uint16_t nextSeq(){if(++txSeq_==0)++txSeq_;return txSeq_;}
};
inline void EsbTransport::updateHopChannels(){
  uint8_t base = channel_ > 80 ? 80 : channel_;
  hopChannels_[0] = base;
  hopChannels_[1] = (base + 20) % 81;
  hopChannels_[2] = (base + 41) % 81;
  hopChannels_[3] = (base + 61) % 81;
}

inline void EsbTransport::advanceHopChannel(){
  if(!afhEnabled_)return;
  currentHopIdx_=(currentHopIdx_+1)&3;
  channel_=hopChannels_[currentHopIdx_];
#if !defined(HOST_TEST)
  NRF_RADIO->FREQUENCY=channel_;
#endif
  hopCount_++;
}
