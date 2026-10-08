// MeshCore companion protocol (include/Companion.h), included at the end of MeshRadio.cpp: the frames of
// examples/companion_radio/MyMesh.cpp, MeshCore companion-v1.17.1 (MIT), over this node's contacts, channels
// and history. Not here: logins to repeaters and rooms, their CLI, telemetry, trace, raw data, signing and the
// private key; those commands answer "unsupported".
#include "App.h"
#include "Version.h"
namespace {
enum:uint8_t {CmdAppStart=1,CmdSendText,CmdSendChannelText,CmdGetContacts,CmdGetTime,CmdSetTime,CmdSendAdvert,CmdSetName,CmdAddContact,
 CmdSyncNext,CmdSetRadio,CmdSetPower,CmdResetPath,CmdSetPosition,CmdRemoveContact,CmdShareContact,CmdExportContact,CmdImportContact,CmdReboot,
 CmdBattery,CmdSetTuning,CmdDeviceQuery,CmdExportKey,CmdImportKey,CmdHasConnection=28,CmdLogout,CmdContactByKey,CmdGetChannel,CmdSetChannel,
 CmdCustomVars=40,CmdGetTuning=43,CmdStats=56,CmdGetAutoAdd=59};
enum:uint8_t {RespOk,RespErr,RespContactsStart,RespContact,RespContactsEnd,RespSelfInfo,RespSent,RespContactMsgOld,RespChannelMsgOld,RespTime,
 RespNoMore,RespExport,RespBattery,RespDeviceInfo,RespDisabled=15,RespContactMsg,RespChannelMsg,RespChannelInfo,RespCustomVars=21,
 RespTuning=23,RespStats,RespAutoAdd};
enum:uint8_t {PushAdvert=0x80,PushSendConfirmed=0x82,PushMsgWaiting=0x83,PushNewAdvert=0x8A};
enum:uint8_t {ErrUnsupported=1,ErrNotFound,ErrTableFull,ErrBadState,ErrFileIo,ErrIllegalArg};
constexpr uint8_t ProtocolVersion=10; // the frames of version 10; later commands answer "unsupported"
}
struct CompanionCore {
 struct Frame {uint8_t length=0;uint8_t data[companion::MaxFrame];};
 Frame out[6];unsigned outHead=0,outCount=0;  // replies and pushes for the linked app
 Frame inbox[12];unsigned inHead=0,inCount=0; // received messages until the app takes them (also while away)
 companion::Link link=companion::LinkNone;uint8_t appVersion=0;
 bool listing=false;unsigned listNext=0;uint32_t listSince=0,listNewest=0;
 // The app's direct messages: its retries keep one history row; the ACK is announced as the app expects it.
 struct Sent {uint64_t to=0;uint32_t stamp=0,id=0,ack=0,at=0;} sent[8];unsigned nextSent=0;
 uint8_t f[companion::MaxFrame];size_t n=0;
 MeshCoreBackend* core() const{return meshRadio.core;}
 void start(uint8_t code){f[0]=code;n=1;}
 void put(const void* data,size_t size){size=min(size,companion::MaxFrame-n);memcpy(f+n,data,size);n+=size;}
 void u8(uint8_t v){put(&v,1);}
 void u16(uint16_t v){put(&v,2);} // both chips are little-endian, as the protocol
 void u32(uint32_t v){put(&v,4);}
 void field(const char* text,size_t size){uint8_t zero[40]={};size_t k=min(strnlen(text,size-1),size-1);put(text,k);put(zero,size-k);}
 void send(){if(outCount>=6)return;auto& o=out[(outHead+outCount++)%6];o.length=n;memcpy(o.data,f,n);}
 void ok(){start(RespOk);send();}
 void err(uint8_t code){start(RespErr);u8(code);send();}
 void clearLink(){outHead=outCount=0;listing=false;}
 void contactFrame(uint8_t code,const ContactInfo& c){
  start(code);put(c.id.pub_key,32);u8(c.type);u8(c.flags);u8(c.out_path_len);put(c.out_path,MAX_PATH_SIZE);field(c.name,32);
  u32(c.last_advert_timestamp);u32(c.gps_lat);u32(c.gps_lon);u32(c.lastmod);
 }
 // A received message waits in the inbox; a full one drops its oldest channel message, else the oldest.
 void keep(){
  if(inCount==12){unsigned drop=0;for(unsigned i=0;i<inCount;i++){uint8_t c=inbox[(inHead+i)%12].data[0];if(c==RespChannelMsg||c==RespChannelMsgOld){drop=i;break;}}
   for(unsigned i=drop;i+1<inCount;i++)inbox[(inHead+i)%12]=inbox[(inHead+i+1)%12];inCount--;}
  auto& m=inbox[(inHead+inCount++)%12];m.length=n;memcpy(m.data,f,n);
  if(link){start(PushMsgWaiting);send();}
 }
 void message(bool channel,const uint8_t* who,int index,const mesh::Packet* packet,uint32_t stamp,const char* text){
  if(appVersion>=3||!link){start(channel?RespChannelMsg:RespContactMsg);u8(uint8_t(int8_t(packet->getSNR()*4)));u8(0);u8(0);}else start(channel?RespChannelMsgOld:RespContactMsgOld);
  if(channel)u8(index);else put(who,6);
  u8(packet->isRouteFlood()?packet->path_len:0xFF);u8(0);u32(stamp);put(text,strlen(text));keep();
 }
 uint32_t confirm(uint32_t ack,uint64_t& to){
  if(!ack)return 0;
  for(auto& s:sent)if(s.ack==ack){s.ack=0;to=s.to;if(link){start(PushSendConfirmed);u32(ack);u32(millis()-s.at);send();}return s.id;}
  return 0;
 }
 static uint8_t channelError(MeshRadio::ChannelResult r){return r==MeshRadio::ChannelFull?ErrTableFull:r==MeshRadio::ChannelStorage?ErrFileIo:r==MeshRadio::ChannelUnavailable?ErrBadState:ErrIllegalArg;}
 String apply(JsonDocument& d){return applySettings(d.as<JsonObjectConst>());}
 void settingsReply(const String& r){if(r.startsWith("OK"))ok();else err(r.indexOf("busy")>=0?ErrBadState:ErrIllegalArg);}
 // A direct message of the app: one attempt with its timestamp and attempt byte; the app retries itself.
 void sendText(const uint8_t* frame,size_t len){
  uint8_t type=frame[1],attempt=frame[2];uint32_t stamp;memcpy(&stamp,frame+3,4);
  ContactInfo* c=core()->lookupContactByPubKey(frame+7,6);if(!c){err(ErrNotFound);return;}
  if(type!=0||c->type!=ADV_TYPE_CHAT){err(ErrUnsupported);return;}
  const char* text=(const char*)frame+13;size_t size=len-13;
  if(!size||size>MAX_TEXT_LEN||memchr(text,0,size)||!meshmesh::validUtf8((const uint8_t*)text,size)){err(ErrIllegalArg);return;}
  auto& r=meshRadio;uint64_t to=aliasOf(c->id.pub_key);Peer* p=r.contact(to);
  if(!r.ready||!config.bootCounter||!p){err(ErrBadState);return;}
  MeshRadio::Pending* slot=nullptr;for(auto& w:r.pending)if(!w.active){slot=&w;break;}if(!slot){err(ErrTableFull);return;}
  Sent* again=nullptr;for(auto& s:sent)if(s.id&&s.to==to&&s.stamp==stamp){again=&s;break;}
  auto& w=*slot;w={};w.active=true;w.app=true;w.appAttempt=attempt;w.wireTimestamp=stamp;
  auto& m=w.message;m.source=r.nodeId;m.destination=to;m.session=config.bootCounter;m.id=again?again->id:++r.sequence;m.timestamp=time(nullptr);m.outgoing=true;
  m.status=ChatMessage::Queued;strcpy(m.name,config.name);memcpy(m.text,text,size);m.text[size]=0;
  if(again)r.status(m.id,ChatMessage::Queued);else r.addMessage(m);
  uint8_t bytes[5+MAX_TEXT_LEN];meshmesh::put32(bytes,stamp);bytes[4]=attempt&3;memcpy(bytes+5,text,size);
  uint32_t ack;mesh::Utils::sha256((uint8_t*)&ack,4,bytes,5+size,core()->self_id.pub_key,32);w.ack[0]=ack;
  bool flood=(attempt&3)||c->out_path_len==OUT_PATH_UNKNOWN||!core()->pathTrusted(*c);
  uint32_t air=coreRadio.getEstAirtimeFor(2+(flood?0:(c->out_path_len&63))+3+(size+5+15)/16*16+2);
  uint32_t timeout=flood?core()->calcFloodTimeoutMillisFor(air):core()->calcDirectTimeoutMillisFor(air,c->out_path_len);
  Sent& s=again?*again:sent[nextSent];if(!again)nextSent=(nextSent+1)%8;s={to,stamp,m.id,ack,uint32_t(millis())};
  r.event="Queued: waiting for delivery";r.dirty=true;
  start(RespSent);u8(flood);u32(ack);u32(timeout);send();
 }
 void setChannel(const uint8_t* frame){
  auto& r=meshRadio;uint8_t index=frame[1];char name[33]={};memcpy(name,frame+2,32);const uint8_t* key=frame+34;
  static const uint8_t none[16]={};
  if(index>=channels::Max){err(ErrNotFound);return;}
  if(!name[0]||!memcmp(key,none,16)){ // an empty slot: the channel leaves
   if(!index){err(ErrIllegalArg);return;}if(index>=r.channelCount){ok();return;}
   if(r.removeChannel(r.channelList[index].id))ok();else err(ErrBadState);return;}
  if(index<r.channelCount&&!memcmp(r.channelList[index].secret,key,16)){ok();return;} // the same channel; its name stays
  if(r.channel(channels::idOf(key))){ok();return;}
  if(!index){err(ErrIllegalArg);return;} // Public stays the first
  if(index<r.channelCount&&!r.removeChannel(r.channelList[index].id)){err(ErrBadState);return;}
  auto result=r.addChannel(String(name),key);if(result==MeshRadio::ChannelAdded||result==MeshRadio::ChannelExists)ok();else err(channelError(result));
 }
 void updateContact(const uint8_t* frame,size_t len){
  ContactInfo c={};uint32_t lastmod=coreRtc.getCurrentTime();
  memcpy(c.id.pub_key,frame+1,32);c.type=frame[33];c.flags=frame[34];c.out_path_len=frame[35];memcpy(c.out_path,frame+36,MAX_PATH_SIZE);
  memcpy(c.name,frame+100,32);c.name[31]=0;memcpy(&c.last_advert_timestamp,frame+132,4);
  if(len>=144){memcpy(&c.gps_lat,frame+136,4);memcpy(&c.gps_lon,frame+140,4);if(len>=148)memcpy(&lastmod,frame+144,4);}
  if((!mesh::Packet::isValidPathLen(c.out_path_len)&&c.out_path_len!=OUT_PATH_UNKNOWN)||!meshmesh::validUtf8((const uint8_t*)c.name,strlen(c.name))){err(ErrIllegalArg);return;}
  ContactInfo* old=core()->lookupContactByPubKey(c.id.pub_key,32);
  if(old){old->type=c.type;old->flags=c.flags;old->out_path_len=c.out_path_len;memcpy(old->out_path,c.out_path,MAX_PATH_SIZE);memcpy(old->name,c.name,32);
   old->last_advert_timestamp=c.last_advert_timestamp;old->gps_lat=c.gps_lat;old->gps_lon=c.gps_lon;old->lastmod=lastmod;core()->updateContact(*old,false);}
  else{c.lastmod=lastmod;if(!core()->addContact(c)){err(ErrTableFull);return;}core()->updateContact(c,false);}
  core()->contactsDue=millis()+2000;ok();
 }
 void stats(uint8_t type){
  auto* d=core();start(RespStats);u8(type);
  if(type==0){u16(hardware.batteryMv);u32(millis()/1000);u16(0);u8(corePool.getOutboundTotal());}
  else if(type==1){u16(0);u8(int8_t(meshRadio.lastRssi));u8(int8_t(meshRadio.lastSnr*4));u32(d->getTotalAirTime()/1000);u32(d->getReceiveAirTime()/1000);}
  else if(type==2){u32(meshRadio.rxCount);u32(meshRadio.txCount);u32(d->getNumSentFlood());u32(d->getNumSentDirect());u32(d->getNumRecvFlood());u32(d->getNumRecvDirect());u32(meshRadio.rejected);}
  else{err(ErrIllegalArg);return;}
  send();
 }
 void selfInfo(){
  bool fix=config.gps&&hardware.gpsFix();int32_t lat=fix?int32_t(hardware.gps.location.lat()*1e6):0,lon=fix?int32_t(hardware.gps.location.lng()*1e6):0;
  start(RespSelfInfo);u8(ADV_TYPE_CHAT);u8(config.power);u8(MM_MAX_POWER);put(core()->self_id.pub_key,32);u32(lat);u32(lon);
  u8(0);u8(config.gps?1:0);u8(0);u8(0); // multi ACKs, position in adverts, telemetry, contacts added automatically
  u32(uint32_t(lroundf(config.frequency*1000)));u32(uint32_t(lroundf(config.bandwidth*1000)));u8(config.sf);u8(config.cr);put(config.name,strlen(config.name));send();
 }
 void deviceInfo(){
  start(RespDeviceInfo);u8(ProtocolVersion);u8(MAX_CONTACTS/2);u8(channels::Max);u32(blePin());field(__DATE__,12);field(MM_BOARD_NAME,40);
  field("MeshMesh " MESHMM_VERSION,20);u8(config.relay?1:0);u8(0);send();
 }
 void handle(const uint8_t* frame,size_t len){
  uint8_t cmd=frame[0];auto& r=meshRadio;
  if(cmd==CmdDeviceQuery&&len>=2){appVersion=frame[1];deviceInfo();return;}
  if(!core()){err(ErrBadState);return;} // repeater and room roles: no chat contacts
  auto* d=core();
  switch(cmd){
   case CmdAppStart:if(len<8)break;listing=false;selfInfo();return;
   case CmdSendText:if(len<14)break;sendText(frame,len);return;
   case CmdSendChannelText:{if(len<8)break;if(frame[1]){err(ErrUnsupported);return;}if(frame[2]>=r.channelCount){err(ErrNotFound);return;}
    char raw[companion::MaxFrame+1]={};memcpy(raw,frame+7,len-7);String text(raw);if(r.sendMessage(text,r.channelList[frame[2]].id))ok();else err(r.ready?ErrIllegalArg:ErrBadState);return;}
   case CmdGetContacts:if(listing){err(ErrBadState);return;}listSince=0;if(len>=5)memcpy(&listSince,frame+1,4);
    start(RespContactsStart);u32(d->getNumContacts());send();listing=true;listNext=0;listNewest=0;return;
   case CmdGetTime:start(RespTime);u32(coreRtc.getCurrentTime());send();return;
   case CmdSetTime:{if(len<5)break;uint32_t t;memcpy(&t,frame+1,4);if(hardware.setUtc(t,"manual",true))ok();else err(ErrIllegalArg);return;}
   case CmdSendAdvert:if(!r.ready)err(ErrBadState);else if(d->advertise(false,!(len>=2&&frame[1]==1)))ok();else err(ErrTableFull);return;
   case CmdSetName:{if(len<2)break;char name[25];size_t k=min(len-1,size_t(32));char raw[33]={};memcpy(raw,frame+1,k);copyUtf8(name,raw,sizeof(name));
    StaticJsonDocument<96> j;j["name"]=name;settingsReply(apply(j));return;}
   case CmdAddContact:if(len<136)break;updateContact(frame,len);return;
   case CmdSyncNext:if(inCount){auto& m=inbox[inHead];inHead=(inHead+1)%12;inCount--;n=m.length;memcpy(f,m.data,n);send();}else{start(RespNoMore);send();}return;
   case CmdSetRadio:{if(len<11)break;uint32_t freq,bw;memcpy(&freq,frame+1,4);memcpy(&bw,frame+5,4);StaticJsonDocument<192> j;
    j["frequency"]=freq/1000.0f;j["bandwidth"]=bw/1000.0f;j["sf"]=frame[9];j["cr"]=frame[10];if(len>11)j["relay"]=frame[11]!=0;settingsReply(apply(j));return;}
   case CmdSetPower:{if(len<2)break;StaticJsonDocument<64> j;j["power"]=int8_t(frame[1]);settingsReply(apply(j));return;}
   case CmdResetPath:if(len<33)break;if(r.resetPath(aliasOf(frame+1)))ok();else err(ErrNotFound);return;
   case CmdRemoveContact:if(len<33)break;if(r.removeContact(aliasOf(frame+1)))ok();else err(ErrNotFound);return;
   case CmdShareContact:{if(len<33)break;ContactInfo* c=d->lookupContactByPubKey(frame+1,32);if(!c)err(ErrNotFound);else if(d->shareContactZeroHop(*c))ok();else err(ErrTableFull);return;}
   case CmdContactByKey:{if(len<33)break;ContactInfo* c=d->lookupContactByPubKey(frame+1,32);if(c){contactFrame(RespContact,*c);send();}else err(ErrNotFound);return;}
   case CmdExportContact:{uint8_t size=0;
    if(len<33){mesh::Packet* p=d->createSelfAdvert(config.name);if(p){p->header|=ROUTE_TYPE_FLOOD;start(RespExport);size=p->writeTo(f+1);d->releasePacket(p);}}
    else{ContactInfo* c=d->lookupContactByPubKey(frame+1,32);if(c){start(RespExport);size=d->exportContact(*c,f+1);}}
    if(size){n=1+size;send();}else err(ErrNotFound);return;}
   case CmdImportContact:if(len<=98)break;if(d->importContact(frame+1,len-1))ok();else err(ErrIllegalArg);return;
   case CmdReboot:if(len<7||memcmp(frame+1,"reboot",6))break;r.flush();executeCommand("restart");return;
   case CmdBattery:{uint32_t used=0,total=0;
#if !defined(MM_NRF52)
    if(hardware.fsOk){used=LittleFS.usedBytes()/1024;total=LittleFS.totalBytes()/1024;}
#endif
    start(RespBattery);u16(hardware.batteryMv);u32(used);u32(total);send();return;}
   case CmdGetTuning:start(RespTuning);u32(0);u32(uint32_t(d->getAirtimeBudgetFactor()*1000));send();return;
   case CmdExportKey:case CmdImportKey:start(RespDisabled);send();return;
   case CmdHasConnection:err(ErrNotFound);return;
   case CmdLogout:ok();return;
   case CmdGetChannel:{if(len<2)break;uint8_t i=frame[1];if(i>=channels::Max){err(ErrNotFound);return;}
    start(RespChannelInfo);u8(i);if(i<r.channelCount){field(r.channelList[i].name,32);put(r.channelList[i].secret,16);}else{field("",32);uint8_t zero[16]={};put(zero,16);}send();return;}
   case CmdSetChannel:if(len<50)break;if(len>=66){err(ErrUnsupported);return;}setChannel(frame);return; // 256-bit keys: not in MeshCore either
   case CmdCustomVars:start(RespCustomVars);send();return;
   case CmdStats:if(len<2)break;stats(frame[1]);return;
   case CmdGetAutoAdd:start(RespAutoAdd);u8(0x1F);u8(0);send();return; // every type, the oldest replaced when full
  }
  err(cmd==CmdSendText||cmd==CmdAddContact||cmd==CmdSetChannel||cmd==CmdGetChannel?ErrIllegalArg:ErrUnsupported);
 }
 // The contact list goes out one frame at a time, as the link takes them.
 void list(){
  auto* d=core();if(!d){listing=false;return;}
  while(listNext<unsigned(d->getNumContacts())){ContactInfo c;if(!d->getContactByIdx(listNext++,c)||c.lastmod<=listSince)continue;
   if(c.lastmod>listNewest)listNewest=c.lastmod;contactFrame(RespContact,c);send();return;}
  start(RespContactsEnd);u32(listNewest);send();listing=false;
 }
};
// On the heap at first use: the classic ESP32 boards have no static DRAM to spare for its 3.5 KB.
static CompanionCore& app(){static CompanionCore* core=new CompanionCore;return *core;}
namespace appLink {
void contactMessage(const ContactInfo& from,const mesh::Packet* packet,uint32_t stamp,const char* text){app().message(false,from.id.pub_key,0,packet,stamp,text);}
void channelMessage(int index,const mesh::Packet* packet,uint32_t stamp,const char* text){if(index>=0)app().message(true,nullptr,index,packet,stamp,text);}
void advert(const ContactInfo& c,bool added){
 auto& a=app();if(!a.link)return;
 if(added)a.contactFrame(PushNewAdvert,c);else{a.start(PushAdvert);a.put(c.id.pub_key,32);}a.send();
}
void keyEvent(uint8_t code,const uint8_t* key){auto& a=app();if(!a.link)return;a.start(code);a.put(key,32);a.send();}
uint32_t confirm(uint32_t ack,uint64_t& destination){return app().confirm(ack,destination);}
}
namespace companion {
void command(const uint8_t* frame,size_t length,Link from){
 if(!length||length>MaxFrame)return;
 auto& a=app();
 if(a.link!=from){a.clearLink();a.link=from;}
 a.handle(frame,length);
}
size_t next(uint8_t out[MaxFrame],Link to){
 auto& a=app();if(a.link!=to||to==LinkNone)return 0;
 if(!a.outCount&&a.listing)a.list();
 if(!a.outCount)return 0;
 auto& o=a.out[a.outHead];a.outHead=(a.outHead+1)%6;a.outCount--;memcpy(out,o.data,o.length);return o.length;
}
void disconnected(Link link){auto& a=app();if(a.link==link){a.clearLink();a.link=LinkNone;}}
bool connected(){return app().link!=LinkNone;}
}
