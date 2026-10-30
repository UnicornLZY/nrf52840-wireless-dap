#define WD_ESB 1
#define __CC_ARM 1
#include "libraries/WirelessDAP/src/WirelessDAP.cpp"
#include <cassert>
uint32_t mockNow=0;
int mockTaskPriority=-1;
MockFicr mockFicr;MockFicr* NRF_FICR=&mockFicr;
MockPower mockPower;MockPower* NRF_POWER=&mockPower;
MockGpio mockP1={};
MockGpio mockGpio={};MockGpio* NRF_P0=&mockGpio;
MockDwt mockDwt={};MockDwt* DWT=&mockDwt;
MockCoreDebug mockCore={};MockCoreDebug* CoreDebug=&mockCore;
MockSerial Serial,Serial2;MockUsbDevice TinyUSBDevice;
bool mockMounted=false,mockCdc=false;
cdc_line_coding_t mockCoding={115200,0,0,8};
std::deque<uint8_t> mockUsbRx;std::vector<uint8_t> mockUsbTx;
std::vector<uint8_t> mockVendorTx;
uint8_t desc_ms_os_20[512]={};
DAP_Data_t DAP_Data={};volatile uint8_t DAP_TransferAbort=0;
void DAP_Setup(){}
static unsigned mockDapExecutions=0;
static uint32_t mockLastSwdClock=0;
static bool mockTargetAttached=false;
#ifdef WD_HW_RESET_TEST
static unsigned mockSoftwareResetWrites=0,mockSoftwareResetReads=0;
static bool mockSoftwareResetFails=false;
#endif
uint32_t DAP_ProcessCommand(const uint8_t* q,uint8_t* r){mockDapExecutions++;r[0]=q[0];r[1]=1;
if(q[0]==0x11)mockLastSwdClock=wd::read32(q+1);
#ifdef WD_HW_RESET_TEST
if(q[0]==9){r[1]=0;return 2;}
if(q[0]==5 && q[2]==1 && q[3]==13 && wd::read32(q+4)==0x05fa0004){
  mockSoftwareResetWrites++;mockSoftwareResetReads=0;r[1]=1;r[2]=1;return 3;
}
if(q[0]==5 && q[2]==4 && q[18]==15){
  mockSoftwareResetReads++;r[1]=4;r[2]=1;
  wd::write32(r+3,mockSoftwareResetFails?0:(mockSoftwareResetReads==1?(1UL<<25):0));return 7;
}
if(q[0]==5){r[1]=q[2];r[2]=1;return 3;}
if(q[0]==6){r[1]=q[2];r[2]=q[3];r[3]=1;return 4;}
if(q[0]==3){DAP_Data.debug_port=0;PORT_OFF();r[1]=0;return 2;}
#endif
#ifdef WD_ROLE_FREE_TEST
if(q[0]==5 && q[2]==1 && q[3]==2){r[1]=mockTargetAttached?1:0;r[2]=mockTargetAttached?1:7;wd::write32(r+3,mockTargetAttached?0x2ba01477:0);return 7;}
#endif
#ifdef WD_PACKET128_TEST
if(q[0]==6){r[1]=q[2];r[2]=q[3];r[3]=1;size_t n=q[2]|size_t(q[3])<<8;if(q[4]&2){for(size_t i=0;i<n;i++)wd::write32(r+4+i*4,0xAABB0000+i);return 4+n*4;}return 4;}
#endif
return 2;}
static bool rfShouldConnect=false;
static bool rfSuspended=false;
static bool rfHostRole=false;
static unsigned rfRoleReinitializations=0;
static std::vector<uint8_t> rfSent;
static bool mockBlockRadioWrites=false;
void EsbTransport::begin(bool host){host_=host;rfHostRole=host;}
void EsbTransport::setRole(bool host){host_=host;rfHostRole=host;rfRoleReinitializations++;rfShouldConnect=false;rfSuspended=false;lastRx_=0;}
void EsbTransport::flush(){rx_.clear();tx_.clear();}
void EsbTransport::suspend(){rfSuspended=true;}
void EsbTransport::poll(bool,bool){assert(!rfSuspended);if(rfShouldConnect)lastRx_=millis();}
bool EsbTransport::connected()const{return rfShouldConnect;}
void EsbTransport::reset(){rfShouldConnect=false;rx_.clear();}
int EsbTransport::read(){return rx_.size()?rx_.pop():-1;}
size_t EsbTransport::write(const uint8_t* p,size_t n){if(mockBlockRadioWrites)return 0;rfSent.insert(rfSent.end(),p,p+n);return n;}
static void tick(){mockNow+=300;wirelessLoop();}

