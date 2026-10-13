#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cstdio>
#include <deque>
#include <vector>
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define pdTRUE 1
#define SERIAL_8N1 0
#define SERIAL_8N2 16
#define SERIAL_8E1 14
#define SERIAL_8E2 30
extern uint32_t mockNow;
inline uint32_t millis(){return mockNow;}
inline uint32_t readResetReason(){return 0;}
inline uint32_t micros(){return mockNow*1000;}
inline void delay(unsigned n){mockNow+=n;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline void taskYIELD(){}
struct MockFicr{uint32_t DEVICEID[2]={123,456};};
extern MockFicr* NRF_FICR;
struct MockQueue{size_t capacity,width;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=MockQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t width){return new MockQueue{n,width,{}};}
inline int xQueueSend(QueueHandle_t q,const void* p,int){
  if(q->items.size()==q->capacity)return 0;
  auto b=static_cast<const uint8_t*>(p);q->items.emplace_back(b,b+q->width);return 1;
}
inline int xQueueSendToFront(QueueHandle_t q,const void* p,int){
  if(q->items.size()==q->capacity)return 0;
  auto b=static_cast<const uint8_t*>(p);q->items.emplace_front(b,b+q->width);return 1;
}
struct MockWorkerIdle{};
static bool mockWorkerStep=false;
inline int xQueueReceive(QueueHandle_t q,void* p,int){
  if(q->items.empty()){if(mockWorkerStep)throw MockWorkerIdle{};return 0;}
  memcpy(p,q->items.front().data(),q->width);q->items.pop_front();return 1;
}
inline void xQueueReset(QueueHandle_t q){q->items.clear();}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q){return q->items.size();}
inline int xQueuePeek(QueueHandle_t q,void* p,int){
  if(q->items.empty())return 0;
  memcpy(p,q->items.front().data(),q->width);return 1;
}
#define pdMS_TO_TICKS(n) (n)
#define TASK_PRIO_LOW 1
#define TASK_PRIO_NORMAL 2
extern int mockTaskPriority;
inline void xTaskCreate(void(*)(void*),const char*,unsigned,void*,int priority,void*){mockTaskPriority=priority;}
inline void taskENTER_CRITICAL(){}
inline void taskEXIT_CRITICAL(){}
inline void vTaskDelay(unsigned){}
struct MockSerial{
  int rxPin=-1,txPin=-1;void setPins(int rx,int tx){rxPin=rx;txPin=tx;}
  uint32_t baud=0;uint16_t config=0;unsigned starts=0;
  std::deque<uint8_t> rx;std::vector<uint8_t> tx;
  void begin(uint32_t b,uint16_t c=0){baud=b;config=c;starts++;}
  void end(){}
  int available(){return rx.size();}
  int availableForWrite(){return 1024;}
  int read(){auto b=rx.front();rx.pop_front();return b;}
  size_t write(const uint8_t* p,size_t n){tx.insert(tx.end(),p,p+n);return n;}
};
extern MockSerial Serial,Serial2;
