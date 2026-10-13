#define WD_ROLE_FREE_TEST 1
#define main unused_role_main
#include "../role_test.cpp"
#undef main
int main(){
  wirelessSetup();roleProbeStarted=roleProbeUsbWasPresent=true;
  mockNow=10000;hostMode=true;localMode=false;ready=true;
  wirelessHostSeen=true;mockMounted=true;mockPower.USBREGSTATUS=1;
  rfShouldConnect=true;wasConnected=true;seenGeneration=generation;
  hostLastHealthy=lastPeer=mockNow;
  txLength=8;txOffset=0;txProgress=mockNow;mockBlockRadioWrites=true;
  dapCount=1;dapSlots[0]={};dapSlots[0].request[0]=6;
  uint32_t before=generation;
  mockNow+=1200;pumpTransmit();assert(generation==before && !hostUsbRecoveryPending);
  mockNow+=1900;pumpTransmit();assert(generation>before && hostUsbRecoveryPending);
  // Lost command is failed, never placed back into the outgoing RF queue.
  resetLinkState();assert(dapSlots[0].ready && dapSlots[0].response[3]==8);
  dapCount=0;xQueueReset(usbRequests);usbPacketTail=usbPacketHead;
  usbDapTx.clear();ready=true;rfShouldConnect=true;hostRestartRequested=false;
  hostLastHealthy=lastPeer=mockNow;seenGeneration=generation;wasConnected=true;
  hostUsbRecoveryIdleAt=mockNow;mockBlockRadioWrites=false;
  mockNow+=900;wirelessLoop();assert(!hostUsbDetached);
  // An outstanding IN transfer postpones the recovery boundary.
  usbDapTx.begin(4);mockNow+=200;wirelessLoop();assert(!hostUsbDetached);
  usbDapTx.complete(4);mockNow+=1000;wirelessLoop();assert(hostUsbDetached);
  assert(hostUsbRemountGrace && !hostUsbRecoveryPending);
  mockNow+=249;wirelessLoop();assert(hostUsbDetached);
  mockNow+=1;wirelessLoop();assert(!hostUsbDetached);
  puts("PASS real recovery: 1.2s tolerated, >3s failed without replay; USB drain, idle detach and 250ms attach");
}
