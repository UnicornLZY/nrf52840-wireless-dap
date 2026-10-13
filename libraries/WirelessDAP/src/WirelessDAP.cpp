#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#ifndef WD_ESB
#define WD_ESB 0
#endif
#if WD_ESB
#include "EsbTransport.h"
#else
#include <bluefruit.h>
#endif
#include "WirelessDAP.h"
#include "BoardIdentity.h"
#include "Protocol.h"
#include "DapGuard.h"
#include "UsbDapResponse.h"
#include "DapWireLength.h"
#include "SwdClockPolicy.h"
#ifndef WD_RUN_KICK
#define WD_RUN_KICK 0
#endif
#ifndef WD_COMPACT_WIRE_TEST
#define WD_COMPACT_WIRE_TEST 0
#endif
#ifndef WD_ONE_ROW_WIRING_TEST
#define WD_ONE_ROW_WIRING_TEST 0
#endif
#ifndef WD_ONE_ROW_OUTER_RESET_TEST
#define WD_ONE_ROW_OUTER_RESET_TEST 0
#endif
#ifndef WD_ONE_ROW_RESET_P113_TEST
#define WD_ONE_ROW_RESET_P113_TEST 0
#endif
#if WD_RUN_KICK
#include "DapRunKick.h"
#endif
#if WD_PROFILE
#include "DapTiming.h"
#endif
#if WD_DAP128
#include "UsbDapAssembler.h"
#endif
#include "UartSettings.h"
#include "SessionRecovery.h"
// Adafruit WebUSB supplies the WinUSB compatible descriptor. Replace its
// generic WebUSB interface GUID with Arm's standardized CMSIS-DAP v2 GUID so
// debuggers (including Keil) enumerate the interface.
extern uint8_t desc_ms_os_20[];
#include "DAP_config.h"
#include "arm/DAP.h"
#include "SoftReset.h"
#include "ResetRun.h"

// One image runs either radio role. USB VBUS selects the polling side;
// without VBUS the board listens and serves the remote target.
static bool hostMode=false;
static bool usbEverMounted=false;
static uint32_t lastUsbMountedAt=0;

using namespace wd;
static constexpr uint16_t PairMarker=0xda42; // different from the old CAR firmware
static constexpr uint32_t Baud=115200;
static constexpr uint32_t DapTimeout=5000;
static constexpr uint8_t DapWindow=4;
#ifndef WD_SPEED_TEST
#define WD_SPEED_TEST 0
#endif
#ifndef WD_WIRELESS_SPEED_TEST
#define WD_WIRELESS_SPEED_TEST 0
#endif
#ifndef WD_WIRED_SLOW_TEST
#define WD_WIRED_SLOW_TEST 0
#endif
#ifndef WD_WIRED_SERIAL_TEST
#define WD_WIRED_SERIAL_TEST 0
#endif
#ifndef WD_DAP_TRACE_CAPACITY
#define WD_DAP_TRACE_CAPACITY 128
#endif
#if WD_DAP_TRACE_CAPACITY < 1 || WD_DAP_TRACE_CAPACITY > 255
#error "DAP trace capacity must fit the one-byte trace protocol"
#endif
static constexpr uint32_t SwdClockMax=swdClockLimit(WD_SPEED_TEST!=0);
static constexpr uint32_t LocalSwdClockMax=WD_WIRED_SLOW_TEST ?
  wiredSwdClockLimit(true) : (WD_WIRELESS_SPEED_TEST ?
  localSwdClockLimit(true) : SwdClockMax);
#ifndef WD_WIRELESS_SWD_HZ
#define WD_WIRELESS_SWD_HZ 0
#endif
static_assert(WD_WIRELESS_SWD_HZ==0 || WD_WIRELESS_SWD_HZ==8000000,
              "Supported independent wireless experiment: 8 MHz");
static constexpr uint32_t WirelessSwdClockMax=WD_WIRELESS_SWD_HZ ?
  WD_WIRELESS_SWD_HZ : WD_WIRELESS_SPEED_TEST ?
  wirelessSwdClockLimit(true) : SwdClockMax;
// Keep USB bursts queued while the SWD worker drains its bounded queue.
// Both dimensions are powers of two because the packet ring uses a mask.
static constexpr uint8_t UsbPacketCount=64;
static constexpr uint8_t UsbRequestQueueDepth=64;
static volatile uint16_t conn=0xffff;
static volatile uint32_t generation=0;
static volatile bool ready=false;
static volatile uint32_t session=0;
static uint32_t seenGeneration=0,startedAt=0,lastPeer=0,lastHello=0,lastPing=0;
static bool wasConnected=false;
static uint8_t tuning=0;
static Parser parser;
static QueueHandle_t txQueue;
static Ring<4096> uartToPeer,uartToLocal;
// USB monitoring has its own best-effort queue: a closed/slow COM port must
// never stall the physical UART used by an independently powered vehicle.
static Ring<4096> uartToUsb;
static UartSettings uartApplied,uartDesired;
static bool uartConfigAcked=false,uartConfigPending=false;
static uint16_t uartConfigSeq=1;
static uint32_t uartConfigSentAt=0,uartRejectedSettings=0;
static UartSettings uartLastRejected;
static bool uartHasRejected=false;
static void resetLinkState();
// Retry a transient loss promptly; sustained receiver backpressure backs off
// instead of flooding the shared DAP/radio queues. All intervals use millis.
static constexpr uint32_t UartRetryFirstMs=20, UartRetryMaxMs=160;
struct UartSlot {Frame frame;uint32_t sentAt,retryMs;bool sent,acked;};
static void markUartSent(UartSlot& s,uint32_t now){
  s.retryMs=s.sent?(s.retryMs<UartRetryMaxMs?s.retryMs*2:UartRetryMaxMs):UartRetryFirstMs;
  s.sent=true;s.sentAt=now;
}
static UartSlot uartPending[4];
static uint8_t uartHead=0,uartCount=0;
static uint16_t uartSeq=0,lastUartSeq=0;
static uint32_t lastUartBatch=0;
// MTU 247 leaves 244 ATT payload bytes. A 64-byte DAP frame normally encodes
// to about 76 bytes, so keep room to aggregate three frames per BLE packet.
static uint8_t txWire[WireMax*3];
static size_t txLength=0,txOffset=0;
static uint32_t txProgress=0;
static uint32_t lastUartTransmit=0;
static Frame incoming;
static uint32_t invalidFrames=0;
static constexpr uint32_t LocalUsbSession=0xffffffffUL;
static Adafruit_USBD_WebUSB dapUsb;
static QueueHandle_t usbRequests;
struct UsbPacket {uint8_t data[DapPacketSize];};
static UsbPacket usbPackets[UsbPacketCount];
#if WD_DAP128
static UsbDapAssembler usbAssembler;
#endif
static volatile uint8_t usbPacketHead=0,usbPacketTail=0;
static volatile bool usbOverflow=false;
static volatile uint32_t usbRingOverflowCount=0;
static volatile uint32_t usbQueueBackpressureCount=0;
static char serialNumber[17];

// UART idles high. Keep the shared physical RX input from floating when the
// USB CDC endpoint is used without a device attached to the board's UART pins;
// otherwise noise on B's unused RX pin can be injected into the USB data stream.
static void configureUartRxPullup(){
#if (WD_ONE_ROW_WIRING_TEST || WD_ONE_ROW_OUTER_RESET_TEST || WD_ONE_ROW_RESET_P113_TEST) && defined(NRF_P1) && defined(GPIO_PIN_CNF_PULL_Msk)
#if WD_ONE_ROW_OUTER_RESET_TEST || WD_ONE_ROW_RESET_P113_TEST
  uint32_t cnf=NRF_P1->PIN_CNF[15]; // Arduino pin 47 = P1.15 (UART RX)
#else
  uint32_t cnf=NRF_P1->PIN_CNF[13]; // Arduino pin 45 = P1.13 (UART RX)
#endif
  cnf&=~GPIO_PIN_CNF_PULL_Msk;
  cnf|=GPIO_PIN_CNF_PULL_Pullup<<GPIO_PIN_CNF_PULL_Pos;
#if WD_ONE_ROW_OUTER_RESET_TEST || WD_ONE_ROW_RESET_P113_TEST
  NRF_P1->PIN_CNF[15]=cnf;
#else
  NRF_P1->PIN_CNF[13]=cnf;
#endif
#elif defined(NRF_P0) && defined(PIN_SERIAL2_RX) && defined(GPIO_PIN_CNF_PULL_Msk)
  uint32_t cnf=NRF_P0->PIN_CNF[PIN_SERIAL2_RX];
  cnf&=~GPIO_PIN_CNF_PULL_Msk;
  cnf|=GPIO_PIN_CNF_PULL_Pullup<<GPIO_PIN_CNF_PULL_Pos;
  NRF_P0->PIN_CNF[PIN_SERIAL2_RX]=cnf;
#endif
}

