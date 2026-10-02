#include "MeshRadio.h"
#include "Config.h"
#include "Hardware.h"
#include <helpers/BaseChatMesh.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <Preferences.h>
#include <LittleFS.h>
#if defined(MM_NRF52)
using HistoryFs=MeshFS; // no SD card; a smaller log in the 100 KB internal storage
static constexpr size_t historySegment=12*1024;
static HistoryFs* historyFs(){return hardware.fsOk?&LittleFS:nullptr;}
#else
#include <SD.h>
using HistoryFs=fs::FS;
static constexpr size_t historySegment=256*1024;
static HistoryFs* historyFs(){return hardware.sdOk?static_cast<fs::FS*>(&SD):hardware.fsOk?static_cast<fs::FS*>(&LittleFS):nullptr;}
#endif
#include <ArduinoJson.h>
#include <esp_system.h>
#include <bootloader_random.h>
#include <time.h>
#include <SHA256.h>
#include "ChessNet.h"
#include "MeshServer.h"
MeshRadio meshRadio;
#if defined(MM_RADIO_SX1262)
constexpr uint32_t irqTxDone=RADIOLIB_SX126X_IRQ_TX_DONE,irqPreamble=RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED,irqRxDone=RADIOLIB_SX126X_IRQ_RX_DONE;
#elif defined(MM_RADIO_SX1276)
// SX127x has no preamble flag in LoRa mode; a valid header marks a packet being received.
constexpr uint32_t irqTxDone=RADIOLIB_SX127X_CLEAR_IRQ_FLAG_TX_DONE,irqPreamble=RADIOLIB_SX127X_CLEAR_IRQ_FLAG_VALID_HEADER,irqRxDone=RADIOLIB_SX127X_CLEAR_IRQ_FLAG_RX_DONE;
#else
constexpr uint32_t irqTxDone=RADIOLIB_LR11X0_IRQ_TX_DONE,irqPreamble=RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED,irqRxDone=RADIOLIB_LR11X0_IRQ_RX_DONE;
#endif
static volatile bool radioIrq=false;
static void IRAM_ATTR onRadioIrq(){radioIrq=true;}
static uint64_t aliasOf(const uint8_t* key){uint64_t n=0;for(unsigned i=0;i<8;i++)n=(n<<8)|key[i];return n;}
static void copyUtf8(char* dest,const char* src,size_t cap){size_t n=strnlen(src,cap);if(n>=cap){n=cap-1;while(n&&(uint8_t(src[n])&0xc0)==0x80)n--;}memcpy(dest,src,n);dest[n]=0;}
class MeshCoreRadioAdapter:public mesh::Radio {
 MeshRadio& owner;
 public:
 explicit MeshCoreRadioAdapter(MeshRadio& o):owner(o){}
 int recvRaw(uint8_t* bytes,int cap) override {
  if(owner.transmitting||!radioIrq)return 0;radioIrq=false;
  int result=0;size_t n=owner.radio.getPacketLength();
  if((owner.radio.getIrqFlags()&irqRxDone)&&n>=2&&n<=size_t(cap)){
   int rc=owner.radio.readData(bytes,n);if(!rc){owner.lastRssi=owner.radio.getRSSI();owner.lastSnr=owner.radio.getSNR();owner.lastRxAt=millis();owner.rxCount++;result=n;}else owner.rejected++;
  }else owner.rejected++;
  owner.radioError=owner.startReceiving();owner.dirty=true;return result;
 }
 uint32_t getEstAirtimeFor(int len) override{return max(uint32_t(1),uint32_t(owner.radio.getTimeOnAir(len)/1000));}
 float packetScore(float snr,int) override{return constrain((snr+20.0f)/30.0f,0.0f,1.0f);}
 bool startSendRaw(const uint8_t* bytes,int len) override {
  radioIrq=false;owner.radioError=owner.radio.startTransmit(bytes,len);
  owner.transmitting=owner.radioError==0;if(owner.transmitting){memcpy(owner.lastFrame,bytes,len);owner.lastFrameSize=len;}else owner.startReceiving();return owner.transmitting;
 }
 bool isSendComplete() override{return owner.transmitting&&radioIrq&&(owner.radio.getIrqFlags()&irqTxDone);}
 void onSendFinished() override{owner.radioError=owner.radio.finishTransmit();owner.transmitting=false;radioIrq=false;owner.startReceiving();}
 bool isInRecvMode() const override{return owner.ready&&!owner.transmitting;}
 bool isReceiving() override{return !owner.transmitting&&(owner.radio.getIrqFlags()&irqPreamble);}
 float getLastRSSI() const override{return owner.lastRssi;}
 float getLastSNR() const override{return owner.lastSnr;}
};
class CoreMillis:public mesh::MillisecondClock{public:unsigned long getMillis() override{return millis();}};
class CoreRandom:public mesh::RNG{public:void random(uint8_t* bytes,size_t n) override{esp_fill_random(bytes,n);}};
class CoreRtc:public mesh::RTCClock{uint32_t floor=1735689600U,baseMillis=0;public:uint32_t getCurrentTime() override{return max(uint32_t(max(time_t(0),time(nullptr))),uint32_t(floor+(millis()-baseMillis)/1000));}void setCurrentTime(uint32_t value) override{floor=max(floor,value);baseMillis=millis();}};
static CoreMillis coreMillis;static CoreRandom coreRandom;static CoreRtc coreRtc;
static StaticPoolPacketManager corePool(24);static SimpleMeshTables coreTables;
static MeshCoreRadioAdapter coreRadio(meshRadio);
// One identity for every role. A stored key must match its private half; a new one avoids the reserved hashes.
static bool loadIdentity(Preferences& p,mesh::LocalIdentity& self,bool& created){
 uint8_t identity[96]={};created=false;bool valid;
 if(p.getBytesLength("identity")==96){p.getBytes("identity",identity,96);self.readFrom(identity,96);mesh::LocalIdentity derived;derived.readFrom(identity,64);valid=derived.matches(self)&&mesh::LocalIdentity::validatePrivateKey(identity);}
 else{bootloader_random_enable();do{self=mesh::LocalIdentity(&coreRandom);}while(self.pub_key[0]==0||self.pub_key[0]==255);bootloader_random_disable();self.writeTo(identity,96);valid=created=p.putBytes("identity",identity,96)==96;}
 memset(identity,0,sizeof(identity));return valid;
}
// Keep advert timestamps monotonic across resets even without an RTC.
static void restoreClockFloor(Preferences& p){uint32_t last=max(p.getUInt("last_advert",0),p.getUInt("last_tx",0));if(last<2147483647U)coreRtc.setCurrentTime(last+1);}
class MeshCoreBackend:public BaseChatMesh {
 MeshRadio& owner;
 uint32_t contactsDue=0;
 struct Forwarded {uint8_t hash[8]={};uint32_t at=0;} forwarded[32];unsigned nextForwarded=0;
 struct SavedContact {uint8_t key[32];char name[32];uint8_t type,pathLength,path[64];uint32_t advert;int32_t lat,lon;};
 struct ContactBlob {uint32_t version=1,count=0;SavedContact contacts[24]={};uint8_t hash[32]={};};
 struct ReceivedId{uint64_t source=0;uint32_t stamp=0,hash=0;} received[128];unsigned nextReceived=0;
 bool duplicate(uint64_t source,uint32_t stamp,const char* text){uint32_t hash;mesh::Utils::sha256((uint8_t*)&hash,4,(const uint8_t*)text,strlen(text));for(const auto& r:received)if(r.source==source&&r.stamp==stamp&&r.hash==hash)return true;received[nextReceived]={source,stamp,hash};nextReceived=(nextReceived+1)%128;return false;}
 void receiveMessage(uint64_t source,uint64_t dest,uint32_t stamp,const char* name,const char* text,const mesh::Packet* packet,uint8_t hops){size_t n=strnlen(text,MAX_TEXT_LEN+1);if(!n||n>MAX_TEXT_LEN||!meshmesh::validUtf8((const uint8_t*)text,n)){owner.rejected++;return;}if(duplicate(source,stamp,text))return;for(unsigned i=0;i<owner.historyCount;i++){const auto& old=owner.history[i];if(old.protocol==2&&!old.outgoing&&old.source==source&&old.session==stamp&&!strcmp(old.text,text))return;}
  // Chess commands come only from direct messages of keyed contacts, never from the channel.
  if(dest!=meshmesh::Broadcast&&chessNet.receive(source,name,text))return;
 ChatMessage m;m.source=source;m.destination=dest;m.timestamp=uint32_t(time(nullptr));m.session=stamp;m.route=packet->isRouteFlood()?ChatMessage::RouteFlood:ChatMessage::RouteDirect;m.hops=hops;mesh::Utils::sha256((uint8_t*)&m.id,4,(const uint8_t*)text,n);copyUtf8(m.name,name,sizeof(m.name));copyUtf8(m.text,text,sizeof(m.text));owner.addMessage(m);hardware.beep();owner.event="New message from "+String(m.name);}
 void updateContact(const ContactInfo& c,bool heard,uint8_t hops=255){ // hops 255: not known, kept
  if(!meshmesh::validUtf8((const uint8_t*)c.name,strnlen(c.name,sizeof(c.name)))){owner.rejected++;return;}
  uint64_t id=aliasOf(c.id.pub_key);Peer* p=owner.contact(id);if(!p){if(owner.peerCount>=24)return;p=&owner.peers[owner.peerCount++];*p={};p->id=id;}
  // Short UI IDs are aliases only. Full keys are always used for cryptography.
  if(p->type&&memcmp(p->publicKey,c.id.pub_key,32)){owner.rejected++;owner.event="Contact ID collision";return;}
  memcpy(p->publicKey,c.id.pub_key,32);copyUtf8(p->name,c.name,sizeof(p->name));p->type=c.type;p->pathLength=c.out_path_len;p->position=c.gps_lat||c.gps_lon;p->latitude=c.gps_lat/1e6f;p->longitude=c.gps_lon/1e6f;
  if(heard){p->heard=true;p->seen=millis();p->rssi=owner.lastRssi;p->snr=owner.lastSnr;if(hops!=255)p->hops=hops;}owner.dirty=true;
 }
 // A full table makes room for a new node: MeshCore overwrites the contact heard longest ago that
 // is not a favourite. Favourites here are chat nodes and any node with chat history or a chess game.
 uint32_t protectedAt=0;
 bool keep(uint64_t id,uint8_t type){
  if(type==ADV_TYPE_CHAT)return true;
  for(unsigned i=0;i<owner.historyCount;i++){auto& m=owner.history[i];if(m.source==id||m.destination==id)return true;}
  for(auto& g:chessNet.matches)if(g.state!=ChessMatch::Free&&g.peer==id)return true;
  return false;
 }
 void protectContacts(){for(int i=0;i<getNumContacts();i++){ContactInfo c;if(!getContactByIdx(i,c))continue;ContactInfo* p=lookupContactByPubKey(c.id.pub_key,32);if(p)p->flags=(p->flags&~1)|(keep(aliasOf(p->id.pub_key),p->type)?1:0);}protectedAt=millis();}
 protected:
 bool shouldOverwriteWhenFull() const override{return true;}
 void onContactOverwrite(const uint8_t* key) override{
  Peer* p=owner.contact(aliasOf(key));if(p){unsigned i=p-owner.peers;memmove(owner.peers+i,owner.peers+i+1,sizeof(Peer)*(owner.peerCount-i-1));owner.peerCount--;owner.peers[owner.peerCount]={};}
  owner.replaced++;contactsDue=millis()+2000;owner.dirty=true;
 }
 void onDiscoveredContact(ContactInfo& c,bool,uint8_t pathLen,const uint8_t*) override{updateContact(c,true,pathLen&63);contactsDue=millis()+2000;}
 ContactInfo* processAck(const uint8_t* data) override{
  uint32_t ack;memcpy(&ack,data,4);
  for(auto& wait:owner.pending)if(wait.active&&wait.started)for(unsigned i=0;i<wait.attempts;i++)if(wait.ack[i]==ack){
   Peer* p=owner.contact(wait.message.destination);ContactInfo* c=p?lookupContactByPubKey(p->publicKey,32):nullptr;if(!c)return nullptr;
   // The ACK packet itself is not at hand: its signal and hops may be a repeater's. Only a zero-hop direct
   // delivery proves the node still hears us directly.
   wait.active=false;updateContact(*c,false);if(wait.route[i]==ChatMessage::RouteDirect&&!wait.hops[i]){p->heard=true;p->seen=millis();p->hops=0;}
   owner.track(wait,i,true);owner.status(wait.message.id,ChatMessage::Delivered);if(!wait.message.game){owner.event="Delivered to "+String(c->name);hardware.beep();}return c;
  }return nullptr;
 }
 void onContactPathUpdated(const ContactInfo& c) override{updateContact(c,false);contactsDue=millis()+2000;}
 // A flood packet carries the hops it travelled; a direct one arrives with its path consumed, so the
 // (symmetric) path to the sender stands for it.
 static uint8_t hopsOf(const ContactInfo& c,const mesh::Packet* packet){return packet->isRouteFlood()?packet->getPathHashCount():c.out_path_len==OUT_PATH_UNKNOWN?255:c.out_path_len&63;}
 void onMessageRecv(const ContactInfo& c,mesh::Packet* packet,uint32_t timestamp,const char* text) override{uint8_t hops=hopsOf(c,packet);updateContact(c,true,hops);receiveMessage(aliasOf(c.id.pub_key),owner.nodeId,timestamp,c.name,text,packet,hops);}
 void onCommandDataRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*) override{}
 void onSignedMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const uint8_t*,const char*) override{}
 uint32_t calcFloodTimeoutMillisFor(uint32_t airtime) const override{return 15000+airtime*4+config.hops*5000;}
 uint32_t calcDirectTimeoutMillisFor(uint32_t airtime,uint8_t path) const override{return 10000+airtime*4*(1+(path&63));}
 void onSendTimeout() override{} // Facade owns four pending sends and bounded retries.
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet* packet,uint32_t stamp,const char* text) override{
  const char* split=strstr(text,": ");String name=split?String(text).substring(0,split-text):"Public";
  uint8_t hash[32];mesh::Utils::sha256(hash,32,(const uint8_t*)name.c_str(),name.length());uint64_t source=aliasOf(hash);
  // Channel sender names are unverified; never turn them into keyed contacts.
  receiveMessage(source,meshmesh::Broadcast,stamp,name.c_str(),split?split+2:text,packet,packet->isRouteFlood()?packet->getPathHashCount():255);
 }
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*) override{return 0;}
 void onContactResponse(const ContactInfo&,const uint8_t*,uint8_t) override{}
 bool allowPacketForward(const mesh::Packet* p) override{bool allowed=config.relay&&config.hops>0&&p->getPathHashCount()<config.hops;if(allowed){auto& f=forwarded[nextForwarded];p->calculatePacketHash(f.hash);f.at=millis();nextForwarded=(nextForwarded+1)%32;}return allowed;}
 int calcRxDelay(float,uint32_t) const override{return 0;}
 void logTx(mesh::Packet* packet,int) override{
  owner.txCount++;uint8_t hash[8];packet->calculatePacketHash(hash);uint32_t id;memcpy(&id,hash,4);
  bool own=false;for(auto& p:owner.pending)if(p.active&&p.started&&p.hash==id){owner.status(p.message.id,ChatMessage::Sent);own=true;if(p.message.destination==meshmesh::Broadcast)p.active=false;}
  if(!own)for(auto& f:forwarded)if(f.at&&millis()-f.at<60000&&!memcmp(f.hash,hash,8)){owner.relayed++;f.at=0;break;}owner.dirty=true;
 }
 void logTxFail(mesh::Packet* packet,int) override{uint8_t hash[8];packet->calculatePacketHash(hash);uint32_t id;memcpy(&id,hash,4);for(auto& p:owner.pending)if(p.active&&p.hash==id){p.active=false;owner.status(p.message.id,ChatMessage::Failed);}owner.radioError=owner.radioError?owner.radioError:RADIOLIB_ERR_TX_TIMEOUT;owner.event="Radio TX error "+String(owner.radioError);owner.dirty=true;}
 public:
 bool identitySaved=false,announce=false;
 MeshCoreBackend(MeshRadio& o):BaseChatMesh(coreRadio,coreMillis,coreRandom,coreRtc,corePool,coreTables),owner(o){}
 bool initialize(){
  Preferences p;if(!p.begin("meshmesh-mc",false))return false;bool created;
  if(!loadIdentity(p,self_id,created)){p.end();return false;}announce=created;identitySaved=true;
  owner.nodeId=aliasOf(self_id.pub_key);addChannel("Public","izOH6cXN6mrJ5e26oRXNcg==");ChannelDetails channel;getChannel(0,channel);mesh::Utils::sha256((uint8_t*)&owner.networkId,4,channel.channel.secret,16);
  // Like a stock companion, the node announces itself on its own only when a new key, a new name or a return
  // from the repeater role has not been announced yet; otherwise adverts are manual (ADV, position, "hello").
  if(p.getString("adv_name","")!=config.name||p.getUChar("adv_type",ADV_TYPE_CHAT)!=ADV_TYPE_CHAT)announce=true;
  restoreClockFloor(p);
  if(p.getBytesLength("contacts")==sizeof(ContactBlob)){
   ContactBlob* blob=new ContactBlob;p.getBytes("contacts",blob,sizeof(*blob));uint8_t digest[32];mesh::Utils::sha256(digest,32,(const uint8_t*)blob,offsetof(ContactBlob,hash));
   if(blob->version==1&&blob->count<=24&&!memcmp(digest,blob->hash,32))for(unsigned i=0;i<blob->count;i++){auto& saved=blob->contacts[i];if(!mesh::Packet::isValidPathLen(saved.pathLength)&&saved.pathLength!=OUT_PATH_UNKNOWN)continue;ContactInfo c={};c.id=mesh::Identity(saved.key);memcpy(c.name,saved.name,32);c.name[31]=0;if(!meshmesh::validUtf8((const uint8_t*)c.name,strlen(c.name)))continue;c.type=saved.type;c.out_path_len=saved.pathLength;memcpy(c.out_path,saved.path,64);c.last_advert_timestamp=saved.advert;c.lastmod=min(saved.advert,coreRtc.getCurrentTime());c.gps_lat=saved.lat;c.gps_lon=saved.lon;addContact(c);updateContact(c,false);} // last advert: the order of replacement
   delete blob;
  }p.end();begin();return true;
 }
 void saveContacts(){ContactBlob* blob=new ContactBlob;blob->count=min(getNumContacts(),24);for(unsigned i=0;i<blob->count;i++){ContactInfo c;if(!getContactByIdx(i,c))continue;auto& dest=blob->contacts[i];memcpy(dest.key,c.id.pub_key,32);memcpy(dest.name,c.name,32);dest.type=c.type;dest.pathLength=c.out_path_len;memcpy(dest.path,c.out_path,64);dest.advert=c.last_advert_timestamp;dest.lat=c.gps_lat;dest.lon=c.gps_lon;}mesh::Utils::sha256(blob->hash,32,(const uint8_t*)blob,offsetof(ContactBlob,hash));Preferences p;if(p.begin("meshmesh-mc",false)){p.putBytes("contacts",blob,sizeof(*blob));p.end();}delete blob;contactsDue=0;}
 bool reserveStamp(uint32_t stamp){Preferences p;if(!p.begin("meshmesh-mc",false))return false;bool saved=p.putUInt("last_tx",stamp)==4;p.end();return saved;}
 bool advertise(bool requirePosition=false){if(!identitySaved||!config.bootCounter)return false;if(requirePosition&&!hardware.gpsFix())return false;coreRtc.setCurrentTime(coreRtc.getCurrentTimeUnique());auto* pkt=config.gps&&hardware.gpsFix()?createSelfAdvert(config.name,hardware.gps.location.lat(),hardware.gps.location.lng()):createSelfAdvert(config.name);if(!pkt)return false;uint32_t stamp=meshmesh::get32(pkt->payload+32);Preferences p;if(!p.begin("meshmesh-mc",false)){releasePacket(pkt);return false;}bool saved=p.putUInt("last_advert",stamp)==4;if(saved){p.putString("adv_name",config.name);p.putUChar("adv_type",ADV_TYPE_CHAT);}p.end();if(!saved){releasePacket(pkt);return false;}sendFlood(pkt);return true;}
 // A relayed path came with a path return and is used as is (the retries flood). "Direct" holds only while the
 // node is heard without relays: within 30 minutes and 5 dB over the SF floor (nodes do not announce periodically).
 bool pathTrusted(const ContactInfo& c){
  if(c.out_path_len&63)return true;Peer* p=owner.contact(aliasOf(c.id.pub_key));
  return p&&p->heard&&p->hops==0&&millis()-p->seen<1800000&&p->snr>=-2.5f*(config.sf-4)+5;
 }
 bool startMessage(MeshRadio::Pending& wait){
  mesh::Packet* packet=nullptr;unsigned attempt=wait.attempts;
  if(wait.message.destination==meshmesh::Broadcast){ChannelDetails channel;getChannel(0,channel);uint8_t bytes[5+MAX_TEXT_LEN]={};meshmesh::put32(bytes,wait.wireTimestamp);String text=String(config.name)+": "+wait.message.text;memcpy(bytes+5,text.c_str(),text.length());packet=createGroupDatagram(PAYLOAD_TYPE_GRP_TXT,channel.channel,bytes,5+text.length());}
  else{Peer* p=owner.contact(wait.message.destination);if(!p)return false;ContactInfo* c=lookupContactByPubKey(p->publicKey,32);if(!c||c->type!=ADV_TYPE_CHAT){owner.event="Contact is not a chat node";return false;}
   // Stock MeshCore layout. Keep timestamp/text stable for retries; attempt changes the ACK/hash.
   uint8_t bytes[5+MAX_TEXT_LEN]={};meshmesh::put32(bytes,wait.wireTimestamp);bytes[4]=attempt&3;size_t n=strlen(wait.message.text);memcpy(bytes+5,wait.message.text,n);mesh::Utils::sha256((uint8_t*)&wait.ack[attempt],4,bytes,5+n,self_id.pub_key,32);packet=createDatagram(PAYLOAD_TYPE_TXT_MSG,c->id,c->getSharedSecret(self_id),bytes,5+n);
   // Only the first attempt follows a trusted path; the retries flood, so a broken route does not lose the
   // message and the receiver's flood reply carries the new path back.
   if(packet){uint8_t hash[8];packet->calculatePacketHash(hash);memcpy(&wait.hash,hash,4);uint32_t airtime=coreRadio.getEstAirtimeFor(packet->getRawLength());
    bool viaPath=!attempt&&c->out_path_len!=OUT_PATH_UNKNOWN&&pathTrusted(*c);wait.route[attempt]=viaPath?ChatMessage::RouteDirect:ChatMessage::RouteFlood;wait.hops[attempt]=viaPath?c->out_path_len&63:255;
    if(viaPath){sendDirect(packet,c->out_path,c->out_path_len);wait.due=millis()+calcDirectTimeoutMillisFor(airtime,c->out_path_len);}else{sendFlood(packet);wait.due=millis()+calcFloodTimeoutMillisFor(airtime);}}
  }
  if(!packet)return false;
  if(wait.message.destination==meshmesh::Broadcast){uint8_t hash[8];packet->calculatePacketHash(hash);memcpy(&wait.hash,hash,4);wait.route[0]=ChatMessage::RouteFlood;wait.hops[0]=255;sendFlood(packet);wait.due=millis()+45000;}
  owner.track(wait,attempt);wait.started=true;wait.attempts++;return true;
 }
 // Path reset and removal are stock MeshCore contact operations; the next advert re-adds a removed node.
 bool resetPath(const uint8_t* key){ContactInfo* c=lookupContactByPubKey(key,32);if(!c)return false;resetPathTo(*c);saveContacts();return true;}
 bool forget(const uint8_t* key){ContactInfo* c=lookupContactByPubKey(key,32);if(!c||!removeContact(*c))return false;saveContacts();return true;}
 void tick(){if(!protectedAt||millis()-protectedAt>=5000)protectContacts();loop();if(contactsDue&&int32_t(millis()-contactsDue)>=0)saveContacts();}
};
#if defined(MM_RADIO_SX1276)
// SX127x routes interrupts to fixed DIO pins: only RX done (DIO0) is used, CRC is checked by readData.
int16_t MeshRadio::startReceiving(){return radio.startReceive();}
#else
int16_t MeshRadio::startReceiving(){constexpr uint32_t mask=(1UL<<RADIOLIB_IRQ_RX_DONE)|(1UL<<RADIOLIB_IRQ_CRC_ERR)|(1UL<<RADIOLIB_IRQ_HEADER_ERR)|(1UL<<RADIOLIB_IRQ_TIMEOUT);return radio.startReceive(UINT32_MAX,RADIOLIB_IRQ_RX_DEFAULT_FLAGS,mask);}
#endif
String MeshRadio::idText(uint64_t id) const{if(id==meshmesh::Broadcast)return "ALL";char b[17];snprintf(b,sizeof(b),"%04lX%08lX",(unsigned long)(id>>32),(unsigned long)(id&0xffffffffu));return b;}
static const mesh::LocalIdentity* selfIdentity(MeshCoreBackend* core){return core?&core->self_id:meshServer.identity();}
String MeshRadio::publicKeyText() const{auto* self=selfIdentity(core);if(!self)return "";char out[65];mesh::Utils::toHex(out,self->pub_key,32);return out;}
unsigned MeshRadio::messageLimit(uint64_t destination) const{return destination==meshmesh::Broadcast?min(151U,unsigned(MAX_TEXT_LEN-strlen(config.name)-2)):151;}
bool MeshRadio::resetPath(uint64_t id){Peer* p=contact(id);if(!p||!core||!core->resetPath(p->publicKey))return false;p->pathLength=255;dirty=true;return true;}
bool MeshRadio::removeContact(uint64_t id){
 Peer* p=contact(id);if(!p||!core)return false;for(auto& wait:pending)if(wait.active&&wait.message.destination==id)return false;
 if(!core->forget(p->publicKey))return false;unsigned i=p-peers;memmove(peers+i,peers+i+1,sizeof(Peer)*(peerCount-i-1));peerCount--;peers[peerCount]={};dirty=true;return true;
}
Peer* MeshRadio::contact(uint64_t id){for(unsigned i=0;i<peerCount;i++)if(peers[i].id==id)return &peers[i];return nullptr;}
void MeshRadio::begin(){
 nodeId=ESP.getEfuseMac();if(!applyConfig()){restoreHistory();return;}
 if(config.role!=RoleNormal){ // MeshCore repeater or room server: no chat contacts or messages, the same key
  Preferences p;mesh::LocalIdentity self;bool created,loaded=p.begin("meshmesh-mc",false)&&loadIdentity(p,self,created);if(loaded)restoreClockFloor(p);p.end();
  if(!loaded||!meshServer.begin(config.role,coreRadio,coreMillis,coreRandom,coreRtc,corePool,coreTables,self)){ready=false;event="MeshCore identity storage error";}else{nodeId=aliasOf(self.pub_key);event=meshServer.room()?"MeshCore room server ready":"MeshCore repeater ready";}
 } else {core=new MeshCoreBackend(*this);if(!core->initialize()){ready=false;event="MeshCore identity storage error";}}
 restoreHistory();if(ready&&core&&core->announce)autoHelloDue=millis()+3000+esp_random()%2000;
}
bool MeshRadio::busy() const{return transmitting||corePool.getOutboundTotal()>0;}
void MeshRadio::cancelPending(){for(auto& p:pending)if(p.active){status(p.message.id,ChatMessage::Failed);p.active=false;}while(corePool.getOutboundTotal()){auto* packet=corePool.removeOutboundByIdx(0);corePool.free(packet);}}
bool MeshRadio::applyConfig(){
 if(transmitting)return false;cancelPending();
 uint16_t preamble=config.sf<=8?32:16;
#if defined(MM_RADIO_SX1276)
 radioError=radio.begin(config.frequency,config.bandwidth,config.sf,config.cr,RADIOLIB_SX127X_SYNC_WORD,config.power,preamble,0);
 if(radioError){delay(150);radioError=radio.begin(config.frequency,config.bandwidth,config.sf,config.cr,RADIOLIB_SX127X_SYNC_WORD,config.power,preamble,0);}ready=radioError==0;
#else
 radioError=radio.begin(config.frequency,config.bandwidth,config.sf,config.cr,RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,config.power,preamble,pins::radioTcxo);
 if(radioError){delay(150);radioError=radio.begin(config.frequency,config.bandwidth,config.sf,config.cr,RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,config.power,preamble,pins::radioTcxo);}ready=radioError==0;
#endif
 if(ready){
#if defined(MM_HELTEC_V4)
 extern int heltecFemTx;radio.setRfSwitchPins(RADIOLIB_NC,heltecFemTx);
#elif defined(MM_RADIO_SX1276)
 radio.setCurrentLimit(120);radio.setCRC(true);
#elif defined(MM_RADIO_SX1262)
 radio.setCurrentLimit(140);if(pins::radioRxEn>=0)radio.setRfSwitchPins(pins::radioRxEn,RADIOLIB_NC);
#else
 static const uint32_t dios[Module::RFSWITCH_MAX_PINS]={RADIOLIB_LR11X0_DIO5,RADIOLIB_LR11X0_DIO6,RADIOLIB_NC,RADIOLIB_NC,RADIOLIB_NC};
 static const Module::RfSwitchMode_t modes[]={{LR11x0::MODE_STBY,{LOW,LOW}},{LR11x0::MODE_RX,{HIGH,LOW}},{LR11x0::MODE_TX,{HIGH,HIGH}},{LR11x0::MODE_TX_HP,{LOW,HIGH}},{LR11x0::MODE_TX_HF,{LOW,LOW}},{LR11x0::MODE_GNSS,{LOW,LOW}},{LR11x0::MODE_WIFI,{LOW,LOW}},END_OF_MODE_TABLE};radio.setRfSwitchTable(dios,modes);
#endif
 // Station G2: its LNA saturates with the boosted gain (MeshCore keeps it off on this board).
#if !defined(MM_RADIO_SX1276) && !defined(MM_BOARD_STATION_G2)
 radio.setRxBoostedGainMode(true);
#endif
 radio.setPacketReceivedAction(onRadioIrq);radioIrq=false;radioError=startReceiving();ready=radioError==0;
 }event=ready?"MeshCore radio ready":"Radio error "+String(radioError);dirty=true;return ready;
}
bool MeshRadio::sendHello(){return ready&&(core?core->advertise():meshServer.advertise());}
bool MeshRadio::sendPosition(){if(!config.gps||!hardware.gpsFix()){event="GPS: waiting for fix";dirty=true;return false;}return sendHello();}
bool MeshRadio::sendMessage(const String& text,uint64_t destination){return queue(text,destination,false);}
uint32_t MeshRadio::sendGame(const String& text,uint64_t destination){return destination==meshmesh::Broadcast?0:queue(text,destination,true);}
uint32_t MeshRadio::queue(const String& text,uint64_t destination,bool game){
 size_t n=text.length();if(!ready||!core||!config.bootCounter||!n||n>messageLimit(destination)||!meshmesh::validUtf8((const uint8_t*)text.c_str(),n)||!destination||destination==nodeId){event="Message: invalid or radio offline";dirty=true;return false;}
 if(destination!=meshmesh::Broadcast){auto* p=contact(destination);if(!p||p->type!=ADV_TYPE_CHAT){event="Send advert and discover chat contact first";dirty=true;return false;}}
 Pending* slot=nullptr;for(auto& wait:pending)if(!wait.active){slot=&wait;break;}if(!slot){event="Waiting for ACKs";dirty=true;return false;}
 auto& wait=*slot;wait={};wait.active=true;auto& m=wait.message;m.source=nodeId;m.destination=destination;m.session=config.bootCounter;m.id=++sequence;m.timestamp=time(nullptr);m.outgoing=true;m.status=ChatMessage::Queued;strcpy(m.name,config.name);strcpy(m.text,text.c_str());m.game=game;wait.wireTimestamp=coreRtc.getCurrentTimeUnique();if(!core->reserveStamp(wait.wireTimestamp)){wait.active=false;event="MeshCore timestamp storage error";dirty=true;return 0;}if(game){dirty=true;return m.id;}addMessage(m);event=destination==meshmesh::Broadcast?"Queued: broadcast":"Queued: waiting for delivery";return m.id;
}
void MeshRadio::status(uint32_t id,ChatMessage::Status value){for(auto& p:pending)if(p.message.game&&p.message.id==id){chessNet.delivery(id,value);return;}for(unsigned i=0;i<historyCount;i++)if(history[i].protocol==2&&history[i].outgoing&&history[i].source==nodeId&&history[i].session==config.bootCounter&&history[i].id==id){if(history[i].status==ChatMessage::Delivered)return;history[i].status=value;persist(history[i]);dirty=true;break;}}
// Route of an outgoing message: the current attempt, or the one that got the ACK. Saved with the next status.
void MeshRadio::track(const Pending& wait,unsigned attempt,bool delivered){
 if(wait.message.game||attempt>2)return;
 for(unsigned i=0;i<historyCount;i++){auto& m=history[i];if(m.protocol!=2||!m.outgoing||m.source!=nodeId||m.session!=config.bootCounter||m.id!=wait.message.id)continue;
  m.route=ChatMessage::Route(wait.route[attempt]);m.hops=wait.hops[attempt];m.tries=attempt+1;
  // A flood delivery brings the path back with the ACK.
  if(delivered&&m.route==ChatMessage::RouteFlood){const Peer* p=contact(m.destination);m.hops=p&&p->pathLength!=255?p->pathLength&63:255;}
  dirty=true;break;}
}
String MeshRadio::routeText(const ChatMessage& m,bool brief) const{
 if(m.protocol!=2||m.route==ChatMessage::RouteNone||(m.outgoing&&(m.status==ChatMessage::Failed||m.destination==meshmesh::Broadcast)))return "";
 String hops=m.hops==255?String():!m.hops?String(brief?tr("direct","напр."):tr("direct","напрямую")):(brief?String(m.hops)+tr(" rpt"," ретр."):String(tr("via ","через "))+m.hops+tr(" rpt"," ретр."));
 String s=m.route==ChatMessage::RouteDirect?(hops.length()?hops:String(tr("routed","по маршруту")))
  :!m.outgoing?(hops.length()?hops:String("flood")) // incoming: how far it came
  :m.status==ChatMessage::Delivered&&hops.length()&&!brief?"flood, "+hops:String("flood");
 if(m.outgoing&&m.tries>1)s+=m.status==ChatMessage::Delivered?(brief?" #"+String(m.tries):String(tr(" · try "," · попытка "))+m.tries):" "+String(m.tries)+"/3";
 return s;
}
void MeshRadio::addMessage(const ChatMessage& m,bool save) {
  if(historyCount==64) {memmove(history,history+1,sizeof(ChatMessage)*63);historyCount=63;}
  history[historyCount++]=m;if(save)persist(m);dirty=true;
}
void MeshRadio::persist(const ChatMessage& m) {
  HistoryFs* fs=historyFs();if(!fs)return;
  File f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);if(!f) {fs->mkdir("/meshmesh");f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);}if(!f)return;
  // Bound the log: retain one previous segment; never touch other apps' files.
  if(f.size()>historySegment) {f.close();fs->remove("/meshmesh/history.previous.jsonl");fs->rename("/meshmesh/history.jsonl","/meshmesh/history.previous.jsonl");f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);}
  StaticJsonDocument<768> d;d["protocol"]=m.protocol;d["source"]=idText(m.source);d["destination"]=idText(m.destination);d["session"]=m.session;d["id"]=m.id;d["time"]=m.timestamp;d["name"]=m.name;d["text"]=m.text;d["outgoing"]=m.outgoing;d["status"]=int(m.status);if(m.route){d["route"]=int(m.route);d["hops"]=m.hops;d["tries"]=m.tries;}serializeJson(d,f);f.println();f.close();
}
void MeshRadio::restoreHistory() {
  HistoryFs* fs=historyFs();if(!fs)return;
  for(const char* path:{"/meshmesh/history.previous.jsonl","/meshmesh/history.jsonl"}) {
    File f=fs->open(path,FILE_READ);if(!f)continue;
    while(f.available()) {
      String row=f.readStringUntil('\n');StaticJsonDocument<768> d;if(deserializeJson(d,row))continue;
      ChatMessage m;m.protocol=d["protocol"]|1;m.source=strtoull(d["source"]|"0",nullptr,16);const char* dest=d["destination"]|"ALL";m.destination=strcmp(dest,"ALL")==0?meshmesh::Broadcast:strtoull(dest,nullptr,16);
      m.session=d["session"]|0;m.id=d["id"]|0;m.timestamp=d["time"]|0;m.outgoing=d["outgoing"]|false;m.status=ChatMessage::Status(constrain(d["status"]|0,0,4));
      m.route=ChatMessage::Route(constrain(d["route"]|0,0,2));m.hops=d["hops"]|255;m.tries=d["tries"]|0;
      strlcpy(m.name,d["name"]|"?",sizeof(m.name));strlcpy(m.text,d["text"]|"",sizeof(m.text));
      bool found=false;for(unsigned i=0;i<historyCount;i++) if(history[i].protocol==m.protocol && history[i].source==m.source && history[i].session==m.session && history[i].id==m.id) {history[i].status=m.status;if(m.route){history[i].route=m.route;history[i].hops=m.hops;history[i].tries=m.tries;}found=true;break;}
      if(!found)addMessage(m,false);
    }
    f.close();
  }
  // Pending delivery belongs to the previous boot session and is not resumed.
  // Preserve confirmed deliveries and completed broadcasts; never invent an ACK.
  for(unsigned i=0;i<historyCount;i++) {
    auto& m=history[i];
    if(m.outgoing && (m.status==ChatMessage::Queued ||
        (m.status==ChatMessage::Sent && m.destination!=meshmesh::Broadcast))) {
      m.status=ChatMessage::Failed;persist(m);
    }
  }
}
// After the last failed attempt a known path is reset: the next message floods and learns a new one.
void MeshRadio::tick(){
 if(!ready)return;if(meshServer.running()){meshServer.tick();return;}if(!core)return;core->tick();uint32_t now=millis();
 for(auto& p:pending)if(p.active){if(!p.started){if(!busy()&&!core->startMessage(p)){p.active=false;status(p.message.id,ChatMessage::Failed);}break;}if(int32_t(now-p.due)>=0&&!busy()){if(p.attempts>=3||p.message.destination==meshmesh::Broadcast){p.active=false;status(p.message.id,ChatMessage::Failed);if(!p.message.game)event="No delivery ACK";if(p.message.destination!=meshmesh::Broadcast){Peer* c=contact(p.message.destination);if(c&&c->pathLength!=255)resetPath(c->id);}}else if(!core->startMessage(p)){p.active=false;status(p.message.id,ChatMessage::Failed);}}}
 if(autoHelloDue&&int32_t(now-autoHelloDue)>=0&&!busy()){if(sendHello())autoHelloDue=0;else autoHelloDue=now+5000;}
}
bool MeshRadio::selfTest(){const mesh::LocalIdentity* self=selfIdentity(core);if(!self||(core&&!core->identitySaved)||!mesh::Utils::selfTestAES())return false;const uint8_t text[]="MeshCore: Привет";uint8_t signature[64];self->sign(signature,text,sizeof(text)-1);if(!self->verify(signature,text,sizeof(text)-1))return false;signature[0]^=1;if(self->verify(signature,text,sizeof(text)-1))return false;uint8_t secret[32]={},cipher[64],plain[64];esp_fill_random(secret,32);int n=mesh::Utils::encryptThenMAC(secret,cipher,text,sizeof(text)-1);int len=mesh::Utils::MACThenDecrypt(secret,plain,cipher,n);if(len<int(sizeof(text)-1)||memcmp(text,plain,sizeof(text)-1))return false;cipher[n-1]^=1;if(mesh::Utils::MACThenDecrypt(secret,plain,cipher,n))return false;SHA256 hmac;hmac.resetHMAC(secret,32);hmac.update(cipher+2,n-3);hmac.finalizeHMAC(secret,32,cipher,2);if(mesh::Utils::MACThenDecrypt(secret,plain,cipher,n-1))return false;return meshmesh::validUtf8(text,sizeof(text)-1);}
String MeshRadio::diagnosticFrame() const{String result;result.reserve(lastFrameSize*2);char hex[3];for(size_t i=0;i<lastFrameSize;i++){snprintf(hex,3,"%02x",lastFrame[i]);result+=hex;}return result;}
bool MeshRadio::diagnosticIngest(const uint8_t*,size_t){rejected++;return false;} // Do not bypass authenticated RF reception.
