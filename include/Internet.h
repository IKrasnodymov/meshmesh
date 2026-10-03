#pragma once
#include <Arduino.h>
// Wi-Fi client with internet access (M9): saved networks, NTP time, web map tiles
// and approximate location by IP. It yields the Wi-Fi radio to the access point,
// the radar and the USB Wi-Fi probe, and reconnects when they release it.
// Network I/O and PNG decoding run on a worker task; SD stays on the loop task
// because SD, display and LoRa share one SPI bus.
#if defined(MM_COMPACT)
class Internet {
 public:
  bool online() const {return false;}
  void yieldRadio() {}
  String address() {return String();}
};
#else
class Internet {
 public:
  enum State {Off,NoNetwork,Scanning,Connecting,Online,Paused};
  static const unsigned MaxSaved=5,MaxSeen=16;
  struct Seen {char ssid[33];int8_t rssi;bool open,saved;};
  State state=Off;bool enabled=false,dirty=false,timeSynced=false;
  String ssid,error;
  Seen seen[MaxSeen];unsigned seenCount=0;uint32_t scannedAt=0;
  uint32_t tiles=0,tileErrors=0,bytes=0;
  void begin();void tick();
  bool online() const {return state==Online;}
  // Someone else is about to reconfigure Wi-Fi: drop our association now.
  void yieldRadio();
  void setEnabled(bool on);void rescan();
  bool save(const String& name,const String& password);bool forget(const String& name);
  unsigned savedCount();String savedName(unsigned i);bool isSaved(const String& name);
  void connectTo(const String& name);
  String info();String command(const String& line);String stateText();
  String address(); // the client's IP while online; the device page answers there too
  // Tile and location jobs; one at a time. Results stay owned by the worker
  // until the loop calls release().
  bool idle();bool fetchTile(int z,int x,int y);bool locate();
  enum Result {None,TileOk,TileFailed,Located,LocateFailed};
  Result result(int& z,int& x,int& y,const uint16_t*& pixels,double& lat,double& lon);
  void release();
  String tileUrl();bool setTileUrl(const String& value);
  bool backingOff();
 private:
  bool owned=false,scanning=false;uint32_t stateAt=0,retryAt=0,lostAt=0,backoffUntil=0;
  unsigned candidate=0;char candidates[MaxSaved][33]={};unsigned candidateCount=0;String preferred;
  void startScan();void finishScan();void nextCandidate();void stop(bool radioOff);
  String names[MaxSaved];
  void load(unsigned i,String& name,String& password);void loadNames();bool store(String* list,String* words,unsigned n);
};
#endif
extern Internet internet;