#if WD_ESB
// The stock core treats closing a 1200-baud COM port as a bootloader request.
// A serial bridge must not reboot on normal line-state changes. The ESB build
// wraps this callback locally; double-tap reset still enters the bootloader.
extern "C" void __wrap_tud_cdc_line_state_cb(uint8_t,bool,bool) {}
#endif

// Preserve every completed USB Bulk OUT transfer. Short CMSIS-DAP commands
// are zero-padded for the fixed 64-byte command parser.
extern "C" void tud_vendor_rx_cb(uint8_t,const uint8_t* buffer,uint16_t size) {
#if WD_DAP128
  if(size>64){usbOverflow=true;usbRingOverflowCount++;tud_vendor_read_flush();usbAssembler.reset();return;}
  uint8_t assembled[128];
  if(!usbAssembler.feed(buffer,size,assembled)){tud_vendor_read_flush();return;}
  uint8_t head=usbPacketHead,next=(head+1)&(UsbPacketCount-1);
  if(next==usbPacketTail){usbOverflow=true;usbRingOverflowCount++;tud_vendor_read_flush();return;}
  memcpy(usbPackets[head].data,assembled,128);
  usbPacketHead=next;tud_vendor_read_flush();
#else
  if(!size)return;
  uint8_t head=usbPacketHead,next=(head+1)&(UsbPacketCount-1);
  if(next==usbPacketTail || size>64){usbOverflow=true;usbRingOverflowCount++;tud_vendor_read_flush();return;}
  memset(usbPackets[head].data,0,DapPacketSize);memcpy(usbPackets[head].data,buffer,size);
  usbPacketHead=next;tud_vendor_read_flush();
#endif
}

#if WD_USB_COMPAT
#include "UsbDapTxState.h"
static UsbDapTxState usbDapTx;
extern "C" void tud_vendor_tx_cb(uint8_t,uint32_t sent){usbDapTx.complete(sent);}
#endif
struct DapUsbEndpoint {
  bool mounted()const{return tud_vendor_mounted();}
  size_t available()const{
#if WD_USB_COMPAT
    if(usbDapTx.isBusy())return 0;
#endif
    return tud_vendor_n_write_available(0);
  }
  size_t write(const uint8_t* p,size_t n){
#if WD_USB_COMPAT
    usbDapTx.begin(n);
#endif
    size_t written=tud_vendor_write(p,n);
#if WD_USB_COMPAT
    if(written!=n)usbDapTx.clear();
#endif
    return written;
  }
  void flush(){tud_vendor_flush();}
};
static DapUsbEndpoint dapEndpoint;
static EsbTransport transport;
struct DapSlot {uint8_t request[DapPacketSize],response[DapPacketSize];uint16_t seq;uint32_t sentAt;uint8_t responseLen;bool remote,ready;};
static DapSlot dapSlots[DapWindow];
static uint8_t dapHead=0,dapCount=0;
static bool debugConnected=false;
// Read-only vendor diagnostic 0x80: snapshot the latest USB commands and
// replies without issuing any target access/reset. Each record is 60 bytes.
static constexpr uint16_t DapTraceCapacity=WD_DAP_TRACE_CAPACITY;
static uint8_t dapTrace[DapTraceCapacity][60]={};
static uint16_t traceHead=0,traceCount=0;
#include "PacketStats.h"
static PacketStats packetStats;
static void recordDap(uint8_t kind,const uint8_t* data,size_t length){
  if(!kind && length)packetStats.request(data);
  // Omit pure pin queries and unchanged pin-state replies. Preserve all pin
  // writes and state transitions so long polls cannot erase reset commands.
  if(kind==0 && data[0]==0x10 && data[2]==0)return;
  if(kind==1 && data[0]==0x10 && length==2){
    static uint16_t lastPins=0x100;
    if(lastPins==data[1])return;
    lastPins=data[1];
  }
  uint8_t* p=dapTrace[traceHead];memset(p,0,60);
  write32(p,millis());p[4]=kind;p[5]=length;
  memcpy(p+6,data,length>54?54:length);
  traceHead=(traceHead+1)%DapTraceCapacity;
  if(traceCount<DapTraceCapacity)traceCount++;
}
static bool usbHidden=false;
static uint16_t dapSeq=0;
static uint32_t sessionCounter=0;
static unsigned hostCancelledRequests=0;
static volatile bool abortRequested=false;
static QueueHandle_t dapJobs,dapResults;
static volatile bool dapRunning=false;
static volatile uint32_t dapStartedAt=0;
static uint16_t acceptedDapSeq=0;
static uint16_t localUsbSeq=0;
static volatile bool localMode=false;
static volatile bool localDapConnected=false;
static BoardPreference preferredRole=AutoRole;
static uint32_t startupResetReason=0;
// Once this power session has served a wireless host, a momentary RF expiry
// during USB insertion must not promote the target into a second PTX host.
static bool targetPeerSeen=false;
#if WD_LINK_RECOVERY_TEST
// A paired host must keep polling after RF loss, rather than access its own
// unconnected SWD pins when Keil retries a failed wireless command.
static bool wirelessHostSeen=false;
#endif
#if WD_SESSION_RESTART_TEST
static bool hostRestartRequested=false,hostUsbDetached=false;
static uint32_t hostRestartAt=0,hostLastHealthy=0,hostRestartCount=0;
static bool hostUsbRemountGrace=false;
static bool hostRecoveryEpisode=false;
static bool hostUsbRecoveryPending=false;
static uint32_t hostUsbRecoveryIdleAt=0;
#endif
static bool localLeaseForDap=false;
static uint32_t lastLocalDapAt=0;
static volatile uint32_t dapEpoch=0;
static volatile bool overflow=false;

// Read-only vendor status for diagnosing role and link recovery. It never
// touches the target SWD pins or changes a session.
static void writeUnifiedStatus(uint8_t* r){
  memset(r,0,DapPacketSize);
  r[0]=0x82;r[1]=3;
  r[2]=hostMode;r[3]=localMode;r[4]=ready;
  r[5]=transport.connected();r[6]=transport.peerLocal();r[7]=usbHidden;
  r[8]=usbEverMounted;
  r[9]=(NRF_POWER->USBREGSTATUS&POWER_USBREGSTATUS_VBUSDETECT_Msk)!=0;
  write32(r+10,generation);
  r[14]=preferredRole;r[15]=targetPeerSeen;
  write32(r+16,startupResetReason);
  write32(r+20,usbRingOverflowCount);
  write32(r+24,usbQueueBackpressureCount);
  const auto& rf=transport.stats();
  write32(r+28,rf.tx);write32(r+32,rf.retries);write32(r+36,rf.received);
  write32(r+40,rf.crcErrors);write32(r+44,rf.timeouts);write32(r+48,rf.backpressure);
  write32(r+52,rf.clockRestarts);r[56]=rf.rssi;r[57]=WD_RADIO_MBPS;r[58]=WD_RADIO_CHANNEL;
  write32(r+60,transport.bootId());
}

