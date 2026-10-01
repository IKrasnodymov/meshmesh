#pragma once
// Minimal Cayenne LPP encoder: the subset MeshCore telemetry replies use (voltage, temperature, GPS).
// Same wire format as ElectronicCats/CayenneLPP; that library pulls in ArduinoJson 7, which conflicts
// with the ArduinoJson 6 pinned by MeshMesh.
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#define LPP_TEMPERATURE 103
#define LPP_GPS 136
#define LPP_VOLTAGE 116

class CayenneLPP {
  uint8_t* buffer;
  uint8_t maxsize, cursor=0;
  uint8_t put(uint8_t channel,uint8_t type,const int32_t* values,const uint8_t* sizes,unsigned count){
    unsigned need=2;for(unsigned i=0;i<count;i++)need+=sizes[i];
    if(cursor+need>maxsize)return 0;
    buffer[cursor++]=channel;buffer[cursor++]=type;
    for(unsigned i=0;i<count;i++)for(int b=sizes[i]-1;b>=0;b--)buffer[cursor++]=uint8_t(uint32_t(values[i])>>(8*b));
    return cursor;
  }
 public:
  explicit CayenneLPP(uint8_t size):buffer(new uint8_t[size]),maxsize(size){}
  ~CayenneLPP(){delete[] buffer;}
  CayenneLPP(const CayenneLPP&)=delete;CayenneLPP& operator=(const CayenneLPP&)=delete;
  void reset(){cursor=0;}
  uint8_t getSize() const{return cursor;}
  uint8_t* getBuffer(){return buffer;}
  uint8_t addVoltage(uint8_t channel,float volts){int32_t v=lroundf(volts*100);const uint8_t s=2;return v<0||v>0xffff?0:put(channel,LPP_VOLTAGE,&v,&s,1);}
  uint8_t addTemperature(uint8_t channel,float celsius){int32_t v=lroundf(celsius*10);const uint8_t s=2;return v<-32768||v>32767?0:put(channel,LPP_TEMPERATURE,&v,&s,1);}
  uint8_t addGPS(uint8_t channel,float latitude,float longitude,float meters){int32_t v[3]={int32_t(lroundf(latitude*10000)),int32_t(lroundf(longitude*10000)),int32_t(lroundf(meters*100))};const uint8_t s[3]={3,3,3};return put(channel,LPP_GPS,v,s,3);}
};
