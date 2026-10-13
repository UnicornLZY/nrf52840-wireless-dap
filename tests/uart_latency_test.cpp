#define WD_ROLE_FREE_TEST 1
#define main unused_role_main
#include "../role_test.cpp"
#undef main

static void clean(bool host=true){
  hostMode=host;localMode=false;resetLinkState();
  ready=true;session=42;rfShouldConnect=true;uartConfigAcked=true;
  mockMounted=host;mockCdc=false;mockBlockRadioWrites=false;
  rfSent.clear();Serial2.rx.clear();Serial2.tx.clear();mockUsbTx.clear();mockUsbRx.clear();
}
static Frame frame(uint8_t type,uint16_t seq,uint8_t value=0){
  Frame f={};f.type=type;f.seq=seq;f.session=session;
  if(type==UartData){f.len=1;f.data[0]=value;}return f;
}
static std::vector<Frame> sent(){
  Parser p;Frame f={};std::vector<Frame> out;
  for(auto b:rfSent)if(p.feed(b,f))out.push_back(f);
  rfSent.clear();return out;
}
int main(){
  wirelessSetup();mockNow=1000;clean();
  uartToPeer.push(0x53);pumpTransmit();assert(sent().empty());
  mockNow++;pumpTransmit();auto first=sent();
  assert(first.size()==1 && first[0].type==UartData && first[0].data[0]==0x53);
  // Drop ACK: resend at 20 ms, with same sequence and bytes; no 500 ms pause.
  mockNow+=19;pumpTransmit();assert(sent().empty());
  mockNow++;pumpTransmit();auto retry=sent();
  assert(retry.size()==1 && retry[0].seq==first[0].seq && retry[0].data[0]==0x53);
  mockNow+=39;pumpTransmit();assert(sent().empty());
  mockNow++;pumpTransmit();assert(sent().size()==1);
  mockNow+=80;pumpTransmit();assert(sent().size()==1);
  mockNow+=159;pumpTransmit();assert(sent().empty());
  mockNow++;pumpTransmit();assert(sent().size()==1);
  handleFrame(frame(UartAck,first[0].seq));assert(uartCount==0);
  mockNow+=500;pumpTransmit();assert(sent().empty());

  // Exercise receiver backpressure, out-of-order retries, duplicate ACKs and
  // byte-for-byte delivery in both directions using the real frame handler.
  for(bool host:{false,true}){
    clean(host);while(uartToLocal.free())uartToLocal.push(0xaa);
    handleFrame(frame(UartData,1,0x11));handleFrame(frame(UartData,2,0x22));
    assert(lastUartSeq==0 && uxQueueMessagesWaiting(txQueue)==0);
    uartToLocal.clear();
    handleFrame(frame(UartData,2,0x22));assert(uartToLocal.size()==0);
    handleFrame(frame(UartData,1,0x11));handleFrame(frame(UartData,1,0x11));
    handleFrame(frame(UartData,2,0x22));
    assert(uartToLocal.size()==2 && uartToLocal.pop()==0x11 && uartToLocal.pop()==0x22);
    handleFrame(frame(UartData,9,0x99));assert(lastUartSeq==2 && uartToLocal.size()==0);
    lastUartSeq=0xfffe;handleFrame(frame(UartData,0xffff,0xfe));
    handleFrame(frame(UartData,0,0xff));handleFrame(frame(UartData,1,0x00));
    assert(uartToLocal.size()==3 && uartToLocal.pop()==0xfe && uartToLocal.pop()==0xff && uartToLocal.pop()==0);
  }
  // A millisecond counter wrap must neither stall nor trigger an early retry.
  mockNow=0xfffffff0U;clean();lastUartBatch=mockNow-1;
  uartToPeer.push(0x77);pumpTransmit();assert(sent().size()==1);
  mockNow+=19;pumpTransmit();assert(sent().empty());
  mockNow++;pumpTransmit();assert(sent().size()==1);

  // Keep DAP progress during a UART retry; don't replace/lose its frame.
  clean();uartToPeer.push(0x33);mockNow++;pumpTransmit();sent();
  mockNow+=20;uint8_t dap[2]={2,1};assert(queueFrame(DapRequest,10,dap,2));
  pumpTransmit();auto mixed=sent();bool sawDap=false,sawUart=false;
  for(auto f:mixed){sawDap|=f.type==DapRequest;sawUart|=f.type==UartData;}
  assert(sawDap && sawUart);
  resetLinkState();assert(uartCount==0 && uartToPeer.size()==0 && uartToLocal.size()==0);
  puts("PASS UART: 1ms batching, 20ms loss retry/backoff, ordered lossless backpressure, duplicates, wraps, DAP progress");
}
