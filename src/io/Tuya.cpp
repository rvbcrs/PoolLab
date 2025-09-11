#include "Tuya.h"
#include "domain/Metrics.h"

namespace io {

static uint8_t DP_TEMP=8, DP_ORP=131, DP_PH=106, DP_ORP_ALT1=122, DP_PH_ALT1=118;

void tuyaConfigure(uint8_t dpTemp, uint8_t dpOrp, uint8_t dpPh, uint8_t dpOrpAlt1, uint8_t dpPhAlt1){
  DP_TEMP=dpTemp; DP_ORP=dpOrp; DP_PH=dpPh; DP_ORP_ALT1=dpOrpAlt1; DP_PH_ALT1=dpPhAlt1;
}

struct Parser {
  enum { HDR1,HDR2,VER,CMD,LH,LL,DATA,CK } st = HDR1;
  uint8_t ver=0, cmd=0; uint16_t len=0, idx=0; static const size_t MAX=600; uint8_t buf[MAX];
  void reset(){ st=HDR1; len=idx=0; }
  void feed(uint8_t b){
    using namespace domain;
    switch(st){
      case HDR1: st=(b==0x55)?HDR2:HDR1; break;
      case HDR2: st=(b==0xAA)?VER:HDR1; break;
      case VER: ver=b; st=CMD; break;
      case CMD: cmd=b; st=LH; break;
      case LH: len=((uint16_t)b<<8); st=LL; break;
      case LL: len|=b; idx=0; st=(len?DATA:CK); break;
      case DATA: if (idx<MAX) buf[idx++]=b; if (idx>=len) st=CK; break;
      case CK: {
        uint32_t sum=0; sum+=0x55; sum+=0xAA; sum+=ver; sum+=cmd; sum+=(uint8_t)(len>>8); sum+=(uint8_t)(len&0xFF);
        for(uint16_t i=0;i<len;i++) sum+=buf[i];
        (void)sum;
        if (len>=4){
          uint16_t p=0; bool phSet=false, orpSet=false, tempSet=false;
          while (p+4<=len){
            uint8_t dpid=buf[p++]; uint8_t dtype=buf[p++]; uint16_t dl=((uint16_t)buf[p++]<<8)|buf[p++];
            if (p+dl>len) break;
            if (dtype==0x02 && dl>=4){
              uint32_t v=((uint32_t)buf[p]<<24)|((uint32_t)buf[p+1]<<16)|((uint32_t)buf[p+2]<<8)|buf[p+3];
              auto &M = Metrics::instance();
              if (dpid==DP_TEMP && !tempSet){ M.tempC = v/10.0f; M.haveTemp=true; tempSet=true; }
              else if (dpid==DP_PH){ M.phVal = v/100.0f; M.havePh=true; phSet=true; M.preferPhPrimary=true; }
              else if (dpid==DP_PH_ALT1 && !phSet && !Metrics::instance().preferPhPrimary){ M.phVal = v/100.0f; M.havePh=true; phSet=true; }
              else if (dpid==DP_ORP && !orpSet){ M.orpMv = (int32_t)v*1.0f; M.haveOrp=true; orpSet=true; }
              else if (dpid==DP_ORP_ALT1 && !orpSet){ M.orpMv = (int32_t)v*1.0f; M.haveOrp=true; orpSet=true; }
            }
            p+=dl;
          }
        }
        reset(); break;
      }
    }
  }
};

static Parser pa, pb;

void tuyaFeedA(uint8_t b){ pa.feed(b); }
void tuyaFeedB(uint8_t b){ pb.feed(b); }

} // namespace io


