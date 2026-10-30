#pragma once
#include <stddef.h>
#include <stdint.h>

// Keil download-session heuristic, NOT proof that Flash programming succeeded.
// A successful bulk data write arms one pulse at an explicit DAP_Disconnect.
// Radio/USB disappearance and idle time never call this completion path.
class DownloadSessionReset {
  bool written_=false,failed_=false;
public:
  void clear(){written_=failed_=false;}
  void resetIssued(){written_=false;}
  bool observe(const uint8_t* q,size_t n,const uint8_t* r,size_t rn){
    if(!n || !rn){failed_=true;return false;}
    if(q[0]==2){clear();return false;}
    if(q[0]==3){
      bool pulse=written_ && !failed_ && rn>=2 && r[0]==3 && r[1]==0;
      clear();return pulse;
    }
    if(q[0]==0x0a || (q[0]==0x10 && n>=3 && (q[2]&0x80) && !(q[1]&0x80))){
      resetIssued();return false;
    }
    if(q[0]==5){
      if(n<3 || rn<3 || r[0]!=5 || r[1]!=q[2] || r[2]!=1)failed_=true;
    }
    if(q[0]==6){
      if(n<5 || rn<4 || r[0]!=6 || r[1]!=q[2] || r[2]!=q[3] || r[3]!=1){
        failed_=true;return false;
      }
      unsigned count=q[2]|(unsigned(q[3])<<8);
      // At least two words of AP DRW data, not reads or DP register setup.
      if(q[1]==0 && q[4]==0x0d && count>=2 && count<=(n-5)/4)written_=true;
    }
    return false;
  }
};
