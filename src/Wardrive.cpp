#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "Wardrive.h"
#include "BlobStore.h"
#include "Config.h"
#include "MeshRadio.h"
#include "App.h"
#if !defined(MM_NO_WIFI)&&!defined(MM_UI_PREVIEW)
#include "Radar.h"
#endif
#include <time.h>
#include <math.h>
namespace wardrive {
Settings settings;
#if MM_WARDRIVE
namespace {
constexpr uint32_t Magic=0x4d4d5701;
constexpr uint8_t TypeAdvert=4; // MeshCore PAYLOAD_TYPE_ADVERT
constexpr uint32_t PhoneValid=30000,PingWindow=30000,QueueLimit=60000,Batch=30000;
#if defined(MM_NRF52)
constexpr unsigned PointCap=256,NetCap=1,RecentMax=48,NetSeen=1; // 100 KB of storage holds the history too
#else
constexpr unsigned PointCap=4096,NetCap=2048,RecentMax=256,NetSeen=128;
#endif
// One log: the current file and the previous one; records wait in RAM until 16 or 30 s. A file whose size is no
// multiple of its record (a reset in the middle of a write) becomes the previous one: whole records stay readable.
template<class T> struct Log {
 const char* path;const char* old;unsigned cap;T* const waiting=new T[16]; // on the heap: static memory is short on the classic ESP32 (dram0)
 unsigned pending=0;uint32_t current=0,previous=0,firstAt=0,lost=0;
 Log(const char* p,const char* o,unsigned c):path(p),old(o),cap(c){}
 static uint32_t bytes(const char* p){if(!LittleFS.exists(p))return 0;File f=LittleFS.open(p,FILE_READ);if(!f)return 0;uint32_t n=f.size();f.close();return n;}
 bool rotate(){if(LittleFS.exists(old)&&!LittleFS.remove(old))return false;if(!LittleFS.rename(path,old))return false;previous=current;current=0;return true;}
 void begin(){if(!hardware.fsOk)return;uint32_t n=bytes(path);current=n/sizeof(T);previous=bytes(old)/sizeof(T);if(n%sizeof(T))rotate();}
 void add(const T& r){if(pending==16)write();if(pending==16){lost++;return;}if(!pending)firstAt=millis();waiting[pending++]=r;}
 void write(){
  if(!pending)return;unsigned n=pending;pending=0;if(!hardware.fsOk){lost+=n;return;}
  if(current>=cap&&!rotate()){lost+=n;return;}
  LittleFS.mkdir("/meshmesh");File f=LittleFS.open(path,FILE_APPEND);if(!f){lost+=n;return;}
  size_t done=f.write((const uint8_t*)waiting,n*sizeof(T));f.close();current+=done/sizeof(T);lost+=n-done/sizeof(T);
  if(done%sizeof(T))rotate();
 }
 void tick(){if(pending&&millis()-firstAt>=Batch)write();}
 uint32_t total() const{return previous+current+pending;}
 void clear(){pending=0;current=previous=lost=0;if(!hardware.fsOk)return;if(LittleFS.exists(path))LittleFS.remove(path);if(LittleFS.exists(old))LittleFS.remove(old);}
 // Records from..from+max-1 in order: the previous file, the current one, then those still in RAM.
 unsigned read(uint32_t from,T* out,unsigned max){
  unsigned n=0;const uint32_t parts[2]={previous,current};const char* files[2]={old,path};uint32_t base=0;
  for(unsigned k=0;k<2&&n<max;k++){uint32_t count=parts[k];if(from+n<base+count&&hardware.fsOk){File f=LittleFS.open(files[k],FILE_READ);
    if(f&&f.seek((from+n-base)*sizeof(T))){while(n<max&&from+n<base+count&&f.read((uint8_t*)&out[n],sizeof(T))==sizeof(T))n++;}if(f)f.close();}
   base+=count;}
  while(n<max&&from+n>=base&&from+n<base+pending){out[n]=waiting[from+n-base];n++;}
  return n;
 }
};
Log<Point> pointLog("/meshmesh/wd-mesh.bin","/meshmesh/wd-mesh.old",PointCap);
Log<Net> netLog("/meshmesh/wd-nets.bin","/meshmesh/wd-nets.old",NetCap);
uint32_t phoneAt=0,noFix=0,pingsSent=0,pingsHeard=0,pingsFailed=0,lastPingAt=0,retryAt=0;int32_t phoneLat=0,phoneLon=0,pingLat=0,pingLon=0;bool havePing=false;
#if !defined(MM_NO_WIFI)&&!defined(MM_UI_PREVIEW)
bool heldRadar=false;
#endif
struct Probe {uint32_t id=0,hash=0,queuedAt=0,sentAt=0,time=0;int32_t lat=0,lon=0;bool phone=false;uint8_t count=0,size=0,hash3[3]={},seen[8][3]={},seenSize[8]={};int8_t best=-128;} probe;
Result result;
Repeater table[24];unsigned tableCount=0;
Recent* const ring=new Recent[RecentMax];unsigned ringCount=0,ringNext=0; // heap, as the logs' buffers
// The last log entry of each repeater (passive) and MAC (nets): logged again after moving or a while.
struct Seen {uint32_t key=0,at=0;int32_t lat=0,lon=0;};Seen hops[16];unsigned hopNext=0;Seen* const macs=new Seen[NetSeen];unsigned macNext=0;
uint8_t three(uint8_t size){return size<3?size:3;}
uint32_t clockNow(){return MeshRadio::clockSet()?uint32_t(time(nullptr)):0;}
int8_t quarter(float snr){return int8_t(constrain(lroundf(snr*4),-128L,127L));}
int8_t dbm(float rssi){return int8_t(constrain(lroundf(rssi),-128L,127L));}
float metres(int32_t aLat,int32_t aLon,int32_t bLat,int32_t bLon){
 const double k=M_PI/180e6;double x=double(bLon-aLon)*k*cos(double(aLat+bLat)*0.5*k),y=double(bLat-aLat)*k;return float(sqrt(x*x+y*y)*6371000.0);}
uint32_t keyOf(const uint8_t* hash,uint8_t size){uint32_t k=size;for(unsigned i=0;i<3;i++)k=k<<8|(i<size?hash[i]:0);return k;}
bool sameHash(const uint8_t* a,uint8_t sa,const uint8_t* b,uint8_t sb){uint8_t n=min(sa,sb);return n&&!memcmp(a,b,n);}
// Logged again: unknown, moved by `far` metres, or `after` ms since.
bool fresh(Seen* list,unsigned size,unsigned& next,uint32_t key,int32_t lat,int32_t lon,float far,uint32_t after){
 uint32_t now=millis();for(unsigned i=0;i<size;i++){auto& s=list[i];if(!s.at||s.key!=key)continue;if(now-s.at<after&&metres(s.lat,s.lon,lat,lon)<far)return false;s={key,now?now:1,lat,lon};return true;}
 list[next]={key,now?now:1,lat,lon};next=(next+1)%size;return true;
}
void remember(const Point& p){ring[ringNext]={p.lat,p.lon,p.kind==Ping&&!p.count?int8_t(-128):p.snr,p.kind};ringNext=(ringNext+1)%RecentMax;if(ringCount<RecentMax)ringCount++;}
void noteRepeater(const uint8_t* hash,uint8_t size,int8_t snr,bool echo){
 Repeater* r=nullptr;for(unsigned i=0;i<tableCount;i++)if(sameHash(table[i].hash,table[i].size,hash,size)){r=&table[i];break;}
 if(!r){if(tableCount<24)r=&table[tableCount++];else{r=&table[0];for(unsigned i=1;i<tableCount;i++)if(table[i].at<r->at)r=&table[i];}*r={};memcpy(r->hash,hash,three(size));r->size=size;r->snr=snr;}
 else if(size>r->size){memcpy(r->hash,hash,three(size));r->size=size;}
 if(snr>r->snr)r->snr=snr;if(echo){if(r->echoes<65535)r->echoes++;}else if(r->rx<65535)r->rx++;r->at=millis()?millis():1;
}
Point start(Kind kind,int32_t lat,int32_t lon,bool phone){Point p{};p.time=clockNow();p.lat=lat;p.lon=lon;p.kind=kind;p.flags=phone?FromPhone:0;p.snr=-128;p.rssi=-128;return p;}
void save(){blob::save("/meshmesh/wardrive",Magic,settings);}
String hex(const uint8_t* b,size_t n){static const char d[]="0123456789ABCDEF";String s;for(size_t i=0;i<n;i++){s+=d[b[i]>>4];s+=d[b[i]&15];}return s;}
// The ping channel: the chosen one while joined, else #wardrive (joined now).
bool pingChannel(uint64_t& id){
 if(settings.channel&&meshRadio.channel(settings.channel)){id=settings.channel;return true;}
 uint64_t added=0;auto r=meshRadio.joinHashtag("wardrive",&added);if(r!=MeshRadio::ChannelAdded&&r!=MeshRadio::ChannelExists)return false;
 settings.channel=id=added;save();return true;
}
bool sendPing(){
 int32_t lat,lon;bool phone;if(probe.id||!position(lat,lon,phone))return false;uint64_t channel;if(!pingChannel(channel)){meshRadio.event="Wardrive: no room for #wardrive";meshRadio.dirty=true;return false;}
 String text="wardrive";if(settings.coords){char b[40];snprintf(b,sizeof b," %.5f,%.5f",lat/1e6,lon/1e6);text+=b;}
 uint32_t id=meshRadio.sendProbe(text,channel);if(!id)return false;
 probe=Probe{};probe.id=id;probe.queuedAt=millis();probe.time=clockNow();probe.lat=lat;probe.lon=lon;probe.phone=phone;
 lastPingAt=millis();pingLat=lat;pingLon=lon;havePing=true;return true;
}
void finishPing(){
 Point p=start(Ping,probe.lat,probe.lon,probe.phone);p.time=probe.time;p.count=probe.count;p.snr=probe.best;p.size=probe.size;memcpy(p.hash,probe.hash3,3);
 pointLog.add(p);remember(p);if(probe.count)pingsHeard++;
 result={};result.at=millis()?millis():1;result.count=probe.count;result.size=probe.size;memcpy(result.hash,probe.hash3,3);result.snr=probe.best;result.done=true;
 probe=Probe{};meshRadio.dirty=true;
}
bool parseId(const char* text,uint64_t& id){if(!text||!*text||strlen(text)>16)return false;char* end=nullptr;id=strtoull(text,&end,16);return end&&!*end&&id;}
}
void begin(){Settings saved{};if(blob::load("/meshmesh/wardrive",Magic,saved)&&saved.distance>=50&&saved.distance<=5000&&saved.interval>=15&&saved.interval<=3600)settings=saved;
#if defined(MM_NO_WIFI)
 settings.nets=false;
#endif
 pointLog.begin();netLog.begin();}
bool running(){return settings.passive||settings.ping||settings.nets;}
bool holdsRadar(){return settings.nets;}
bool pinging(){return probe.id;}
const Result& last(){return result;}
unsigned repeaters(const Repeater*& out){out=table;return tableCount;}
unsigned recent(const Recent*& out){out=ring;return ringCount;}
uint32_t points(){return pointLog.total();}
uint32_t nets(){return netLog.total();}
bool position(int32_t& lat,int32_t& lon,bool& phone){
 if(config.gps&&hardware.gpsFix()&&(!hardware.gps.hdop.isValid()||hardware.gps.hdop.hdop()<=5)){lat=lround(hardware.gps.location.lat()*1e6);lon=lround(hardware.gps.location.lng()*1e6);phone=false;return true;}
 if(phoneAt&&millis()-phoneAt<PhoneValid){lat=phoneLat;lon=phoneLon;phone=true;return true;}
 return false;
}
void flush(){pointLog.write();netLog.write();}
bool ping(){if(probe.id||(lastPingAt&&millis()-lastPingAt<10000))return false;return sendPing();}
void tick(){
 uint32_t now=millis();pointLog.tick();netLog.tick();
 if(probe.id&&!probe.sentAt&&now-probe.queuedAt>=QueueLimit){probe=Probe{};pingsFailed++;}
 if(probe.sentAt&&now-probe.sentAt>=PingWindow)finishPing();
 if(settings.ping&&!probe.id&&(!retryAt||int32_t(now-retryAt)>=0)){int32_t lat,lon;bool phone;
  if(position(lat,lon,phone)&&(!havePing||(now-lastPingAt>=settings.interval*1000UL&&metres(pingLat,pingLon,lat,lon)>=settings.distance)))if(!sendPing())retryAt=now+5000;}
#if !defined(MM_NO_WIFI)&&!defined(MM_UI_PREVIEW)
 // The radar sweeps for the nets log; the screen and web page may hold it as well.
 if(holdsRadar()&&!radar.active){radar.open();heldRadar=true;}
 else if(!holdsRadar()&&heldRadar){heldRadar=false;if(radar.active&&!uiRadarPage()&&!webRadarActive())radar.close();}
#endif
}
void heard(uint32_t hash,uint8_t type,bool flood,uint8_t pathLen,const uint8_t* path,const uint8_t* payload,size_t length,float snr,float rssi){
 if(!settings.passive||(probe.hash&&hash==probe.hash))return; // copies of our ping are its echoes
 int32_t lat,lon;bool phone;if(!position(lat,lon,phone)){noFix++;return;}
 Point p=start(Rx,lat,lon,phone);p.type=type;p.snr=quarter(snr);p.rssi=dbm(rssi);
 uint8_t size=(pathLen>>6)+1,count=pathLen&63;
 if(!flood)p.flags|=Direct;else{p.hops=count;
  if(count){p.size=size;memcpy(p.hash,path+(count-1)*size,three(size));}
  else if(type==TypeAdvert&&length>=3){p.size=3;memcpy(p.hash,payload,3);p.flags|=SenderKey;}} // heard from the node itself
 if(p.size)noteRepeater(p.hash,p.size,p.snr,false);
 if(!fresh(hops,16,hopNext,keyOf(p.hash,p.size)^(uint32_t(p.flags&Direct)<<31),lat,lon,max(25.0f,settings.distance/4.0f),120000))return;
 pointLog.add(p);remember(p);meshRadio.dirty=true;
}
void echo(uint32_t packet,uint8_t pathLen,const uint8_t* path,float snr){
 if(!probe.sentAt||packet!=probe.hash)return;uint8_t size=(pathLen>>6)+1,count=pathLen&63;if(!count)return;
 const uint8_t* last=path+(count-1)*size;uint8_t n=three(size);
 for(unsigned i=0;i<(probe.count<8?probe.count:8);i++)if(sameHash(probe.seen[i],probe.seenSize[i],last,size))return;
 if(probe.count<8){memcpy(probe.seen[probe.count],last,n);probe.seenSize[probe.count]=size;}
 int8_t q=quarter(snr);if(probe.count<255)probe.count++;if(q>probe.best||!probe.size){probe.best=q;probe.size=size;memset(probe.hash3,0,3);memcpy(probe.hash3,last,n);}
 Point p=start(Echo,probe.lat,probe.lon,probe.phone);p.time=probe.time;p.snr=q;p.hops=count;p.size=size;memcpy(p.hash,last,n);
 pointLog.add(p);noteRepeater(p.hash,size,q,true);meshRadio.dirty=true;
}
void probeSent(uint32_t id,uint32_t hash){if(probe.id!=id||probe.sentAt)return;probe.hash=hash;probe.sentAt=millis()?millis():1;pingsSent++;}
void probeFailed(uint32_t id){if(probe.id!=id||probe.sentAt)return;probe=Probe{};pingsFailed++;}
void wifi(const uint8_t* mac,const char* ssid,uint8_t channel,uint8_t auth,int rssi){
#if !defined(MM_NO_WIFI)
 if(!settings.nets)return;int32_t lat,lon;bool phone;if(!position(lat,lon,phone))return;
 uint32_t key=blob::checksum(mac,6)&~1u;if(!fresh(macs,NetSeen,macNext,key,lat,lon,100,300000))return;
 Net n{};n.time=clockNow();n.lat=lat;n.lon=lon;memcpy(n.mac,mac,6);n.kind=0;n.channel=channel;n.rssi=dbm(rssi);n.auth=auth;n.flags=phone?FromPhone:0;strlcpy(n.name,ssid,sizeof n.name);netLog.add(n);
#endif
}
void ble(uint64_t address,const char* name,int rssi){
#if !defined(MM_NO_WIFI)
 if(!settings.nets)return;int32_t lat,lon;bool phone;if(!position(lat,lon,phone))return;
 uint8_t mac[6];for(int i=0;i<6;i++)mac[i]=uint8_t(address>>(8*(5-i)));
 uint32_t key=blob::checksum(mac,6)|1u;if(!fresh(macs,NetSeen,macNext,key,lat,lon,100,300000))return;
 Net n{};n.time=clockNow();n.lat=lat;n.lon=lon;memcpy(n.mac,mac,6);n.kind=1;n.rssi=dbm(rssi);n.flags=phone?FromPhone:0;strlcpy(n.name,name?name:"",sizeof n.name);netLog.add(n);
#endif
}
String json(){
 DynamicJsonDocument d(4096);d["supported"]=true;d["passive"]=settings.passive;d["ping"]=settings.ping;d["coords"]=settings.coords;d["nets"]=settings.nets;
#if defined(MM_NO_WIFI)
 d["nets_supported"]=false;
#else
 d["nets_supported"]=true;
#endif
 d["distance"]=settings.distance;d["interval"]=settings.interval;
 if(settings.channel){d["channel"]=meshRadio.idText(settings.channel);const auto* c=meshRadio.channel(settings.channel);if(c)d["channel_name"]=c->name;}
 int32_t lat,lon;bool phone;if(position(lat,lon,phone)){JsonObject p=d.createNestedObject("position");p["lat"]=lat/1e6;p["lon"]=lon/1e6;p["source"]=phone?"phone":"gps";}
 else d["position"]=nullptr;
 if(phoneAt)d["phone_age"]=(millis()-phoneAt)/1000;d["gps_fix"]=hardware.gpsFix();d["clock"]=MeshRadio::clockSet();
 d["points"]=pointLog.total();d["nets_logged"]=netLog.total();d["capacity"]=PointCap*2;d["nets_capacity"]=NetCap>1?NetCap*2:0;d["lost"]=pointLog.lost+netLog.lost;d["no_fix"]=noFix;
 d["pings"]=pingsSent;d["pings_heard"]=pingsHeard;d["pings_failed"]=pingsFailed;d["pinging"]=pinging();
 if(havePing)d["ping_age"]=(millis()-lastPingAt)/1000;
 if(result.done){JsonObject r=d.createNestedObject("last");r["age"]=(millis()-result.at)/1000;r["count"]=result.count;if(result.count){r["hash"]=hex(result.hash,three(result.size));r["snr"]=result.snr/4.0f;}}
 JsonArray a=d.createNestedArray("repeaters");for(unsigned i=0;i<tableCount;i++){auto& r=table[i];JsonObject o=a.createNestedObject();o["hash"]=hex(r.hash,three(r.size));o["snr"]=r.snr/4.0f;o["rx"]=r.rx;o["echoes"]=r.echoes;o["age"]=(millis()-r.at)/1000;}
 String s;serializeJson(d,s);return s;
}
String command(JsonObjectConst v){
 String action=v["action"]|"";
 if(action=="ping")return !settings.ping&&!running()?"ERR wardrive is off":ping()?"OK ping queued":"ERR ping: one under way, too soon, no position or no free send slot";
 if(action=="fix"){
  if(!v["lat"].is<double>()||!v["lon"].is<double>())return "ERR fix: lat, lon (degrees), accuracy (m)";double lat=v["lat"],lon=v["lon"],acc=v["accuracy"]|1e9;
  if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>85||fabs(lon)>180||(lat==0&&lon==0))return "ERR fix coordinates";if(!(acc<=50))return "ERR fix: accuracy over 50 m";
  phoneLat=lround(lat*1e6);phoneLon=lround(lon*1e6);phoneAt=millis()?millis():1;return "OK position";}
 if(action=="clear"){flush();pointLog.clear();netLog.clear();ringCount=ringNext=0;tableCount=0;result={};noFix=pingsSent=pingsHeard=pingsFailed=0;memset(hops,0,sizeof hops);for(unsigned i=0;i<NetSeen;i++)macs[i]={};meshRadio.dirty=true;return "OK wardrive log cleared";}
 if(action=="stop"){Settings next=settings;next.passive=next.ping=next.nets=false;settings=next;save();flush();meshRadio.dirty=true;return "OK wardrive stopped";}
 if(action!="settings")return "ERR wardrive action: settings|ping|fix|clear|stop";
 Settings next=settings;
 for(JsonPairConst kv:v){String k=kv.key().c_str();JsonVariantConst x=kv.value();if(k=="action")continue;
  if(k=="passive"||k=="ping"||k=="coords"||k=="nets"){if(!x.is<bool>())return "ERR "+k+": boolean";bool b=x;if(k=="passive")next.passive=b;if(k=="ping")next.ping=b;if(k=="coords")next.coords=b;if(k=="nets")next.nets=b;}
  else if(k=="distance"){if(!x.is<int>()||x.as<int>()<50||x.as<int>()>5000)return "ERR distance 50..5000 m";next.distance=x.as<int>();}
  else if(k=="interval"){if(!x.is<int>()||x.as<int>()<15||x.as<int>()>3600)return "ERR interval 15..3600 s";next.interval=x.as<int>();}
  else if(k=="channel"){uint64_t id;if(!parseId(x.as<const char*>(),id)||!channels::isChannel(id)||id==meshmesh::Broadcast||!meshRadio.channel(id))return "ERR channel: a joined channel ID other than Public";next.channel=id;}
  else return "ERR unknown wardrive setting: "+k;}
#if defined(MM_NO_WIFI)
 if(next.nets)return "ERR board has no Wi-Fi";
#endif
 bool started=!settings.ping&&next.ping;settings=next;save();if(started){havePing=false;retryAt=0;}if(!running())flush();meshRadio.dirty=true;return "OK wardrive settings saved";
}
// "mesh FROM" and "nets FROM": the log from record FROM on, a part at a time ("next" until "total").
String log(const String& args){
 int space=args.indexOf(' ');String kind=space<0?args:args.substring(0,space);uint32_t from=space<0?0:strtoul(args.c_str()+space+1,nullptr,10);
 if(kind!="mesh"&&kind!="nets")return "ERR wardrive log mesh|nets FROM";bool mesh=kind=="mesh";
 DynamicJsonDocument d(mesh?12288:12288);d["kind"]=kind;d["total"]=mesh?pointLog.total():netLog.total();d["from"]=from;JsonArray a=d.createNestedArray("records");
 if(mesh){Point r[32];unsigned n=pointLog.read(from,r,32);for(unsigned i=0;i<n;i++){JsonArray o=a.createNestedArray();o.add(r[i].time);o.add(r[i].lat);o.add(r[i].lon);o.add(r[i].kind);o.add(r[i].type);o.add(r[i].hops);o.add(r[i].snr/4.0f);o.add(r[i].rssi);o.add(hex(r[i].hash,three(r[i].size)));o.add(r[i].count);o.add(r[i].flags);}d["next"]=from+n;}
 else{Net r[16];unsigned n=netLog.read(from,r,16);for(unsigned i=0;i<n;i++){JsonArray o=a.createNestedArray();o.add(r[i].time);o.add(r[i].lat);o.add(r[i].lon);o.add(r[i].kind);char mac[18];snprintf(mac,sizeof mac,"%02X:%02X:%02X:%02X:%02X:%02X",r[i].mac[0],r[i].mac[1],r[i].mac[2],r[i].mac[3],r[i].mac[4],r[i].mac[5]);o.add(mac);o.add(r[i].channel);o.add(r[i].rssi);o.add(r[i].auth);r[i].name[32]=0;o.add(meshmesh::validUtf8((const uint8_t*)r[i].name,strlen(r[i].name))?r[i].name:"");o.add(r[i].flags);}d["next"]=from+n;}
 String s;serializeJson(d,s);return s;
}
#else
// Left out of this image (Modules.h): the commands say so and the radio passes it nothing.
void begin(){}void tick(){}void flush(){}bool running(){return false;}bool holdsRadar(){return false;}bool pinging(){return false;}
bool position(int32_t&,int32_t&,bool&){return false;}bool ping(){return false;}
static Result none;const Result& last(){return none;}unsigned repeaters(const Repeater*& out){out=nullptr;return 0;}unsigned recent(const Recent*& out){out=nullptr;return 0;}
uint32_t points(){return 0;}uint32_t nets(){return 0;}
void heard(uint32_t,uint8_t,bool,uint8_t,const uint8_t*,const uint8_t*,size_t,float,float){}void echo(uint32_t,uint8_t,const uint8_t*,float){}
void probeSent(uint32_t,uint32_t){}void probeFailed(uint32_t){}void wifi(const uint8_t*,const char*,uint8_t,uint8_t,int){}void ble(uint64_t,const char*,int){}
String json(){return "{\"supported\":false}";}String command(JsonObjectConst){return "ERR wardrive is not in this build";}String log(const String&){return "ERR wardrive is not in this build";}
#endif
}
