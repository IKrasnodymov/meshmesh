// Radio actions added by the UI; kept here so the preview links.
bool MeshRadio::resetPath(uint64_t id){Peer* p=contact(id);if(!p)return false;p->pathLength=255;return true;}
bool MeshRadio::removeContact(uint64_t id){Peer* p=contact(id);if(!p)return false;unsigned i=p-peers;memmove(peers+i,peers+i+1,sizeof(Peer)*(peerCount-i-1));peerCount--;return true;}
void Hardware::ping(uint16_t,uint16_t){}
// Signal radar: synthetic sweep results; the table and trend logic are the real src/RadarModel.cpp.
#include "Radar.h"
Radar radar;
void Radar::open(){if(active)return;active=true;wifi=WifiReady;sweeps=3;count=0;
#if defined(MM_NO_WIFI)
 wifi=WifiOff;
#endif
 struct {uint64_t id;const char* name;uint8_t ch;bool open;int rssi;} aps[]={{0x5A1F0C9E1101ULL,"MM-9E77D2",1,false,-41},{0x34E8941A2B3CULL,"Keenetic-4471",6,false,-58},{0xA0F3C1223344ULL,"TP-Link_5GHz",11,false,-66},{0x10FEED000001ULL,"",3,false,-72},{0x28CDC1ABCDEFULL,"Кафе Бахетле",1,true,-79},{0xB827EB000102ULL,"DIRECT-HP-M28",6,false,-88}};
#if !defined(MM_NO_WIFI)
 for(auto& a:aps){RadarTarget* t=upsert(RadarTarget::Wifi,a.id);strcpy(t->name,a.name);t->channel=a.ch;t->open=a.open;t->rssi=a.rssi;t->seen=millis();}
#endif
 struct {uint64_t id;const char* name;RadarTarget::Device device;uint16_t vendor;int rssi;} bles[]={{0x6A11,"",RadarTarget::Personal,0x004c,-52},{0x6A12,"Galaxy Watch6",RadarTarget::Watch,0x0075,-61},{0x6A13,"",RadarTarget::Audio,0x004c,-70},{0x6A14,"MeshMesh 0E44",RadarTarget::Unknown,0xffff,-47},{0x6A15,"",RadarTarget::Unknown,0x0006,-83}};
 for(auto& b:bles){RadarTarget* t=upsert(RadarTarget::Ble,b.id);strcpy(t->name,b.name);t->device=b.device;t->vendor=b.vendor;t->rssi=b.rssi;t->seen=millis()-(b.id&3)*7000;}
 ble=BleReady;
 for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(!p.heard||p.hops)continue;RadarTarget* t=upsert(RadarTarget::Lora,p.id);strcpy(t->name,p.name);t->rssi=p.rssi;t->seen=p.seen;}
 sort();}
void Radar::close(){setCsi(CsiOff);untrack();active=false;count=0;wifi=WifiOff;ble=BleOff;}
void Radar::release(){}
void Radar::releaseBle(){}
void Radar::tick(){if(csi==CsiSensor)csiLast=millis();}
// CSI: a still room, then someone walking between the boards.
void Radar::setCsi(CsiRole role){if(role==csi)return;csi=role;csiRunning=role!=CsiOff;csiRate=role==CsiOff?0:role==CsiBeacon?50:48;motionSamples=0;moving=false;
 if(role==CsiSensor){csiRssi=-41;csiLast=millis();baseline=.004f;for(int i=0;i<100;i++){float v=i<70?.0035f+.0015f*((i*37)%7)/6:.018f+.03f*((i*53)%9)/8;addActivity(v,millis()-(100-i)*500);}}}
bool Radar::track(unsigned index){if(index>=count)return false;focus=targets[index];reset();tracking=true;
 // A walk towards the access point: noisy approach, then a short step back.
 for(int i=0;i<90;i++){int r=-78+i/3+int((i*37)%7)-3-(i>80?(i-80)*2:0);addSample(r,millis()-(90-i)*100);}
 lastSample=millis()-300;rate=21;return true;}