#if WD_ESB
static bool connected(){return transport.connected();}
static void disconnectLink(){
#if WD_SESSION_RESTART_TEST
  if(hostMode && !localMode && dapCount && wirelessHostSeen){
    hostUsbRecoveryPending=true;hostUsbRecoveryIdleAt=millis();
  }
#endif
  transport.reset();ready=false;generation++;
}
#else
static bool connected() {return conn!=BLE_CONN_HANDLE_INVALID && Bluefruit.connected(conn);}
static void disconnectLink() {if(connected()) Bluefruit.disconnect(conn);ready=false;}
#endif
#if WD_SESSION_RESTART_TEST
static void requestHostRestart(){
  if(hostMode && !localMode && !usbHidden && wirelessHostSeen)
    hostRestartRequested=true;
}
#endif
static bool queueFrame(uint8_t type,uint16_t seq,const uint8_t* data=nullptr,size_t n=0) {
  if(n>PayloadMax) return false;
  Frame f={};f.type=type;f.session=session;f.seq=seq;f.len=n;
  if(n) memcpy(f.data,data,n);
  if(xQueueSend(txQueue,&f,0)!=pdTRUE) {disconnectLink();return false;}
  return true;
}
#if !WD_ESB
static void onDisconnect(uint16_t,uint8_t) {ready=false;conn=BLE_CONN_HANDLE_INVALID;generation++;}
#endif
#if !WD_ESB
static void onScan(ble_gap_evt_adv_report_t* report) {
  if(Bluefruit.Scanner.checkReportForService(report,transport)) {
    if(!Bluefruit.Central.connect(report)) Bluefruit.Scanner.resume();
  } else Bluefruit.Scanner.resume();
}
static void onConnect(uint16_t h) {
  if(!transport.discover(h) || !transport.enableTXD()){Bluefruit.disconnect(h);return;}
  conn=h;generation++;
}
#endif
static uint8_t localInfo(const uint8_t* q,uint8_t* r) {
  memset(r,0,DapPacketSize); const char* text=nullptr;
  switch(q[1]) {
    case 1:text="SuperMini DIY";break;
    case 2:text=WD_ESB?"Wireless ESB CMSIS-DAP":"Wireless CMSIS-DAP";break;
    case 3:text=serialNumber;break;
    case 4:text="2.1.0";break;
    case 9:text=WD_ESB?"3.1.0-PACKET":"0.6.0";break;
    case 0xf0:r[1]=1;r[2]=1;break; // SWD only; no atomic commands
    // Wired USB bypasses the RF bridge's natural request pacing. Advertise a
    // single outstanding DAP packet locally to keep the target algorithm and
    // SWD worker synchronized; wireless retains the normal four-packet window.
    case 0xfe:r[1]=1;r[2]=(WD_WIRED_SERIAL_TEST && localMode)?1:DapWindow;break;
    case 0xff:r[1]=2;r[2]=DapPacketSize;break;
  }
  if(text){r[1]=strlen(text)+1;memcpy(r+2,text,r[1]);}
  return uint8_t(2+r[1]);
}
#if !WD_ESB
static void onConnect(uint16_t h){conn=h;generation++;}
static void onOverflow(uint16_t,uint16_t){overflow=true;}
#endif
// Only the DAP worker owns this state, including session invalidation.
static volatile bool roleProbePending=false,roleProbeFound=false;
static bool roleProbeStarted=false,roleProbeUsbWasPresent=false,roleProbeApply=false;
// SWD-only discovery: line reset, JTAG-to-SWD selection, then read DP IDCODE.
// No target memory write, halt or reset. The worker exclusively owns pins.
static bool detectLocalSwdTarget(){
  DAP_TransferAbort=0;DAP_Setup();uint8_t response[DapPacketSize]={};
  const uint8_t connect[]={2,1};DAP_ProcessCommand(connect,response);
  uint8_t clock[]={0x11,0xa0,0x86,0x01,0}; // 100 kHz
  DAP_ProcessCommand(clock,response);
  uint8_t sequence[]={0x12,136,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x9e,0xe7,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00};
  DAP_ProcessCommand(sequence,response);
  const uint8_t readId[]={5,0,1,2};
  memset(response,0,sizeof(response));
  uint32_t n=DAP_ProcessCommand(readId,response)&0xffff;
  uint32_t id=read32(response+3);
  bool found=n>=7 && response[1]==1 && response[2]==1 &&
    id!=0xffffffffUL && id!=0 && (id&1);
  PORT_OFF();DAP_Setup();
  return found;
}
static void dapWorker(void*) {
  ResetRun resetRun;
#if WD_RUN_KICK
  DapRunKick runKick;
#endif
#if WD_PROFILE
  DapTiming timing;
#endif
  Frame job={},result={};uint32_t initializedSession=0,initializedEpoch=0;
  DAP_Setup();
  for(;;){
    if(xQueueReceive(dapJobs,&job,pdMS_TO_TICKS(5))!=pdTRUE)continue;
    if(job.type==0xfe){
      roleProbeFound=detectLocalSwdTarget();
      initializedSession=initializedEpoch=0;
      roleProbePending=false;
      continue;
    }
    bool local=job.session==LocalUsbSession;
    uint32_t epoch=job.epoch;
    if(epoch!=dapEpoch)continue;
    if(local!=localMode)continue;
    if(!local && (!ready || job.session!=session))continue;
    if(initializedSession!=job.session || initializedEpoch!=epoch){
      resetRun.clear();DAP_Setup();initializedSession=job.session;initializedEpoch=epoch;
      DAP_Data.debug_port=DAP_PORT_SWD;
      PORT_SWD_SETUP();

#if WD_RUN_KICK
      runKick.clear();
#endif
      {
        // Match the proven wired baseline even when the debugger does not
        // send an explicit SWJ_Clock.
        uint8_t clockRequest[5]={0x11},clockResponse[DapPacketSize]={};
        write32(clockRequest+1,local?LocalSwdClockMax:WirelessSwdClockMax);
        DAP_ProcessCommand(clockRequest,clockResponse);
      }
    }
    result={};result.type=DapResponse;result.session=job.session;result.seq=job.seq;result.epoch=epoch;result.len=2;
#if WD_PROFILE
    if(local && job.data[0]==0x81){
      timing.encode(result.data);result.len=64;
      if(job.data[1]==1)timing=DapTiming{};
    }
    else
#endif
    if(local && job.data[0]==0x80){
      result.data[0]=0x80;result.data[1]=1;
      result.data[2]=traceCount;result.data[3]=job.data[1];
      if(job.data[1]<traceCount)
        memcpy(result.data+4,dapTrace[(traceHead+DapTraceCapacity-traceCount+job.data[1])%DapTraceCapacity],60);
      result.len=DapPacketSize;
    }
    else if(job.data[0]==0){result.len=localInfo(job.data,result.data);}
    else if(job.data[0]==0x83){packetStats.encode(result.data,DapPacketSize);result.len=64;}
    else if(job.data[0]==0x82){
      writeUnifiedStatus(result.data);result.len=64;
    }
    else if(!validDap(job.data)){
      dapError(job.data[0],result.data);
      if(job.data[0]==0x05)result.len=3;
      else if(job.data[0]==0x06)result.len=4;
    }
    else if(job.data[0]==0x0a){
      // Explicit reset command: execute SYSRESETREQ over SWD, without NRST.
      if(DAP_Data.debug_port!=DAP_PORT_SWD || (NRF_P0->PIN_CNF[WD_CLK]&1)==0){
        DAP_Data.debug_port=DAP_PORT_SWD;
        PORT_SWD_SETUP();
      }
      dapStartedAt=millis();dapRunning=true;
      bool ok=DAP_Data.debug_port==DAP_PORT_SWD && resetCortexMViaSwd(DAP_ProcessCommand);
      dapRunning=false;
      result.data[0]=0x0a;result.data[1]=ok?0:0xff;result.data[2]=ok?1:0;result.len=3;
    }
    else if(job.data[0]==9 && job.data[1]==0 && job.data[2]==0){
      result.data[0]=9; // upstream delay loop requires a nonzero iteration count
      result.len=1;
    } else {
      // Keep margin for the nRF52840 software SWD engine while still running
      // three times faster than the original stable 1 MHz implementation.
      uint32_t clockMax=local?LocalSwdClockMax:WirelessSwdClockMax;
      if(job.data[0]==0x11 && read32(job.data+1)>clockMax)write32(job.data+1,clockMax);
      // Keep WAIT/match loops bounded. Hosts can repeat a WAIT response.
      if(job.data[0]==4){
        uint16_t retry=job.data[2]|uint16_t(job.data[3])<<8;
        uint16_t match=job.data[4]|uint16_t(job.data[5])<<8;
        if(retry>10000){job.data[2]=0x10;job.data[3]=0x27;}
        if(match>10000){job.data[4]=0x10;job.data[5]=0x27;}
      }
      if(job.data[0]!=0 && job.data[0]!=3){
        if(DAP_Data.debug_port!=DAP_PORT_SWD || (NRF_P0->PIN_CNF[WD_CLK]&1)==0){
          DAP_Data.debug_port=DAP_PORT_SWD;
          PORT_SWD_SETUP();
        }
      }
      dapStartedAt=millis();dapRunning=true;
#if WD_PROFILE
      uint32_t commandStartedUs=micros();
#endif
      uint32_t n=DAP_ProcessCommand(job.data,result.data)&0xffff;
#if WD_RUN_KICK
      if(runKick.observe(job.data,result.data)){
        // Keil's RAM flash algorithm writes this exact idempotent DHCSR value
        // to resume the core. A second same-speed write recovers a lost SWD
        // data phase without lowering the configured SWD clock.
        uint8_t resumeRequest[DapPacketSize]={0x05,0x00,0x03,
          0x08,0x00,0x00,0x00,0x00,
          0x05,0xf0,0xed,0x00,0xe0,
          0x0d,0x01,0x00,0x5f,0xa0};
        uint8_t resumeResponse[DapPacketSize]={};
        DAP_ProcessCommand(resumeRequest,resumeResponse);
      }
#endif
#if WD_PROFILE
      uint32_t commandElapsedUs=micros()-commandStartedUs;
#endif
      dapRunning=false;
      if(n && n<=DapPacketSize)result.len=(uint8_t)n;
      else {dapError(job.data[0],result.data);result.len=dapErrorLength(job.data[0]);}
#if WD_PROFILE
      uint16_t wordsDone=0;
      if(job.data[0]==0x06 && result.len>=4 && result.data[3]==1)
        wordsDone=uint16_t(result.data[1])|uint16_t(result.data[2])<<8;
      timing.record(local,job.data[0],commandElapsedUs,wordsDone);
#endif
      // A's existing local USB identity remains a 64-byte CMSIS-DAP probe.
      // Keil may cache that size by VID/PID/serial. Only B advertises 128.
#if WD_DAP128
      if(local && job.data[0]==0 && job.data[1]==0xff)result.data[2]=DapPacketSize;
#endif
      if(job.data[0]==0 && job.data[1]==0xf0)result.data[2]&=~0x10;
    }
    if(epoch!=dapEpoch || local!=localMode || (!local && (!ready || result.session!=session))){resetRun.clear();PORT_OFF();continue;}
    if(resetRun.observe(job.data,result.data) && DAP_Data.debug_port==DAP_PORT_SWD){
      dapStartedAt=millis();dapRunning=true;
      // SWJ_Pins has no status field. Keep the actual pin reply; do not
      // fabricate success of the software reset from the released pin level.
      resetCortexMViaSwd(DAP_ProcessCommand);
      dapRunning=false;
    }
    if(local){
      if(job.data[0]==2 && result.len>=2 && result.data[1])localDapConnected=true;
      if(job.data[0]==3)localDapConnected=false;
    }
    bool queued=false;
    // A disconnected USB host must never leave this worker blocked forever
    // behind local replies, preventing subsequent wireless DAP jobs.
    while(epoch==dapEpoch && local==localMode){
      taskENTER_CRITICAL();
      if(epoch==dapEpoch)queued=xQueueSend(dapResults,&result,0)==pdTRUE;
      taskEXIT_CRITICAL();
      if(queued)break;
      vTaskDelay(1);
    }
    // Disconnect releases pins only. Reset is exclusively controlled by the
    // debugger's explicit reset commands, never by link changes/disconnect.
    taskYIELD();
  }
}
static void handleFrame(const Frame& f) {
  if(f.type==Hello){
if(!hostMode){
    if(!validHello(f))return;
    if(!ready || f.session!=session){
      resetLinkState();session=f.session;ready=true;
    }
    lastPeer=millis();uint8_t ver=LinkVersion;queueFrame(HelloAck,0,&ver,1);
}
    return;
  }
if(hostMode){
  if(resetRequestsOurSession(f,session)){
    // A lost its application session (USB ownership change/reboot). Restart
    // both layers instead of keeping the radio ACKs' stale "connected" state.
    disconnectLink();return;
  }
} else {
  if(needsSessionReset(f,ready,session)){
    uint8_t oldSession[4];write32(oldSession,f.session);
    queueFrame(SessionReset,0,oldSession,4);return;
  }
}
  if(f.session!=session)return;
if(hostMode){
  if(f.type==HelloAck && f.len==1 && f.data[0]==LinkVersion){
    ready=true;lastPeer=millis();
#if WD_LINK_RECOVERY_TEST
    wirelessHostSeen=true;
#endif
#if WD_SESSION_RESTART_TEST
    hostLastHealthy=millis();
    hostRecoveryEpisode=false;
#endif
    return;
  }
}
  if(!ready)return;
  lastPeer=millis();
#if WD_SESSION_RESTART_TEST
  if(hostMode)hostLastHealthy=millis();
#endif
  switch(f.type){
    case Ping:queueFrame(Pong,0);break;
    case Pong:break;
    case UartConfig:
if(!hostMode){
      if(f.len==7 && UartSettings::decode(f.data).valid()){
        uartDesired=UartSettings::decode(f.data);uartConfigSeq=f.seq;
        uartConfigPending=true;
      }
}
      break;
    case UartConfigAck:
if(hostMode){
      if(f.len==7 && f.seq==uartConfigSeq && UartSettings::decode(f.data)==uartDesired)
        uartConfigAcked=true;
}
      break;
    case UartAck:
      for(unsigned i=0;i<uartCount;i++){
        UartSlot& s=uartPending[(uartHead+i)%4];
        if(s.frame.seq==f.seq && s.sent)s.acked=true;
      }
      while(uartCount && uartPending[uartHead].acked){uartHead=(uartHead+1)%4;uartCount--;}
      break;
    case UartData:
      if(uint16_t(lastUartSeq-f.seq)<4){queueFrame(UartAck,f.seq);break;}
      if(f.seq!=uint16_t(lastUartSeq+1)){
        // An earlier window entry may be waiting for receiver buffer space.
        // Drop later entries without ACK; the sender retries them in order.
        // Never jump across a missing byte-stream fragment, even if a corrupt
        // or stale sequence lies outside the current four-frame window.
        break;

      }
      if(uartToLocal.free()<f.len)break; // withhold ACK: backpressure
      for(unsigned i=0;i<f.len;i++)uartToLocal.push(f.data[i]);
if(hostMode){
      if(tud_cdc_n_ready(0))for(unsigned i=0;i<f.len;i++)uartToUsb.push(f.data[i]);
}
      lastUartSeq=f.seq;queueFrame(UartAck,f.seq);break;
    case DapResponse:
      if(!hostMode)break;
      if(f.len && f.len<=DapPacketSize){
        for(unsigned i=0;i<dapCount;i++){
          DapSlot& s=dapSlots[(dapHead+i)%DapWindow];
          if(s.remote && !s.ready && s.seq==f.seq){
            if(!validDapResponse(s.request,f.data,f.len)){invalidFrames++;disconnectLink();return;}
            memset(s.response,0,DapPacketSize);memcpy(s.response,f.data,f.len);s.responseLen=(uint8_t)f.len;s.ready=true;
            if(s.request[0]==2)debugConnected=s.response[1]==1;
            if(s.request[0]==3)debugConnected=false;
            break;
          }
        }
      }
      break;
    case DapRequest:
      if(hostMode)break;
#if WD_COMPACT_WIRE_TEST
      if(f.len==0 || f.len>DapPacketSize || !f.seq){disconnectLink();break;}
#else
      if(f.len!=DapPacketSize || !f.seq){disconnectLink();break;}
#endif
      if(f.seq!=nextDapSequence(acceptedDapSeq)){disconnectLink();break;}
      acceptedDapSeq=f.seq;
#if WD_COMPACT_WIRE_TEST
      {
        Frame job=f;job.epoch=dapEpoch;
        if(f.len<DapPacketSize)memset(job.data+f.len,0,DapPacketSize-f.len);
        if(!validDap(job.data) || xQueueSend(dapJobs,&job,0)!=pdTRUE)disconnectLink();
      }
#else
      if(xQueueSend(dapJobs,&f,0)!=pdTRUE)disconnectLink();
#endif
      break;
    case Abort:if(!hostMode)DAP_TransferAbort=1;break;
    default:invalidFrames++;break;
  }
}

