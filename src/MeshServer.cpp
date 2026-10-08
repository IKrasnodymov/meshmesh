#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: the repeater and room server logic are not speed-critical (the rest of the nRF52 image is -O2)
#endif
// Server roles: the MeshCore repeater and room server. Ported from MeshCore examples/simple_repeater
// and examples/simple_room_server (MyMesh.cpp, RateLimiter.h; MIT, https://github.com/meshcore-dev/MeshCore,
// companion-v1.17.1) onto the vendored core-v1.17.4 helpers. What both share is in ServerMesh.
// MeshMesh replaces the board, radio driver, file system and identity storage; packet handling, replies,
// CLI, forwarding rules and post sync keep the stock behaviour. Room posts are also kept on LittleFS.
// Bridges, OTA and power saving are absent.
#include "MeshServer.h"
#include "Config.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "Board.h"
#include <Mesh.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/ClientACL.h>
#include <helpers/CommonCLI.h>
#include <helpers/RegionMap.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/TxtDataHelpers.h>
#include <Mm1Packet.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <RTClib.h>
#include <SHA256.h>
#include <algorithm>
#include <math.h>
#include <time.h>
#include <esp_system.h>
MeshServer meshServer;
namespace {
constexpr unsigned maxNeighbours=32,maxPosts=32,maxPostText=160-9;
constexpr uint32_t serverResponseDelay=300,txtAckDelay=200,cliReplyDelay=600,lazyContactsWrite=5000;
constexpr uint32_t replyDelay=1500,pushNotifyDelay=2000,syncPushInterval=1200,pushAckTimeoutFlood=12000,pushTimeoutBase=4000,pushAckTimeoutFactor=2000,postSyncDelaySecs=6;
enum:uint8_t {ReqGetStatus=0x01,ReqKeepAlive=0x02,ReqGetTelemetry=0x03,ReqGetAccessList=0x05,ReqGetNeighbours=0x06,ReqGetOwnerInfo=0x07};
enum:uint8_t {RespServerLoginOk=0};
enum:uint8_t {AnonReqRegions=0x01,AnonReqOwner=0x02,AnonReqBasic=0x03};
enum:uint8_t {CtlNodeDiscoverReq=0x80,CtlNodeDiscoverResp=0x90};
const char* const packetLog="/packet_log";
const char* const postsFile="/room_posts";
const char* const serverFiles[]={"/prefs.json","/com_prefs","/s_contacts","/regions2","/packet_log","/room_posts"};

struct RepeaterStats {
  uint16_t batt_milli_volts,curr_tx_queue_len;
  int16_t noise_floor,last_rssi;
  uint32_t n_packets_recv,n_packets_sent,total_air_time_secs,total_up_time_secs;
  uint32_t n_sent_flood,n_sent_direct,n_recv_flood,n_recv_direct;
  uint16_t err_events;
  int16_t last_snr; // x 4
  uint16_t n_direct_dups,n_flood_dups;
  uint32_t total_rx_air_time_secs,n_recv_errors;
};
struct RoomStats {
  uint16_t batt_milli_volts,curr_tx_queue_len;
  int16_t noise_floor,last_rssi;
  uint32_t n_packets_recv,n_packets_sent,total_air_time_secs,total_up_time_secs;
  uint32_t n_sent_flood,n_sent_direct,n_recv_flood,n_recv_direct;
  uint16_t err_events;
  int16_t last_snr; // x 4
  uint16_t n_direct_dups,n_flood_dups,n_posted,n_post_push;
};
struct NeighbourInfo {mesh::Identity id;uint32_t advert_timestamp=0,heard_timestamp=0;int8_t snr=0;}; // snr x 4
struct PostInfo {mesh::Identity author;uint32_t post_timestamp=0;char text[maxPostText+1]={};}; // timestamp by our clock

class RateLimiter {
  uint32_t start=0,secs;uint16_t maximum,count=0;
 public:
  RateLimiter(uint16_t maximum,uint32_t secs):secs(secs),maximum(maximum){}
  bool allow(uint32_t now){if(now<start+secs){count++;return count<=maximum;}start=now;count=1;return true;}
};

class DeviceBoard:public mesh::MainBoard {
 public:
  uint16_t getBattMilliVolts() override{return hardware.batteryMv;}
  float getMCUTemperature() override{return temperatureRead();}
  const char* getManufacturerName() const override{return MM_BOARD_NAME;}
  void reboot() override{ESP.restart();}
  uint8_t getStartupReason() const override{return 0;}
};

// GPS for the CLI ("gps", "gps setloc", "gps advert share") and for telemetry with location permission.
class GpsLocation:public LocationProvider {
 public:
  long getLatitude() override{return lround(hardware.gps.location.lat()*1e6);}
  long getLongitude() override{return lround(hardware.gps.location.lng()*1e6);}
  long getAltitude() override{return lround(hardware.gps.altitude.meters()*1000);}
  long satellitesCount() override{return hardware.gps.satellites.value();}
  bool isValid() override{return hardware.gpsFix();}
  long getTimestamp() override{return time(nullptr);}
  void sendSentence(const char*) override{}
  void reset() override{}
  void begin() override{}
  void stop() override{}
  void loop() override{}
  bool isEnabled() override{return config.gps;}
};
class Sensors:public SensorManager {
  GpsLocation gps;
 public:
  bool querySensors(uint8_t permissions,CayenneLPP& telemetry) override{
    if((permissions&TELEM_PERM_LOCATION)&&hardware.gpsFix())telemetry.addGPS(TELEM_CHANNEL_SELF,hardware.gps.location.lat(),hardware.gps.location.lng(),hardware.gps.altitude.meters());
    return true;
  }
  void loop() override{if(hardware.gpsFix()){node_lat=hardware.gps.location.lat();node_lon=hardware.gps.location.lng();node_altitude=hardware.gps.altitude.meters();}}
  int getNumSettings() const override{return 1;}
  const char* getSettingName(int i) const override{return i==0?"gps":nullptr;}
  const char* getSettingValue(int i) const override{return i==0?(config.gps?"1":"0"):nullptr;}
  bool setSettingValue(const char* name,const char* value) override{
    if(strcmp(name,"gps"))return false;bool on=!strcmp(value,"1");
    if(on!=config.gps){config.gps=on;config.save();hardware.setGps(on);}return true;
  }
  LocationProvider* getLocationProvider() override{return &gps;}
};
DeviceBoard board;Sensors sensors;
uint8_t maxLoopMinimal[]={0,4,2,1},maxLoopModerate[]={0,2,1,1},maxLoopStrict[]={0,1,1,1}; // by path hash size 1..3
bool isShare(const mesh::Packet* p){return p->hasTransportCodes()&&p->transport_codes[0]==0&&p->transport_codes[1]==0;} // codes {0,0}: "send to nowhere"
bool validName(const char* name){size_t n=strnlen(name,sizeof(Config::name));return n&&n<sizeof(Config::name)&&meshmesh::validUtf8((const uint8_t*)name,n);}
// Copies at most cap-1 bytes without splitting a UTF-8 character.
void copyText(char* dest,const char* src,size_t cap){size_t n=strnlen(src,cap);if(n>=cap){n=cap-1;while(n&&(uint8_t(src[n])&0xc0)==0x80)n--;}memcpy(dest,src,n);dest[n]=0;}
}

// What the repeater and the room server share: prefs and CLI, ACL, regions, adverts, logins' replies,
// requests' routing, settings shared with MeshMesh.
class ServerMesh:public mesh::Mesh,public CommonCLICallbacks {
 public:
  NodePrefs prefs;
  ClientACL acl;
  uint64_t uptimeMillis=0;
  const uint8_t type;
 protected:
  decltype(&LittleFS) fs=&LittleFS; // ESP32 LittleFS or the nRF52 MeshFS, both with open(name,mode,create)
  uint32_t lastMillis=0;
  unsigned long nextLocalAdvert=0,nextFloodAdvert=0,setRadioAt=0,revertRadioAt=0,dirtyContactsExpiry=0;
  bool logging=false,regionLoadActive=false;String rejected; // why the last CLI change was refused
  CommonCLI cliHandler;
  uint8_t replyData[MAX_PACKET_PAYLOAD];
  TransportKeyStore keyStore;
  RegionMap regionMap,tempMap;
  RegionEntry* loadStack[8]={};
  RegionEntry* recvPktRegion=nullptr;
  TransportKey defaultScope;
  CayenneLPP telemetry;
  float pendingFreq=0,pendingBw=0;uint8_t pendingSf=0,pendingCr=0;
  int matchingPeerIndexes[MAX_CLIENTS];
  struct Forwarded {uint8_t hash[8]={};uint32_t at=0;} forwarded[32];unsigned nextForwarded=0;