void Radar::untrack(){tracking=false;reset();}
// Chess: the real ChessNet with its saved games kept in memory; sent commands only get an ID.
#include "ChessNet.h"
#include <vector>
static std::vector<uint8_t> chessSaved;
bool chessStoreWrite(const uint8_t* d,size_t n){chessSaved.assign(d,d+n);return true;}
size_t chessStoreRead(uint8_t* d,size_t cap){if(chessSaved.size()>cap)return 0;memcpy(d,chessSaved.data(),chessSaved.size());return chessSaved.size();}
struct PreviewSent{uint32_t id;uint64_t peer;std::string text;};static std::vector<PreviewSent> previewSent; // game and tournament commands sent
uint32_t MeshRadio::sendGame(const String& text,uint64_t peer){previewSent.push_back({++nextId,peer,text.c_str()});return nextId;}
// Rated results: the real src/ChessRating.cpp with its file in memory; the node "signs" with a fixed pattern.
#include "ChessRating.h"
static std::vector<uint8_t> ratingSaved;
bool rating::storeWrite(const uint8_t* d,size_t n){ratingSaved.assign(d,d+n);return true;}
size_t rating::storeRead(uint8_t* d,size_t cap){if(ratingSaved.size()>cap)return 0;memcpy(d,ratingSaved.data(),ratingSaved.size());return ratingSaved.size();}
const uint8_t* MeshRadio::nodeKey() const{static const uint8_t key[32]={0x5A,0x1F,0x0C,0x9E,0x77,0xD2};return key;}
bool MeshRadio::nodeSign(uint8_t sig[64],const uint8_t*,size_t) const{memset(sig,0x5A,64);return true;}
bool MeshRadio::nodeVerify(const uint8_t*,const uint8_t*,const uint8_t*,size_t){return true;}
bool MeshRadio::learnContact(const uint8_t*,const char*){return true;}
// The SD archive of finished games: names in memory.
static std::vector<std::pair<std::string,std::string>> previewArchive;
bool chessArchiveWrite(const String& name,const String& text){for(auto& a:previewArchive)if(a.first==name.c_str()){a.second=text.c_str();return true;}previewArchive.push_back({name.c_str(),text.c_str()});return true;}
String chessArchiveList(){String s="{\"games\":[";for(size_t i=0;i<previewArchive.size();i++)s+=String(i?",":"")+"\""+previewArchive[i].first.c_str()+"\"";return s+"],\"sd\":true}";}
String chessArchiveRead(const String& name){for(auto& a:previewArchive)if(a.first==name.c_str())return a.second.c_str();return "";}
// Tournaments: the real src/ChessTour.cpp with its file in memory.
#include "ChessTour.h"
static std::vector<uint8_t> tourSaved;
bool tour::storeWrite(const uint8_t* d,size_t n){tourSaved.assign(d,d+n);return true;}
size_t tour::storeRead(uint8_t* d,size_t cap){if(tourSaved.size()!=cap)return 0;memcpy(d,tourSaved.data(),cap);return cap;}
// Internet over Wi-Fi and the web radar holder: switched off in the preview.
#include "Internet.h"
Internet internet;
#if !defined(MM_HELTEC_V4)
static int previewRescans=0; // the T-Deck touch check: DEL in the network list rescans
void Internet::setEnabled(bool on){enabled=on;}void Internet::rescan(){previewRescans++;}void Internet::connectTo(const String&){}
bool Internet::save(const String&,const String&){return true;}bool Internet::forget(const String&){return true;}
String Internet::savedName(unsigned){return "";}String Internet::stateText(){return online()?String("Home-WiFi · 192.168.1.101"):tr("Off","Выключен");}
String Internet::address(){return "192.168.1.101";}
#endif
bool Maps::inArea() const{return true;}
bool webRadarActive(){return false;}
static String previewCommands; // commands the screens ran, for the checks
String executeCommand(const String& line){previewCommands+=line+";";return "OK";} // UiHeltec.cpp: the GAT562 GPS menu
// Same as src/MeshRadio.cpp (the preview does not link the radio).
String MeshRadio::routeText(const ChatMessage& m,bool brief) const{
 if(m.protocol!=2||m.route==ChatMessage::RouteNone||(m.outgoing&&(m.status==ChatMessage::Failed||channels::isChannel(m.destination))))return "";
 String hops=m.hops==255?String():!m.hops?String(brief?tr("direct","напр."):tr("direct","напрямую")):(brief?String(m.hops)+tr(" rpt"," ретр."):String(tr("via ","через "))+m.hops+tr(" rpt"," ретр."));
 String s=m.route==ChatMessage::RouteDirect?(hops.length()?hops:String(tr("routed","по маршруту")))
  :!m.outgoing?(hops.length()?hops:String("flood")) // incoming: how far it came
  :m.status==ChatMessage::Delivered&&hops.length()&&!brief?"flood, "+hops:String("flood");
 if(m.outgoing&&m.tries>1)s+=m.status==ChatMessage::Delivered?(brief?" #"+String(m.tries):String(tr(" · try "," · попытка "))+m.tries):" "+String(m.tries)+"/3";
 return s;
}
// Device modes: a repeater or room server with made-up passwords and counts.
#include "MeshServer.h"
MeshServer meshServer;
bool MeshServer::room() const{return config.role==RoleRoom;}
ServerView MeshServer::view(){ServerView v;v.running=config.role!=RoleNormal;v.room=room();v.forwarding=!v.room;v.password="48213906";v.guest=v.room?"garden":"";
 v.advertMinutes=120;v.floodAdvertHours=47;v.floodMax=64;v.neighbours=v.room?0:3;v.posts=v.room?12:0;v.clients=v.room?4:1;v.admins=1;v.uptime=3*3600+1240;return v;}
