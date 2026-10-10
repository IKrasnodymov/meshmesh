#pragma once
#include "Arduino.h"
#include <time.h>
class DateTime{uint32_t stamp;public:explicit DateTime(uint32_t epoch):stamp(epoch){}DateTime(uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint8_t minute,uint8_t s){tm value={};value.tm_year=y-1900;value.tm_mon=m-1;value.tm_mday=d;value.tm_hour=h;value.tm_min=minute;value.tm_sec=s;stamp=timegm(&value);}uint32_t unixtime()const{return stamp;}};
class RTC_PCF8563{public:void adjust(const DateTime&){};};