static void resetLinkState() {
  ready=false;parser.reset();transport.flush();xQueueReset(txQueue);
  txLength=txOffset=0;uartHead=uartCount=0;uartToPeer.clear();uartToLocal.clear();uartToUsb.clear();
  uartConfigAcked=false;uartConfigPending=false;uartConfigSentAt=0;
  uartSeq=lastUartSeq=0;lastUartBatch=lastUartTransmit=millis();
  lastPeer=startedAt=millis();lastHello=lastPing=0;tuning=0;
if(hostMode){
  // All requests already accepted before this boundary belong to the failed
  // session. Return errors in FIFO order instead of replaying them on a new A.
  hostCancelledRequests=uxQueueMessagesWaiting(usbRequests)+
    ((usbPacketHead-usbPacketTail)&(UsbPacketCount-1));
  debugConnected=false;
  for(unsigned i=0;i<dapCount;i++){
    DapSlot& s=dapSlots[(dapHead+i)%DapWindow];
    if(!s.ready){dapError(s.request[0],s.response);s.responseLen=dapErrorLength(s.request[0]);s.ready=true;}
  }
  session=transport.bootId()^millis()^(++sessionCounter*2654435761UL);
  if(!session || session==LocalUsbSession)session=1;
  dapSeq=0;
} else {
  dapEpoch++;DAP_TransferAbort=1;session=0;acceptedDapSeq=0;
  xQueueReset(dapJobs);xQueueReset(dapResults);overflow=false;
}
}
static void pumpTransmit() {
  if(!connected())return;
  if(txOffset==txLength){
    Frame f={};txOffset=txLength=0;
    if(!ready){
      // HELLO/HELLO_ACK are sent before the application session is ready.
      if(xQueueReceive(txQueue,&f,0)!=pdTRUE)return;
      txLength=encode(f,txWire);txProgress=millis();
    } else {
    // Coalesce the byte stream so a four-packet window is not consumed by
    // four tiny packets. Full packets go immediately; short bursts wait at
    // most one millisecond of batching (not an end-to-end latency guarantee).
    if((!hostMode || uartConfigAcked) && uartCount<4 && uartToPeer.size() &&
       (uartToPeer.size()>=PayloadMax || uint32_t(millis()-lastUartBatch)>=1)){
      UartSlot& s=uartPending[(uartHead+uartCount)%4];s={};
      s.frame.type=UartData;s.frame.session=session;s.frame.seq=++uartSeq;
      while(s.frame.len<PayloadMax && uartToPeer.size())s.frame.data[s.frame.len++]=uartToPeer.pop();
      uartCount++;lastUartBatch=millis();
    }
    UartSlot* uartNext=nullptr;
    for(unsigned i=0;i<uartCount;i++){
      UartSlot& s=uartPending[(uartHead+i)%4];
      if(!s.acked && (!s.sent || uint32_t(millis()-s.sentAt)>=s.retryMs)){uartNext=&s;break;}
    }
    // Reserve regular airtime for serial data even while DAP is continuously
    // queued. This fixes CDC appearing dead during a long flash operation.
    bool serveUart=uartNext && uint32_t(millis()-lastUartTransmit)>=8;
    if(serveUart){
      f=uartNext->frame;markUartSent(*uartNext,millis());lastUartTransmit=millis();
    } else if(xQueueReceive(txQueue,&f,0)!=pdTRUE) {
      if(!uartNext)return;
      f=uartNext->frame;markUartSent(*uartNext,millis());lastUartTransmit=millis();
    }
      txLength=encode(f,txWire);txProgress=millis();
    }
  }
#if WD_ESB
  size_t mtu=transport.mtu();
#else
  BLEConnection* c=Bluefruit.Connection(conn);if(!c)return;
  size_t mtu=c->getMtu()-3;
#endif
  // Fill one ATT payload with as many queued control/DAP frames as fit. The
  // receiver parser already accepts back-to-back framed messages.
  if(ready && txOffset==0 && txLength<mtu){
    Frame extra={};
    while(xQueueReceive(txQueue,&extra,0)==pdTRUE){
      uint8_t encoded[WireMax];size_t encodedLength=encode(extra,encoded);
      if(!encodedLength || txLength+encodedLength>mtu){
        xQueueSendToFront(txQueue,&extra,0);break;
      }
      memcpy(txWire+txLength,encoded,encodedLength);txLength+=encodedLength;
    }
  }
  size_t n=txLength-txOffset;if(n>mtu)n=mtu;
  // One ATT packet per call: BLEUart.notify() otherwise hides partial multi-packet sends.
  size_t sent=transport.write(txWire+txOffset,n);
  if(sent){txOffset+=sent;txProgress=millis();}
  if(txOffset<txLength && uint32_t(millis()-txProgress)>3000){
#if WD_SESSION_RESTART_TEST
    requestHostRestart();
#endif
    disconnectLink();
  }
}
static void serviceUart() {
if(hostMode && !localMode){
  // Fairly admit at most one USB packet and one UART packet per iteration.
  unsigned usbBudget=64;
  while(usbBudget-- && tud_cdc_n_available(0) &&
        (ready?uartToPeer.free():uartToLocal.free())){
    uint8_t b;if(tud_cdc_n_read(0,&b,1)!=1)break;
    if(ready)uartToPeer.push(b);else uartToLocal.push(b);
  }
  unsigned pinBudget=64;
  while(pinBudget-- && Serial2.available() &&
        (ready?uartToPeer.free():(!tud_cdc_n_ready(0) || uartToUsb.free()))){
    uint8_t b=Serial2.read();
    if(ready)uartToPeer.push(b);
    else if(tud_cdc_n_ready(0))uartToUsb.push(b);
  }
  if(!tud_cdc_n_ready(0))uartToUsb.clear();
  size_t room=tud_cdc_n_ready(0)?tud_cdc_n_write_available(0):0;
  while(room-- && uartToUsb.size()){
    uint8_t b=uartToUsb.peek();if(tud_cdc_n_write(0,&b,1)!=1)break;uartToUsb.pop();
  }
  tud_cdc_n_write_flush(0);
} else {
  if(localMode){
    while(tud_cdc_n_available(0) && uartToLocal.free()){
      uint8_t b;if(tud_cdc_n_read(0,&b,1)!=1)break;uartToLocal.push(b);
    }
    if(!tud_cdc_n_ready(0))uartToPeer.clear();
    while(Serial2.available() && (!tud_cdc_n_ready(0) || uartToPeer.free())){
      uint8_t b=Serial2.read();if(tud_cdc_n_ready(0))uartToPeer.push(b);
    }
    size_t room=tud_cdc_n_ready(0)?tud_cdc_n_write_available(0):0;
    while(room-- && uartToPeer.size()){
      uint8_t b=uartToPeer.peek();if(tud_cdc_n_write(0,&b,1)!=1)break;uartToPeer.pop();
    }
    tud_cdc_n_write_flush(0);
  } else {
  while(Serial2.available() && (!ready || uartToPeer.free())){
    uint8_t b=Serial2.read();if(ready)uartToPeer.push(b);
  }
  }
}
  // Both roles drive P0.06 TX. Submit only when the previous DMA TX is complete.
  uint8_t b[64];size_t n=0;
  if(!Serial2.availableForWrite())return;
  while(n<sizeof(b) && uartToLocal.size())b[n++]=uartToLocal.pop();
  if(n)Serial2.write(b,n);
}