String MeshServer::command(const String&){return "OK";}
bool MeshServer::post(const String&){return true;}
bool MeshServer::advertise(bool){return true;}
const char* roleName(uint8_t role){return role==RoleRepeater?"repeater":role==RoleRoom?"room":"normal";}
String setRole(uint8_t role){return String("OK role ")+roleName(role)+"; restarting";}
// Channels: the table in memory (src/MeshRadio.cpp keeps it in NVS and gives it to MeshCore); the names,
// keys, links and QR codes are the real src/Channels.cpp.
const channels::Channel* MeshRadio::channel(uint64_t id) const{int i=channelIndex(id);return i<0?nullptr:&channelList[i];}
int MeshRadio::channelIndex(uint64_t id) const{for(unsigned i=0;i<channelCount;i++)if(channelList[i].id==id)return i;return -1;}
MeshRadio::ChannelResult MeshRadio::addChannel(const String& name,const uint8_t secret[16],uint64_t* id){
 uint64_t key=channels::idOf(secret);if(id)*id=key;if(channel(key))return ChannelExists;if(!channels::validName(name))return ChannelBadName;if(channelCount>=channels::Max)return ChannelFull;
 auto& c=channelList[channelCount++];c={};strncpy(c.name,name.c_str(),channels::NameBytes);memcpy(c.secret,secret,16);c.id=key;
 uint8_t hash=channels::hashOf(secret);for(unsigned i=0;i<heardCount;)if(heard[i].hash==hash){memmove(heard+i,heard+i+1,sizeof(HeardChannel)*(heardCount-i-1));heardCount--;}else i++;return ChannelAdded;
}
MeshRadio::ChannelResult MeshRadio::joinHashtag(const String& raw,uint64_t* id){String tag=channels::hashtag(raw);if(!tag.length())return ChannelBadName;uint8_t k[16];channels::hashtagSecret(tag,k);return addChannel(tag,k,id);}
MeshRadio::ChannelResult MeshRadio::joinLink(const String& text,uint64_t* id){String name;uint8_t k[16];if(!channels::parseLink(text,name,k))return ChannelBadLink;return addChannel(name,k,id);}
MeshRadio::ChannelResult MeshRadio::createChannel(const String& name,uint64_t* id){if(!channels::validName(name))return ChannelBadName;uint8_t k[16];for(int i=0;i<16;i++)k[i]=uint8_t(i*37+11);return addChannel(name,k,id);}
bool MeshRadio::removeChannel(uint64_t id){int i=channelIndex(id);if(i<=0)return false;memmove(channelList+i,channelList+i+1,sizeof(channels::Channel)*(channelCount-i-1));channelCount--;return true;}
bool MeshRadio::sendInvite(uint64_t contact,uint64_t id){const channels::Channel* c=channel(id);if(!c)return false;String l=channels::link(*c);if(l.length()>151)l=channels::link(*c,true);return sendMessage(l,contact);}
int MeshRadio::probeHashtag(const String& raw){String tag=channels::hashtag(raw);if(!tag.length())return -1;for(unsigned i=0;i<heardCount;i++)if(tag==heard[i].name)return heard[i].packets;return tag=="#test"?2:0;}
// MeshMesh storage files: none in the preview (read marks fall back to the Preferences shim).
bool readStored(const char*,const char*,void*,size_t){return false;}
bool writeStored(const char*,const char*,const void*,size_t){return false;}
bool MeshRadio::clockSet(){return time(nullptr)>=1735689600;}
