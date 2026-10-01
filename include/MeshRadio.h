#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <Mm1Packet.h>
#include "BoardPins.h"
#if defined(MM_HELTEC_V4)
using DeviceRadio=SX1262;
#else
using DeviceRadio=LR1110;
#endif
class MeshCoreBackend;
class MeshCoreRadioAdapter;
struct ChatMessage {
  uint64_t source=0,destination=meshmesh::Broadcast;
  uint32_t session=0,id=0,timestamp=0;
  char name[25]={},text[161]={};
  bool outgoing=false;
  uint8_t protocol=2; // 1: archived MM/1; 2: MeshCore
  bool game=false;    // a chess command: sent like a message, kept out of the chat history
  enum Status:uint8_t { Received,Queued,Sent,Delivered,Failed } status=Received;
  // How it travelled: outgoing - the current attempt, then the one that got the ACK; incoming - the received packet.
  // Flood hops of a delivered message are those of the path returned with the ACK. 255: not known.
  enum Route:uint8_t { RouteNone,RouteDirect,RouteFlood } route=RouteNone;
  uint8_t hops=255,tries=0;
};
struct Peer {
  uint64_t id=0;char name[25]={};uint32_t seen=0;
  float rssi=0,snr=0,latitude=0,longitude=0;
  uint8_t hops=0;bool position=false,heard=false;
  uint8_t publicKey[32]={},type=0,pathLength=255;
};
class MeshRadio {
 public:
  DeviceRadio radio;
  bool ready=false;int16_t radioError=0;
  uint64_t nodeId=0;uint32_t networkId=0,rxCount=0,txCount=0,rejected=0,relayed=0,replaced=0; // replaced: contacts overwritten by new nodes
  float lastRssi=0,lastSnr=0;uint32_t lastRxAt=0;
  ChatMessage history[64];unsigned historyCount=0;
  Peer peers[24];unsigned peerCount=0;
  String event="Ready";bool dirty=true;
  MeshRadio():radio(new Module(pins::radioCs,pins::radioIrq,pins::radioReset,pins::radioBusy,SPI)) {}
  void begin();void tick();bool applyConfig();
  bool sendMessage(const String& text,uint64_t destination=meshmesh::Broadcast);
  uint32_t sendGame(const String& text,uint64_t destination); // message ID for delivery, 0 when refused
  bool sendHello();bool sendPosition();bool selfTest();
  String diagnosticFrame() const;bool diagnosticIngest(const uint8_t* data,size_t size);
  uint32_t diagnosticRx=0;bool busy() const;void cancelPending();void restoreHistory();
  bool resetPath(uint64_t id);bool removeContact(uint64_t id);
  String idText(uint64_t id) const;String publicKeyText() const;
  unsigned messageLimit(uint64_t destination=meshmesh::Broadcast) const;
  String routeText(const ChatMessage& m,bool brief=false) const; // e.g. "via 2 rpt · 2/3"; empty when unknown
 private:
  friend class MeshCoreBackend;friend class MeshCoreRadioAdapter;
  MeshCoreBackend* core=nullptr;
  struct Pending {bool active=false,started=false;ChatMessage message;uint32_t due=0,ack[3]={},hash=0,wireTimestamp=0;uint8_t attempts=0,route[3]={},hops[3]={};} pending[4];
  uint32_t sequence=0,lastHello=0,autoHelloDue=0;
  bool transmitting=false;
  uint8_t lastFrame[255]={};size_t lastFrameSize=0;
  uint32_t queue(const String& text,uint64_t destination,bool game);
  int16_t startReceiving();void addMessage(const ChatMessage& m,bool persist=true);
  void status(uint32_t id,ChatMessage::Status value);void track(const Pending& wait,unsigned attempt,bool delivered=false);void persist(const ChatMessage& m);
  Peer* contact(uint64_t id);
};
extern MeshRadio meshRadio;