  uint32_t now(){return getRTCClock()->getCurrentTime();}
  ClientInfo* peer(int senderIdx){int i=matchingPeerIndexes[senderIdx];return i>=0&&i<acl.getNumClients()?acl.getClientByIdx(i):nullptr;}
  // Status replies share their first fields; the rest is the role's.
  template<typename T>void fillStats(T& s){
    s.batt_milli_volts=board.getBattMilliVolts();s.curr_tx_queue_len=_mgr->getOutboundTotal();s.noise_floor=_radio->getNoiseFloor();s.last_rssi=_radio->getLastRSSI();
    s.n_packets_recv=meshRadio.rxCount;s.n_packets_sent=meshRadio.txCount;s.total_air_time_secs=getTotalAirTime()/1000;s.total_up_time_secs=uptimeMillis/1000;
    s.n_sent_flood=getNumSentFlood();s.n_sent_direct=getNumSentDirect();s.n_recv_flood=getNumRecvFlood();s.n_recv_direct=getNumRecvDirect();s.err_events=_err_flags;s.last_snr=int16_t(_radio->getLastSNR()*4);
    auto* t=(SimpleMeshTables*)getTables();s.n_direct_dups=t->getNumDirectDups();s.n_flood_dups=t->getNumFloodDups();
  }
  int telemetryReply(ClientInfo* sender,const uint8_t* payload){
    uint8_t mask=~payload[1]; // first reserved byte: inverse permission mask
    telemetry.reset();telemetry.addVoltage(TELEM_CHANNEL_SELF,board.getBattMilliVolts()/1000.0f);
    if((sender->permissions&PERM_ACL_ROLE_MASK)==PERM_ACL_GUEST)mask=0; // base telemetry only
    sensors.querySensors(mask,telemetry);
    float t=board.getMCUTemperature();if(!isnan(t))telemetry.addTemperature(TELEM_CHANNEL_SELF,t);
    uint8_t n=telemetry.getSize();memcpy(&replyData[4],telemetry.getBuffer(),n);return 4+n;
  }
  int aclReply(bool adminsOnly){
    uint8_t ofs=4;
    for(int i=0;i<acl.getNumClients()&&ofs+7<=sizeof(replyData)-4;i++){auto c=acl.getClientByIdx(i);if(adminsOnly?!c->isAdmin():!c->permissions)continue;memcpy(&replyData[ofs],c->id.pub_key,6);ofs+=6;replyData[ofs++]=c->permissions;}
    return ofs;
  }
  // A request by flood gets the path back with the reply inside; otherwise the reply goes the known way.
  void reply(ClientInfo* client,const uint8_t* secret,mesh::Packet* request,uint8_t type,const uint8_t* data,size_t n,uint32_t delay,bool pathReturn){
    if(pathReturn&&request->isRouteFlood()){mesh::Packet* path=createPathReturn(client->id,secret,request->path,request->path_len,PAYLOAD_TYPE_RESPONSE,data,n);if(path)sendFloodReply(path,delay,request->getPathHashSize());return;}
    mesh::Packet* p=createDatagram(type,client->id,secret,data,n);if(!p)return;
    if(client->out_path_len==OUT_PATH_UNKNOWN)sendFloodReply(p,delay,request->getPathHashSize());else sendDirect(p,client->out_path,client->out_path_len,delay);
  }
  void ackTo(ClientInfo* client,uint32_t ack,mesh::Packet* request,uint32_t delay){
    if(mesh::Packet* a=createAck(ack)){if(client->out_path_len==OUT_PATH_UNKNOWN)sendFloodReply(a,delay,request->getPathHashSize());else sendDirect(a,client->out_path,client->out_path_len,delay);}
  }
  // A location of 0,0 means "not set": MeshMesh leaves it out of the advert instead of announcing it.
  mesh::Packet* createSelfAdvert(){
    uint8_t data[MAX_ADVERT_DATA_SIZE],policy=prefs.advert_loc_policy;
    bool share=policy==ADVERT_LOC_SHARE,unset=share?(sensors.node_lat==0&&sensors.node_lon==0)||!hardware.gpsFix():prefs.node_lat==0&&prefs.node_lon==0;
    if(policy!=ADVERT_LOC_NONE&&unset)prefs.advert_loc_policy=ADVERT_LOC_NONE;
    uint8_t n=cliHandler.buildAdvertData(type,data);prefs.advert_loc_policy=policy;
    // Adverts carry a timestamp that must keep growing across restarts; the last one is kept with the identity.
    getRTCClock()->setCurrentTime(getRTCClock()->getCurrentTimeUnique());
    mesh::Packet* p=createAdvert(self_id,data,n);if(!p)return nullptr;
    uint32_t stamp=meshmesh::get32(p->payload+PUB_KEY_SIZE);Preferences nvs;
    if(!nvs.begin("meshmesh-mc",false)){releasePacket(p);return nullptr;}
    bool saved=nvs.putUInt("last_advert",stamp)==4;if(saved){nvs.putString("adv_name",prefs.node_name);nvs.putUChar("adv_type",type);}nvs.end();
    if(!saved){releasePacket(p);return nullptr;}return p;
  }
  File openAppend(const char* name){return fs->open(name,"a",true);}
  // How a flooded reply is scoped: as the request, the default region when the request's is unknown, or unscoped.
  void sendFloodReply(mesh::Packet* p,unsigned long delay,uint8_t pathHashSize){
    bool wildcard=recvPktRegion&&recvPktRegion->isWildcard();TransportKey scope;
    if(recvPktRegion&&!wildcard&&regionMap.getTransportKeysFor(*recvPktRegion,&scope,1)>0)sendFloodScoped(scope,p,delay,pathHashSize);
    else if(!wildcard&&!defaultScope.isNull())sendFloodScoped(defaultScope,p,delay,pathHashSize);
    else sendFlood(p,delay,pathHashSize);
  }
  bool floodLimited(const mesh::Packet* p){
    uint8_t hops=p->getPathHashCount();
    return hops>=prefs.flood_max||(p->getRouteType()==ROUTE_TYPE_FLOOD&&hops>=prefs.flood_max_unscoped)||(p->getPayloadType()==PAYLOAD_TYPE_ADVERT&&hops>=prefs.flood_max_advert);
  }
  bool forwardAllowed(const mesh::Packet* p){auto& f=forwarded[nextForwarded];p->calculatePacketHash(f.hash);f.at=millis();nextForwarded=(nextForwarded+1)%32;return true;} // counted when sent
  void logPacket(const char* what,mesh::Packet* p,int len,const char* extra=""){
    if(!logging)return;File f=openAppend(packetLog);if(!f)return;
    f.print(getLogDateTime());f.printf(": %s, len=%d (type=%d, route=%s, payload_len=%d)%s",what,len,p->getPayloadType(),p->isRouteDirect()?"D":"F",p->payload_len,extra);
    uint8_t t=p->getPayloadType();
    if(t==PAYLOAD_TYPE_PATH||t==PAYLOAD_TYPE_REQ||t==PAYLOAD_TYPE_RESPONSE||t==PAYLOAD_TYPE_TXT_MSG)f.printf(" [%02X -> %02X]\n",(uint32_t)p->payload[1],(uint32_t)p->payload[0]);else f.printf("\n");
    f.close();
  }
  // Settings shared with MeshMesh: name, frequency, bandwidth, SF, CR and power live in Config.
  void syncToConfig(){
    Config next=config;memcpy(next.name,prefs.node_name,sizeof(next.name));next.name[sizeof(next.name)-1]=0;
    next.frequency=prefs.freq;next.bandwidth=prefs.bw;next.sf=prefs.sf;next.cr=prefs.cr;next.power=prefs.tx_power_dbm;
    if(!validName(prefs.node_name))rejected="Error: name must be 1-24 bytes of UTF-8";
    else if(!next.valid())rejected="Error: " MM_BOARD_NAME " takes 863-870 MHz, BW 62.5/125/250/500, SF7-12, CR5-8, 0-"+String(MM_MAX_POWER)+" dBm";
    if(rejected.length()){loadFromConfig();return;}
    if(strcmp(next.name,config.name)||next.frequency!=config.frequency||next.bandwidth!=config.bandwidth||next.sf!=config.sf||next.cr!=config.cr||next.power!=config.power){config=next;config.save();meshRadio.dirty=true;}
  }
  virtual void saveAcl(){acl.save(fs);}
  virtual bool roleCommand(char*,char*){return false;} // "discover.neighbors", "room.post"
  virtual void roleLoop(){}
 protected:
  float getAirtimeBudgetFactor() const override{return prefs.airtime_factor;}
  int getInterferenceThreshold() const override{return prefs.interference_threshold;}
  bool getCADEnabled() const override{return prefs.cad_enabled;}
  int getAGCResetInterval() const override{return int(prefs.agc_reset_interval)*4000;}
  uint8_t getExtraAckTransmitCount() const override{return prefs.multi_acks;}
  const char* getLogDateTime() override{static char tmp[32];DateTime dt(now());snprintf(tmp,sizeof(tmp),"%02d:%02d:%02d - %d/%d/%d U",dt.hour(),dt.minute(),dt.second(),dt.day(),dt.month(),dt.year());return tmp;}
  void logRx(mesh::Packet* p,int len,float score) override{
    if(!logging)return;char extra[64];snprintf(extra,sizeof(extra)," SNR=%d RSSI=%d score=%d",int(_radio->getLastSNR()),int(_radio->getLastRSSI()),int(score*1000));logPacket("RX",p,len,extra);
  }
  void logTx(mesh::Packet* p,int len) override{
    meshRadio.txCount++;uint8_t hash[8];p->calculatePacketHash(hash);
    for(auto& f:forwarded)if(f.at&&millis()-f.at<60000&&!memcmp(f.hash,hash,8)){meshRadio.relayed++;f.at=0;break;}
    meshRadio.dirty=true;logPacket("TX",p,len);
  }
  void logTxFail(mesh::Packet* p,int len) override{meshRadio.event="Radio TX error "+String(meshRadio.radioError);meshRadio.dirty=true;logPacket("TX FAIL!",p,len);}
  int calcRxDelay(float score,uint32_t airtime) const override{return prefs.rx_delay_base<=0?0:int((pow(prefs.rx_delay_base,0.85f-score)-1.0)*airtime);}
  uint32_t getRetransmitDelay(const mesh::Packet* p) override{uint32_t t=_radio->getEstAirtimeFor(p->getPathByteLen()+p->payload_len+2)*prefs.tx_delay_factor;return getRNG()->nextInt(0,5*t+1);}
  uint32_t getDirectRetransmitDelay(const mesh::Packet* p) override{uint32_t t=_radio->getEstAirtimeFor(p->getPathByteLen()+p->payload_len+2)*prefs.direct_tx_delay_factor;return getRNG()->nextInt(0,5*t+1);}
  mesh::DispatcherAction onRecvPacket(mesh::Packet* p) override{
    if(p->getRouteType()==ROUTE_TYPE_TRANSPORT_FLOOD)recvPktRegion=regionMap.findMatch(p,REGION_DENY_FLOOD);
    else if(p->getRouteType()==ROUTE_TYPE_FLOOD)recvPktRegion=regionMap.getWildcard().flags&REGION_DENY_FLOOD?nullptr:&regionMap.getWildcard();
    else recvPktRegion=nullptr;
    return Mesh::onRecvPacket(p);
  }
  int searchPeersByHash(const uint8_t* hash) override{int n=0;for(int i=0;i<acl.getNumClients()&&n<MAX_CLIENTS;i++)if(acl.getClientByIdx(i)->id.isHashMatch(hash))matchingPeerIndexes[n++]=i;return n;}
  void getPeerSharedSecret(uint8_t* secret,int peerIdx) override{if(auto* c=peer(peerIdx))memcpy(secret,c->shared_secret,PUB_KEY_SIZE);}
 public:
  ServerMesh(uint8_t advertType,mesh::Radio& radio,mesh::MillisecondClock& ms,mesh::RNG& rng,mesh::RTCClock& rtc,mesh::PacketManager& pool,mesh::MeshTables& tables)
   :mesh::Mesh(radio,ms,rng,rtc,pool,tables),type(advertType),cliHandler(board,rtc,sensors,regionMap,acl,&prefs,this),regionMap(keyStore),tempMap(keyStore),telemetry(MAX_PACKET_PAYLOAD-4){
    // Stock defaults, except: the name and radio come from MeshMesh, a random admin password replaces
    // "password", and the zero-hop advert repeats every 2 hours rather than every 2 minutes.
    prefs.airtime_factor=1.0;prefs.rx_delay_base=0;prefs.tx_delay_factor=0.5f;
    snprintf(prefs.password,sizeof(prefs.password),"%08lu",(unsigned long)(esp_random()%100000000UL));
    prefs.advert_interval=60;prefs.flood_advert_interval=47;prefs.flood_max=64;prefs.flood_max_unscoped=64;prefs.flood_max_advert=8;
    prefs.bridge_enabled=0;prefs.bridge_delay=500;prefs.bridge_baud=115200;prefs.bridge_channel=1;
    prefs.advert_loc_policy=ADVERT_LOC_PREFS;prefs.radio_fem_rxgain=1;
    memset(defaultScope.key,0,sizeof(defaultScope.key));loadFromConfig();
  }
  virtual ~ServerMesh(){}
  virtual void flush(){if(dirtyContactsExpiry){saveAcl();dirtyContactsExpiry=0;}}
  void loadFromConfig(){strlcpy(prefs.node_name,config.name,sizeof(prefs.node_name));prefs.freq=config.frequency;prefs.bw=config.bandwidth;prefs.sf=config.sf;prefs.cr=config.cr;prefs.tx_power_dbm=config.power;prefs.gps_enabled=config.gps;}
  virtual void begin(){
    mesh::Mesh::begin();
    bool fresh=!fs->exists("/prefs.json")&&!fs->exists("/com_prefs");
    cliHandler.loadPrefs(fs);loadFromConfig();if(fresh)cliHandler.savePrefs(fs); // keeps the generated password
    acl.load(fs,self_id);regionMap.load(fs);
    if(RegionEntry* r=regionMap.getDefaultRegion())regionMap.getTransportKeysFor(*r,&defaultScope,1);
    updateAdvertTimer();updateFloodAdvertTimer();
  }
  void sendFloodScoped(const TransportKey& scope,mesh::Packet* p,uint32_t delay,uint8_t pathHashSize){
    if(scope.isNull()){sendFlood(p,delay,pathHashSize);return;}
    uint16_t codes[2]={scope.calcTransportCode(p),0};sendFlood(p,codes,delay,pathHashSize);
  }
  void handleCommand(uint32_t senderTimestamp,char* command,char* reply){
    if(regionLoadActive){
      if(StrHelper::isBlank(command)){regionMap=tempMap;regionLoadActive=false;sprintf(reply,"OK - loaded %d regions",regionMap.getCount());return;} // a blank line ends "region load"
      char* np=command;while(*np==' ')np++;int indent=np-command;
      char* ep=np;while(RegionMap::is_name_char(*ep))ep++;if(*ep)*ep++=0;while(*ep&&*ep!='F')ep++; // optional flags
      if(indent>0&&indent<8&&strlen(np)){
        if(RegionEntry* parent=loadStack[indent-1]){
          auto old=regionMap.findByName(np);auto nw=tempMap.putRegion(np,parent->id,old?old->id:0);
          if(nw){nw->flags=old?old->flags:(*ep=='F'?0:REGION_DENY_FLOOD);loadStack[indent]=nw;}
        }
      }
      reply[0]=0;return;
    }
    while(*command==' ')command++;
    if(strlen(command)>4&&command[2]=='|'){memcpy(reply,command,3);reply+=3;command+=3;} // companion CLI prefix, reflected back
    if(!memcmp(command,"setperm ",8)){ // setperm {pubkey-hex} {permissions}
      char* hex=&command[8];char* sp=strchr(hex,' ');
      if(!sp){strcpy(reply,"Err - bad params");return;}*sp++=0;
      uint8_t key[PUB_KEY_SIZE];int hexLen=min(int(sp-hex),PUB_KEY_SIZE*2);
      if(!mesh::Utils::fromHex(key,hexLen/2,hex))strcpy(reply,"Err - bad pubkey");
      else if(acl.applyPermissions(self_id,key,hexLen/2,atoi(sp))){dirtyContactsExpiry=futureMillis(lazyContactsWrite);strcpy(reply,"OK");}
      else strcpy(reply,"Err - invalid params");
    } else if(senderTimestamp==0&&!strcmp(command,"get acl")){
      char* dp=reply;for(int i=0;i<acl.getNumClients()&&dp-reply<120;i++){auto c=acl.getClientByIdx(i);if(!c->permissions)continue;if(dp!=reply)*dp++='\n';dp+=sprintf(dp,"%02X ",c->permissions);mesh::Utils::toHex(dp,c->id.pub_key,8);dp+=16;}
      if(dp==reply)strcpy(reply,"-none-");
    } else if(!roleCommand(command,reply)){
      cliHandler.handleCommand(senderTimestamp,command,reply);
      if(rejected.length()){strlcpy(reply,rejected.c_str(),160);rejected="";}
    }
  }
  void loop(){
    mesh::Mesh::loop();sensors.loop();roleLoop();
    if(nextFloodAdvert&&millisHasNowPassed(nextFloodAdvert)){if(auto* p=createSelfAdvert())sendFloodScoped(defaultScope,p,0,prefs.path_hash_mode+1);updateFloodAdvertTimer();updateAdvertTimer();} // the local advert does not follow at once
    else if(nextLocalAdvert&&millisHasNowPassed(nextLocalAdvert)){if(auto* p=createSelfAdvert())sendZeroHop(p);updateAdvertTimer();}
    // Temporary radio parameters ("tempradio"): applied after the CLI reply is out, reverted when they expire.
    if(setRadioAt&&millisHasNowPassed(setRadioAt)&&!meshRadio.busy()){Config saved=config;config.frequency=pendingFreq;config.bandwidth=pendingBw;config.sf=pendingSf;config.cr=pendingCr;if(meshRadio.applyConfig())setRadioAt=0;config=saved;}
    if(revertRadioAt&&!setRadioAt&&millisHasNowPassed(revertRadioAt)&&!meshRadio.busy()&&meshRadio.applyConfig())revertRadioAt=0;
    if(dirtyContactsExpiry&&millisHasNowPassed(dirtyContactsExpiry)){saveAcl();dirtyContactsExpiry=0;}
    uint32_t t=millis();uptimeMillis+=t-lastMillis;lastMillis=t;
  }
  // CommonCLICallbacks
  void savePrefs() override{syncToConfig();if(!rejected.length())cliHandler.savePrefs(fs);}
  const char* getFirmwareVer() override{return "v1.17.1-meshmesh";}
  const char* getBuildDate() override{return __DATE__;}
  bool formatFileSystem() override{for(auto* f:serverFiles)fs->remove(f);return true;} // only the server's files: MeshMesh data shares this file system
  void sendSelfAdvertisement(int delay,bool flood) override{mesh::Packet* p=createSelfAdvert();if(!p)return;if(flood)sendFloodScoped(defaultScope,p,delay,prefs.path_hash_mode+1);else sendZeroHop(p,delay);}
  void updateAdvertTimer() override{nextLocalAdvert=prefs.advert_interval?futureMillis(uint32_t(prefs.advert_interval)*2*60*1000):0;}
  void updateFloodAdvertTimer() override{nextFloodAdvert=prefs.flood_advert_interval?futureMillis(uint32_t(prefs.flood_advert_interval)*60*60*1000):0;}
  void setLoggingOn(bool enable) override{logging=enable;}
  void eraseLogFile() override{fs->remove(packetLog);}
  void dumpLogFile() override{File f=fs->open(packetLog);if(!f)return;while(f.available()){int c=f.read();if(c<0)break;Serial.print(char(c));}f.close();}
  void setTxPower(int8_t dbm) override{if(dbm>=0&&dbm<=MM_MAX_POWER&&dbm==config.power)meshRadio.radio.setOutputPower(dbm);}
  void formatNeighborsReply(char* reply) override{strcpy(reply,"not supported");}
  void formatStatsReply(char* reply) override{sprintf(reply,"{\"battery_mv\":%u,\"uptime_secs\":%u,\"errors\":%u,\"queue_len\":%u}",board.getBattMilliVolts(),unsigned(_ms->getMillis()/1000),_err_flags,_mgr->getOutboundTotal());}
  void formatRadioStatsReply(char* reply) override{sprintf(reply,"{\"noise_floor\":%d,\"last_rssi\":%d,\"last_snr\":%.2f,\"tx_air_secs\":%u,\"rx_air_secs\":%u}",_radio->getNoiseFloor(),int(_radio->getLastRSSI()),_radio->getLastSNR(),unsigned(getTotalAirTime()/1000),unsigned(getReceiveAirTime()/1000));}
  void formatPacketStatsReply(char* reply) override{sprintf(reply,"{\"recv\":%u,\"sent\":%u,\"flood_tx\":%u,\"direct_tx\":%u,\"flood_rx\":%u,\"direct_rx\":%u,\"recv_errors\":%u}",unsigned(meshRadio.rxCount),unsigned(meshRadio.txCount),unsigned(getNumSentFlood()),unsigned(getNumSentDirect()),unsigned(getNumRecvFlood()),unsigned(getNumRecvDirect()),unsigned(meshRadio.rejected));}
  void startRegionsLoad() override{tempMap.resetFrom(regionMap);memset(loadStack,0,sizeof(loadStack));loadStack[0]=&tempMap.getWildcard();regionLoadActive=true;}
  bool saveRegions() override{return regionMap.save(fs);}
  void onDefaultRegionChanged(const RegionEntry* r) override{if(r)regionMap.getTransportKeysFor(*r,&defaultScope,1);else memset(defaultScope.key,0,sizeof(defaultScope.key));}
  mesh::LocalIdentity& getSelfId() override{return self_id;}
  // One identity for every role, kept in NVS beside the chat contacts.
  void saveIdentity(const mesh::LocalIdentity& id) override{uint8_t bytes[96];const_cast<mesh::LocalIdentity&>(id).writeTo(bytes,96);Preferences p;if(p.begin("meshmesh-mc",false)){p.putBytes("identity",bytes,96);p.end();}memset(bytes,0,sizeof(bytes));}
  void clearStats() override{resetStats();((SimpleMeshTables*)getTables())->resetStats();meshRadio.rxCount=meshRadio.txCount=meshRadio.rejected=meshRadio.relayed=0;meshRadio.dirty=true;}
  void applyTempRadioParams(float freq,float bw,uint8_t sf,uint8_t cr,int minutes) override{
    Config temp=config;temp.frequency=freq;temp.bandwidth=bw;temp.sf=sf;temp.cr=cr;if(!temp.valid())return; // outside the board's range: ignored
    pendingFreq=freq;pendingBw=bw;pendingSf=sf;pendingCr=cr;setRadioAt=futureMillis(2000);revertRadioAt=futureMillis(2000+minutes*60*1000); // the CLI reply goes first
  }
  unsigned clientCount(bool admins){unsigned n=0;for(int i=0;i<acl.getNumClients();i++){auto* c=acl.getClientByIdx(i);if(c->permissions&&(!admins||c->isAdmin()))n++;}return n;}
  bool announce(bool flood){mesh::Packet* p=createSelfAdvert();if(!p)return false;if(flood)sendFloodScoped(defaultScope,p,0,prefs.path_hash_mode+1);else sendZeroHop(p);return true;}
  virtual void addJson(JsonDocument&){}
};

