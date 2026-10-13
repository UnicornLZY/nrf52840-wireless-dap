#define WD_ROLE_FREE_TEST 1
#define main unused_role_main
#include "../role_test.cpp"
#undef main
static void worker(){mockWorkerStep=true;try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;}
static void settle(){for(int i=0;i<5;i++){tick();worker();}}
int main(){
  assert(boardPreference(0x8441EBA0,0xF854BE35)==AutoRole);
  assert(boardPreference(0x6660EF39,0x088585B8)==AutoRole);
  assert(boardPreference(0x049802E7,0xC427D941)==AutoRole);
  for(unsigned i=0;i<3;i++){
    mockFicr.DEVICEID[0]=i+123;mockFicr.DEVICEID[1]=456;
    mockTargetAttached=true;mockMounted=false;mockPower.USBREGSTATUS=0;rfShouldConnect=false;
    wirelessSetup();dapHead=dapCount=0;settle();
    assert(preferredRole==TargetRole && !hostMode && !localMode);
    mockPower.USBREGSTATUS=1;mockMounted=true;settle();
    assert(!hostMode && localMode && TinyUSBDevice.pid==0x80e2);
    uint8_t connect[128]={2,1};tud_vendor_rx_cb(0,connect,64);tud_vendor_rx_cb(0,connect+64,64);
    settle();assert(localDapConnected && !mockVendorTx.empty());
    while(usbDapTx.isBusy())tud_vendor_tx_cb(0,usbDapTx.remaining);
    mockVendorTx.clear();
    mockPower.USBREGSTATUS=0;mockMounted=false;settle();
    assert(!hostMode && !localMode);
    // Same identity now has no target and serves as USB radio host.
    mockTargetAttached=false;mockPower.USBREGSTATUS=1;mockMounted=true;rfShouldConnect=false;settle();
    assert(preferredRole==AutoRole && hostMode && !localMode);
    // Host loses USB and returns to receiver; power-only USB is not a host.
    mockPower.USBREGSTATUS=0;mockMounted=false;settle();
    assert(!hostMode);
    mockPower.USBREGSTATUS=1;settle();assert(!hostMode);
    mockPower.USBREGSTATUS=0;settle();
  }
  puts("PASS free roles: arbitrary IDs, target discovery, wired worker, unplug, same board as USB host, charging-only");
}
