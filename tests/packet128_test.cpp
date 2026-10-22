#define WD_PACKET128_TEST 1
#define WD_ROLE_FREE_TEST 1
#define main unused_baseline_main
#include "../role_test.cpp"
#undef main
static void executeWorker(){mockWorkerStep=true;try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;}
static void finishUsb(){while(usbDapTx.isBusy())tud_vendor_tx_cb(0,usbDapTx.remaining>64?64:usbDapTx.remaining);tud_vendor_tx_cb(0,0);}
static void submit(const uint8_t* q){
 auto oldHead=usbPacketHead;
 for(size_t off=0; off<DapPacketSize; off+=64){
   size_t n = DapPacketSize - off;
   if(n > 64) n = 64;
   tud_vendor_rx_cb(0, q + off, n);
 }
 assert(usbPacketHead==uint8_t((oldHead+1)&63));
}
int main(){
 const uint8_t w=(DapPacketSize-5)/4,r=(DapPacketSize-4)/4;
const size_t responseBytes=4+r*4;
 mockFicr.DEVICEID[0]=0x049802E7;mockFicr.DEVICEID[1]=0xC427D941;
 mockTargetAttached=true;
 wirelessSetup();wirelessLoop();executeWorker();wirelessLoop();
 mockMounted=true;mockPower.USBREGSTATUS=1;wirelessLoop();executeWorker();tick();
 assert(localMode && TinyUSBDevice.pid==0x80E2);
 uint8_t info[DapPacketSize]={0,0xff};submit(info);wirelessLoop();executeWorker();wirelessLoop();
 assert(mockVendorTx.size()==4 && mockVendorTx[2]==DapPacketSize);finishUsb();mockVendorTx.clear();
 // First half is never interpreted as a command; short/ZLP boundaries do not
 // leak into the next command. Exact 128 needs no trailing ZLP.
 uint8_t q[DapPacketSize]={6,0,w,0,0};for(unsigned i=0;i<w;i++)wd::write32(q+5+i*4,i+123);
 assert(validDap(q) && dapWireRequestLength(q,DapPacketSize)==size_t(5+w*4));
 submit(q);tud_vendor_rx_cb(0,q,0);wirelessLoop();assert(uxQueueMessagesWaiting(dapJobs)==1);
 executeWorker();wirelessLoop();assert(mockVendorTx.size()==4 && mockVendorTx[1]==w);finishUsb();mockVendorTx.clear();
 q[2]=r;q[4]=2;submit(q);wirelessLoop();executeWorker();wirelessLoop();
 assert(mockVendorTx.size()==responseBytes && wd::read32(mockVendorTx.data()+responseBytes-4)==0xAABB0000u+r-1);
 if(responseBytes>64){tud_vendor_tx_cb(0,64);assert(usbDapTx.isBusy());assert(!sendDapUsbPacket(dapEndpoint,info,4));}
 tud_vendor_tx_cb(0,64);assert(!usbDapTx.isBusy());
 assert(sendDapUsbPacket(dapEndpoint,info,4));tud_vendor_tx_cb(0,0);assert(usbDapTx.isBusy());finishUsb();mockVendorTx.clear();
 assert(packetStats.maxWrite==w && packetStats.maxRead==r && packetStats.largeBlocks==(WD_DAP128?2u:0u));
 q[2]=r+1;assert(!validDap(q));q[2]=w+1;q[4]=0;assert(!validDap(q));
 // Exercise response validation beyond byte 64 of a Transfer request.
 memset(q,0,DapPacketSize);q[0]=5;q[2]=WD_DAP128?24:12;for(unsigned i=0;i<q[2];i++)q[3+i*5]=0;
 uint8_t rsp[DapPacketSize]={5,q[2],1};assert(validDap(q) && validDapResponse(q,rsp,3));
 // A partial command must be abandoned on USB removal.
 if(WD_DAP128){tud_vendor_rx_cb(0,q,64);}
 mockMounted=false;wirelessLoop();mockMounted=true;
 uint8_t shortInfo[2]={0,0xff};tud_vendor_rx_cb(0,shortInfo,2);
 assert(usbPackets[usbPacketTail].data[0]==0 && usbPackets[usbPacketTail].data[1]==0xff);
 xQueueReset(usbRequests);usbPacketTail=usbPacketHead;
 // Host pipeline: compact 31-word read, then full 128-byte radio response.
 hostMode=true;localMode=false;ready=true;session=77;rfShouldConnect=true;wasConnected=true;
 seenGeneration=generation;lastPeer=mockNow;hostLastHealthy=mockNow;dapHead=dapCount=0;rfSent.clear();
 memset(q,0,DapPacketSize);q[0]=6;q[2]=r;q[4]=2;submit(q);wirelessLoop();
 assert(dapCount==1 && dapSlots[dapHead].remote);
 Frame answer={};answer.type=DapResponse;answer.session=session;answer.seq=dapSlots[dapHead].seq;answer.len=responseBytes;
 answer.data[0]=6;answer.data[1]=r;answer.data[3]=1;
 handleFrame(answer);assert(dapSlots[dapHead].ready);wirelessLoop();
 assert(mockVendorTx.size()==responseBytes && mockVendorTx[1]==r);finishUsb();
 Frame hello={};hello.type=Hello;hello.len=1;hello.session=1;hello.data[0]=3;assert(!wd::validHello(hello));hello.data[0]=wd::LinkVersion;assert(wd::validHello(hello));
 Frame worst={};worst.type=DapRequest;worst.session=0x7e7e7e7e;worst.seq=0x7d7d;worst.len=DapPacketSize;memset(worst.data,0x7e,DapPacketSize);
 uint8_t encoded[wd::WireMax];size_t encodedSize=wd::encode(worst,encoded);wd::Parser parser;Frame decoded={};unsigned frames=0;
 for(size_t offset=0;offset<encodedSize;offset+=wd::RadioPayload){size_t n=encodedSize-offset;if(n>wd::RadioPayload)n=wd::RadioPayload;for(size_t i=0;i<n;i++)if(parser.feed(encoded[offset+i],decoded))frames++;}
 assert(frames==1 && decoded.len==DapPacketSize && !memcmp(decoded.data,worst.data,DapPacketSize));
 if(WD_DAP128)assert(encodedSize>wd::RadioPayload);
 for(unsigned n=1;n<=DapPacketSize;n++){
   usbDapTx.begin(n);for(unsigned done=0;done<n;){unsigned part=n-done;if(part>64)part=64;tud_vendor_tx_cb(0,0);assert(usbDapTx.isBusy());tud_vendor_tx_cb(0,part);done+=part;assert(usbDapTx.isBusy()==(done<n));}
 }
 printf("PASS real application DAP%u: wired Info/worker, %u write/%u read words, bounds, USB callbacks/ZLP, unplug, host RF response, escaped RF fragmentation, protocol isolation and statistics\n",unsigned(DapPacketSize),w,r);
}



