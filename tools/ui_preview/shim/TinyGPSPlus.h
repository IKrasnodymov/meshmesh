#pragma once
#include "Arduino.h"
// Values are set directly by the preview scenario.
struct TinyGPSLocation{bool valid=false;double la=0,lo=0;bool isValid() const{return valid;}uint32_t age() const{return valid?500:UINT32_MAX;}double lat() const{return la;}double lng() const{return lo;}};
struct TinyGPSInteger{uint32_t v=0;uint32_t value() const{return v;}bool isValid() const{return true;}uint32_t age() const{return 500;}};
struct TinyGPSHDOP{double v=0;double hdop() const{return v;}bool isValid() const{return v>0;}};
struct TinyGPSDate{bool valid=false;bool isValid() const{return valid;}uint32_t age() const{return 500;}uint16_t y=2026;uint8_t m=7,d=8;uint16_t year() const{return y;}uint8_t month() const{return m;}uint8_t day() const{return d;}};
struct TinyGPSTime{bool valid=false;uint8_t h=0,m=0,s=0;bool isValid() const{return valid;}uint32_t age() const{return 500;}uint8_t hour() const{return h;}uint8_t minute() const{return m;}uint8_t second() const{return s;}};
class TinyGPSPlus{public:TinyGPSLocation location;TinyGPSInteger satellites;TinyGPSHDOP hdop;TinyGPSDate date;TinyGPSTime time;uint32_t sentences=0;uint32_t passedChecksum() const{return sentences;}};
