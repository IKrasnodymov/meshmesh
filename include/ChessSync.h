#pragma once
#include "Modules.h"
#include <Arduino.h>
// Exchange of signed chess results between nodes (docs/chess.md, «Обмен журналами»). A record carries both
// players' keys and signatures, so a node checks it by itself and accepts it from any contact; with the same
// records every node shows the same ratings, also for games it did not play. Two nodes compare a digest of
// their newest records, then the record numbers, and send each other the missing records (three messages
// each, ten records a session at most). A sync starts when a rated player is heard (every 6 h per node) or
// on request. Messages are handled in tick(), not in the radio callback.
namespace ledger {
constexpr unsigned Newest=64,PerSession=10,MaxPeers=16,Inbox=8,Outbox=36,Parts=4;
struct PeerState {uint64_t id;uint32_t lastAt,startedAt;uint16_t got,sent;bool invSent;};
class Ledger {
 public:
  PeerState peers[MaxPeers]={};
  uint16_t received=0;String event;uint32_t events=0;
  bool receive(uint64_t from,const char* name,const char* text); // true when it was a ledger message
  void tick();
  bool start(uint64_t peer);   // sync with one node now
  unsigned startAll();         // with every rated player among the contacts heard in the last day
  unsigned synced() const;     // nodes compared with since the start
  String json() const;
 private:
  struct In {uint64_t peer;char text[152];bool used;};
  struct Out {uint64_t peer;char text[152];bool used;};
  struct Part {uint64_t peer;uint32_t id;uint8_t mask;char b64[300];uint32_t at;bool used;};
  // On the heap only during a session (classic ESP32 boards have no static RAM to spare).
  struct Buffers {In inbox[Inbox];Out outbox[Outbox];Part parts[Parts];};Buffers* buf=nullptr;
  uint32_t sendAt=0,autoAt=0,busyAt=0;
  bool buffers();
  PeerState* state(uint64_t peer,bool make);
  void queue(uint64_t peer,const String& text);
  void handle(uint64_t from,const char* text);
  void sendInventory(uint64_t peer);
  void sendRecord(uint64_t peer,uint32_t id);
  void gotPart(uint64_t from,const char* args);
};
extern Ledger exchange;
}
