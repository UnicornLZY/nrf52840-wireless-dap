#pragma once
#include <stddef.h>
// Count physical USB completions across the entire logical response.
struct UsbDapTxState {
  volatile size_t remaining=0;
  void begin(size_t n){remaining=n;}
  void complete(size_t n){
    // A trailing ZLP from the previous response cannot finish a new one.
    if(!n)return;
    remaining=n>=remaining?0:remaining-n;
  }
  void clear(){remaining=0;}
  bool isBusy()const{return remaining!=0;}
};
