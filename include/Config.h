#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Board.h"
#include "I18n.h"

// Device role, chosen at boot: the usual chat device, a MeshCore repeater or room server. A change takes a restart.
enum DeviceRole:uint8_t {RoleNormal=0,RoleRepeater=1,RoleRoom=2,RoleCount};
// The radio parameters shared by a network (a profile of the networks catalogue): frequency, bandwidth, SF, CR.
struct RadioProfile {
  float frequency=0,bandwidth=0;uint8_t sf=0,cr=0;
  bool set() const {return frequency>0;}
};
struct Config {
  char name[25]=MM_NODE_NAME;
  float frequency=868.731f, bandwidth=62.5f;
  uint8_t sf=8, cr=6, hops=3, brightness=180;
  uint8_t pathHash=2; // bytes per hop in the path of our flood packets, 1..3 (MeshCore path hash mode + 1)
  int8_t power=10;
  bool relay=true, gps=true, sound=true, batteryVolts=false;
  uint8_t lang=LangEn; // interface language, I18n.h
  uint16_t autoLock=90,dimAfter=30;
  int16_t utcOffset=180;
  uint8_t key[32]={};
  uint32_t bootCounter=0;
  uint8_t role=RoleNormal;
  uint32_t blePin=0; // pairing PIN, made once: a restart (e.g. a USB-UART reset) keeps it
  bool bleOn=false;  // Bluetooth on after boot: the last choice, else on in the normal role
  bool lockDetails=true; // the locked screen names chess opponents, moves and message senders
  uint8_t notifyLed=1,notifyPopup=1;
#if defined(MM_COMPACT)
  uint8_t notifyWake=1;
#else
  uint8_t notifyWake=0; // preserve the keyboard boards' existing screen-off behaviour
#endif
  bool notifyFailed=false;char notifyWords[97]="";
  char apps[160]=""; // screen menu: order of the apps, "-" hides one (App.h); empty = the default
  // Default region of our flood packets (Regions.h): empty - none; the key is that of "#name" unless an app set
  // another (a private region of the MeshCore app).
  char region[31]="";uint8_t regionKey[16]={};
  // The profile before the last change of the shared radio parameters ("radio previous" returns to it), and
  // during a test ("radio try") the saved one: the test lives in memory only, a restart brings the saved one back.
  RadioProfile previous,trial;
  bool netAuto=true; // the network search runs by itself near a catalogue place with other profiles (NetFind.h)
  bool doubleBack=false; // one-button boards: a double click steps back (single clicks then wait for a second one)
  RadioProfile radio() const {RadioProfile r;r.frequency=frequency;r.bandwidth=bandwidth;r.sf=sf;r.cr=cr;return r;}
  void setRadio(const RadioProfile& r){frequency=r.frequency;bandwidth=r.bandwidth;sf=r.sf;cr=r.cr;}
  bool sameRadio(const RadioProfile& r) const {return fabsf(frequency-r.frequency)<.0005f&&bandwidth==r.bandwidth&&sf==r.sf&&cr==r.cr;}
  void load();
  bool save();
  bool valid() const;
  String keyHex() const;
  bool setKey(const String& text);
  bool saveRole(uint8_t next); // stored for the next boot; role stays the running one
  void saveBle(bool on);
};
extern Config config;