static void serviceUartSettings(){
  // Apply line coding only for a locally opened COM port. Otherwise retain
  // the current settings; cold boot without a PC uses 115200/8N1 on both ends.
  bool localCdc=hostMode || localMode;
  // Do not gate serial data or line coding on DTR: host applications may
  // intentionally leave DTR low to avoid reset, while still using the CDC.
  if(localCdc && tud_cdc_n_ready(0)){
    cdc_line_coding_t coding;tud_cdc_n_get_line_coding(0,&coding);
    UartSettings next(coding.bit_rate,coding.stop_bits,coding.parity,coding.data_bits);
    if(next.valid() && next!=uartDesired){
      uartDesired=next;uartConfigAcked=false;uartConfigSentAt=0;
      if(++uartConfigSeq==0)uartConfigSeq=1;
    } else if(!next.valid()){
      // Count a rejected *setting*, not every loop iteration while the host
      // keeps that unsupported line coding selected.
      if(!uartHasRejected || next!=uartLastRejected){
        uartLastRejected=next;uartHasRejected=true;uartRejectedSettings++;
      }
    } else uartHasRejected=false;
  }
  // Finish submitted TX and queued old-format output before changing hardware.
  if(uartDesired!=uartApplied && uartToLocal.size()==0 && Serial2.availableForWrite()){
    uint16_t config=uartDesired.parity==2?
      (uartDesired.stop==2?SERIAL_8E2:SERIAL_8E1):
      (uartDesired.stop==2?SERIAL_8N2:SERIAL_8N1);
    Serial2.end();Serial2.begin(uartDesired.baud,config);configureUartRxPullup();uartApplied=uartDesired;
  }
if(hostMode && !localMode){
  if(ready && !uartConfigAcked && uartApplied==uartDesired &&
      (!uartConfigSentAt || uint32_t(millis()-uartConfigSentAt)>=250)){
    uint8_t wire[7];uartDesired.encode(wire);queueFrame(UartConfig,uartConfigSeq,wire,7);
    uartConfigSentAt=millis();
  }
} else {
  if(!localMode && ready && uartConfigPending && uartApplied==uartDesired){
    uint8_t wire[7];uartApplied.encode(wire);queueFrame(UartConfigAck,uartConfigSeq,wire,7);
    uartConfigPending=false;
  }
}
}

