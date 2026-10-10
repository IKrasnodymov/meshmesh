#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: not speed-critical (AGENTS.md, "Языки")
#endif
#include "Hardware.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
// The day before this firmware was built (UTC, from __DATE__): no correct clock is earlier.
static uint32_t buildFloor(){
  static const char months[]="JanFebMarAprMayJunJulAugSepOctNovDec";const char* d=__DATE__;
  int m=(strstr(months,String(d).substring(0,3).c_str())-months)/3+1,day=atoi(d+4),y=atoi(d+7);
  y-=m<=2;int era=y/400,yoe=y-era*400,doy=(153*(m+(m>2?-3:9))+2)/5+day-1,doe=yoe*365+yoe/4-yoe/100+doy;
  return uint32_t(era*146097+doe-719468-1)*86400U;
}
#if !defined(MM_NRF52) && !defined(MM_UI_PREVIEW)
#include <esp_attr.h>
// The ESP32 keeps the system time over a reset (not over a power cycle). Its source is kept beside it:
// a time of an unknown source (a firmware before this one) or before the build is not kept.
RTC_NOINIT_ATTR static uint32_t keptMagic;RTC_NOINIT_ATTR static char keptSource[8];
static constexpr uint32_t KeptMagic=0x4d4d4b31;
#endif
void Hardware::beginClock(){Preferences p;if(p.begin("meshmesh-clock",true)){clockTrusted=p.getBool("trusted",false);p.end();}
#if !defined(MM_NRF52) && !defined(MM_UI_PREVIEW)
  if(time(nullptr)>=1735689600){
    if(keptMagic==KeptMagic&&memchr(keptSource,0,sizeof keptSource)&&keptSource[0]&&uint32_t(time(nullptr))>=buildFloor()){clockSource=keptSource;utc=time(nullptr);}
    else{timeval tv={0,0};settimeofday(&tv,nullptr);}
  }
#endif
}
bool Hardware::setUtc(uint32_t epoch,const char* source,bool persist){
  if(epoch<1735689600U||epoch>2147483647U)return false;
  time_t current=time(nullptr);bool gnss=!strcmp(source,"GPS");
  // A fix does not prove the receiver's date: GNSS signals here are spoofed (a fix of 10 satellites dated
  // 8 July 2026 in October). A date before the firmware's build is wrong, as is one that disagrees with a
  // clock set from the phone or NTP; the position of such a receiver is not used either (gpsFix).
  if((gnss||!strcmp(source,"RTC"))&&epoch<buildFloor()){if(gnss)clockConflict=true;return false;}
  if(gnss&&(clockTrusted||clockSource=="manual"||clockSource=="NTP")&&current>=1735689600){int64_t delta=int64_t(epoch)-current;if(delta>300||delta<-300){clockConflict=true;return false;}}
  timeval tv={time_t(epoch),0};
#if !defined(MM_UI_PREVIEW)
  if(settimeofday(&tv,nullptr))return false;
#endif
  utc=epoch;clockSource=source;clockConflict=false;
  // A fresh phone/NTP sync must not briefly make an already conflicting GPS position usable.
  if(!gnss&&gpsTime()){DateTime fix(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());int64_t delta=int64_t(fix.unixtime())-epoch;clockConflict=fix.unixtime()<buildFloor()||delta>300||delta<-300;}
  clockSyncAt=millis()?millis():1;
#if !defined(MM_NRF52) && !defined(MM_UI_PREVIEW)
  strlcpy(keptSource,source,sizeof keptSource);keptMagic=KeptMagic;
#endif
  if(persist){clockTrusted=true;if(rtcOk){rtc.adjust(DateTime(epoch));rtcValid=true;Preferences p;if(p.begin("meshmesh-clock",false)){p.putBool("trusted",true);p.end();}}}
  return true;
}
String Hardware::clockInfo(){StaticJsonDocument<512>d;d["unix"]=int64_t(time(nullptr));d["source"]=clockSource;d["trusted"]=clockTrusted;d["gps_conflict"]=clockConflict;d["build_floor"]=buildFloor();d["rtc"]=rtcOk;d["rtc_valid"]=rtcValid;
  if(gps.date.isValid()){d["gps_year"]=gps.date.year();d["gps_month"]=gps.date.month();d["gps_day"]=gps.date.day();d["gps_date_age"]=gps.date.age();}
  d["gps_satellites"]=gps.satellites.value();d["gps_location"]=gps.location.isValid();if(gps.location.isValid())d["gps_location_age"]=gps.location.age();
  if(gps.time.isValid()){d["gps_hour"]=gps.time.hour();d["gps_minute"]=gps.time.minute();d["gps_second"]=gps.time.second();d["gps_time_age"]=gps.time.age();}
  String s;serializeJson(d,s);return s;
}

bool Hardware::gpsFix(){return gps.location.isValid()&&gps.location.age()<10000&&gps.date.isValid()&&gps.date.age()<10000&&gps.date.year()>=2025&&!clockConflict;}
// Without satellites a receiver reports a "fix" with the date of its own clock or a default one
// (an L76K on the Heltec V4 indoors: 8 July 2026, 0 satellites): the time needs a navigation fix.
bool Hardware::gpsTime(){return gps.date.isValid()&&gps.time.isValid()&&gps.time.age()<10000&&gps.date.age()<10000&&gps.location.isValid()&&gps.location.age()<10000&&gps.date.year()>=2025
  &&gps.satellites.isValid()&&gps.satellites.age()<10000&&gps.satellites.value()>=3;}