int main(){
  assert(boardPreference(0x8441EBA0,0xF854BE35)==TargetRole);
  assert(boardPreference(0x049802E7,0xC427D941)==TargetRole);
  assert(boardPreference(0x6660EF39,0x088585B8)==HostRole);
  assert(boardPreference(123,456)==AutoRole);
  mockFicr.DEVICEID[0]=0x8441EBA0;mockFicr.DEVICEID[1]=0xF854BE35;
  wirelessSetup();assert(!hostMode && !localMode);
  assert(Serial2.rxPin==47 && Serial2.txPin==2);
  assert(WD_CLK==31 && WD_DIO==29 && WD_RST==13 && WD_RST_PORT==NRF_P1);
  assert(preferredRole==TargetRole && targetPeerSeen);
  const char cmsisGuid[]="{CDB3B5AD-293B-4663-AA36-1AAE46463776}";
  for(size_t i=0;i<sizeof(cmsisGuid);i++){
    assert(desc_ms_os_20[98+i*2]==uint8_t(cmsisGuid[i]));
    assert(desc_ms_os_20[99+i*2]==0);
  }
  assert(mockTaskPriority==TASK_PRIO_LOW);
  mockPower.USBREGSTATUS=1;tick();
  assert(!hostMode && localMode); // Known A follows original VBUS ownership.
  mockMounted=true;tick();
  assert(!hostMode && localMode); // Known A is wired target even before RF pairing.
  mockMounted=false;mockPower.USBREGSTATUS=0;tick();
  assert(!hostMode && !localMode);
  mockPower.USBREGSTATUS=0;tick();
  rfShouldConnect=true;tick();
  assert(targetPeerSeen);
  // RF expiry in the same instant as USB insertion must keep the established
  // target role; the other board is already the PTX host.
  rfShouldConnect=false;
  mockPower.USBREGSTATUS=1;mockMounted=true;tick();
  assert(!hostMode && localMode); // Active target gains wired priority.
  mockMounted=false;tick();assert(localMode); // Transient unmount keeps ownership.
  unsigned beforeRelease=rfRoleReinitializations;
  for(int i=0;i<7;i++)tick();
  assert(localMode); // A must retain SWD throughout a long USB re-enumeration.
  mockPower.USBREGSTATUS=0;tick();
  assert(!localMode); // Actual cable removal releases A's local ownership.
  assert(rfRoleReinitializations>beforeRelease && !rfHostRole);
  rfShouldConnect=true;tick();
  Frame resumedHello={};resumedHello.type=Hello;resumedHello.session=0x12345678;
  resumedHello.len=1;resumedHello.data[0]=3;
  handleFrame(resumedHello);
  assert(ready && session==resumedHello.session); // Wireless handshake can restart.
  rfShouldConnect=false;
  mockPower.USBREGSTATUS=0;mockMounted=false;tick();
  assert(!hostMode && !localMode);
  preferredRole=HostRole;targetPeerSeen=false; // Model the other known board.
  rfShouldConnect=false;tick();
  mockPower.USBREGSTATUS=1;mockMounted=true;tick();
  assert(hostMode && !localMode); // Idle board becomes polling side.
  uint8_t statusRequest[64]={0x82};tud_vendor_rx_cb(0,statusRequest,sizeof(statusRequest));tick();tick();
  assert(mockVendorTx.size()>=16 && mockVendorTx[0]==0x82 && mockVendorTx[2]==1);
  mockUsbRx.push_back(0x55);Serial2.rx.push_back(0x66);tick();
  assert(!localMode && Serial2.tx.back()==0x55 && mockUsbTx.back()==0x66);
  rfShouldConnect=true;tick();
  Frame helloAck={};helloAck.type=HelloAck;helloAck.session=session;
  helloAck.len=1;helloAck.data[0]=3;handleFrame(helloAck);tick();
  assert(ready && !localMode);
  uint8_t remoteRequest[64]={2,1};
  tud_vendor_rx_cb(0,remoteRequest,sizeof(remoteRequest));tick();
  bool remoteDapSent=false;Parser check;Frame decoded={};
  for(uint8_t b:rfSent)if(check.feed(b,decoded) && decoded.type==DapRequest)remoteDapSent=true;
  assert(remoteDapSent && !localMode);
  rfShouldConnect=false;tick();
  assert(!ready);dapHead=dapCount=0;xQueueReset(usbRequests);
  uint8_t request[64]={2,1};
#if WD_LINK_RECOVERY_TEST
  // Reproduce a failed wireless download followed by a Keil retry while the
  // radios are out of range. B must keep PTX discovery, never local SWD.
  unsigned beforeRetry=rfRoleReinitializations;
  tud_vendor_rx_cb(0,request,sizeof(request));tick();tick();
  assert(hostMode && !localMode && rfHostRole);
  assert(rfRoleReinitializations==beforeRetry);
  assert(uxQueueMessagesWaiting(dapJobs)==0);
  uint32_t failedSession=session;
  rfShouldConnect=true;tick();
  helloAck.session=session;handleFrame(helloAck);tick();
  assert(ready && hostMode && !localMode);
  mockVendorTx.clear();
  tud_vendor_rx_cb(0,request,sizeof(request));tick();
  assert(dapCount==1 && dapSlots[dapHead].remote);
  Frame recovered={};recovered.type=DapResponse;recovered.session=failedSession;
  recovered.seq=dapSlots[dapHead].seq;recovered.len=2;
  recovered.data[0]=2;recovered.data[1]=1;
  handleFrame(recovered);assert(!dapSlots[dapHead].ready);
  recovered.session=session;handleFrame(recovered);tick();
  assert(dapCount==0 && mockVendorTx.size()==64 && mockVendorTx[1]==1);
  // Radio ACKs/heartbeat may still succeed while the DAP reply is missing.
  // Timeout must fail the old request and create a fresh application session.
  tud_vendor_rx_cb(0,request,sizeof(request));tick();
  uint32_t timedSession=session;
  uint16_t timedSeq=dapSlots[dapHead].seq;
  mockNow+=DapTimeout+1;wirelessLoop();
  assert(!ready && hostMode && !localMode);
  rfShouldConnect=true;tick();
  helloAck.session=session;handleFrame(helloAck);tick();
  assert(ready && session!=timedSession && !dapCount);
  tud_vendor_rx_cb(0,request,sizeof(request));tick();
  recovered.session=timedSession;recovered.seq=timedSeq;
  handleFrame(recovered);assert(!dapSlots[dapHead].ready);
  recovered.session=session;recovered.seq=dapSlots[dapHead].seq;
  handleFrame(recovered);tick();assert(!dapCount);
  Frame configAck={};configAck.type=UartConfigAck;configAck.session=session;
  configAck.seq=uartConfigSeq;configAck.len=7;uartDesired.encode(configAck.data);
  handleFrame(configAck);assert(uartConfigAcked);
  Frame serial={};serial.type=UartData;serial.session=session;
  serial.seq=1;serial.len=1;serial.data[0]=0xA7;
  handleFrame(serial);tick();assert(Serial2.tx.back()==0xA7);
  // Model a fresh standalone board to preserve cold-start wired operation.
  wirelessHostSeen=false;preferredRole=AutoRole;rfShouldConnect=false;tick();
  dapHead=dapCount=0;xQueueReset(usbRequests);
#endif
  tud_vendor_rx_cb(0,request,sizeof(request));tick();
  assert(!hostMode && localMode && !rfHostRole && !rfSuspended && uxQueueMessagesWaiting(dapJobs)==1);
  Frame job={};assert(xQueueReceive(dapJobs,&job,0)==pdTRUE);
  assert(job.session==LocalUsbSession && job.data[0]==2);
  Frame response={};response.session=LocalUsbSession;response.len=2;response.epoch=dapEpoch;
  response.data[0]=2;response.data[1]=1;
  assert(xQueueSend(dapResults,&response,0)==pdTRUE);tick();
  assert(mockVendorTx.size()>=2 && mockVendorTx[mockVendorTx.size()-64]==2 && mockVendorTx[mockVendorTx.size()-63]==1);
  localDapConnected=true;
  for(int i=0;i<60;i++)tick();
  assert(localMode); // An active wired DAP session cannot be stolen by RF.
  localDapConnected=false;lastLocalDapAt=mockNow;
  for(int i=0;i<5;i++)tick();
  assert(hostMode && !localMode && rfHostRole); // Finished wired session resumes discovery.
  mockMounted=false;
  for(int i=0;i<7;i++)tick();
  assert(!hostMode && !rfHostRole); // Power remains, USB data host has gone.
  mockPower.USBREGSTATUS=0;mockMounted=false;tick();
  assert(!hostMode && !localMode && !rfSuspended); // Cable removal restores RF.
  mockPower.USBREGSTATUS=1;mockMounted=true;preferredRole=TargetRole;
  hostMode=false;localMode=true;localDapConnected=true;dapEpoch++;
  xQueueReset(usbRequests);xQueueReset(dapJobs);xQueueReset(dapResults);
  usbPacketHead=usbPacketTail=0;usbOverflow=false;
  uint8_t queuedRequest[64]={9,0,0};
  for(unsigned i=0;i<UsbRequestQueueDepth;i++)
    assert(xQueueSend(usbRequests,queuedRequest,0)==pdTRUE);
  uint8_t pendingRequest[64]={9,0,0};
  tud_vendor_rx_cb(0,pendingRequest,sizeof(pendingRequest));
  uint8_t pendingTail=usbPacketTail;
  tick();
  assert(usbPacketTail==pendingTail); // Full request queue keeps USB ring head pending.
  assert(usbQueueBackpressureCount>0 && !usbOverflow);
  assert(uxQueueMessagesWaiting(usbRequests)==UsbRequestQueueDepth-DapWindow);
  xQueueReset(usbRequests);xQueueReset(dapJobs);xQueueReset(dapResults);
  usbPacketHead=usbPacketTail;
  mockPower.USBREGSTATUS=0;mockMounted=false;tick();
  // Execute the actual worker until it blocks on an empty job queue.
  localMode=true;hostMode=false;xQueueReset(dapJobs);xQueueReset(dapResults);
  Frame oldJob={};oldJob.session=LocalUsbSession;oldJob.epoch=dapEpoch-1;
  oldJob.len=64;oldJob.data[0]=2;oldJob.data[1]=1;
  xQueueSend(dapJobs,&oldJob,0);mockWorkerStep=true;
  try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;
  assert(mockDapExecutions==0 && !uxQueueMessagesWaiting(dapResults));
  oldJob.epoch=dapEpoch;xQueueSend(dapJobs,&oldJob,0);mockWorkerStep=true;
  try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;
  assert(mockDapExecutions==2); // default clock and the one current Connect
  Frame workerReply={};assert(xQueueReceive(dapResults,&workerReply,0));
  assert(workerReply.epoch==dapEpoch && workerReply.data[0]==2);
  // Actual target admission crosses sequence wrap without a forced disconnect.
  localMode=false;ready=true;session=987;acceptedDapSeq=65535;
  Frame wrapped={};wrapped.type=DapRequest;wrapped.session=session;wrapped.seq=1;
  wrapped.len=64;wrapped.data[0]=2;wrapped.data[1]=1;handleFrame(wrapped);
  assert(ready && acceptedDapSeq==1 && uxQueueMessagesWaiting(dapJobs)==1);
  puts("PASS: unified runtime roles, local USB priority, wired fallback and role release");
return 0;
}



