#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: the ledger exchange are not speed-critical (the rest of the nRF52 image is -O2)
#endif
#include "ChessSync.h"
#include "ChessRating.h"
#include "MeshRadio.h"
#include "Config.h"
#include <ArduinoJson.h>
#if MM_CHESS
namespace ledger {
Ledger exchange;
namespace {
const char Tag[]="\xe2\x99\x9c"; // ♜
constexpr uint32_t Every=6UL*3600*1000,Pace=6000,HeardWindow=600000,PartLife=600000,Session=900000;
constexpr size_t WireSize=86+8+128,Chunk=120; // the core without "MMR1" and its flags byte, both clocks, both signatures
const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;i++)p[i]=v>>(8*i);}
uint32_t get32(const uint8_t* p){return p[0]|p[1]<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
String hex8(uint32_t v){char b[10];snprintf(b,sizeof b,"%08lX",(unsigned long)v);return b;}
String b64(const uint8_t* d,size_t n){String s;s.reserve((n+2)/3*4);for(size_t i=0;i<n;i+=3){uint32_t v=uint32_t(d[i])<<16|(i+1<n?d[i+1]<<8:0)|(i+2<n?d[i+2]:0);
  s+=B64[v>>18&63];s+=B64[v>>12&63];s+=i+1<n?B64[v>>6&63]:'=';s+=i+2<n?B64[v&63]:'=';}return s;}
size_t unb64(const char* t,size_t len,uint8_t* out,size_t cap){size_t n=0;uint32_t v=0;int bits=0;
  for(size_t i=0;i<len;i++){char c=t[i];if(c=='=')continue;const char* p=strchr(B64,c);if(!c||!p)return 0;v=v<<6|uint32_t(p-B64);bits+=6;if(bits>=8){bits-=8;if(n<cap)out[n++]=v>>bits;}}return n;}
void pack(const rating::Record& r,uint8_t* w){memcpy(w,r.core+4,86);put32(w+86,r.timeW);put32(w+90,r.timeB);memcpy(w+94,r.sigW,64);memcpy(w+158,r.sigB,64);}
void unpack(const uint8_t* w,rating::Record& r){memcpy(r.core,"MMR1",4);memcpy(r.core+4,w,86);r.core[90]=1;r.timeW=get32(w+86);r.timeB=get32(w+90);memcpy(r.sigW,w+94,64);memcpy(r.sigB,w+158,64);}
// Both signatures with the keys inside the record: no contact is needed to trust it.
bool verified(const rating::Record& r){
  uint8_t m[rating::CoreSize+4];rating::signedBytes(m,r.core,r.timeW);if(!MeshRadio::nodeVerify(r.core+4,r.sigW,m,sizeof m))return false;
  rating::signedBytes(m,r.core,r.timeB);return MeshRadio::nodeVerify(r.core+36,r.sigB,m,sizeof m);
}
}

