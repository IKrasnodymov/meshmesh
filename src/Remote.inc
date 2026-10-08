// Repeaters and rooms from this node's screen and web page (include/Remote.h), included at the end of MeshRadio.cpp
// after Companion.inc: logins, status, CLI and trace go out through MeshCoreBackend like a companion app's; the
// answers are matched here first, the app gets the rest.
#include "Remote.h"
// The private parts the sessions use.
struct RemoteAccess {
 static MeshCoreBackend* core(){return meshRadio.core;}
 static Peer* peer(uint64_t id){return meshRadio.contact(id);}
 static uint32_t directTimeout(uint32_t air,uint8_t hops){return meshRadio.core->calcDirectTimeoutMillisFor(air,hops);}
 static void keepAlive(const ContactInfo& c,uint16_t seconds){meshRadio.core->startConnection(c,seconds);}
};
namespace remote {
Session sessions[MaxSessions];Trace trace;
namespace {
// Passwords that logged in: the first 8 bytes of the server's key and the password.
struct SavedLogin {uint8_t key[8];char password[16];};
struct LoginFile {uint32_t version=1;SavedLogin logins[6]={};uint8_t hash[8]={};};
const char* const LoginPath="/meshmesh/remote.bin";const char* const LoginTemp="/meshmesh/remote.tmp";
LoginFile* logins=nullptr;
LoginFile& loginFile(){
 if(logins)return *logins;logins=new LoginFile;uint8_t digest[8];
 if(!readStored(LoginPath,LoginTemp,logins,sizeof(*logins))||(mesh::Utils::sha256(digest,8,(const uint8_t*)logins,offsetof(LoginFile,hash)),memcmp(digest,logins->hash,8))||logins->version!=1)*logins=LoginFile();
 return *logins;
}
SavedLogin* savedLogin(const uint8_t* key){for(auto& s:loginFile().logins)if(!memcmp(s.key,key,8))return &s;return nullptr;}
void keep(const uint8_t* key,const char* password){
 auto& f=loginFile();SavedLogin* s=savedLogin(key);
 if(s&&!strcmp(s->password,password))return;
 if(!s){memmove(&f.logins[1],&f.logins[0],sizeof(SavedLogin)*5);s=&f.logins[0];} // the oldest goes
 memcpy(s->key,key,8);strlcpy(s->password,password,sizeof(s->password));
 mesh::Utils::sha256(f.hash,8,(const uint8_t*)&f,offsetof(LoginFile,hash));writeStored(LoginPath,LoginTemp,&f,sizeof(f));
}
MeshCoreBackend* core(){return RemoteAccess::core();}
ContactInfo* contactOf(uint64_t id){Peer* p=RemoteAccess::peer(id);return p&&core()?core()->lookupContactByPubKey(p->publicKey,32):nullptr;}
Session* open(uint64_t id){
 Session* s=find(id);if(s)return s;
 Session* oldest=&sessions[0];for(auto& x:sessions){if(!x.id){oldest=&x;break;}if(x.loginAt<oldest->loginAt)oldest=&x;}
 *oldest=Session();oldest->id=id;return oldest;
}
bool ready(){return core()&&meshRadio.ready&&config.bootCounter;}
void waitFor(Session& s,int result,uint32_t timeout){s.due=millis()+timeout+3000;meshRadio.dirty=true;(void)result;}
const char* stateName(State s){const char* n[]={"idle","waiting","done","refused","silent"};return n[s];}
}
Session* find(uint64_t id){for(auto& s:sessions)if(s.id==id)return &s;return nullptr;}
bool saved(uint64_t id){Peer* p=RemoteAccess::peer(id);return p&&savedLogin(p->publicKey);}
bool login(uint64_t id,const String& password,bool useSaved){
 ContactInfo* c=contactOf(id);if(!c||!ready()||password.length()>15)return false;
 String pass=password;if(!pass.length()&&useSaved)if(SavedLogin* s=savedLogin(c->id.pub_key))pass=s->password;
 uint32_t timeout;int result=core()->sendLogin(*c,pass.c_str(),timeout);if(result==MSG_SEND_FAILED)return false;
 Session& s=*open(id);s.login=Waiting;s.loginAt=millis();s.room=c->type==ADV_TYPE_ROOM;strlcpy(s.password,pass.c_str(),sizeof(s.password));waitFor(s,result,timeout);return true;
}
bool status(uint64_t id){
 ContactInfo* c=contactOf(id);if(!c||!ready())return false;uint32_t tag,timeout;int result=core()->sendRequest(*c,REQ_TYPE_GET_STATUS,tag,timeout);if(result==MSG_SEND_FAILED)return false;
 Session& s=*open(id);s.status=Waiting;s.statusAt=millis();s.room=c->type==ADV_TYPE_ROOM;waitFor(s,result,timeout);return true;
}
bool command(uint64_t id,const String& line){
 ContactInfo* c=contactOf(id);Session* s=find(id);if(!c||!ready()||!s||s->login!=Done||!s->admin||!line.length()||line.length()>MAX_TEXT_LEN)return false;
 uint32_t timeout;int result=core()->sendCommandData(*c,coreRtc.getCurrentTimeUnique(),0,line.c_str(),timeout);if(result==MSG_SEND_FAILED)return false;
 s->command=Waiting;s->commandAt=millis();waitFor(*s,result,timeout);return true;
}
// The route's repeaters (first byte of each hash), the repeater itself when it is the target, then the way back.
static uint8_t tracePath(const ContactInfo& c,uint8_t path[16]){
 uint8_t n=0;
 if(c.out_path_len!=OUT_PATH_UNKNOWN){uint8_t size=(c.out_path_len>>6)+1,count=c.out_path_len&63;for(uint8_t i=0;i<count&&n<8;i++)path[n++]=c.out_path[i*size];}
 if(c.type==ADV_TYPE_REPEATER&&n<8)path[n++]=c.id.pub_key[0];
 if(!n)return 0;for(int i=n-2;i>=0;i--)path[n++]=path[i];return n;
}
bool traceable(uint64_t id){ContactInfo* c=contactOf(id);uint8_t path[16];return c&&tracePath(*c,path);}
bool traceTo(uint64_t id){
 ContactInfo* c=contactOf(id);uint8_t path[16];uint8_t n=c?tracePath(*c,path):0;if(!n||!ready())return false;
 uint32_t tag;coreRandom.random((uint8_t*)&tag,4);mesh::Packet* p=core()->createTrace(tag,0,0);if(!p)return false;
 core()->sendDirect(p,path,n);uint32_t air=coreRadio.getEstAirtimeFor(p->payload_len+2);
 trace=Trace();trace.id=id;trace.state=Waiting;trace.at=millis();trace.tag=tag;trace.due=millis()+RemoteAccess::directTimeout(air,n)+3000;meshRadio.dirty=true;return true;
}
void tick(){
 uint32_t now=millis();
 for(auto& s:sessions)if(s.id&&(s.login==Waiting||s.status==Waiting||s.command==Waiting)&&int32_t(now-s.due)>=0){
  if(s.login==Waiting)s.login=Silent;if(s.status==Waiting)s.status=Silent;if(s.command==Waiting)s.command=s.replies?Done:Silent;meshRadio.dirty=true;}
 if(trace.state==Waiting&&int32_t(now-trace.due)>=0){trace.state=Silent;meshRadio.dirty=true;}
}
String json(){
 DynamicJsonDocument d(3072);JsonArray a=d.createNestedArray("sessions");
 for(auto& s:sessions){if(!s.id)continue;JsonObject o=a.createNestedObject();o["id"]=meshRadio.idText(s.id);Peer* p=RemoteAccess::peer(s.id);if(p)o["name"]=p->name;
  o["room"]=s.room;o["login"]=stateName(s.login);o["admin"]=s.admin;o["saved"]=saved(s.id);o["status_state"]=stateName(s.status);o["command_state"]=stateName(s.command);
  if(s.statusAt&&s.status==Done){JsonObject st=o.createNestedObject("status");st["battery_mv"]=s.battery;st["uptime"]=s.uptime;st["received"]=s.received;st["sent"]=s.sent;st["airtime"]=s.airtime;st["queue"]=s.queue;st["noise"]=s.noise;st["rssi"]=s.rssi;st["snr"]=s.snr/4.0f;if(s.room)st["posts"]=s.posted;st["age"]=(millis()-s.statusAt)/1000;}
  JsonArray r=o.createNestedArray("replies");for(unsigned i=0;i<s.replies;i++)r.add(s.reply[i]);}
 if(trace.id){JsonObject t=d.createNestedObject("trace");t["id"]=meshRadio.idText(trace.id);t["state"]=stateName(trace.state);
  if(trace.state==Done){JsonArray h=t.createNestedArray("hops");for(unsigned i=0;i<trace.hops;i++){JsonObject x=h.createNestedObject();char hash[3];snprintf(hash,3,"%02X",trace.hashes[i]);x["hash"]=hash;x["snr"]=trace.snr[i]/4.0f;}t["snr"]=trace.snr[trace.hops]/4.0f;}}
 String out;serializeJson(d,out);return out;
}
String usbCommand(const String& line){
 if(line=="remote")return json();
 int sp=line.indexOf(' ',7);String what=line.substring(7,sp<0?line.length():sp);String rest=sp<0?String():line.substring(sp+1);
 int sp2=rest.indexOf(' ');String node=sp2<0?rest:rest.substring(0,sp2),arg=sp2<0?String():rest.substring(sp2+1);
 uint64_t id=strtoull(node.c_str(),nullptr,16);if(!id||!RemoteAccess::peer(id))return "ERR remote login|status|cli|trace NODE_ID [text]: unknown node";
 if(what=="login")return login(id,arg)?"OK login sent":"ERR login not sent (radio, contact or password over 15 bytes)";
 if(what=="status")return status(id)?"OK status requested":"ERR status not sent";
 if(what=="cli")return command(id,arg)?"OK command sent":"ERR command needs an admin login";
 if(what=="trace")return traceTo(id)?"OK trace sent":"ERR no route through repeaters to trace";
 return "ERR remote login|status|cli|trace NODE_ID [text]";
}
// The backend's answers; true when this was ours.
bool onResponse(const ContactInfo& c,const uint8_t* data,uint8_t len){
 Session* s=find(aliasOf(c.id.pub_key));if(!s||len<5)return false;
 if(s->login==Waiting){
  bool ok=!memcmp(data+4,"OK",2)||(len>=7&&data[4]==RESP_SERVER_LOGIN_OK);
  s->login=ok?Done:Refused;s->admin=ok&&len>=7&&data[4]==RESP_SERVER_LOGIN_OK&&(data[6]&1);s->loginAt=millis();
  if(ok){keep(c.id.pub_key,s->password);uint16_t keepAlive=len>=6&&data[4]==RESP_SERVER_LOGIN_OK?uint16_t(data[5])*16:0;if(keepAlive)RemoteAccess::keepAlive(c,keepAlive);}
  meshRadio.event=ok?String("Logged in: ")+c.name:String("Login refused: ")+c.name;meshRadio.dirty=true;return true;
 }
 if(s->status==Waiting&&len>=4+28){
  const uint8_t* b=data+4;memcpy(&s->battery,b,2);memcpy(&s->queue,b+2,2);memcpy(&s->noise,b+4,2);memcpy(&s->rssi,b+6,2);memcpy(&s->received,b+8,4);memcpy(&s->sent,b+12,4);
  memcpy(&s->airtime,b+16,4);memcpy(&s->uptime,b+20,4);if(len>=4+44)memcpy(&s->snr,b+42,2);if(s->room&&len>=4+50)memcpy(&s->posted,b+48,2);
  s->status=Done;s->statusAt=millis();meshRadio.dirty=true;return true;
 }
 return false;
}
void onCli(const ContactInfo& c,const char* text){
 Session* s=find(aliasOf(c.id.pub_key));if(!s)return;
 memmove(s->reply[1],s->reply[0],sizeof(s->reply[0])*2);copyUtf8(s->reply[0],text,sizeof(s->reply[0]));s->replies=min<uint8_t>(s->replies+1,3);
 s->command=Done;s->commandAt=millis();meshRadio.dirty=true;
}
bool onTrace(const mesh::Packet* packet,uint32_t tag,uint8_t flags,const uint8_t* snrs,const uint8_t* hashes,uint8_t pathLen){
 if(trace.state!=Waiting||tag!=trace.tag||(flags&3))return false;
 uint8_t n=min<uint8_t>(pathLen,16);trace.hops=n;memcpy(trace.hashes,hashes,n);memcpy(trace.snr,snrs,n);trace.snr[n]=int8_t(packet->getSNR()*4);
 trace.state=Done;meshRadio.dirty=true;return true;
}
}
