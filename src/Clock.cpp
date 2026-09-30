#include "Hardware.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
void Hardware::beginClock(){Preferences p;if(p.begin("meshmesh-clock",true)){clockTrusted=p.getBool("trusted",false);p.end();}}
bool Hardware::setUtc(uint32_t epoch,const char* source,bool persist){
  if(epoch<1735689600U||epoch>2147483647U)return false;
  time_t current=time(nullptr);
  // A fresh position alone does not prove the receiver's date is correct.
  // Do not let a conflicting GNSS date overwrite a clock explicitly set by user.
  if(!strcmp(source,"GPS")&&clockTrusted&&current>=1735689600){int64_t delta=int64_t(epoch)-current;if(delta>300||delta<-300){clockConflict=true;return false;}}
  timeval tv={time_t(epoch),0};if(settimeofday(&tv,nullptr))return false;
  utc=epoch;clockSource=source;clockConflict=false;
  if(persist){clockTrusted=true;if(rtcOk){rtc.adjust(DateTime(epoch));rtcValid=true;Preferences p;if(p.begin("meshmesh-clock",false)){p.putBool("trusted",true);p.end();}}}
  return true;
}
String Hardware::clockInfo(){StaticJsonDocument<512>d;d["unix"]=int64_t(time(nullptr));d["source"]=clockSource;d["trusted"]=clockTrusted;d["gps_conflict"]=clockConflict;d["rtc"]=rtcOk;d["rtc_valid"]=rtcValid;
  if(gps.date.isValid()){d["gps_year"]=gps.date.year();d["gps_month"]=gps.date.month();d["gps_day"]=gps.date.day();d["gps_date_age"]=gps.date.age();}
  if(gps.time.isValid()){d["gps_hour"]=gps.time.hour();d["gps_minute"]=gps.time.minute();d["gps_second"]=gps.time.second();d["gps_time_age"]=gps.time.age();}
  String s;serializeJson(d,s);return s;
}

bool Hardware::gpsFix(){return gps.location.isValid()&&gps.location.age()<10000&&gps.date.isValid()&&gps.date.age()<10000&&gps.date.year()>=2025&&!clockConflict;}
