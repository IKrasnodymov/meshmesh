#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Board.h"
#include "I18n.h"

// Device role, chosen at boot: the usual chat device, a MeshCore repeater or room server. A change takes a restart.
enum DeviceRole:uint8_t {RoleNormal=0,RoleRepeater=1,RoleRoom=2,RoleCount};
struct Config {
  char name[25]=MM_NODE_NAME;
  float frequency=868.731f, bandwidth=62.5f;
  uint8_t sf=8, cr=6, hops=3, brightness=180;
  int8_t power=10;
  bool relay=true, gps=true, sound=true, batteryVolts=false;
  uint8_t lang=LangEn; // interface language, I18n.h
  uint16_t autoLock=90,dimAfter=30;
  int16_t utcOffset=180;
  uint8_t key[32]={};
  uint32_t bootCounter=0;
  uint8_t role=RoleNormal;
  uint32_t blePin=0; // pairing PIN, made once: a restart (e.g. a USB-UART reset) keeps it
  bool bleOn=false;  // Bluetooth was on: it comes back after a restart
  void load();
  void save();
  bool valid() const;
  String keyHex() const;
  bool setKey(const String& text);
  bool saveRole(uint8_t next); // stored for the next boot; role stays the running one
  void saveBle(bool on);
};
extern Config config;
