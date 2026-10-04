#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <Mm1Packet.h>
#include "Board.h"
#include "BoardPins.h"
#include "Channels.h"
#if defined(MM_RADIO_SX1262)
using DeviceRadio=SX1262;
#elif defined(MM_RADIO_SX1276)
using DeviceRadio=SX1276;
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
// A channel heard on air that this node has not joined: its hash byte and, when a common or probed
// hashtag opened one of its packets, that name.
struct HeardChannel {uint8_t hash=0;uint16_t packets=0;uint32_t at=0;char name[channels::NameBytes+1]={};};
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
  // Group channels: [0] is Public; the others are kept in NVS. Removing one keeps its history, joining it again shows it.
  channels::Channel channelList[channels::Max];unsigned channelCount=0;
  HeardChannel heard[8];unsigned heardCount=0,heardSamples=0;
  bool contactsSaved=true; // the last save of the contact list reached storage
  enum ChannelResult:uint8_t {ChannelAdded,ChannelExists,ChannelFull,ChannelBadName,ChannelBadKey,ChannelBadLink,ChannelUnavailable,ChannelStorage};
  ChannelResult addChannel(const String& name,const uint8_t secret[16],uint64_t* id=nullptr);
  ChannelResult joinHashtag(const String& raw,uint64_t* id=nullptr);
  ChannelResult joinLink(const String& text,uint64_t* id=nullptr); // a meshcore://channel/add link, also inside a message
  ChannelResult createChannel(const String& name,uint64_t* id=nullptr); // a new random key
  bool removeChannel(uint64_t id); // never Public; not while a message to it waits
  bool sending(uint64_t id) const; // a message to it waits to be sent
  const channels::Channel* channel(uint64_t id) const;
  int channelIndex(uint64_t id) const;
  bool sendInvite(uint64_t contact,uint64_t channel); // the channel link as a direct message
  int probeHashtag(const String& raw); // stored packets of unjoined channels this hashtag opens; -1: not a hashtag
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
  uint32_t sequence=0,autoHelloDue=0;
  bool transmitting=false;
  uint8_t lastFrame[255]={};size_t lastFrameSize=0;
  uint32_t queue(const String& text,uint64_t destination,bool game);
  int16_t startReceiving();void addMessage(const ChatMessage& m,bool persist=true);
  void status(uint32_t id,ChatMessage::Status value);void track(const Pending& wait,unsigned attempt,bool delivered=false);void persist(const ChatMessage& m);
  Peer* contact(uint64_t id);
  struct Sample {uint8_t length=0,data[184]={};} samples[6];unsigned nextSample=0; // packets of unjoined channels
  uint32_t seenGroup[16]={};unsigned nextSeenGroup=0;
  void loadChannels();bool saveChannels();void noteChannel(const uint8_t* payload,size_t length,uint32_t packetHash);
};
extern MeshRadio meshRadio;
void radioIrqPending(); // light sleep: the IRQ line rose while interrupts were held
// A small file replaced whole through a temporary copy (MeshMesh storage); false without that storage.
bool readStored(const char* path,const char* temp,void* out,size_t size);
bool writeStored(const char* path,const char* temp,const void* data,size_t size);
