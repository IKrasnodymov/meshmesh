// MeshCore companion protocol (include/Companion.h), included at the end of MeshRadio.cpp: the frames of
// examples/companion_radio/MyMesh.cpp, MeshCore companion-v1.17.1 (MIT), over this node's contacts, channels
// and history, with logins to repeaters and rooms, their CLI, status, telemetry, room posts, path trace and path
// discovery. Not here: raw data, signing and the private key; those commands answer "unsupported".
#include "App.h"
#include "Version.h"
namespace {
enum:uint8_t {CmdAppStart=1,CmdSendText,CmdSendChannelText,CmdGetContacts,CmdGetTime,CmdSetTime,CmdSendAdvert,CmdSetName,CmdAddContact,
 CmdSyncNext,CmdSetRadio,CmdSetPower,CmdResetPath,CmdSetPosition,CmdRemoveContact,CmdShareContact,CmdExportContact,CmdImportContact,CmdReboot,
 CmdBattery,CmdSetTuning,CmdDeviceQuery,CmdExportKey,CmdImportKey,CmdSendRaw,CmdLogin,CmdStatusReq,CmdHasConnection,CmdLogout,CmdContactByKey,
 CmdGetChannel,CmdSetChannel,CmdSignStart,CmdSignData,CmdSignFinish,CmdTrace,CmdTelemetryReq=39,CmdCustomVars,CmdGetTuning=43,CmdBinaryReq=50,CmdPathDiscovery=52,CmdStats=56,CmdAnonReq,CmdGetAutoAdd=59,CmdSetPathHashMode=61};
enum:uint8_t {RespOk,RespErr,RespContactsStart,RespContact,RespContactsEnd,RespSelfInfo,RespSent,RespContactMsgOld,RespChannelMsgOld,RespTime,
 RespNoMore,RespExport,RespBattery,RespDeviceInfo,RespDisabled=15,RespContactMsg,RespChannelMsg,RespChannelInfo,RespCustomVars=21,
 RespTuning=23,RespStats,RespAutoAdd};
enum:uint8_t {PushAdvert=0x80,PushSendConfirmed=0x82,PushMsgWaiting=0x83,PushLoginOk=0x85,PushLoginFail,PushStatus,PushTrace=0x89,PushNewAdvert,PushTelemetry,PushBinary,PushDiscovery};
enum:uint8_t {TextPlain=0,TextCli=1,TextSigned=2};
constexpr uint8_t RequestTelemetry=0x03; // REQ_TYPE_GET_TELEMETRY_DATA of the companion
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
 // The one request awaiting its answer, as in stock MeshCore: a login and a status by the server's key, the others by tag.
 uint32_t pendingLogin=0,pendingStatus=0,pendingTelemetry=0,pendingReq=0,pendingDiscovery=0;
 void clearPending(){pendingLogin=pendingStatus=pendingTelemetry=pendingReq=pendingDiscovery=0;}
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
 // type: plain, CLI reply of a repeater or room, or a room post (extra: 4 bytes of its author's key).
 void message(bool channel,const uint8_t* who,int index,const mesh::Packet* packet,uint32_t stamp,const char* text,uint8_t type=TextPlain,const uint8_t* extra=nullptr){
  if(appVersion>=3||!link){start(channel?RespChannelMsg:RespContactMsg);u8(uint8_t(int8_t(packet->getSNR()*4)));u8(0);u8(0);}else start(channel?RespChannelMsgOld:RespContactMsgOld);
  if(channel)u8(index);else put(who,6);
  u8(packet->isRouteFlood()?packet->path_len:0xFF);u8(type);u32(stamp);if(extra)put(extra,4);put(text,strlen(text));keep();
 }
 // The answer of a server to a login or a request (onContactResponse of the stock companion).
 void response(const ContactInfo& c,const uint8_t* data,uint8_t len){
  if(len<4)return;uint32_t tag;memcpy(&tag,data,4);
  if(pendingLogin&&!memcmp(&pendingLogin,c.id.pub_key,4)){pendingLogin=0;
   if(len>=6&&!memcmp(data+4,"OK",2)){start(PushLoginOk);u8(0);put(c.id.pub_key,6);} // older servers
   else if(len>=13&&data[4]==RESP_SERVER_LOGIN_OK){uint16_t keepAlive=uint16_t(data[5])*16;if(keepAlive)core()->startConnection(c,keepAlive);
    start(PushLoginOk);u8(data[6]);put(c.id.pub_key,6);u32(tag);u8(data[7]);u8(data[12]);} // admin, server time, ACL permissions, firmware level
   else{start(PushLoginFail);u8(0);put(c.id.pub_key,6);}
   if(link)send();return;}
  if(len<=4)return;
  if(pendingStatus&&!memcmp(&pendingStatus,c.id.pub_key,4)){pendingStatus=0;start(PushStatus);u8(0);put(c.id.pub_key,6);}
  else if(pendingTelemetry&&tag==pendingTelemetry){pendingTelemetry=0;start(PushTelemetry);u8(0);put(c.id.pub_key,6);}
  else if(pendingReq&&tag==pendingReq){pendingReq=0;start(PushBinary);u8(0);u32(tag);}
  else return;
  put(data+4,len-4);if(link)send();
 }
 void sentReply(int result,uint32_t tag,uint32_t timeout){if(result==MSG_SEND_FAILED){err(ErrTableFull);return;}start(RespSent);u8(result==MSG_SEND_SENT_FLOOD);u32(tag);u32(timeout);send();}
 // Requests to a server (repeater, room, sensor) found by its whole key.
 void request(uint8_t cmd,const uint8_t* frame,size_t len){
  auto* d=core();const uint8_t* key=frame+(cmd==CmdTelemetryReq?4:1);ContactInfo* c=d->lookupContactByPubKey(key,32);
  ContactInfo anon;
  if(!c&&cmd==CmdAnonReq){anon={};memcpy(anon.id.pub_key,key,32);anon.out_path_len=0;anon.type=ADV_TYPE_NONE;anon.lastmod=coreRtc.getCurrentTime();if(d->addContact(anon))c=&anon;else{err(ErrTableFull);return;}}
  if(!c){err(ErrNotFound);return;}
  if(!meshRadio.ready){err(ErrBadState);return;}
  uint32_t tag=0,timeout=0;int result;
  if(cmd==CmdLogin){char password[companion::MaxFrame]={};memcpy(password,frame+33,min(len-33,sizeof(password)-1));
   result=d->sendLogin(*c,password,timeout);if(result!=MSG_SEND_FAILED){clearPending();memcpy(&pendingLogin,c->id.pub_key,4);tag=pendingLogin;}}
  else if(cmd==CmdStatusReq){result=d->sendRequest(*c,REQ_TYPE_GET_STATUS,tag,timeout);if(result!=MSG_SEND_FAILED){clearPending();memcpy(&pendingStatus,c->id.pub_key,4);}}
  else if(cmd==CmdTelemetryReq){result=d->sendRequest(*c,RequestTelemetry,tag,timeout);if(result!=MSG_SEND_FAILED){clearPending();pendingTelemetry=tag;}}
  else if(cmd==CmdAnonReq){result=d->sendAnonReq(*c,frame+33,len-33,tag,timeout);if(result!=MSG_SEND_FAILED){clearPending();pendingReq=tag;}}
  else{result=d->sendRequest(*c,frame+33,len-33,tag,timeout);if(result!=MSG_SEND_FAILED){clearPending();pendingReq=tag;}}
  sentReply(result,tag,timeout);
 }
 // Path trace: the app gives the hashes of the repeaters to pass; each adds the SNR it heard, the last
 // node sends it back the same way (Mesh::onTraceRecv).
 void trace(const uint8_t* frame,size_t len){
  uint8_t pathLen=len-10,flags=frame[9],size=flags&3;
  if((pathLen>>size)>MAX_PATH_SIZE||pathLen%(1<<size)){err(ErrIllegalArg);return;}
  if(!meshRadio.ready){err(ErrBadState);return;}
  uint32_t tag,auth;memcpy(&tag,frame+1,4);memcpy(&auth,frame+5,4);auto* d=core();mesh::Packet* p=d->createTrace(tag,auth,flags);
  if(!p){err(ErrTableFull);return;}
  d->sendDirect(p,frame+10,pathLen);uint32_t air=coreRadio.getEstAirtimeFor(p->payload_len+p->path_len+2);
  start(RespSent);u8(0);u32(tag);u32(d->calcDirectTimeoutMillisFor(air,pathLen>>size));send();
 }
 void traced(const mesh::Packet* packet,uint32_t tag,uint32_t auth,uint8_t flags,const uint8_t* snrs,const uint8_t* hashes,uint8_t pathLen){
  uint8_t size=flags&3;if(!link||12+pathLen+(pathLen>>size)+1>companion::MaxFrame)return;
  start(PushTrace);u8(0);u8(pathLen);u8(flags);u32(tag);u32(auth);put(hashes,pathLen);put(snrs,pathLen>>size);u8(uint8_t(int8_t(packet->getSNR()*4)));send();
 }
 // Path discovery: a flooded telemetry request; the reply brings both paths (onContactPathRecv), which are not stored.
 void discover(const uint8_t* frame){
  auto* d=core();ContactInfo* c=d->lookupContactByPubKey(frame+2,32);if(!c){err(ErrNotFound);return;}
  if(!meshRadio.ready){err(ErrBadState);return;}
  uint8_t req[9]={RequestTelemetry,uint8_t(~TELEM_PERM_BASE)};coreRandom.random(req+5,4); // base telemetry only; random bytes keep the packet unique
  uint32_t tag=0,timeout=0;uint8_t saved=c->out_path_len;c->out_path_len=OUT_PATH_UNKNOWN;int result=d->sendRequest(*c,req,sizeof(req),tag,timeout);c->out_path_len=saved;
  if(result!=MSG_SEND_FAILED){clearPending();pendingDiscovery=tag;}
  sentReply(result,tag,timeout);
 }
 bool discovered(const ContactInfo& c,const uint8_t* inPath,uint8_t inLen,const uint8_t* outPath,uint8_t outLen,uint8_t extraType,const uint8_t* extra,uint8_t extraLen){
  if(extraType!=PAYLOAD_TYPE_RESPONSE||extraLen<=4||!pendingDiscovery||memcmp(extra,&pendingDiscovery,4))return false;
  pendingDiscovery=0;if(!mesh::Packet::isValidPathLen(inLen)||!mesh::Packet::isValidPathLen(outLen))return true;
  start(PushDiscovery);u8(0);put(c.id.pub_key,6);u8(outLen);n+=mesh::Packet::writePath(f+n,outPath,outLen);u8(inLen);n+=mesh::Packet::writePath(f+n,inPath,inLen);if(link)send();
  return true;
 }
 // The node's own telemetry (a request without a key): the battery, as CayenneLPP.
 void selfTelemetry(){
  CayenneLPP lpp(16);lpp.addVoltage(TELEM_CHANNEL_SELF,hardware.batteryMv/1000.0f);
  start(PushTelemetry);u8(0);put(core()->self_id.pub_key,6);put(lpp.getBuffer(),lpp.getSize());send();
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
  if(type!=TextPlain&&type!=TextCli){err(ErrUnsupported);return;}
  const char* text=(const char*)frame+13;size_t size=len-13;
  if(!size||size>MAX_TEXT_LEN||memchr(text,0,size)||!meshmesh::validUtf8((const uint8_t*)text,size)){err(ErrIllegalArg);return;}
  auto& r=meshRadio;uint64_t to=aliasOf(c->id.pub_key);Peer* p=r.contact(to);
  if(!r.ready||!config.bootCounter||!p){err(ErrBadState);return;}
  // A CLI command to a repeater or room: one packet with this node's time (the server's replay check), no ACK.
  if(type==TextCli){char line[MAX_TEXT_LEN+1];memcpy(line,text,size);line[size]=0;uint32_t timeout;
   sentReply(core()->sendCommandData(*c,coreRtc.getCurrentTimeUnique(),attempt,line,timeout),0,timeout);return;}
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
  field("MeshMesh " MESHMM_VERSION,20);u8(config.relay?1:0);u8(config.pathHash-1);send();
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
   case CmdLogin:case CmdStatusReq:case CmdAnonReq:case CmdBinaryReq:if(len<33||((cmd==CmdAnonReq||cmd==CmdBinaryReq)&&len<34))break;request(cmd,frame,len);return;
   case CmdTelemetryReq:if(len==4){selfTelemetry();return;}if(len<36)break;request(cmd,frame,len);return;
   case CmdTrace:if(len<=10||len-10>=MAX_PACKET_PAYLOAD-5)break;trace(frame,len);return;
   case CmdPathDiscovery:if(len<34||frame[1])break;discover(frame);return;
   case CmdHasConnection:if(len<33)break;if(d->hasConnectionTo(frame+1))ok();else err(ErrNotFound);return;
   case CmdLogout:if(len<33)break;d->stopConnection(frame+1);ok();return;
   case CmdGetChannel:{if(len<2)break;uint8_t i=frame[1];if(i>=channels::Max){err(ErrNotFound);return;}
    start(RespChannelInfo);u8(i);if(i<r.channelCount){field(r.channelList[i].name,32);put(r.channelList[i].secret,16);}else{field("",32);uint8_t zero[16]={};put(zero,16);}send();return;}
   case CmdSetChannel:if(len<50)break;if(len>=66){err(ErrUnsupported);return;}setChannel(frame);return; // 256-bit keys: not in MeshCore either
   case CmdCustomVars:start(RespCustomVars);send();return;
   case CmdStats:if(len<2)break;stats(frame[1]);return;
   case CmdSetPathHashMode:{if(len<3||frame[1])break;if(frame[2]>=3){err(ErrIllegalArg);return;}StaticJsonDocument<32> j;j["path_hash"]=frame[2]+1;settingsReply(apply(j));return;}
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
void cliMessage(const ContactInfo& from,const mesh::Packet* packet,uint32_t stamp,const char* text){app().message(false,from.id.pub_key,0,packet,stamp,text,TextCli);}
void roomPost(const ContactInfo& room,const mesh::Packet* packet,uint32_t stamp,const uint8_t* author,const char* text){app().message(false,room.id.pub_key,0,packet,stamp,text,TextSigned,author);}
void response(const ContactInfo& c,const uint8_t* data,uint8_t len){app().response(c,data,len);}
void traced(const mesh::Packet* packet,uint32_t tag,uint32_t auth,uint8_t flags,const uint8_t* snrs,const uint8_t* hashes,uint8_t pathLen){app().traced(packet,tag,auth,flags,snrs,hashes,pathLen);}
bool discovered(const ContactInfo& c,const uint8_t* inPath,uint8_t inLen,const uint8_t* outPath,uint8_t outLen,uint8_t extraType,const uint8_t* extra,uint8_t extraLen){return app().discovered(c,inPath,inLen,outPath,outLen,extraType,extra,extraLen);}
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
