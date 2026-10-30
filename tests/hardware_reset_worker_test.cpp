#define WD_ROLE_FREE_TEST 1
#define WD_HW_RESET_TEST 1
#define main unused_baseline_main
#include "../role_test.cpp"
#undef main
static void executeWorker(){mockWorkerStep=true;try{dapWorker(nullptr);}catch(const MockWorkerIdle&){}mockWorkerStep=false;}
int main(){
  wirelessSetup();hostMode=false;localMode=true;
  Frame job={};job.type=DapRequest;job.session=LocalUsbSession;job.epoch=dapEpoch;
  job.len=1;job.data[0]=0x0a;
  uint32_t before=mockNow;
  xQueueSend(dapJobs,&job,0);executeWorker();
  Frame result={};assert(xQueueReceive(dapResults,&result,0));
  assert(result.len==3 && result.data[0]==0x0a && result.data[1]==0 && result.data[2]==1);
  assert(mockNow==before && mockSoftwareResetWrites==1);
  assert(mockP1.IN==0 && mockP1.DIRSET==0); // No reset wire or pulse required.
  // Disconnect, Info, stale jobs and link teardown must not pulse NRST.
  before=mockNow;job.data[0]=3;xQueueSend(dapJobs,&job,0);executeWorker();
  job.data[0]=0;job.len=2;job.data[1]=0xff;xQueueSend(dapJobs,&job,0);executeWorker();
  job.data[0]=0x0a;job.epoch=dapEpoch-1;xQueueSend(dapJobs,&job,0);executeWorker();
  resetLinkState();assert(mockNow==before);
  // Original software reset also remains active on the wireless worker.
  localMode=false;ready=true;session=77;job.session=session;job.epoch=dapEpoch;job.len=1;
  unsigned swBefore=mockSoftwareResetWrites;
  xQueueSend(dapJobs,&job,0);executeWorker();assert(mockNow==before);
  assert(mockSoftwareResetWrites==swBefore+1);
  // Direct AIRCR requests retain the original SWD operation without a pulse.
  uint8_t aircr[23]={5,0,4,8,0,0,0,0,1,0x12,0,0,0x23,5,0x0c,0xed,0,0xe0,13,4,0,0xfa,5};
  job.len=sizeof(aircr);memcpy(job.data,aircr,sizeof(aircr));
  before=mockNow;xQueueSend(dapJobs,&job,0);executeWorker();assert(mockNow==before);
  job.data[19]=0;before=mockNow;xQueueSend(dapJobs,&job,0);executeWorker();assert(mockNow==before);
  xQueueReset(dapResults);
  // Restore the captured Keil Reset and Run fallback: DHCSR debug-off,
  // DEMCR clear, then NRST low/high commands. It must still issue SYSRESETREQ
  // even with the physical P1.13 input low/unconnected in the model.
  uint8_t teardown[13]={5,0,2,5,0xf0,0xed,0,0xe0,13,0,0,0x5f,0xa0};
  job.len=13;memcpy(job.data,teardown,13);xQueueSend(dapJobs,&job,0);
  teardown[4]=0xfc;memset(teardown+9,0,4);memcpy(job.data,teardown,13);xQueueSend(dapJobs,&job,0);
  memset(job.data,0,sizeof(job.data));job.len=7;job.data[0]=0x10;job.data[2]=0x80;xQueueSend(dapJobs,&job,0);
  job.data[1]=0x80;xQueueSend(dapJobs,&job,0);
  swBefore=mockSoftwareResetWrites;before=mockNow;executeWorker();
  assert(mockSoftwareResetWrites==swBefore+1 && mockNow==before);
  xQueueReset(dapResults);
  // A failed software-reset verification must retain the original failure
  // response; a nonexistent NRST wire must never fabricate success.
  job.len=1;job.data[0]=0x0a;mockSoftwareResetFails=true;
  xQueueSend(dapJobs,&job,0);executeWorker();
  assert(xQueueReceive(dapResults,&result,0));
  assert(result.data[1]==0xff && result.data[2]==0);mockSoftwareResetFails=false;
  // Execute both jobs in one worker lifetime: no ResetTarget, AIRCR or
  // SWJ_Pins command, and Disconnect has already disabled the SWD port.
  for(unsigned wired=0;wired<2;wired++){
    localMode=wired;ready=true;session=77;
    job={};job.type=DapRequest;job.epoch=dapEpoch;
    job.session=wired?LocalUsbSession:session;
    job.len=13;job.data[0]=6;job.data[2]=2;job.data[4]=13;
    xQueueSend(dapJobs,&job,0);
    job.len=1;job.data[0]=3;xQueueSend(dapJobs,&job,0);
    before=mockNow;executeWorker();assert(mockNow-before==20);
    assert(DAP_Data.debug_port==0);
    xQueueReset(dapResults);
    // Read-only sessions don't get the automatic pulse.
    job.len=5;job.data[0]=6;job.data[4]=15;xQueueSend(dapJobs,&job,0);
    job.len=1;job.data[0]=3;xQueueSend(dapJobs,&job,0);
    before=mockNow;executeWorker();assert(mockNow==before);
    xQueueReset(dapResults);
  }
  puts("PASS real worker: original four-wire software reset + Keil teardown fallback/failure status preserved; extra 20ms P1.13 auto reset wired/wireless");
}