class RepeaterMesh:public ServerMesh {
  uint8_t replyPath[MAX_PATH_SIZE];uint8_t replyPathLen=0;
  RateLimiter discoverLimiter{4,120},anonLimiter{4,180}; // 4 per 2 min, 4 per 3 min
  uint32_t pendingDiscoverTag=0;unsigned long pendingDiscoverUntil=0;
  void putNeighbour(const mesh::Identity& id,uint32_t timestamp,float snr){
    // An existing neighbour is updated, otherwise the one heard longest ago is replaced.
    NeighbourInfo* n=&neighbours[0];uint32_t oldest=0xffffffff;
    for(auto& v:neighbours){if(id.matches(v.id)){n=&v;break;}if(v.heard_timestamp<oldest){n=&v;oldest=v.heard_timestamp;}}
    n->id=id;n->advert_timestamp=timestamp;n->heard_timestamp=now();n->snr=int8_t(snr*4);
  }
  uint8_t handleLoginReq(const mesh::Identity& sender,const uint8_t* secret,uint32_t senderTimestamp,const uint8_t* data,bool isFlood){
    ClientInfo* client=nullptr;
    if(data[0]==0)client=acl.getClient(sender.pub_key,PUB_KEY_SIZE); // blank password: a client already in the ACL
    if(!client){
      uint8_t perms;
      if(!strcmp((const char*)data,prefs.password))perms=PERM_ACL_ADMIN;
      else if(!strcmp((const char*)data,prefs.guest_password))perms=PERM_ACL_GUEST;
      else return 0;
      client=acl.putClient(sender,0);
      if(!client||senderTimestamp<=client->last_timestamp)return 0; // table full, or a replayed login
      client->last_timestamp=senderTimestamp;client->last_activity=now();
      client->permissions=(client->permissions&~0x03)|perms;memcpy(client->shared_secret,secret,PUB_KEY_SIZE);
      if(perms!=PERM_ACL_GUEST)dirtyContactsExpiry=futureMillis(lazyContactsWrite); // keep flash writes few
    }
    if(isFlood)client->out_path_len=OUT_PATH_UNKNOWN; // rediscover the path
    uint32_t stamp=getRTCClock()->getCurrentTimeUnique();memcpy(replyData,&stamp,4);
    replyData[4]=RespServerLoginOk;replyData[5]=0;replyData[6]=client->isAdmin()?1:0;replyData[7]=client->permissions;
    getRNG()->random(&replyData[8],4);replyData[12]=2;return 13; // firmware level 2
  }
  // Anonymous requests carry {reply-path-len}{reply-path}; the reply starts with their timestamp and our clock.
  bool anonReplyPath(const uint8_t*& data,uint32_t senderTimestamp){
    if(!anonLimiter.allow(now()))return false;replyPathLen=*data++;if(!mesh::Packet::isValidPathLen(replyPathLen))return false;
    mesh::Packet::writePath(replyPath,data,replyPathLen);memcpy(replyData,&senderTimestamp,4);uint32_t t=now();memcpy(&replyData[4],&t,4);return true;
  }
  int handleRequest(ClientInfo* sender,uint32_t senderTimestamp,uint8_t* payload){
    memcpy(replyData,&senderTimestamp,4); // the sender's timestamp comes back as a tag
    if(payload[0]==ReqGetStatus){RepeaterStats s={};fillStats(s);s.total_rx_air_time_secs=getReceiveAirTime()/1000;s.n_recv_errors=meshRadio.rejected;memcpy(&replyData[4],&s,sizeof(s));return 4+sizeof(s);} // guests too
    if(payload[0]==ReqGetTelemetry)return telemetryReply(sender,payload);
    if(payload[0]==ReqGetAccessList&&sender->isAdmin()&&payload[1]==0&&payload[2]==0)return aclReply(false);
    if(payload[0]==ReqGetNeighbours&&payload[1]==0){ // request version 0
      uint8_t count=payload[2],orderBy=payload[5],prefix=min<uint8_t>(payload[6],PUB_KEY_SIZE);uint16_t offset;memcpy(&offset,&payload[3],2);
      NeighbourInfo* sorted[maxNeighbours];int16_t total=sortedNeighbours(sorted,orderBy);
      uint8_t results[130];int resultCount=0,resultOffset=0;
      for(int i=0;i<count&&i+offset<total;i++){
        if(resultOffset+prefix+5>int(sizeof(results)))break;
        auto* n=sorted[i+offset];uint32_t ago=now()-n->heard_timestamp;
        memcpy(&results[resultOffset],n->id.pub_key,prefix);resultOffset+=prefix;memcpy(&results[resultOffset],&ago,4);resultOffset+=4;memcpy(&results[resultOffset],&n->snr,1);resultOffset+=1;resultCount++;
      }
      int ofs=4;memcpy(&replyData[ofs],&total,2);ofs+=2;memcpy(&replyData[ofs],&resultCount,2);ofs+=2;memcpy(&replyData[ofs],results,resultOffset);return ofs+resultOffset;
    }
    if(payload[0]==ReqGetOwnerInfo){snprintf((char*)&replyData[4],sizeof(replyData)-4,"%s\n%s\n%s",getFirmwareVer(),prefs.node_name,prefs.owner_info);return 4+strlen((char*)&replyData[4]);}
    return 0;
  }
  bool isLooped(const mesh::Packet* p,const uint8_t maxCounters[]){
    uint8_t size=p->getPathHashSize(),count=p->getPathHashCount(),n=0;const uint8_t* path=p->path;
    for(;count;count--,path+=size)if(self_id.isHashMatch(path,size))n++; // times this node is already in the path
    return n>=maxCounters[size];
  }
  void sendNodeDiscoverReq(){
    uint8_t data[10]={CtlNodeDiscoverReq,uint8_t(1<<ADV_TYPE_REPEATER)};getRNG()->random(&data[2],4);memcpy(&pendingDiscoverTag,&data[2],4);pendingDiscoverUntil=futureMillis(60000); // since: 0
    if(auto* p=createControlData(data,sizeof(data)))sendZeroHop(p);
  }
 protected:
  bool allowPacketForward(const mesh::Packet* p) override{
    if(prefs.disable_fwd)return false;
    if(p->isRouteFlood()){
      if(floodLimited(p))return false;
      if(!recvPktRegion)return false; // unknown transport code, or the wildcard denies flooding
      if(prefs.loop_detect!=LOOP_DETECT_OFF&&isLooped(p,prefs.loop_detect==LOOP_DETECT_MINIMAL?maxLoopMinimal:prefs.loop_detect==LOOP_DETECT_MODERATE?maxLoopModerate:maxLoopStrict))return false;
    }
    return forwardAllowed(p);
  }
  void onAnonDataRecv(mesh::Packet* p,const uint8_t* secret,const mesh::Identity& sender,uint8_t* data,size_t len) override{
    if(p->getPayloadType()!=PAYLOAD_TYPE_ANON_REQ)return; // a first request by a possible client, unknown so far
    uint32_t timestamp;memcpy(&timestamp,data,4);data[len]=0;uint8_t n=0;replyPathLen=0xff;const uint8_t* rest=&data[5];
    if(data[4]==0||data[4]>=' ')n=handleLoginReq(sender,secret,timestamp,&data[4],p->isRouteFlood()); // a password
    else if(p->isRouteDirect()&&data[4]==AnonReqRegions){if(anonReplyPath(rest,timestamp))n=8+regionMap.exportNamesTo((char*)&replyData[8],sizeof(replyData)-12,REGION_DENY_FLOOD);}
    else if(p->isRouteDirect()&&data[4]==AnonReqOwner){if(anonReplyPath(rest,timestamp)){snprintf((char*)&replyData[8],sizeof(replyData)-8,"%s\n%s",prefs.node_name,prefs.owner_info);n=8+strlen((char*)&replyData[8]);}}
    else if(p->isRouteDirect()&&data[4]==AnonReqBasic){if(anonReplyPath(rest,timestamp)){replyData[8]=prefs.disable_fwd?0x80:0;n=9;}} // features: 0x80 forwarding off
    if(!n)return;
    ClientInfo* client=acl.getClient(sender.pub_key,PUB_KEY_SIZE);bool outPath=client&&client->out_path_len!=OUT_PATH_UNKNOWN;
    if(p->isRouteFlood()){ // tell the sender the path to here, with the reply inside
      mesh::Packet* path=createPathReturn(sender,secret,p->path,p->path_len,PAYLOAD_TYPE_RESPONSE,replyData,n);if(path)sendFloodReply(path,serverResponseDelay,p->getPathHashSize());return;
    }
    mesh::Packet* r=createDatagram(PAYLOAD_TYPE_RESPONSE,sender,secret,replyData,n);if(!r)return;
    if(replyPathLen!=0xff)sendDirect(r,replyPath,replyPathLen,serverResponseDelay);
    else if(outPath)sendDirect(r,client->out_path,client->out_path_len,serverResponseDelay);
    else sendFloodReply(r,serverResponseDelay,p->getPathHashSize());
  }
  void onAdvertRecv(mesh::Packet* p,const mesh::Identity& id,uint32_t timestamp,const uint8_t* appData,size_t appDataLen) override{
    mesh::Mesh::onAdvertRecv(p,id,timestamp,appData,appDataLen);
    if(p->getPathHashCount()==0&&!isShare(p)){AdvertDataParser parser(appData,appDataLen);if(parser.isValid()&&parser.getType()==ADV_TYPE_REPEATER)putNeighbour(id,timestamp,p->getSNR());} // zero-hop repeaters only
  }
  void onPeerDataRecv(mesh::Packet* p,uint8_t type,int senderIdx,const uint8_t* secret,uint8_t* data,size_t len) override{
    ClientInfo* client=peer(senderIdx);if(!client)return;
    if(type==PAYLOAD_TYPE_REQ){ // a request from a known client
      uint32_t timestamp;memcpy(&timestamp,data,4);if(timestamp<=client->last_timestamp)return; // replay
      int n=handleRequest(client,timestamp,&data[4]);if(!n)return;
      client->last_timestamp=timestamp;client->last_activity=now();
      reply(client,secret,p,PAYLOAD_TYPE_RESPONSE,replyData,n,serverResponseDelay,true);
    } else if(type==PAYLOAD_TYPE_TXT_MSG&&len>5&&client->isAdmin()){ // a CLI command
      uint32_t senderTimestamp;memcpy(&senderTimestamp,data,4);uint8_t flags=data[4]>>2;
      if((flags!=TXT_TYPE_PLAIN&&flags!=TXT_TYPE_CLI_DATA)||senderTimestamp<client->last_timestamp)return;
      bool retry=senderTimestamp==client->last_timestamp;client->last_timestamp=senderTimestamp;client->last_activity=now();
      data[len]=0; // the text may be zero-padded
      if(flags==TXT_TYPE_PLAIN){uint32_t ack;mesh::Utils::sha256((uint8_t*)&ack,4,data,5+strlen((char*)&data[5]),client->id.pub_key,PUB_KEY_SIZE);ackTo(client,ack,p,txtAckDelay);} // legacy CLI expects an ACK
      uint8_t temp[166];char* text=(char*)&temp[5];*text=0;
      if(!retry)handleCommand(senderTimestamp,(char*)&data[5],text);
      int n=strlen(text);if(!n)return;
      uint32_t stamp=getRTCClock()->getCurrentTimeUnique();if(stamp==senderTimestamp)stamp++; // the CLI view needs them different
      memcpy(temp,&stamp,4);temp[4]=TXT_TYPE_CLI_DATA<<2;reply(client,secret,p,PAYLOAD_TYPE_TXT_MSG,temp,5+n,cliReplyDelay,false);
    }
  }
  bool onPeerPathRecv(mesh::Packet*,int senderIdx,const uint8_t*,uint8_t* path,uint8_t pathLen,uint8_t,uint8_t*,uint8_t) override{
    if(auto* c=peer(senderIdx)){c->out_path_len=mesh::Packet::copyPath(c->out_path,path,pathLen);c->last_activity=now();}
    return false; // no reciprocal path
  }
  void onControlDataRecv(mesh::Packet* p) override{
    uint8_t type=p->payload[0]&0xf0;
    if(type==CtlNodeDiscoverReq&&p->payload_len>=6&&!prefs.disable_fwd&&discoverLimiter.allow(now())){
      uint8_t filter=p->payload[1];uint32_t tag,since=0;memcpy(&tag,&p->payload[2],4);if(p->payload_len>=10)memcpy(&since,&p->payload[6],4);
      if((filter&(1<<ADV_TYPE_REPEATER))&&prefs.discovery_mod_timestamp>=since){
        bool prefixOnly=p->payload[0]&1;uint8_t data[6+PUB_KEY_SIZE];data[0]=CtlNodeDiscoverResp|ADV_TYPE_REPEATER;data[1]=p->_snr;memcpy(&data[2],&tag,4);memcpy(&data[6],self_id.pub_key,PUB_KEY_SIZE);
        if(auto* r=createControlData(data,prefixOnly?6+8:6+PUB_KEY_SIZE))sendZeroHop(r,getRetransmitDelay(r)*4); // several nodes may answer
      }
    } else if(type==CtlNodeDiscoverResp&&p->payload_len>=6+PUB_KEY_SIZE&&(p->payload[0]&0x0f)==ADV_TYPE_REPEATER){
      if(!pendingDiscoverTag||millisHasNowPassed(pendingDiscoverUntil)){pendingDiscoverTag=0;return;}
      uint32_t tag;memcpy(&tag,&p->payload[2],4);if(tag!=pendingDiscoverTag)return;
      mesh::Identity id(&p->payload[6]);if(!id.matches(self_id))putNeighbour(id,now(),p->getSNR());
    }
  }
  bool roleCommand(char* command,char* reply) override{
    if(memcmp(command,"discover.neighbors",18))return false;
    const char* sub=command+18;while(*sub==' ')sub++;
    if(*sub)strcpy(reply,"Err - discover.neighbors has no options");else{sendNodeDiscoverReq();strcpy(reply,"OK - Discover sent");}
    return true;
  }
 public:
  NeighbourInfo neighbours[maxNeighbours];
  RepeaterMesh(mesh::Radio& radio,mesh::MillisecondClock& ms,mesh::RNG& rng,mesh::RTCClock& rtc,mesh::PacketManager& pool,mesh::MeshTables& tables)
   :ServerMesh(ADV_TYPE_REPEATER,radio,ms,rng,rtc,pool,tables){prefs.direct_tx_delay_factor=0.3f;}
  const char* getRole() override{return "repeater";}
  // order: 0 newest first, 1 oldest first, 2 strongest first, 3 weakest first
  int16_t sortedNeighbours(NeighbourInfo** sorted,uint8_t order){
    int16_t n=0;for(auto& v:neighbours)if(v.heard_timestamp)sorted[n++]=&v;
    auto by=[&](bool(*less)(const NeighbourInfo*,const NeighbourInfo*)){std::sort(sorted,sorted+n,less);};
    if(order==0)by([](const NeighbourInfo* a,const NeighbourInfo* b){return a->heard_timestamp>b->heard_timestamp;});
    else if(order==1)by([](const NeighbourInfo* a,const NeighbourInfo* b){return a->heard_timestamp<b->heard_timestamp;});
    else if(order==2)by([](const NeighbourInfo* a,const NeighbourInfo* b){return a->snr>b->snr;});
    else if(order==3)by([](const NeighbourInfo* a,const NeighbourInfo* b){return a->snr<b->snr;});
    return n;
  }
  void formatNeighborsReply(char* reply) override{
    NeighbourInfo* sorted[maxNeighbours];int16_t n=sortedNeighbours(sorted,0);char* dp=reply;
    for(int i=0;i<n&&dp-reply<134;i++){if(i)*dp++='\n';char hex[10];mesh::Utils::toHex(hex,sorted[i]->id.pub_key,4);dp+=sprintf(dp,"%s:%d:%d",hex,int(now()-sorted[i]->heard_timestamp),sorted[i]->snr);}
    if(dp==reply)strcpy(reply,"-none-");else *dp=0;
  }
  void removeNeighbor(const uint8_t* key,int len) override{for(auto& n:neighbours)if(!memcmp(n.id.pub_key,key,len))n=NeighbourInfo();}
  void addJson(JsonDocument& d) override{
    JsonArray a=d.createNestedArray("neighbours");NeighbourInfo* sorted[maxNeighbours];int16_t n=sortedNeighbours(sorted,0);uint32_t t=now();
    for(int i=0;i<n;i++){JsonObject o=a.createNestedObject();char hex[17];mesh::Utils::toHex(hex,sorted[i]->id.pub_key,8);o["key"]=hex;o["seconds"]=t-sorted[i]->heard_timestamp;o["snr"]=sorted[i]->snr/4.0f;}
  }
};