PeerState* Ledger::state(uint64_t peer,bool make){
  for(auto& p:peers)if(p.id==peer)return &p;if(!make)return nullptr;
  PeerState* old=&peers[0];for(auto& p:peers){if(!p.id){old=&p;break;}if(int32_t(p.lastAt-old->lastAt)<0)old=&p;}
  *old=PeerState();old->id=peer;return old;
}
bool Ledger::buffers(){if(!buf)buf=(Buffers*)calloc(1,sizeof(Buffers));busyAt=millis();return buf;}
unsigned Ledger::synced() const{unsigned n=0;for(auto& p:peers)n+=p.id&&p.lastAt;return n;}
void Ledger::queue(uint64_t peer,const String& text){
  if(!buffers())return;
  for(auto& o:buf->outbox)if(!o.used){o.used=true;o.peer=peer;strlcpy(o.text,text.c_str(),sizeof o.text);return;}
  // Full: the session goes on next time.
}
bool Ledger::receive(uint64_t from,const char* name,const char*text){
  if(strncmp(text,Tag,3))return false;
  if(!buffers())return true;
  for(auto& i:buf->inbox)if(!i.used){i.used=true;i.peer=from;strlcpy(i.text,text,sizeof i.text);return true;}
  return true; // busy: the other side repeats on its next sync
}
bool Ledger::start(uint64_t peer){
  if(!rating::book.ready||!peer||peer==meshRadio.nodeId)return false;
  unsigned n=0;uint32_t d=rating::book.digest(n,Newest);PeerState* s=state(peer,true);
  s->startedAt=millis();s->lastAt=millis();s->invSent=false;queue(peer,String(Tag)+"sum "+String(n)+" "+hex8(d));return true;
}
unsigned Ledger::startAll(){
  unsigned n=0;uint32_t now=millis();
  for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(p.type!=1||!p.heard||now-p.seen>86400000UL)continue;
    if(rating::book.games(p.id)||rating::book.elo(p.id)!=rating::Start||state(p.id,false))n+=start(p.id);}
  return n;
}
void Ledger::sendInventory(uint64_t peer){
  uint32_t ids[Newest];unsigned n=rating::book.newest(ids,Newest),pages=n?(n+14)/15:1;
  for(unsigned p=0;p<pages;p++){String line=String(Tag)+"inv "+String(p+1)+"/"+String(pages)+" ";
    for(unsigned k=p*15;k<n&&k<p*15+15;k++)line+=(k>p*15?",":"")+hex8(ids[k]);queue(peer,line);}
  if(PeerState* s=state(peer,true))s->invSent=true;
}
void Ledger::sendRecord(uint64_t peer,uint32_t id){
  rating::Record r;if(!rating::book.get(id,r))return;uint8_t w[WireSize];pack(r,w);String text=b64(w,WireSize);unsigned n=(text.length()+Chunk-1)/Chunk;
  for(unsigned k=0;k<n;k++)queue(peer,String(Tag)+"rec "+hex8(id)+" "+String(k+1)+"/"+String(n)+" "+text.substring(k*Chunk,min(text.length(),(k+1)*Chunk)));
  if(PeerState* s=state(peer,true))s->sent++;
}
void Ledger::gotPart(uint64_t from,const char* args){
  char idh[10]={};unsigned k=0,n=0;int at=0;if(sscanf(args,"%8s %u/%u %n",idh,&k,&n,&at)<3||!at||!k||k>n||n>3)return;
  uint32_t id=strtoul(idh,nullptr,16);const char* chunk=args+at;size_t len=strlen(chunk);if(len>Chunk)return;
  if(!buffers())return;
  Part* p=nullptr;for(auto& x:buf->parts)if(x.used&&x.peer==from&&x.id==id)p=&x;
  if(!p){p=&buf->parts[0];for(auto& x:buf->parts){if(!x.used){p=&x;break;}if(int32_t(x.at-p->at)<0)p=&x;}*p=Part();p->used=true;p->peer=from;p->id=id;}
  memcpy(p->b64+(k-1)*Chunk,chunk,len);if(k==n)p->b64[(k-1)*Chunk+len]=0;p->mask|=1<<(k-1);p->at=millis();
  if(p->mask!=(1u<<n)-1)return;
  uint8_t w[WireSize];size_t got=unb64(p->b64,strlen(p->b64),w,sizeof w);p->used=false;if(got!=WireSize)return;
  rating::Record r;unpack(w,r);if(rating::Book::idOfRecord(r)!=id||!verified(r))return;
  if(rating::book.add(r,"")){received++;if(PeerState* s=state(from,true))s->got++;
    event=tr("Rating ledger: records received ","Журнал рейтинга: получено записей ")+String(received);events++;}
}
void Ledger::handle(uint64_t from,const char* text){
  const char* rest=text+3;char verb[6]={};int used=0;if(sscanf(rest,"%5s%n",verb,&used)<1)return;const char* args=rest+used;while(*args==' ')args++;
  PeerState* s=state(from,true);uint32_t now=millis();
  if(s->startedAt&&now-s->startedAt>Session){s->invSent=false;s->startedAt=0;}
  if(!strcmp(verb,"sum")){unsigned n=0;char dh[10]={};if(sscanf(args,"%u %8s",&n,dh)<2)return;unsigned mine=0;uint32_t d=rating::book.digest(mine,Newest);
    s->lastAt=now;if(!s->startedAt)s->startedAt=now;
    if(mine==n&&d==strtoul(dh,nullptr,16))queue(from,String(Tag)+"ok");else if(!s->invSent)sendInventory(from);return;}
  if(!strcmp(verb,"ok")){s->lastAt=now;return;}
  if(!strcmp(verb,"inv")){ // their newest records: ask for the ones missing here, and tell them ours once
    s->lastAt=now;if(!s->startedAt)s->startedAt=now;
    uint32_t have[rating::MaxRecords];unsigned hn=rating::book.newest(have,rating::MaxRecords);const char* list=strchr(args,' ');String want;unsigned asked=0;
    while(list&&*list&&asked<PerSession){while(*list==' '||*list==',')list++;if(!*list)break;uint32_t id=strtoul(list,nullptr,16);bool known=false;
      for(unsigned i=0;i<hn&&!known;i++)known=have[i]==id;if(!known&&id){want+=(want.length()?",":"")+hex8(id);asked++;}list=strchr(list,',');}
    if(want.length())queue(from,String(Tag)+"want "+want);
    if(!s->invSent)sendInventory(from);return;}
  if(!strcmp(verb,"want")){unsigned sent=0;for(const char* p=args;*p&&sent<PerSession;){uint32_t id=strtoul(p,nullptr,16);if(id){sendRecord(from,id);sent++;}p=strchr(p,',');if(!p)break;p++;}return;}
  if(!strcmp(verb,"rec"))gotPart(from,args);
}
void Ledger::tick(){
  uint32_t now=millis();
  if(buf){
    for(auto& i:buf->inbox)if(i.used){busyAt=now;In copy=i;i.used=false;handle(copy.peer,copy.text);break;} // one message per pass
    for(auto& p:buf->parts)if(p.used&&now-p.at>PartLife)p.used=false;
    if(now-sendAt>=Pace&&!meshRadio.busy())for(auto& o:buf->outbox)if(o.used){busyAt=now;if(meshRadio.sendGame(o.text,o.peer))o.used=false;sendAt=now;break;}
    bool idle=true;for(auto& i:buf->inbox)idle&=!i.used;for(auto& o:buf->outbox)idle&=!o.used;for(auto& p:buf->parts)idle&=!p.used;
    if(idle&&now-busyAt>60000){free(buf);buf=nullptr;}
  }
  // Automatic: a rated player heard in the last 10 minutes, at most every 6 hours per node.
  if(now-autoAt>=10000&&rating::book.ready){autoAt=now;
    for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(p.type!=1||!p.heard||now-p.seen>HeardWindow)continue;
      if(!rating::book.games(p.id)&&!state(p.id,false))continue;PeerState* s=state(p.id,false);if(s&&s->lastAt&&now-s->lastAt<Every)continue;start(p.id);break;}}
}
String Ledger::json() const{
  DynamicJsonDocument d(2048);d["received"]=received;d["synced"]=synced();unsigned q=0;if(buf)for(auto& o:buf->outbox)q+=o.used;d["queue"]=q;
  JsonArray list=d.createNestedArray("peers");uint32_t now=millis();
  for(auto& p:peers){if(!p.id)continue;JsonObject o=list.createNestedObject();o["id"]=meshRadio.idText(p.id);o["ago"]=p.lastAt?(now-p.lastAt)/1000:-1;o["got"]=p.got;o["sent"]=p.sent;}
  String s;serializeJson(d,s);return s;
}
}
#else
namespace ledger {
Ledger exchange;
bool Ledger::receive(uint64_t,const char*,const char*){return false;}
void Ledger::tick(){}
}
#endif
