#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Board.h"

struct Config {
  char name[25]=MM_NODE_NAME;
  float frequency=868.731f, bandwidth=62.5f;
  uint8_t sf=8, cr=6, hops=3, brightness=180;
  int8_t power=10;
  bool relay=true, gps=true, sound=true, russian=false, batteryVolts=false;
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
