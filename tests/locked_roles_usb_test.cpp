#define WD_ROLE_FREE_TEST 1
#define main unused_role_main
#include "../role_test.cpp"
#undef main
static void worker(){mockWorkerStep=true;try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;}
static void settle(){for(unsigned i=0;i<5;i++){tick();worker();}}
static void boot(const char* role,bool target,bool vbus,bool mounted){
  configDisk.init();uint8_t text[512]={};snprintf(reinterpret_cast<char*>(text),512,"ROLE=%s\r\n",role);
  configDisk.handleSectorWrite(8,text,512);configDisk.handleFlush();
  configDisk.commitToFlash();configDisk.clearReboot();
  mockTargetAttached=target;mockPower.USBREGSTATUS=vbus;mockMounted=mounted;
  rfShouldConnect=false;localDapConnected=false;hostUsbRecoveryPending=false;
  dapHead=dapCount=0;usbDapTx.clear();usbPacketTail=usbPacketHead;
  wirelessHostSeen=false;usbHidden=false;
  hostRestartRequested=false;hostUsbRemountGrace=false;hostUsbDetached=false;
  mockNow+=10000;wirelessSetup();mockMounted=mounted;settle();
}
int main(){
  // Saved TARGET is honored without a detectable target, even on a PC.
  boot("TARGET",false,true,true);
  assert(configuredRole==2 && preferredRole==TargetRole && !hostMode && localMode);
  assert(!roleProbeStarted && !roleProbePending);
  // A brief USB re-enumeration must not interrupt an active wired session.
  localDapConnected=true;uint32_t wiredEpoch=dapEpoch;
  mockMounted=false;uint32_t last=lastUsbMountedAt;
  mockNow=last+1999;wirelessLoop();assert(localMode && dapEpoch==wiredEpoch);
  mockMounted=true;wirelessLoop();assert(localMode);
  // Data cable replaced with power-only: VBUS remains high, but after 2s
  // no USB configuration the target releases local SWD and resumes radio.
  mockMounted=false;mockNow=lastUsbMountedAt+2001;wirelessLoop();
  assert(!hostMode && !localMode && !localDapConnected && mockPower.USBREGSTATUS==1);
  rfShouldConnect=true;settle();assert(!hostMode && !localMode);
  // With a real target attached, charger-only must never seize local SWD.
  boot("TARGET",true,true,false);
  assert(!hostMode && !localMode && !roleProbeStarted);
  rfShouldConnect=true;settle();assert(!localMode);
  mockMounted=true;settle();assert(localMode && !hostMode);
  Frame connect={};connect.type=DapRequest;connect.session=LocalUsbSession;
  connect.epoch=dapEpoch;connect.len=2;connect.data[0]=2;connect.data[1]=1;
  xQueueSend(dapJobs,&connect,0);worker();assert(localDapConnected);
  xQueueReset(dapResults);
  mockMounted=false;mockPower.USBREGSTATUS=0;settle();
  assert(!localMode && !hostMode && !localDapConnected);
  mockPower.USBREGSTATUS=1;settle();assert(!localMode); // charge again
  // Saved HOST stays PTX on battery/charger and cannot become local TARGET,
  // even if its SWD pads happen to have a live MCU attached.
  boot("HOST",true,true,false);
  assert(configuredRole==1 && preferredRole==HostRole && hostMode && !localMode);
  assert(rfHostRole && !roleProbeStarted);
  mockPower.USBREGSTATUS=0;settle();assert(hostMode && !localMode);
  mockPower.USBREGSTATUS=1;mockMounted=true;settle();assert(hostMode && !localMode);
  uint8_t clock[DapPacketSize]={0x11};wd::write32(clock+1,1000000);
  xQueueSend(usbRequests,clock,0);wirelessLoop();
  assert(hostMode && !localMode); // no isolated-board local DAP fallback
  // AUTO still probes a real target but charger-only does not take USB lease.
  boot("AUTO",true,true,false);
  assert(configuredRole==0 && preferredRole==TargetRole && !hostMode && !localMode);
  mockMounted=true;settle();assert(localMode && !hostMode);
  mockMounted=false;mockNow=lastUsbMountedAt+2001;wirelessLoop();worker();
  assert(!localMode && !hostMode);
  puts("PASS saved roles + USB: locked TARGET wired/charging/grace/unplug, locked HOST battery/charger/PC, AUTO charging target; no role override");
}