void wirelessSetup() {
  // RADIO IRQ deadlines use the cycle counter, never an RTOS API from ISR.
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  startupResetReason=readResetReason();
  // Start listening. A USB power-only source must not make this board a host;
  // promotion happens after an actual USB configuration is mounted.
  hostMode=false;
  roleProbePending=roleProbeFound=false;
  roleProbeStarted=roleProbeUsbWasPresent=roleProbeApply=false;
  preferredRole=boardPreference(NRF_FICR->DEVICEID[0],NRF_FICR->DEVICEID[1]);
  targetPeerSeen=preferredRole==TargetRole;
  usbEverMounted=false;
  lastUsbMountedAt=0;
  localMode=false;
  pinMode(15,OUTPUT);digitalWrite(15,LOW);
  // GPIO 13 is reserved for board power control by the custom variant.
  Serial.begin(Baud);
#if WD_ONE_ROW_WIRING_TEST
  // HardwareSerial::setPins takes Arduino pin indices; this variant maps
  // 45 -> P1.13 (RX), 47 -> P1.15 (TX), adjacent to the SWD pins.
  Serial2.setPins(45,47);
#elif WD_ONE_ROW_OUTER_RESET_TEST || WD_ONE_ROW_RESET_P113_TEST
  // Keep SWCLK, SWDIO, UART TX and RX contiguous. P0.09 or P1.13 is the
  // optional target nRESET, depending on the selected one-row mapping.
  Serial2.setPins(47,2);
#endif
  Serial2.begin(Baud);
  configureUartRxPullup();
  txQueue=xQueueCreate(16,sizeof(Frame));
  usbRequests=xQueueCreate(UsbRequestQueueDepth,DapPacketSize);
  snprintf(serialNumber,sizeof(serialNumber),"%08lX%08lX",(unsigned long)NRF_FICR->DEVICEID[1],(unsigned long)NRF_FICR->DEVICEID[0]);
  TinyUSBDevice.detach();
  // A's earlier 80DE WinUSB instance was cached without the CMSIS-DAP
  // DeviceInterfaceGUIDs property. Use a fresh identity for this revision.
  TinyUSBDevice.setID(0x239A,0x80E2);
  TinyUSBDevice.setProductDescriptor("SuperMini Auto Role CMSIS-DAP v2");
  TinyUSBDevice.setSerialDescriptor(serialNumber);
  static const char cmsisDapGuid[]="{CDB3B5AD-293B-4663-AA36-1AAE46463776}";
  for(size_t i=0;i<sizeof(cmsisDapGuid);i++){
    desc_ms_os_20[98+i*2]=(uint8_t)cmsisDapGuid[i];
    desc_ms_os_20[99+i*2]=0;
  }
  dapUsb.setStringDescriptor("CMSIS-DAP v2");dapUsb.begin();
  delay(10);TinyUSBDevice.attach();
  dapJobs=xQueueCreate(DapWindow,sizeof(Frame));dapResults=xQueueCreate(DapWindow,sizeof(Frame));
  xTaskCreate(dapWorker,"SWD",1024,nullptr,TASK_PRIO_LOW,nullptr);
if(hostMode){
#if WD_ESB
  transport.begin(true);
#else
  Bluefruit.configCentralBandwidth(BANDWIDTH_MAX);Bluefruit.begin(0,1);
  Bluefruit.setName("WDAP_HOST_B");transport.begin();
  Bluefruit.Central.setConnectCallback(onConnect);Bluefruit.Central.setDisconnectCallback(onDisconnect);
  Bluefruit.Scanner.setRxCallback(onScan);Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(80,64);Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE);
  Bluefruit.Scanner.filterMSD(PairMarker);Bluefruit.Scanner.useActiveScan(false);Bluefruit.Scanner.start(0);
#endif
} else {
#if WD_ESB
  transport.begin(false);
#else
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);Bluefruit.begin(1,0);
  Bluefruit.setName("WDAP_TARGET_A");transport.begin();transport.setRxOverflowCallback(onOverflow);
  Bluefruit.Periph.setConnectCallback(onConnect);Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(transport);Bluefruit.Advertising.addManufacturerData(&PairMarker,sizeof(PairMarker));
  Bluefruit.ScanResponse.addName();Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32,244);Bluefruit.Advertising.setFastTimeout(30);Bluefruit.Advertising.start(0);
#endif
}
#if !WD_ESB
  Bluefruit.autoConnLed(false);Bluefruit.setTxPower(4);
#endif
  resetLinkState();
}

