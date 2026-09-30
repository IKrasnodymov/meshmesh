#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct Config {
#if defined(MM_HELTEC_V4)
  char name[25]="Heltec V4";
#else
  char name[25]="M9";
#endif
  float frequency=868.731f, bandwidth=62.5f;
  uint8_t sf=8, cr=6, hops=3, brightness=180;
  int8_t power=10;
  bool relay=true, gps=true, sound=true, russian=false;
  uint16_t autoLock=90,dimAfter=30;
  int16_t utcOffset=180;
  uint8_t key[32]={};
  uint32_t bootCounter=0;
  void load();
  void save();
  bool valid() const;
  String keyHex() const;
  bool setKey(const String& text);
};
extern Config config;
