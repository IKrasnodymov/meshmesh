#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>
// GPS receivers on community boards: the published TX/RX pairs disagree for some boards and
// the modules ship at different rates. Each pin and rate is listened to (receive only, the
// ESP never drives the module's TX line) until NMEA sentences pass their checksums.
class GpsProbe {
 public:
  GpsProbe(int pinA,int pinB,uint32_t firstRate):pins{pinA,pinB},rates{firstRate,firstRate==9600?38400u:9600u,firstRate==115200?38400u:115200u} {}
  bool locked=false;
  void open(TinyGPSPlus& gps) {
    unsigned n=pins[1]>=0?2:1;int rx=pins[attempt%n];uint32_t rate=rates[(attempt/n)%3];
    Serial1.end();Serial1.begin(rate,SERIAL_8N1,rx,-1);startedAt=millis();checked=gps.passedChecksum();
  }
  void close() {Serial1.end();}
  // Reads pending bytes into gps; returns the number read.
  unsigned tick(TinyGPSPlus& gps) {
    unsigned read=0,budget=1024;while(Serial1.available()&&budget--){gps.encode(Serial1.read());read++;}
    if(!locked&&gps.passedChecksum()>checked+2){locked=true;Serial.printf("GPS NMEA on GPIO%d\n",pins[attempt%(pins[1]>=0?2:1)]);}
    if(!locked&&millis()-startedAt>3000){attempt=(attempt+1)%6;open(gps);}
    return read;
  }
 private:
  int pins[2];uint32_t rates[3];unsigned attempt=0;uint32_t startedAt=0,checked=0;
};