void wirelessLoop() {
#if WD_USB_COMPAT
  if(!TinyUSBDevice.mounted()){usbDapTx.clear();
#if WD_DAP128
    usbAssembler.reset();
#endif
  }
#endif
  bool now=false;
  if(!hostMode && transport.connected())targetPeerSeen=true;
  bool usbPresent=(NRF_POWER->USBREGSTATUS&POWER_USBREGSTATUS_VBUSDETECT_Msk)!=0;
  if(roleProbePending){taskYIELD();return;}
  if(roleProbeApply){
    preferredRole=roleProbeFound?TargetRole:AutoRole;
    targetPeerSeen=roleProbeFound || (!hostMode && transport.connected());
    roleProbeApply=false;
  }
  if((!roleProbeStarted || (usbPresent && !roleProbeUsbWasPresent)) &&
     !dapRunning && dapCount==0 && uxQueueMessagesWaiting(dapJobs)==0 &&
     uxQueueMessagesWaiting(dapResults)==0 && !localDapConnected){
    Frame probe={};probe.type=0xfe;
    roleProbePending=true;
    if(xQueueSend(dapJobs,&probe,0)==pdTRUE){
      roleProbeStarted=true;roleProbeUsbWasPresent=usbPresent;roleProbeApply=true;
      taskYIELD();return;
    }
    roleProbePending=false;
  }
  if(!usbPresent)roleProbeUsbWasPresent=false;
  if(!usbPresent)usbEverMounted=false;
  else if(TinyUSBDevice.mounted()){
    usbEverMounted=true;lastUsbMountedAt=millis();
  }
  // VBUS may stay high when a data cable is replaced by a power-only source.
  // Retain local ownership across a brief USB re-enumeration, then release it
  // when there has been no configured USB host for two seconds.
  bool usbDataPresent=usbPresent && usbEverMounted &&
    (TinyUSBDevice.mounted() || uint32_t(millis()-lastUsbMountedAt)<2000);
#if WD_SESSION_RESTART_TEST
  // Recovery deliberately detaches USB. Preserve PTX ownership while Windows
  // remounts, rather than interpreting our own detach as cable removal.
  if(hostUsbRemountGrace){
    if(!usbPresent || TinyUSBDevice.mounted() || uint32_t(millis()-hostRestartAt)>10000)
      hostUsbRemountGrace=false;
    else usbDataPresent=true;
  }
  if(hostMode && !localMode && !usbHidden && wirelessHostSeen &&
     uint32_t(millis()-hostLastHealthy)>4000 &&
     uint32_t(millis()-hostRestartAt)>4000)hostRestartRequested=true;
  if(hostUsbDetached){
    if(uint32_t(millis()-hostRestartAt)>=250){
      hostUsbDetached=false;lastUsbMountedAt=millis();TinyUSBDevice.attach();
    }
    taskYIELD();return;
  }
  if(hostUsbRecoveryPending){
    // Finish failed replies first. Re-enumerate only after the DAP pipeline
    // and USB IN are idle; never replay a failed command or reset the target.
    if(dapCount || uxQueueMessagesWaiting(usbRequests) ||
       usbPacketHead!=usbPacketTail || usbDapTx.isBusy())
      hostUsbRecoveryIdleAt=millis();
    if(hostMode && !localMode && !usbHidden && usbPresent && ready &&
       TinyUSBDevice.mounted() && uint32_t(millis()-hostUsbRecoveryIdleAt)>=1000){
      hostUsbRecoveryPending=false;hostUsbDetached=true;hostUsbRemountGrace=true;
      hostRestartAt=millis();
      xQueueReset(usbRequests);usbPacketTail=usbPacketHead;
      usbAssembler.reset();usbDapTx.clear();
      dapHead=dapCount=0;hostCancelledRequests=0;
      resetLinkState();hostCancelledRequests=0;
      TinyUSBDevice.detach();taskYIELD();return;
    }
  }
  if(hostRestartRequested){
    hostRestartRequested=false;
    if(hostMode && !localMode && !usbHidden && usbPresent){
      hostRecoveryEpisode=true;
      hostRestartAt=millis();hostRestartCount++;
      transport.setRole(true);generation++;
      resetLinkState();seenGeneration=generation;wasConnected=false;
      // Keep queued USB replies. No detach, no dropped host transactions.
      digitalWrite(15,LOW);taskYIELD();return;
    }
  }
#endif
  // The proven target-side firmware owns local SWD for the full VBUS period.
  // USB re-enumeration during a flash operation must not tear down its DAP
  // worker/session. A's independent 3.3 V supply does not assert USB VBUS.
  bool targetLocalUsb=preferredRole==TargetRole?usbPresent:usbDataPresent;
  // A power-only supply can keep VBUS high after its data cable is removed.
  // Only an intentionally hidden B USB may retain the PTX role without a
  // configured host. Otherwise two former USB boards can both stay PTX.
  if(hostMode && !usbDataPresent && !usbHidden){
    hostMode=false;localMode=false;localDapConnected=false;
    targetPeerSeen=preferredRole==TargetRole;
    localLeaseForDap=false;usbHidden=false;
    transport.setRole(false);dapEpoch++;DAP_TransferAbort=1;
    xQueueReset(dapJobs);xQueueReset(dapResults);
    xQueueReset(usbRequests);usbPacketTail=usbPacketHead;dapHead=dapCount=0;
    resetLinkState();seenGeneration=generation;wasConnected=false;
  } else if(!hostMode && usbDataPresent && !localMode &&
            (preferredRole==HostRole ||
             (preferredRole==AutoRole && !targetPeerSeen && !transport.connected()))){
    // An idle board connected to a PC becomes the radio polling side.
    // A connected target keeps its current role and gives local USB priority.
    hostMode=true;localMode=false;localDapConnected=false;
    localLeaseForDap=false;usbHidden=false;
    transport.setRole(true);dapEpoch++;DAP_TransferAbort=1;
    xQueueReset(dapJobs);xQueueReset(dapResults);
    xQueueReset(usbRequests);usbPacketTail=usbPacketHead;dapHead=dapCount=0;
    resetLinkState();seenGeneration=generation;wasConnected=false;
  }
if(!hostMode){
  // Keep local ownership while the cable still supplies VBUS. A transient
  // unmount/re-enumeration must not invalidate an active SWD flash session.
  if(targetLocalUsb!=localMode){
    localMode=targetLocalUsb;
    if(!localMode){localDapConnected=false;localLeaseForDap=false;}
    if(localMode){
      disconnectLink();
    } else {
      // USB removal can stop the shared HFXO after the last RF transaction.
      // Reconfigure PRX and start receive afresh at the ownership boundary.
      transport.setRole(false);
      ready=false;generation++;
    }
    resetLinkState();
    seenGeneration=generation;wasConnected=false;
    xQueueReset(usbRequests);usbOverflow=false;
#if WD_DAP128
    if(!localMode)usbAssembler.reset();
#endif
    // Keep the first packets already received after enumeration; discard
    // unfinished USB input only when leaving local mode.
    if(!localMode)usbPacketTail=usbPacketHead;
    while(Serial2.available())Serial2.read();
    if(!localMode)tud_cdc_n_read_flush(0);
#if WD_ESB
    transport.setLocalOwner(localMode);
#endif
  }
}
#if WD_ESB
if(hostMode){
  // A may have switched to local USB while an RF data packet was pending.
  // Clear it on link expiry so empty discovery polls can start again.
  if(wasConnected && !transport.connected())disconnectLink();
  transport.poll(dapCount!=0,
    txOffset<txLength || uxQueueMessagesWaiting(txQueue)!=0);
  bool hide=!localMode && transport.peerLocal();
  if(hide!=usbHidden){
    usbHidden=hide;resetLinkState();dapHead=dapCount=0;
    xQueueReset(usbRequests);usbPacketTail=usbPacketHead;
    if(hide)TinyUSBDevice.detach();
    else {
      // Give Windows time to remount after B releases local ownership before
      // treating the temporary unmounted state as a removed data cable.
      lastUsbMountedAt=millis();
      TinyUSBDevice.attach();
    }
  }
  now=transport.connected() && !usbHidden && !localMode;
} else {
  transport.poll(); // Local mode replies only with ownership notifications.
  now=!localMode && transport.connected();
}
#else
  now=connected();
if(!hostMode){
  now=now && !localMode;
}
#endif
  if(generation!=seenGeneration || now!=wasConnected){seenGeneration=generation;wasConnected=now;resetLinkState();}
  uint32_t ms=millis();
  while(usbPacketTail!=usbPacketHead){
    uint8_t tail=usbPacketTail;
if(hostMode && !localMode){
    if(usbPackets[tail].data[0]==7)abortRequested=true;
    else if(xQueueSend(usbRequests,usbPackets[tail].data,0)!=pdTRUE){usbQueueBackpressureCount++;break;}
} else {
    if(usbPackets[tail].data[0]==7)DAP_TransferAbort=1;
    else if(xQueueSend(usbRequests,usbPackets[tail].data,0)!=pdTRUE){usbQueueBackpressureCount++;break;}
}
    usbPacketTail=(tail+1)&(UsbPacketCount-1);
  }
  if(hostMode && !localMode && !ready && !transport.connected() && TinyUSBDevice.mounted()
#if WD_LINK_RECOVERY_TEST
     && !wirelessHostSeen && preferredRole!=HostRole
#endif
     ){
    uint8_t first[DapPacketSize];
    bool localRequest=xQueuePeek(usbRequests,first,0)==pdTRUE &&
       first[0]!=0 && first[0]!=1 && first[0]!=0x80 && first[0]!=0x82 && first[0]!=0x83;
    if(localRequest){
      // Execute an isolated board's wired DAP through the same target-side
      // worker path as the original A firmware. The former host+local hybrid
      // kept PTX discovery polling during SWD flash operations.
      hostMode=false;localMode=true;localDapConnected=false;
      localLeaseForDap=localRequest;lastLocalDapAt=millis();
      transport.setRole(false);
      transport.setLocalOwner(true);
      resetLinkState();seenGeneration=generation;wasConnected=false;
      dapEpoch++;DAP_TransferAbort=0;
      xQueueReset(dapJobs);xQueueReset(dapResults);
      dapHead=dapCount=0;
    }
  }
if(!hostMode || localMode){
  // Wireless commands need a bounded execution time because a stalled peer
  // would otherwise block the shared DAP worker. A wired debugger owns the
  // local session and may legitimately issue long flash programming batches;
  // do not abort those based on this wireless timeout.
  if(dapRunning && !localMode && int32_t(ms-dapStartedAt)>2000)DAP_TransferAbort=1;
  uint8_t localRaw[DapPacketSize];
  while(xQueuePeek(usbRequests,localRaw,0)==pdTRUE){
    // Match the reference design's exclusive work modes. Once local USB DAP
    // is used, do not interleave BLE and USB requests in the same DAP state.
    if(!localMode){xQueueReceive(usbRequests,localRaw,0);continue;}
    Frame job={};job.type=DapRequest;job.session=LocalUsbSession;job.seq=uint16_t(localUsbSeq+1);job.len=DapPacketSize;job.epoch=dapEpoch;
    memcpy(job.data,localRaw,DapPacketSize);
    // A full worker queue is ordinary backpressure, not a lost USB command.
    // Keep the request at the head until the worker accepts it.
    if(xQueueSend(dapJobs,&job,0)!=pdTRUE)break;
    localUsbSeq=job.seq;
    if(job.data[0]!=0x80)recordDap(0,job.data,DapPacketSize);
    if(localLeaseForDap)lastLocalDapAt=ms;
    xQueueReceive(usbRequests,localRaw,0);
  }
}
  if(now){
#if !WD_ESB
    BLEConnection* c=Bluefruit.Connection(conn);
#endif
if(hostMode){
#if !WD_ESB
    if(c && ms-startedAt>uint32_t(200+tuning*250) && tuning<4){
      switch(tuning++){
        case 0:c->requestMtuExchange(247);break;
        case 1:c->requestDataLengthUpdate();break;
        case 2:c->requestPHY(BLE_GAP_PHY_2MBPS);break;
        case 3:c->requestConnectionParameter(6,0,200);break;
      }
    }
#endif
    if(!ready && ms-lastHello>=250){uint8_t ver=LinkVersion;queueFrame(Hello,0,&ver,1);lastHello=ms;}
    if(ready && ms-lastPing>=300){queueFrame(Ping,0);lastPing=ms;}
} else {
#if !WD_ESB
    (void)c;
#endif
    if(overflow){disconnectLink();overflow=false;}
  }
  }
  if(!localMode){
    while(transport.available()){
      int b=transport.read();if(b<0)break;
      if(parser.feed(b,incoming))handleFrame(incoming);
    }
    if(ready && uint32_t(millis()-lastPeer)>3000){
#if WD_SESSION_RESTART_TEST
      requestHostRestart();
#endif
      disconnectLink();
    }
  }
  if(hostMode && localMode){
    while(transport.available())transport.read();
  }
if(hostMode && !localMode){
  if(usbOverflow){usbOverflow=false;disconnectLink();xQueueReset(usbRequests);}
  if(abortRequested){abortRequested=false;if(ready)queueFrame(Abort,0);}
  if(!TinyUSBDevice.mounted()){
    xQueueReset(usbRequests);dapHead=dapCount=0;
    usbPacketTail=usbPacketHead;
    // USB absence is normal in vehicle-to-vehicle mode, not an RF disconnect.
    if(debugConnected){queueFrame(Abort,0);debugConnected=false;}
  } else {
    bool timedOut=false;
    for(unsigned i=0;i<dapCount;i++){
      DapSlot& s=dapSlots[(dapHead+i)%DapWindow];
      uint32_t timeout=DapTimeout;
      // SWJ_Pins wait is specified in microseconds, DapTimeout in milliseconds.
      if(s.request[0]==0x10)timeout=DapTimeout+read32(s.request+3)/1000;
      if(s.remote && !s.ready && uint32_t(ms-s.sentAt)>timeout){timedOut=true;break;}
    }
    if(timedOut){
#if WD_SESSION_RESTART_TEST
      requestHostRestart();
#endif
      for(unsigned i=0;i<dapCount;i++){
        DapSlot& s=dapSlots[(dapHead+i)%DapWindow];
        if(!s.ready){dapError(s.request[0],s.response);s.responseLen=dapErrorLength(s.request[0]);s.ready=true;}
      }
      disconnectLink();
    }
    if(dapCount && dapSlots[dapHead].ready &&
       sendDapUsbPacket(dapEndpoint,dapSlots[dapHead].response,dapSlots[dapHead].responseLen)){
      if(dapSlots[dapHead].request[0]!=0x80)
        recordDap(1,dapSlots[dapHead].response,dapSlots[dapHead].responseLen);
      dapHead=(dapHead+1)%DapWindow;dapCount--;
    }
    while(dapCount<DapWindow){
      uint8_t raw[DapPacketSize];if(xQueueReceive(usbRequests,raw,0)!=pdTRUE)break;
      DapSlot& s=dapSlots[(dapHead+dapCount)%DapWindow];s={};memcpy(s.request,raw,DapPacketSize);dapCount++;
      if(raw[0]!=0x80)recordDap(0,raw,DapPacketSize);
      bool cancelled=hostCancelledRequests!=0;
      if(cancelled)hostCancelledRequests--;
      if(raw[0]==0x82){
        writeUnifiedStatus(s.response);s.responseLen=64;s.ready=true;
      }
      else if(raw[0]==0x80){
        s.response[0]=0x80;s.response[1]=1; // trace format version
        s.response[2]=traceCount;s.response[3]=raw[1];
        if(raw[1]<traceCount)memcpy(s.response+4,dapTrace[(traceHead+DapTraceCapacity-traceCount+raw[1])%DapTraceCapacity],60);
        s.responseLen=DapPacketSize;s.ready=true;
      }
      else if(raw[0]==0x83){packetStats.encode(s.response,DapPacketSize);s.responseLen=64;s.ready=true;}
      else if(raw[0]==0){s.responseLen=localInfo(raw,s.response);s.ready=true;}
      else if(raw[0]==1){s.response[0]=1;s.response[1]=raw[1]<=1?0:0xff;s.responseLen=dapErrorLength(s.request[0]);s.ready=true;}
      else if(cancelled || !ready || !validDap(raw)){dapError(raw[0],s.response);s.responseLen=dapErrorLength(s.request[0]);s.ready=true;}
      else {
        dapSeq=nextDapSequence(dapSeq);
        s.seq=dapSeq;s.sentAt=ms;s.remote=true;
        size_t requestLength=DapPacketSize;
#if WD_COMPACT_WIRE_TEST
        requestLength=dapWireRequestLength(raw,DapPacketSize);
#endif
        if(!queueFrame(DapRequest,s.seq,raw,requestLength)){dapError(raw[0],s.response);s.responseLen=dapErrorLength(s.request[0]);s.ready=true;}
      }
    }
  }
} else {
  // Preserve requests already accepted by the local DAP path. A packet-ring
  // overflow is counted above; clearing this queue used to discard otherwise
  // valid flash commands and their expected responses.
  if(usbOverflow)usbOverflow=false;
  Frame r;
  while(xQueuePeek(dapResults,&r,0)==pdTRUE){
    if(r.epoch!=dapEpoch){xQueueReceive(dapResults,&r,0);continue;}
    if(r.session==LocalUsbSession){
      if(!localMode || !tud_vendor_mounted()){xQueueReceive(dapResults,&r,0);continue;}
      if(!sendDapUsbPacket(dapEndpoint,r.data,r.len))break;
      if(r.data[0]!=0x80)recordDap(1,r.data,r.len);
      xQueueReceive(dapResults,&r,0);
    } else {
      xQueueReceive(dapResults,&r,0);
      if(ready && r.session==session && !queueFrame(DapResponse,r.seq,r.data,r.len))break;
    }
  }
}
  serviceUartSettings();
  serviceUart();
  if(localMode && localLeaseForDap && !localDapConnected && !dapRunning &&
     uxQueueMessagesWaiting(dapJobs)==0 && uxQueueMessagesWaiting(dapResults)==0 &&
     uxQueueMessagesWaiting(usbRequests)==0 && usbPacketHead==usbPacketTail &&
     uint32_t(ms-lastLocalDapAt)>1000U){
    // A completed local DAP session must not permanently prevent this board
    // from becoming the wireless computer-side probe again.
    localMode=false;localDapConnected=false;localLeaseForDap=false;
    transport.setLocalOwner(false);
    hostMode=usbDataPresent;
    transport.setRole(hostMode);
    dapEpoch++;DAP_TransferAbort=1;
    disconnectLink();resetLinkState();seenGeneration=generation;wasConnected=false;
  }
  bool indicatedReady=false;
if(hostMode){
  if(!localMode)pumpTransmit();
  indicatedReady=localMode || ready;
} else {
  if(!localMode)pumpTransmit();
  indicatedReady=localMode || ready;
}
  digitalWrite(15,indicatedReady?HIGH:((ms%1000)<120?HIGH:LOW));
  taskYIELD();
}


