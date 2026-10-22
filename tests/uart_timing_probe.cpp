#define WD_ROLE_FREE_TEST 1
#define main unused_role_main
#ifdef UART_BASELINE
#include "../../unified_v3_free_recovery/role_test.cpp"
#else
#include "../role_test.cpp"
#endif
#undef main
int main(){
  wirelessSetup();mockNow=1000;hostMode=true;localMode=false;resetLinkState();
  ready=true;session=42;rfShouldConnect=true;uartConfigAcked=true;
  lastUartBatch=mockNow;uartToPeer.push(0x55);rfSent.clear();
  unsigned first=0,second=0;Parser parser;Frame f={};
  for(unsigned dt=0;dt<=600;dt++){
    mockNow=1000+dt;pumpTransmit();
    for(auto b:rfSent)if(parser.feed(b,f) && f.type==UartData){
      assert(f.seq==1 && f.len==1 && f.data[0]==0x55);
      if(!first)first=dt;else {second=dt;break;}
    }
    rfSent.clear();if(second)break;
  }
#ifdef UART_BASELINE
  assert(first==4 && second-first==500);
  printf("BASELINE ");
#else
  assert(first==1 && second-first==20);
  printf("CANDIDATE ");
#endif
  printf("model: first short frame %u ms, lost-ACK retry %u ms\n",first,second-first);
}
