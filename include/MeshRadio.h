#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
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
struct CompanionCore;
struct RemoteAccess;
struct RegionAccess;
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
  // The path (MeshCore path_len: hash size in the top bits, count below; 255: not kept): incoming flood - the
  // repeaters from the sender to us; outgoing direct - the route used; delivered flood - the route returned.
  // Hashes beyond 24 bytes are not kept.
  uint8_t pathLen=255,path[24]={};
  // Incoming: the packet as heard (SNR in quarter dB). heard: copies of the packet heard - incoming: all of them,
  // outgoing: repeaters passing our flood packet on; echo: the last repeater of each copy (other ones, up to 4).
  bool signal=false;int8_t snr=0;int16_t rssi=0;uint8_t heard=0,echoes=0;
  struct Echo {uint8_t hash[3],size;int8_t snr;} echo[4]={};
  uint32_t packet=0; // RAM only: the start of the MeshCore packet hash, to match the copies; 0: none
  bool echoSaved=true; // RAM only: a copy heard since it was last saved
  // RAM only. uptime: the second of this boot it was recorded in while the clock was not set (timestamp 0);
  // the time is filled in when the clock is set. seen: an incoming one read on the screen.
  uint32_t uptime=0;bool seen=false;
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
  uint32_t received=0,delivered=0; // chat messages received and delivery ACKs since boot (the pet counts them)
  float lastRssi=0,lastSnr=0;uint32_t lastRxAt=0;
  // On the heap: in static memory it does not fit the classic ESP32 boards (dram0 segment).
  ChatMessage* const history=new ChatMessage[64];unsigned historyCount=0;
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
  const Peer* findContact(uint64_t id) const {for(unsigned i=0;i<peerCount;i++)if(peers[i].id==id)return &peers[i];return nullptr;}
  bool sendInvite(uint64_t contact,uint64_t channel); // the channel link as a direct message
  int probeHashtag(const String& raw); // stored packets of unjoined channels this hashtag opens; -1: not a hashtag
  bool setChannelRegion(uint64_t id,const String& region); // "", "*" or a region name (Regions.h); saved
  // Region scope of a flood packet: false - none; channel: index in channelList, -1 for the other packets.
  bool scopeKey(int channel,uint8_t key[16]) const;
  String event="Ready";bool dirty=true;
  MeshRadio():radio(new Module(pins::radioCs,pins::radioIrq,pins::radioReset,pins::radioBusy,SPI)) {}
  void begin();void tick();bool applyConfig();
  // Deaf receiver guard: the transceiver is set up again as at boot (reset, calibration, settings)
  // after 10 min without a packet, or by the server's agc.reset.interval; the queue is kept.
  bool recalibrate();uint32_t recalibrations=0;
  // Power off (Power.cpp): delayed writes now, then the transceiver sleeps until the next boot.
  void flush();void sleep();
  bool sendMessage(const String& text,uint64_t destination=meshmesh::Broadcast);
  uint32_t sendGame(const String& text,uint64_t destination); // message ID for delivery, 0 when refused
  bool sendHello();bool sendPosition();bool selfTest();
  String diagnosticFrame() const;bool diagnosticIngest(const uint8_t* data,size_t size);
  uint32_t diagnosticRx=0;bool busy() const;void cancelPending();void restoreHistory();void trimHistory();
  static bool clockSet();
  bool resetPath(uint64_t id);bool removeContact(uint64_t id);
  String idText(uint64_t id) const;String publicKeyText() const;
  // The node key signs rated chess results (ChessRating.h): nullptr / false before the identity is loaded.
  const uint8_t* nodeKey() const;bool nodeSign(uint8_t sig[64],const uint8_t* data,size_t size) const;
  static bool nodeVerify(const uint8_t key[32],const uint8_t sig[64],const uint8_t* data,size_t size);
  bool learnContact(const uint8_t key[32],const char* name); // a chat contact from its key (a tournament opponent)
  unsigned messageLimit(uint64_t destination=meshmesh::Broadcast) const;
  String routeText(const ChatMessage& m,bool brief=false) const; // e.g. "via 2 rpt · 2/3"; empty when unknown
  // A copy of a flood packet heard again (the mesh tables): a message of the history counts it as a repeat.
  void echo(uint32_t packet,uint8_t pathLen,const uint8_t* path);
  // The path, signal and repeats of a message in JSON (the history file and /api/messages); read back by pathRead.
  static void pathJson(JsonObject j,const ChatMessage& m);static void pathRead(JsonObjectConst j,ChatMessage& m);
 private:
  friend class MeshCoreBackend;friend class MeshCoreRadioAdapter;friend struct CompanionCore;friend struct RemoteAccess;friend struct RegionAccess;
  MeshCoreBackend* core=nullptr;
  struct Pending {bool active=false,started=false;ChatMessage message;uint32_t due=0,ack[3]={},hash=0,wireTimestamp=0;uint8_t attempts=0,route[3]={},hops[3]={};
  // app: sent for a companion app, which retries itself: one attempt with its timestamp and attempt byte.
  bool app=false;uint8_t appAttempt=0;
  // Its region, fixed when queued (an app may choose another right after): scoped - with scopeKey.
  bool scoped=false;uint8_t scopeKey[16]={};} pending[4];
  uint32_t sequence=0,autoHelloDue=0;
  bool transmitting=false;
  uint8_t lastFrame[255]={};size_t lastFrameSize=0;
  uint32_t queue(const String& text,uint64_t destination,bool game);
  bool startRadio(bool quiet);int16_t startReceiving();void addMessage(const ChatMessage& m,bool persist=true);
  void status(uint32_t id,ChatMessage::Status value,bool txError=false);void track(const Pending& wait,unsigned attempt,bool delivered=false,uint8_t pathLen=255,const uint8_t* path=nullptr);uint32_t echoDue=0;void tickEchoes();void persist(const ChatMessage& m);void stampLate();unsigned unstamped=0;
  Peer* contact(uint64_t id);
  bool historyProtected(const ChatMessage& m) const;void historyErase(unsigned index);void compactHistoryTick();
  uint32_t historyGeneration=0;bool compactRequested=false;
  struct Sample {uint8_t length=0,data[184]={};} samples[6];unsigned nextSample=0; // packets of unjoined channels
  uint32_t seenGroup[16]={};unsigned nextSeenGroup=0;
  void loadChannels();bool saveChannels();void noteChannel(const uint8_t* payload,size_t length,uint32_t packetHash);
};
extern MeshRadio meshRadio;
void radioIrqPending(); // light sleep: the IRQ line rose while interrupts were held
// A small file replaced whole through a temporary copy (MeshMesh storage); false without that storage.
bool readStored(const char* path,const char* temp,void* out,size_t size);
bool writeStored(const char* path,const char* temp,const void* data,size_t size);
