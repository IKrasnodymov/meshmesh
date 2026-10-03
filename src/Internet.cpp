#include "Internet.h"
Internet internet;
#if !defined(MM_COMPACT)
#include "App.h"
#include "Hardware.h"
#include "Radar.h"
#include "Version.h"
#include "WifiDiagnostics.h"
#include "RootCerts.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
bool decodePngTile(const uint8_t* png,size_t size,uint16_t* out,String& error);
namespace {
const char* DefaultTiles="https://tile.openstreetmap.org/{z}/{x}/{y}.png";
// OSM tile policy: identify the application; tiles are cached on SD.
const char* Agent="MeshMesh/" MESHMM_VERSION " (ESP32-S3 ThinkNode M9 map viewer)";
const size_t BodyMax=768*1024;
// Worker hand-off: the loop fills a job and sets Busy; the worker sets Done;
// the loop reads the result and sets Idle. Buffers belong to whoever holds the phase.
enum Phase {Idle,Busy,Done};
enum Kind {JobTile,JobLocate};
struct Job {Kind kind;int z,x,y;bool ok;int code;double lat,lon;uint32_t bytes;char error[64];char url[192];};
volatile Phase phase=Idle;Job job;portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t worker=nullptr;uint16_t* pixels=nullptr;uint8_t* body=nullptr;
char tileTemplate[160]="";
Phase current(){taskENTER_CRITICAL(&guard);Phase p=phase;taskEXIT_CRITICAL(&guard);return p;}
void setPhase(Phase p){taskENTER_CRITICAL(&guard);phase=p;taskEXIT_CRITICAL(&guard);}
// Collects a response body in PSRAM; HTTPClient handles chunked encoding.
class Sink:public Stream{
 public:
  size_t size=0;bool overflow=false;
  size_t write(const uint8_t* bytes,size_t n) override{if(size+n>BodyMax){overflow=true;return 0;}memcpy(body+size,bytes,n);size+=n;return n;}
  size_t write(uint8_t c) override{return write(&c,1);}
  int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}void flush() override{}
};
WiFiClientSecure* tls=nullptr;WiFiClient* plain=nullptr;HTTPClient* http=nullptr;bool lastSecure=true;
bool get(const char* url,Sink& sink,int& code,char* error){
  bool secure=!strncmp(url,"https://",8);
  if(secure!=lastSecure){http->end();tls->stop();plain->stop();lastSecure=secure;}
  if(!(secure?http->begin(*tls,url):http->begin(*plain,url))){strlcpy(error,"Bad URL",64);code=0;return false;}
  http->setUserAgent(Agent);http->setConnectTimeout(10000);http->setTimeout(12000);
  code=http->GET();
  if(code!=200){
    if(code<0)snprintf(error,64,"%s",HTTPClient::errorToString(code).c_str());else snprintf(error,64,"HTTP %d",code);
    http->end();if(code<0){tls->stop();plain->stop();}return false;
  }
  int length=http->getSize();if(length>int(BodyMax)){http->end();strlcpy(error,"Response too large",64);return false;}
  int got=http->writeToStream(&sink);http->end();
  if(got<0||sink.overflow||(length>0&&got!=length)){snprintf(error,64,"Transfer error %d",got);tls->stop();plain->stop();return false;}
  return true;
}
double number(JsonVariantConst v){return v.is<const char*>()?atof(v.as<const char*>()):v.as<double>();}
bool locateBy(const char* url,Job& j){
  Sink sink;if(!get(url,sink,j.code,j.error))return false;
  StaticJsonDocument<96> filter;filter["latitude"]=true;filter["longitude"]=true;
  DynamicJsonDocument d(512);if(deserializeJson(d,(const char*)body,sink.size,DeserializationOption::Filter(filter))){strlcpy(j.error,"Location JSON",64);return false;}
  j.lat=number(d["latitude"]);j.lon=number(d["longitude"]);
  if(!isfinite(j.lat)||!isfinite(j.lon)||fabs(j.lat)>85||fabs(j.lon)>180||(j.lat==0&&j.lon==0)){strlcpy(j.error,"No location",64);return false;}
  return true;
}
void work(void*){
  tls=new WiFiClientSecure();plain=new WiFiClient();http=new HTTPClient();
  tls->setCACert(rootCertificates);tls->setHandshakeTimeout(15);http->setReuse(true);
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);if(current()!=Busy)continue;
    Job j=job;j.ok=false;j.code=0;j.bytes=0;j.error[0]=0;
    // Certificate validity needs the date; NTP normally sets it within seconds.
    if(time(nullptr)<1735689600)strlcpy(j.error,"Clock not set",64);
    else if(j.kind==JobTile){
      Sink sink;j.ok=get(j.url,sink,j.code,j.error);j.bytes=sink.size;
      if(j.ok){String reason;j.ok=decodePngTile(body,sink.size,pixels,reason);if(!j.ok)strlcpy(j.error,reason.c_str(),64);}
    }
    else j.ok=locateBy("https://get.geojs.io/v1/ip/geo.json",j)||locateBy("https://ipapi.co/json/",j);
    job=j;setPhase(Done);
  }
}
String urlFor(int z,int x,int y){String u=tileTemplate;u.replace("{z}",String(z));u.replace("{x}",String(x));u.replace("{y}",String(y));return u;}
esp_err_t scanStart(){WiFi.scanDelete();wifi_scan_config_t c={};c.show_hidden=false;c.scan_type=WIFI_SCAN_TYPE_ACTIVE;c.scan_time.active.min=100;c.scan_time.active.max=300;return esp_wifi_scan_start(&c,false);}
bool othersUseWifi(){return portalActive()||radar.active||wifiProbeActive();}
}
void Internet::begin(){
  loadNames();Preferences p;if(p.begin("mm-wifi",true)){enabled=p.getBool("on",false);preferred=p.getString("pref","");String t=p.getString("tiles","");strlcpy(tileTemplate,t.length()?t.c_str():DefaultTiles,sizeof tileTemplate);p.end();}
  else strlcpy(tileTemplate,DefaultTiles,sizeof tileTemplate);
  pixels=(uint16_t*)heap_caps_malloc(256*256*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);body=(uint8_t*)heap_caps_malloc(BodyMax,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  // Core 0 beside the Wi-Fi stack, below its priority; the loop (LoRa, UI, SD) keeps core 1.
  if(!pixels||!body||xTaskCreatePinnedToCore(work,"internet",12288,nullptr,1,&worker,0)!=pdPASS){free(pixels);free(body);pixels=nullptr;body=nullptr;worker=nullptr;error="Internet task RAM";}
}
void Internet::load(unsigned i,String& name,String& password){Preferences p;name="";password="";if(p.begin("mm-wifi",true)){name=p.getString(("s"+String(i)).c_str(),"");password=p.getString(("p"+String(i)).c_str(),"");p.end();}}
void Internet::loadNames(){Preferences p;bool open=p.begin("mm-wifi",true);for(unsigned i=0;i<MaxSaved;i++)names[i]=open?p.getString(("s"+String(i)).c_str(),""):String();if(open)p.end();}
unsigned Internet::savedCount(){unsigned n=0;for(unsigned i=0;i<MaxSaved;i++)if(names[i].length())n=i+1;return n;}
String Internet::savedName(unsigned i){return i<MaxSaved?names[i]:String();}
bool Internet::isSaved(const String& name){for(auto& n:names)if(name.length()&&n==name)return true;return false;}
// Saved networks stay in order of use; the sixth replaces the oldest.
bool Internet::store(String* list,String* words,unsigned n){
  Preferences p;if(!p.begin("mm-wifi",false)){error="NVS unavailable";return false;}
  for(unsigned i=0;i<MaxSaved;i++){String k=String(i);if(i<n){p.putString(("s"+k).c_str(),list[i]);p.putString(("p"+k).c_str(),words[i]);}else{p.remove(("s"+k).c_str());p.remove(("p"+k).c_str());}}
  if(preferred.length())p.putString("pref",preferred);else p.remove("pref");p.end();loadNames();return true;
}
bool Internet::save(const String& name,const String& password){
  if(!name.length()||name.length()>32||password.length()>63||(password.length()&&password.length()<8)){error="SSID 1-32 bytes, password empty or 8-63";return false;}
  String list[MaxSaved],words[MaxSaved];unsigned n=0;
  for(unsigned i=0;i<MaxSaved;i++){String a,b;load(i,a,b);if(a.length()&&a!=name){list[n]=a;words[n]=b;n++;}}
  if(n==MaxSaved){for(unsigned i=1;i<n;i++){list[i-1]=list[i];words[i-1]=words[i];}n--;}
  list[n]=name;words[n]=password;n++;preferred=name;
  if(!store(list,words,n))return false;
  for(unsigned i=0;i<seenCount;i++)if(name==seen[i].ssid)seen[i].saved=true;
  error="";dirty=true;return true;
}
bool Internet::forget(const String& name){
  String list[MaxSaved],words[MaxSaved];unsigned n=0;bool found=false;
  for(unsigned i=0;i<MaxSaved;i++){String a,b;load(i,a,b);if(!a.length())continue;if(a==name){found=true;continue;}list[n]=a;words[n]=b;n++;}
  if(!found)return false;if(preferred==name)preferred="";if(!store(list,words,n))return false;
  for(unsigned i=0;i<seenCount;i++)if(name==seen[i].ssid)seen[i].saved=false;
  if(owned&&ssid==name&&(state==Online||state==Connecting)){WiFi.disconnect();ssid="";state=Scanning;startScan();}
  dirty=true;return true;
}
void Internet::setEnabled(bool on){
  enabled=on;Preferences p;if(p.begin("mm-wifi",false)){p.putBool("on",on);p.end();}
  if(!on&&owned)stop(true);dirty=true;
}
void Internet::rescan(){retryAt=0;if(owned&&(state==Online||state==NoNetwork))startScan();}
void Internet::connectTo(const String& name){
  if(!enabled)setEnabled(true);preferred=name;Preferences p;if(p.begin("mm-wifi",false)){p.putString("pref",name);p.end();}
  if(!owned)return; // tick() scans and picks the preferred network first
  if(scanning){esp_wifi_scan_stop();WiFi.scanDelete();scanning=false;}
  WiFi.disconnect();candidateCount=1;strlcpy(candidates[0],name.c_str(),33);candidate=0;nextCandidate();
}
void Internet::startScan(){
  if(scanning)return;
  if(scanStart()!=ESP_OK){error="Wi-Fi scan failed";if(state!=Online){state=NoNetwork;retryAt=millis()+15000;}return;}
  scanning=true;stateAt=millis();if(state!=Online)state=Scanning;dirty=true;
}
void Internet::finishScan(){
  int n=WiFi.scanComplete();scanning=false;scannedAt=millis();seenCount=0;
  for(int i=0;i<n;i++){
    String name;uint8_t security;int32_t rssi,channel;uint8_t* mac;
    if(!WiFi.getNetworkInfo(i,name,security,rssi,mac,channel)||!name.length()||name.length()>32)continue;
    int at=-1;for(unsigned k=0;k<seenCount;k++)if(name==seen[k].ssid)at=k;
    if(at>=0){if(rssi>seen[at].rssi)seen[at].rssi=rssi;continue;}
    if(seenCount<MaxSeen){Seen& s=seen[seenCount++];strlcpy(s.ssid,name.c_str(),sizeof s.ssid);s.rssi=constrain(rssi,-127,0);s.open=security==WIFI_AUTH_OPEN;s.saved=isSaved(name);}
  }
  WiFi.scanDelete();
  for(unsigned i=1;i<seenCount;i++)for(unsigned j=i;j>0&&seen[j].rssi>seen[j-1].rssi;j--){Seen t=seen[j];seen[j]=seen[j-1];seen[j-1]=t;}
  dirty=true;
  if(state==Online)return;
  // Saved networks in range, strongest first, the last used one ahead of the rest.
  candidateCount=0;candidate=0;
  for(unsigned i=0;i<seenCount&&candidateCount<MaxSaved;i++)if(seen[i].saved&&preferred==seen[i].ssid)strlcpy(candidates[candidateCount++],seen[i].ssid,33);
  for(unsigned i=0;i<seenCount&&candidateCount<MaxSaved;i++)if(seen[i].saved&&preferred!=seen[i].ssid)strlcpy(candidates[candidateCount++],seen[i].ssid,33);
  if(candidateCount)nextCandidate();else{state=NoNetwork;retryAt=millis()+30000;if(savedCount())error="No saved network in range";}
}
void Internet::nextCandidate(){
  if(candidate>=candidateCount){state=NoNetwork;retryAt=millis()+30000;dirty=true;return;}
  String name,password;for(unsigned i=0;i<MaxSaved;i++){load(i,name,password);if(name==candidates[candidate])break;name="";}
  candidate++;if(!name.length()){nextCandidate();return;}
  ssid=name;WiFi.begin(name.c_str(),password.length()?password.c_str():nullptr);state=Connecting;stateAt=millis();dirty=true;
}
void Internet::stop(bool radioOff){
  if(scanning){esp_wifi_scan_stop();WiFi.scanDelete();scanning=false;}
  WiFi.disconnect();if(radioOff&&!othersUseWifi())WiFi.mode(WIFI_OFF);
  owned=false;ssid="";state=enabled?Paused:Off;dirty=true;
}
void Internet::yieldRadio(){if(owned){stop(false);state=Paused;}}
void Internet::tick(){
  uint32_t now=millis();
  if(!enabled){if(owned)stop(true);if(state!=Off){state=Off;dirty=true;}return;}
  if(othersUseWifi()){if(owned){owned=false;scanning=false;ssid="";}if(state!=Paused){state=Paused;dirty=true;}return;}
  if(!owned){
    if(!WiFi.mode(WIFI_STA)){error="Wi-Fi start failed";return;}
    WiFi.setSleep(true); // required with BLE on ESP32-S3; see AGENTS.md
    owned=true;state=Scanning;error="";startScan();return;
  }
  if(scanning){
    if(WiFiGenericClass::getStatusBits()&WIFI_SCAN_DONE_BIT)finishScan();
    else if(now-stateAt>8000){esp_wifi_scan_stop();WiFi.scanDelete();scanning=false;if(state!=Online){state=NoNetwork;retryAt=now+15000;error="Wi-Fi scan timed out";}}
    return;
  }
  wl_status_t link=WiFi.status();
  if(state==Scanning||(state==NoNetwork&&int32_t(now-retryAt)>=0))startScan();
  else if(state==Connecting){
    if(link==WL_CONNECTED){state=Online;lostAt=0;error="";timeSynced=false;dirty=true;
      configTime(0,0,"pool.ntp.org","time.google.com","time.cloudflare.com");}
    else if(link==WL_CONNECT_FAILED||now-stateAt>20000){error=link==WL_CONNECT_FAILED?"Wrong password or refused: "+ssid:"No answer from "+ssid;WiFi.disconnect();ssid="";nextCandidate();}
  }
  else if(state==Online){
    if(link==WL_CONNECTED)lostAt=0;
    else if(!lostAt)lostAt=now;
    else if(now-lostAt>15000){error="Connection lost";ssid="";state=Scanning;dirty=true;WiFi.disconnect();startScan();}
    if(!timeSynced&&sntp_get_sync_status()==SNTP_SYNC_STATUS_COMPLETED){timeSynced=true;hardware.setUtc(time(nullptr),"NTP",true);dirty=true;}
  }
}
bool Internet::idle(){return worker&&current()==Idle;}
bool Internet::backingOff(){return int32_t(millis()-backoffUntil)<0;}
bool Internet::fetchTile(int z,int x,int y){
  if(!online()||!idle()||backingOff())return false;
  job.kind=JobTile;job.z=z;job.x=x;job.y=y;strlcpy(job.url,urlFor(z,x,y).c_str(),sizeof job.url);setPhase(Busy);xTaskNotifyGive(worker);return true;
}
bool Internet::locate(){if(!online()||!idle())return false;job.kind=JobLocate;setPhase(Busy);xTaskNotifyGive(worker);return true;}
Internet::Result Internet::result(int& z,int& x,int& y,const uint16_t*& out,double& lat,double& lon){
  if(!worker||current()!=Done)return None;
  z=job.z;x=job.x;y=job.y;out=pixels;lat=job.lat;lon=job.lon;bytes+=job.bytes;
  if(!job.ok){error=job.error;if(job.code==403||job.code==418||job.code==429)backoffUntil=millis()+120000;}
  if(job.kind==JobLocate)return job.ok?Located:LocateFailed;
  if(job.ok)tiles++;else tileErrors++;return job.ok?TileOk:TileFailed;
}
void Internet::release(){if(current()==Done)setPhase(Idle);}
String Internet::tileUrl(){return tileTemplate;}
bool Internet::setTileUrl(const String& value){
  String v=value=="default"?String(DefaultTiles):value;
  if(v.length()>=sizeof tileTemplate||!(v.startsWith("https://")||v.startsWith("http://"))||v.indexOf("{z}")<0||v.indexOf("{x}")<0||v.indexOf("{y}")<0||!idle())return false;
  strlcpy(tileTemplate,v.c_str(),sizeof tileTemplate);Preferences p;if(p.begin("mm-wifi",false)){if(v==DefaultTiles)p.remove("tiles");else p.putString("tiles",v);p.end();}return true;
}
String Internet::address(){return online()?WiFi.localIP().toString():String();}
String Internet::stateText(){
  switch(state){
  case Off:return tr("Off","Выключен");
  case Paused:return portalActive()?tr("Paused: access point on","Пауза: включена точка доступа"):radar.active?tr("Paused: radar open","Пауза: открыт радар"):tr("Paused","Пауза");
  case Scanning:return tr("Scanning...","Поиск сетей...");
  case Connecting:return tr("Connecting: ","Подключение: ")+ssid;
  case Online:return ssid+" · "+WiFi.localIP().toString();
  default:return savedCount()?tr("No saved network in range","Сохранённых сетей рядом нет"):tr("Choose a network","Выберите сеть");
  }
}
// No passwords and no names of other people's networks: only the saved ones.
String Internet::info(){
  static const char* names[]={"off","no_network","scanning","connecting","online","paused"};
  DynamicJsonDocument d(1536);d["enabled"]=enabled;d["state"]=names[state];d["online"]=online();d["ssid"]=ssid;d["error"]=error;
  if(online()){d["ip"]=WiFi.localIP().toString();d["rssi"]=WiFi.RSSI();}
  d["time_synced"]=timeSynced;d["clock_source"]=hardware.clockSource;d["visible"]=seenCount;d["scan_age"]=scannedAt?int32_t((millis()-scannedAt)/1000):-1;
  JsonArray a=d.createNestedArray("saved");for(unsigned i=0;i<MaxSaved;i++){String n=savedName(i);if(!n.length())continue;JsonObject o=a.createNestedObject();o["ssid"]=n;bool inRange=false;for(unsigned k=0;k<seenCount;k++)if(n==seen[k].ssid)inRange=true;o["in_range"]=inRange;}
  d["preferred"]=preferred;d["tile_url"]=tileTemplate;d["tiles"]=tiles;d["tile_errors"]=tileErrors;d["bytes"]=bytes;d["worker"]=worker!=nullptr;d["busy"]=worker&&current()!=Idle;d["backoff"]=backingOff();
  d["heap"]=ESP.getFreeHeap();d["heap_min_block"]=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  String s;serializeJson(d,s);return s;
}
String Internet::command(const String& line){
  if(line=="internet"||line=="internet info")return info();
  if(line=="internet on"){setEnabled(true);return "OK internet on";}
  if(line=="internet off"){setEnabled(false);return "OK internet off";}
  if(line=="internet scan"){if(!enabled)return "ERR internet off";rescan();return "OK scan requested";}
  if(line.startsWith("internet add ")){StaticJsonDocument<256>d;if(deserializeJson(d,line.substring(13))||!d["ssid"].is<const char*>())return "ERR internet add JSON";
    String name=d["ssid"].as<String>(),password=d["password"]|"";if(!save(name,password))return "ERR "+error;connectTo(name);return "OK network saved";}
  if(line.startsWith("internet forget "))return forget(line.substring(16))?"OK network forgotten":"ERR network not saved";
  if(line.startsWith("internet connect ")){String name=line.substring(17);if(!isSaved(name))return "ERR network not saved";connectTo(name);return "OK connecting";}
  if(line.startsWith("internet tiles "))return setTileUrl(line.substring(15))?"OK tile server set":"ERR tile URL: http(s) with {z} {x} {y}, worker idle";
  return "ERR internet: info|on|off|scan|add {json}|forget SSID|connect SSID|tiles URL|default";
}
#endif