// Room server: logged-in clients post text; every new post is pushed to each other client in turn
// and confirmed by its ACK. Read-write login with the room password, read-only without one when allowed.
class RoomMesh:public ServerMesh {
  unsigned long nextPush=0,postsDue=0;
  int nextClientIdx=0;
  struct PostFile {uint32_t magic=0x50524d4d,version=1,count=0;struct Rec{uint8_t author[PUB_KEY_SIZE];uint32_t stamp;char text[maxPostText+1];} posts[maxPosts]={};uint8_t hash[32]={};}; // "MMRP"
  static bool adminsOnly(ClientInfo* c){return c->isAdmin();}
  void storePost(const mesh::Identity& author,const char* text){
    auto& p=posts[nextPostIdx];p.author=author;copyText(p.text,text,sizeof(p.text));p.post_timestamp=getRTCClock()->getCurrentTimeUnique();
    nextPostIdx=(nextPostIdx+1)%maxPosts;nextPush=futureMillis(pushNotifyDelay);posted++;postsDue=futureMillis(lazyContactsWrite);meshRadio.dirty=true;
  }
  void pushPostToClient(ClientInfo* client,PostInfo& post){
    int len=0;memcpy(&replyData[len],&post.post_timestamp,4);len+=4; // a past timestamp, accepted by clients
    uint8_t attempt;getRNG()->random(&attempt,1);replyData[len++]=(TXT_TYPE_SIGNED_PLAIN<<2)|(attempt&3); // retries get another hash and ACK
    memcpy(&replyData[len],post.author.pub_key,4);len+=4; // author prefix
    int n=strlen(post.text);memcpy(&replyData[len],post.text,n);len+=n;
    mesh::Utils::sha256((uint8_t*)&client->extra.room.pending_ack,4,replyData,len,client->id.pub_key,PUB_KEY_SIZE);client->extra.room.push_post_timestamp=post.post_timestamp;
    auto* r=createDatagram(PAYLOAD_TYPE_TXT_MSG,client->id,client->shared_secret,replyData,len);
    if(!r){client->extra.room.pending_ack=0;return;}
    if(client->out_path_len==OUT_PATH_UNKNOWN){sendFloodScoped(defaultScope,r,0,prefs.path_hash_mode+1);client->extra.room.ack_timeout=futureMillis(pushAckTimeoutFlood);}
    else{sendDirect(r,client->out_path,client->out_path_len);client->extra.room.ack_timeout=futureMillis(pushTimeoutBase+pushAckTimeoutFactor*((client->out_path_len&63)+1));}
    pushes++;
  }
  uint8_t unsyncedCount(ClientInfo* client){uint8_t n=0;for(auto& p:posts)if(p.post_timestamp>client->extra.room.sync_since&&!p.author.matches(client->id))n++;return n;}
  bool processAck(const uint8_t* data){
    for(int i=0;i<acl.getNumClients();i++){auto* c=acl.getClientByIdx(i);
      if(c->extra.room.pending_ack&&!memcmp(data,&c->extra.room.pending_ack,4)){c->extra.room.pending_ack=0;c->extra.room.push_failures=0;c->extra.room.sync_since=c->extra.room.push_post_timestamp;return true;} // the next post can follow
    }
    return false;
  }
  int handleRequest(ClientInfo* sender,uint32_t senderTimestamp,uint8_t* payload){
    memcpy(replyData,&senderTimestamp,4); // the sender's timestamp comes back as a tag
    if(payload[0]==ReqGetStatus){RoomStats s={};fillStats(s);s.n_posted=posted;s.n_post_push=pushes;memcpy(&replyData[4],&s,sizeof(s));return 4+sizeof(s);}
    if(payload[0]==ReqGetTelemetry)return telemetryReply(sender,payload);
    if(payload[0]==ReqGetAccessList&&sender->isAdmin()&&payload[1]==0&&payload[2]==0)return aclReply(true);
    return 0;
  }
  void savePosts(){
    PostFile* f=new PostFile;
    for(unsigned k=0;k<maxPosts;k++){auto& p=posts[(nextPostIdx+k)%maxPosts];if(!p.post_timestamp)continue;auto& r=f->posts[f->count++];memcpy(r.author,p.author.pub_key,PUB_KEY_SIZE);r.stamp=p.post_timestamp;memcpy(r.text,p.text,sizeof(r.text));} // oldest first
    mesh::Utils::sha256(f->hash,32,(const uint8_t*)f,offsetof(PostFile,hash));
    File out=fs->open("/room_posts.new","w",true);bool ok=out&&out.write((const uint8_t*)f,sizeof(*f))==sizeof(*f);if(out)out.close();
    if(ok){fs->remove(postsFile);ok=fs->rename("/room_posts.new",postsFile);} // the previous file stays until the new one is whole
    delete f;postsDue=0;
  }
  void loadPosts(){
    File in=fs->open(postsFile);if(!in)return;PostFile* f=new PostFile;bool ok=in.read((uint8_t*)f,sizeof(*f))==sizeof(*f);in.close();uint8_t hash[32];
    if(ok){mesh::Utils::sha256(hash,32,(const uint8_t*)f,offsetof(PostFile,hash));ok=f->magic==0x50524d4d&&f->version==1&&f->count<=maxPosts&&!memcmp(hash,f->hash,32);}
    if(ok)for(unsigned i=0;i<f->count;i++){auto& r=f->posts[i];r.text[maxPostText]=0;if(!meshmesh::validUtf8((const uint8_t*)r.text,strlen(r.text)))continue;auto& p=posts[nextPostIdx];p.author=mesh::Identity(r.author);p.post_timestamp=r.stamp;memcpy(p.text,r.text,sizeof(p.text));nextPostIdx=(nextPostIdx+1)%maxPosts;}
    delete f;
  }
 protected:
  void saveAcl() override{acl.save(fs,adminsOnly);} // as stock: only admins are kept
  bool allowPacketForward(const mesh::Packet* p) override{
    if(prefs.disable_fwd)return false;
    if(p->isRouteFlood()&&floodLimited(p))return false;
    return forwardAllowed(p);
  }
  void onAnonDataRecv(mesh::Packet* p,const uint8_t* secret,const mesh::Identity& sender,uint8_t* data,size_t len) override{
    if(p->getPayloadType()!=PAYLOAD_TYPE_ANON_REQ)return; // a login by a possible client, unknown so far
    uint32_t senderTimestamp,syncSince;memcpy(&senderTimestamp,data,4);memcpy(&syncSince,&data[4],4);data[len]=0;
    ClientInfo* client=nullptr;
    if(data[8]==0)client=acl.getClient(sender.pub_key,PUB_KEY_SIZE); // blank password: a client already in the ACL
    if(!client){
      uint8_t perm;const char* pass=(const char*)&data[8];
      if(!strcmp(pass,prefs.password))perm=PERM_ACL_ADMIN;
      else if(!strcmp(pass,prefs.guest_password))perm=PERM_ACL_READ_WRITE; // the room password
      else if(prefs.allow_read_only)perm=PERM_ACL_GUEST;
      else return; // no reply: the client times out
      client=acl.putClient(sender,0);
      if(!client||senderTimestamp<=client->last_timestamp)return; // table full, or a replayed login
      client->last_timestamp=senderTimestamp;client->extra.room.sync_since=syncSince;client->extra.room.pending_ack=0;client->extra.room.push_failures=0;
      client->last_activity=now();client->permissions=(client->permissions&~0x03)|perm;memcpy(client->shared_secret,secret,PUB_KEY_SIZE);
      dirtyContactsExpiry=futureMillis(lazyContactsWrite);
    }
    if(p->isRouteFlood())client->out_path_len=OUT_PATH_UNKNOWN; // rediscover the path
    uint32_t stamp=getRTCClock()->getCurrentTimeUnique();memcpy(replyData,&stamp,4);
    replyData[4]=RespServerLoginOk;replyData[5]=0;replyData[6]=client->isAdmin()?1:client->permissions==0?2:0;replyData[7]=client->permissions;
    getRNG()->random(&replyData[8],4);replyData[12]=1; // firmware level 1
    nextPush=futureMillis(pushNotifyDelay); // the login reply goes first
    reply(client,client->shared_secret,p,PAYLOAD_TYPE_RESPONSE,replyData,13,serverResponseDelay,true);
  }
  void onPeerDataRecv(mesh::Packet* p,uint8_t type,int senderIdx,const uint8_t* secret,uint8_t* data,size_t len) override{
    ClientInfo* client=peer(senderIdx);if(!client)return;
    if(type==PAYLOAD_TYPE_TXT_MSG&&len>5){ // a CLI command or a new post
      uint32_t senderTimestamp;memcpy(&senderTimestamp,data,4);uint8_t flags=data[4]>>2;
      if((flags!=TXT_TYPE_PLAIN&&flags!=TXT_TYPE_CLI_DATA)||senderTimestamp<client->last_timestamp)return; // replay; retries still get ACKs
      bool retry=senderTimestamp==client->last_timestamp;client->last_timestamp=senderTimestamp;
      uint32_t stamp=getRTCClock()->getCurrentTimeUnique();client->last_activity=stamp;client->extra.room.push_failures=0; // pushes resume
      data[len]=0; // the text may be zero-padded
      uint32_t ack;mesh::Utils::sha256((uint8_t*)&ack,4,data,5+strlen((char*)&data[5]),client->id.pub_key,PUB_KEY_SIZE);
      uint8_t temp[166];temp[5]=0;bool sendAck=false;
      if(flags==TXT_TYPE_CLI_DATA){if(client->isAdmin()&&!retry){handleCommand(senderTimestamp,(char*)&data[5],(char*)&temp[5]);temp[4]=TXT_TYPE_CLI_DATA<<2;}}
      else if((client->permissions&PERM_ACL_ROLE_MASK)!=PERM_ACL_GUEST){if(!retry)storePost(client->id,(const char*)&data[5]);sendAck=true;} // read-only clients cannot post
      uint32_t delay=0;
      if(sendAck){
        if(client->out_path_len==OUT_PATH_UNKNOWN){ackTo(client,ack,p,txtAckDelay);delay=txtAckDelay+replyDelay;}
        else{uint32_t d=txtAckDelay;if(getExtraAckTransmitCount()>0){if(auto* a1=createMultiAck(ack,1))sendDirect(a1,client->out_path,client->out_path_len,d);d+=300;}if(auto* a2=createAck(ack))sendDirect(a2,client->out_path,client->out_path_len,d);delay=d+replyDelay;}
      }
      int n=strlen((char*)&temp[5]);if(!n)return;
      if(stamp==senderTimestamp)stamp++; // the CLI view needs them different
      memcpy(temp,&stamp,4);reply(client,secret,p,PAYLOAD_TYPE_TXT_MSG,temp,5+n,delay+serverResponseDelay,false);
    } else if(type==PAYLOAD_TYPE_REQ&&len>=5){
      uint32_t senderTimestamp;memcpy(&senderTimestamp,data,4);if(senderTimestamp<client->last_timestamp)return; // replay
      client->last_timestamp=senderTimestamp;client->last_activity=now();client->extra.room.push_failures=0;
      if(data[4]==ReqKeepAlive&&p->isRouteDirect()){
        uint32_t forceSince=0;if(len>=9)memcpy(&forceSince,&data[5],4);else memcpy(&data[5],&forceSince,4); // optional: the last post the client has
        if(forceSince>0)client->extra.room.sync_since=forceSince;client->extra.room.pending_ack=0;
        if(client->out_path_len!=OUT_PATH_UNKNOWN){ // keep-alive answers go direct only
          uint32_t ack;mesh::Utils::sha256((uint8_t*)&ack,4,data,9,client->id.pub_key,PUB_KEY_SIZE);
          if(auto* r=createAck(ack)){r->payload[r->payload_len++]=unsyncedCount(client);sendDirect(r,client->out_path,client->out_path_len,serverResponseDelay);} // with the unsynced count
        }
      } else if(int n=handleRequest(client,senderTimestamp,&data[4]))reply(client,secret,p,PAYLOAD_TYPE_RESPONSE,replyData,n,serverResponseDelay,true);
    }
  }
  bool onPeerPathRecv(mesh::Packet*,int senderIdx,const uint8_t*,uint8_t* path,uint8_t pathLen,uint8_t extraType,uint8_t* extra,uint8_t extraLen) override{
    if(auto* c=peer(senderIdx)){c->out_path_len=mesh::Packet::copyPath(c->out_path,path,pathLen);c->last_activity=now();}
    if(extraType==PAYLOAD_TYPE_ACK&&extraLen>=4)processAck(extra); // an ACK came with the path
    return false; // no reciprocal path
  }
  void onAckRecv(mesh::Packet* p,uint32_t ack) override{if(processAck((uint8_t*)&ack))p->markDoNotRetransmit();} // ours: not forwarded
  bool roleCommand(char* command,char* reply) override{
    if(strncmp(command,"room.post",9))return false;
    char* msg=command+9;while(*msg==' ')msg++;
    if(!*msg)strcpy(reply,"ERR empty message");else{storePost(self_id,msg);strcpy(reply,"OK");}
    return true;
  }
  void flush() override{ServerMesh::flush();if(postsDue)savePosts();}
  void roleLoop() override{
    if(postsDue&&millisHasNowPassed(postsDue))savePosts();
    if(!millisHasNowPassed(nextPush)||!acl.getNumClients())return;
    for(int i=0;i<acl.getNumClients();i++){auto* c=acl.getClientByIdx(i);if(c->extra.room.pending_ack&&millisHasNowPassed(c->extra.room.ack_timeout)){c->extra.room.push_failures++;c->extra.room.pending_ack=0;}} // ACK timeouts
    // Round robin: the next client gets its next new post, unless it is waiting for an ACK or failed 3 times.
    if(nextClientIdx>=acl.getNumClients())nextClientIdx=0;
    auto* client=acl.getClientByIdx(nextClientIdx);bool pushed=false;
    if(!client->extra.room.pending_ack&&client->last_activity&&client->extra.room.push_failures<3){
      uint32_t t=now();
      for(unsigned k=0,idx=nextPostIdx;k<maxPosts;k++,idx=(idx+1)%maxPosts){auto& p=posts[idx];
        if(t>=p.post_timestamp+postSyncDelaySecs&&p.post_timestamp>client->extra.room.sync_since&&!p.author.matches(client->id)){pushPostToClient(client,p);pushed=true;break;}} // not to the author
    }
    nextClientIdx=(nextClientIdx+1)%acl.getNumClients();
    nextPush=futureMillis(pushed?syncPushInterval:syncPushInterval/8); // nothing for this client: the next one soon
  }
 public:
  PostInfo posts[maxPosts]; // cyclic
  unsigned nextPostIdx=0;uint16_t posted=0,pushes=0;
  RoomMesh(mesh::Radio& radio,mesh::MillisecondClock& ms,mesh::RNG& rng,mesh::RTCClock& rtc,mesh::PacketManager& pool,mesh::MeshTables& tables)
   :ServerMesh(ADV_TYPE_ROOM,radio,ms,rng,rtc,pool,tables){prefs.direct_tx_delay_factor=0.2f;prefs.disable_fwd=1;} // a room does not forward unless asked
  const char* getRole() override{return "room_server";}
  void begin() override{ServerMesh::begin();loadPosts();}
  bool post(const char* text){if(!*text)return false;storePost(self_id,text);return true;}
  unsigned postCount(){unsigned n=0;for(auto& p:posts)if(p.post_timestamp)n++;return n;}
  void addJson(JsonDocument& d) override{
    d["read_only_login"]=bool(prefs.allow_read_only);d["posted"]=posted;d["pushed"]=pushes;
    JsonArray a=d.createNestedArray("posts");
    for(unsigned k=maxPosts;k>0;k--){auto& p=posts[(nextPostIdx+k-1)%maxPosts];if(!p.post_timestamp)continue; // newest first
      JsonObject o=a.createNestedObject();char hex[9];mesh::Utils::toHex(hex,p.author.pub_key,4);o["author"]=hex;o["own"]=p.author.matches(self_id);o["time"]=p.post_timestamp;o["text"]=p.text;}
  }
};

