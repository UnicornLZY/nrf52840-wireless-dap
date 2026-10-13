#pragma once
#include "Arduino.h"
struct cdc_line_coding_t{uint32_t bit_rate;uint8_t stop_bits,parity,data_bits;};
extern cdc_line_coding_t mockCoding;
extern bool mockMounted,mockCdc;
extern std::deque<uint8_t> mockUsbRx;
extern std::vector<uint8_t> mockUsbTx;
struct MockUsbDevice{
  unsigned detaches=0,attaches=0;int vid=0,pid=0;
  void detach(){detaches++;mockMounted=false;mockCdc=false;}
  void attach(){attaches++;}
  bool mounted(){return mockMounted;}
  void setID(int v,int p){vid=v;pid=p;}
  void setProductDescriptor(const char*){}
  void setSerialDescriptor(const char*){}
};
extern MockUsbDevice TinyUSBDevice;
struct Adafruit_USBD_WebUSB{void setStringDescriptor(const char*){} void begin(){}};
inline void tud_vendor_read_flush(){}
inline bool tud_vendor_mounted(){return mockMounted;}
#if WD_DAP128
inline unsigned tud_vendor_write_available(){return 256;}
#else
inline unsigned tud_vendor_write_available(){return 64;}
#endif
extern std::vector<uint8_t> mockVendorTx;
inline unsigned tud_vendor_n_write_available(uint8_t){return tud_vendor_write_available();}
inline unsigned tud_vendor_write(const void* p,unsigned n){
  const auto b=static_cast<const uint8_t*>(p);mockVendorTx.insert(mockVendorTx.end(),b,b+n);return n;
}
inline void tud_vendor_flush(){}
inline unsigned tud_cdc_n_available(int){return mockUsbRx.size();}
inline unsigned tud_cdc_n_read(int,void* p,unsigned){
  if(mockUsbRx.empty())return 0;
  *static_cast<uint8_t*>(p)=mockUsbRx.front();mockUsbRx.pop_front();return 1;
}
inline bool tud_cdc_n_connected(int){return mockMounted && mockCdc;}
inline bool tud_cdc_n_ready(int){return mockMounted;}
inline unsigned tud_cdc_n_write_available(int){return 64;}
inline unsigned tud_cdc_n_write(int,const void* p,unsigned n){
  const auto b=static_cast<const uint8_t*>(p);mockUsbTx.insert(mockUsbTx.end(),b,b+n);return n;
}
inline void tud_cdc_n_write_flush(int){}
inline void tud_cdc_n_read_flush(int){mockUsbRx.clear();}
inline void tud_cdc_n_get_line_coding(int,cdc_line_coding_t* p){*p=mockCoding;}

