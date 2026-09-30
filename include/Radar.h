#pragma once
#include <Arduino.h>
// Signal radar, after Stevee87's RSSI target tracker: a sweep of nearby Wi-Fi access points,
// Bluetooth LE devices (phones, watches, headphones) and directly heard LoRa nodes, and homing
// on one of them. RSSI measures signal strength only: it gives neither a bearing nor a distance.
struct RadarTarget {
  enum Kind:uint8_t {Wifi,Ble,Lora};
  // Bluetooth device class: from the advertised appearance or the maker's advertising format.
  enum Device:uint8_t {Unknown,Phone,Watch,Audio,Personal};
  Kind kind=Wifi;uint64_t id=0;char name[33]={};
  uint8_t channel=0;bool open=false;Device device=Unknown;uint16_t vendor=0xffff;
  int8_t rssi=-127;uint32_t seen=0;
};
class Radar {
 public:
  static constexpr unsigned MaxTargets=64,HistorySize=120;
  enum WifiState:uint8_t {WifiOff,WifiReady,WifiPortal,WifiBusy,WifiFailed};
  enum BleState:uint8_t {BleOff,BleReady,BleBusy,BleFailed};
  RadarTarget targets[MaxTargets];unsigned count=0;
  bool active=false,tracking=false,dirty=false;WifiState wifi=WifiOff;BleState ble=BleOff;uint32_t sweeps=0;
  // USB diagnostics: last scan start error and result count, scan time, driver scan-done status.
  int16_t scanStart=0,scanResult=0;uint32_t scanMs=0,doneStatus=0,doneNumber=0;
  // Homing on `focus`: the median of recent windows feeds a fast and a slow average; their
  // difference, with hysteresis and a hold time, is the trend. sample(0) is the oldest kept sample.
  RadarTarget focus;float fast=0,slow=0;uint16_t rate=0;float peak=-127;uint32_t samples=0,lastSample=0;
  void open();void close();void tick();
  bool track(unsigned index);void untrack();
  // The Wi-Fi portal/probe or the BLE service/probe takes the radio: stop using it now.
  void release();void releaseBle();
  void resetPeak(){peak=fast;dirty=true;}
  int trend() const; // +1 stronger, -1 weaker, 0 steady, stale or too few samples
  bool fresh() const; // a sample within 1.5 s (Wi-Fi, BLE) or 10 min (LoRa)
  unsigned historyCount() const{return samples<HistorySize?samples:HistorySize;}
  int8_t sample(unsigned i) const;
  unsigned counted(RadarTarget::Kind kind) const;
  unsigned personal(uint32_t within) const; // phones, watches and headphones seen recently
  String json() const;
  // Target table and statistics (src/RadarModel.cpp, also linked by the host preview).
  RadarTarget* upsert(RadarTarget::Kind kind,uint64_t id);
  void prune(uint32_t now);void sort();void addSample(int rssi,uint32_t at);void reset();
  static RadarTarget::Device classify(uint16_t appearance,const uint8_t* maker,size_t makerSize);
 private:
  int8_t history[HistorySize]={},recent[5]={};int8_t trendState=0,candidate=0;uint32_t candidateAt=0;
  bool scanning=false,sniffing=false,bleOwned=false,bleScanning=false;
  uint32_t lastSweep=0,scanAt=0,windowAt=0,rateAt=0,rateFrames=0,loraSeen=0,loraAt=0;
  int32_t windowSum=0;uint32_t windowCount=0;
  void wifiStart();void sniff(bool on);void mergeScan(int n);void refreshLora(uint32_t now);
  void bleTick(uint32_t now);void drainBle(uint32_t now);
};
extern Radar radar;