bool MeshServer::begin(uint8_t role,mesh::Radio& radio,mesh::MillisecondClock& ms,mesh::RNG& rng,mesh::RTCClock& rtc,mesh::PacketManager& pool,mesh::MeshTables& tables,const mesh::LocalIdentity& identity){
  if(role==RoleRoom)node=new RoomMesh(radio,ms,rng,rtc,pool,tables);
  else if(role==RoleRepeater)node=new RepeaterMesh(radio,ms,rng,rtc,pool,tables);
  else return false;
  node->self_id=identity;node->begin();
  // The prefs file is shared by both roles: on a switch between them forwarding takes the new role's
  // default (repeater on, room off); passwords and admins stay.
  Preferences p;if(p.begin("meshmesh-mc",false)){if(p.getUChar("srv_role",RoleNormal)!=role){node->prefs.disable_fwd=role==RoleRoom;node->savePrefs();p.putUChar("srv_role",role);}p.end();}
  // Announced at boot like a stock server: flooded when the network does not know it in this role yet.
  bool known=false;if(p.begin("meshmesh-mc",true)){known=p.getUChar("adv_type",ADV_TYPE_CHAT)==node->type&&p.getString("adv_name","")==node->prefs.node_name;p.end();}
  node->sendSelfAdvertisement(5000,!known);
  return true;
}
bool MeshServer::room() const{return node&&node->type==ADV_TYPE_ROOM;}
void MeshServer::tick(){if(node)node->loop();}
void MeshServer::flush(){if(node)node->flush();}
void MeshServer::configChanged(){if(!node)return;node->loadFromConfig();node->savePrefs();}
bool MeshServer::advertise(bool flood){return node&&node->announce(flood);}
bool MeshServer::post(const String& text){return room()&&text.length()<=maxPostText&&meshmesh::validUtf8((const uint8_t*)text.c_str(),text.length())&&static_cast<RoomMesh*>(node)->post(text.c_str());}
const mesh::LocalIdentity* MeshServer::identity() const{return node?&node->self_id:nullptr;}
String MeshServer::command(const String& line){
  if(!node)return "ERR server role is not running";if(line.length()>160)return "ERR CLI command too long";
  char text[161],reply[166]={};strlcpy(text,line.c_str(),sizeof(text));node->handleCommand(0,text,reply);
  return reply[0]?String(reply):String("OK");
}
ServerView MeshServer::view(){
  ServerView v;if(!node)return v;auto& p=node->prefs;
  v.running=true;v.room=room();v.forwarding=!p.disable_fwd;v.readOnlyLogin=p.allow_read_only;v.password=p.password;v.guest=p.guest_password;
  v.advertMinutes=p.advert_interval*2;v.floodAdvertHours=p.flood_advert_interval;v.floodMax=p.flood_max;
  if(v.room)v.posts=static_cast<RoomMesh*>(node)->postCount();else for(auto& n:static_cast<RepeaterMesh*>(node)->neighbours)if(n.heard_timestamp)v.neighbours++;
  v.clients=node->clientCount(false);v.admins=node->clientCount(true);
  v.uptime=node->uptimeMillis/1000;v.floodRx=node->getNumRecvFlood();v.directRx=node->getNumRecvDirect();v.floodTx=node->getNumSentFlood();v.directTx=node->getNumSentDirect();return v;
}
String MeshServer::json(bool secrets){
  DynamicJsonDocument d(12288);d["running"]=running();if(!node){String s;serializeJson(d,s);return s;}
  if(secrets){d["password"]=node->prefs.password;d["guest_password"]=node->prefs.guest_password;String s;serializeJson(d,s);return s;}
  ServerView v=view();auto& p=node->prefs;
  d["role"]=room()?"room":"repeater";d["name"]=p.node_name;d["forwarding"]=v.forwarding;d["advert_minutes"]=v.advertMinutes;d["flood_advert_hours"]=v.floodAdvertHours;d["flood_max"]=v.floodMax;
  d["guest_login"]=!p.guest_password[0]?"blank":"password";d["owner_info"]=p.owner_info;d["uptime"]=v.uptime;d["relayed"]=meshRadio.relayed;
  d["flood_rx"]=v.floodRx;d["direct_rx"]=v.directRx;d["flood_tx"]=v.floodTx;d["direct_tx"]=v.directTx;d["clients"]=v.clients;d["admins"]=v.admins;
  node->addJson(d);
  String s;serializeJson(d,s);return s;
}
