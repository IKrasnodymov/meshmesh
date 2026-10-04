#include "Version.h"
#include <Utils.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "App.h"
#include "MeshRadio.h"
#include "Hardware.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "Internet.h"
#include "ChessNet.h"
#include "MeshServer.h"
#include "Power.h"
#include <LittleFS.h>
#if !defined(MM_NRF52)
#include <nvs.h>
#include "Storage.h"
#endif
#include <time.h>
String statusJson() {
  StaticJsonDocument<3072> d;
  d["board"]=MM_BOARD_ID;d["board_name"]=MM_BOARD_NAME;d["max_power"]=MM_MAX_POWER;
#if defined(MM_COMPACT)
  d["family"]="compact";d["button"]=MM_BUTTON;
#else
  d["family"]="full";
#endif
  {static const int absent[]={MM_ABSENT -1};JsonArray a=d.createNestedArray("absent");for(int i:absent)if(i>=0)a.add(i);}
  d["firmware"]=MESHMM_FIRMWARE;d["role"]=roleName(config.role);d["node"]=meshRadio.idText(meshRadio.nodeId);d["name"]=config.name;d["network"]=meshRadio.networkId;
  char buildHash[65];mesh::Utils::toHex(buildHash,esp_ota_get_app_description()->app_elf_sha256,32);d["build_sha256"]=buildHash;d["protocol"]="MeshCore";d["public_key"]=meshRadio.publicKeyText();d["channel"]="Public";d["channels"]=meshRadio.channelCount;d["public_message_limit"]=meshRadio.messageLimit();d["unix_time"]=int64_t(time(nullptr));d["clock_source"]=hardware.clockSource;d["clock_conflict"]=hardware.clockConflict;d["uptime"]=millis()/1000;d["boot"]=config.bootCounter;d["reset_reason"]=int(esp_reset_reason());d["heap"]=ESP.getFreeHeap();d["psram"]=ESP.getFreePsram();d["cpu_mhz"]=powerMhz();d["sleeps"]=powerSleeps();d["sleep_ms"]=powerSleptMs();d["slow_ms"]=powerSlowMs();
  d["radio"]=meshRadio.ready;d["radio_error"]=meshRadio.radioError;d["tx"]=meshRadio.txCount;d["radio_recal"]=meshRadio.recalibrations;d["rx"]=meshRadio.rxCount;d["rejected"]=meshRadio.rejected;d["relayed"]=meshRadio.relayed;d["contacts_replaced"]=meshRadio.replaced;d["contacts_saved"]=meshRadio.contactsSaved;
  d["diagnostic_rx"]=meshRadio.diagnosticRx;d["rssi"]=meshRadio.lastRssi;d["snr"]=meshRadio.lastSnr;d["keyboard"]=hardware.keyboardOk;d["key_count"]=hardware.keyCount;d["last_key"]=hardware.lastKey;
  d["battery_mv"]=hardware.batteryMv;d["sd"]=hardware.sdOk;d["storage"]=hardware.fsOk;d["rtc"]=hardware.rtcOk;d["rtc_valid"]=hardware.rtcValid;
  d["compass"]=hardware.compassOk;d["compass_sample"]=hardware.compassSample;d["imu"]=hardware.imuOk;d["imu_sample"]=hardware.imuSample;
  d["gps_bytes"]=hardware.gpsBytes;d["gps_sentences"]=hardware.gps.passedChecksum();d["gps_fix"]=hardware.gpsFix();d["satellites"]=hardware.gps.satellites.value();
  if(d["gps_fix"].as<bool>()) {d["latitude"]=hardware.gps.location.lat();d["longitude"]=hardware.gps.location.lng();}
  JsonArray a=d.createNestedArray("mag");for(float n:hardware.mag)a.add(n);a=d.createNestedArray("accel");for(float n:hardware.accel)a.add(n);
  d["clock_trusted"]=hardware.clockTrusted;d["busy"]=meshRadio.busy();if(meshRadio.lastRxAt)d["rx_age"]=(millis()-meshRadio.lastRxAt)/1000;
#if !defined(MM_NRF52)
  {nvs_stats_t nvs;if(nvs_get_stats(nullptr,&nvs)==ESP_OK){d["nvs_used"]=nvs.used_entries;d["nvs_free"]=nvs.free_entries;}} // 32-byte entries; the contacts no longer live there
#endif
  d["event"]=meshRadio.event;d["wifi"]=portalActive();
#if defined(MM_NRF52)
  d["wifi_radio"]=false; // nRF52: no Wi-Fi; the page arrives through the app over BLE or USB
#endif
  d["internet"]=internet.online();d["ble"]=bleActive();String s;serializeJson(d,s);return s;
}
String messagesJson() {
  DynamicJsonDocument d(32768);JsonArray a=d.to<JsonArray>();
  for(unsigned i=0;i<meshRadio.historyCount;i++) {const auto& m=meshRadio.history[i];JsonObject j=a.createNestedObject();j["protocol"]=m.protocol;j["source"]=meshRadio.idText(m.source);j["destination"]=meshRadio.idText(m.destination);j["session"]=m.session;j["id"]=m.id;j["name"]=m.name;j["text"]=m.text;j["time"]=m.timestamp;j["outgoing"]=m.outgoing;j["status"]=int(m.status);
   if(m.route){j["route"]=m.route==ChatMessage::RouteDirect?"direct":"flood";if(m.hops!=255)j["hops"]=m.hops;if(m.tries)j["tries"]=m.tries;}}
  String s;serializeJson(d,s);return s;
}
String nodesJson(){DynamicJsonDocument d(16384);JsonArray a=d.to<JsonArray>();for(unsigned i=0;i<meshRadio.peerCount;i++){auto& p=meshRadio.peers[i];JsonObject j=a.createNestedObject();char key[65];mesh::Utils::toHex(key,p.publicKey,32);j["public_key"]=key;j["type"]=p.type;j["heard"]=p.heard;j["path_length"]=p.pathLength;j["id"]=meshRadio.idText(p.id);j["name"]=p.name;j["rssi"]=p.rssi;j["snr"]=p.snr;if(p.heard)j["age_seconds"]=(millis()-p.seen)/1000;else j["age_seconds"]=nullptr;j["hops"]=p.hops;j["position"]=p.position;if(p.position){j["latitude"]=p.latitude;j["longitude"]=p.longitude;}}String s;serializeJson(d,s);return s;}
String channelsJson(bool secrets){
  DynamicJsonDocument d(6144);d["max"]=channels::Max;JsonArray a=d.createNestedArray("channels");
  for(unsigned i=0;i<meshRadio.channelCount;i++){const auto& c=meshRadio.channelList[i];JsonObject j=a.createNestedObject();bool open=!i||channels::isHashtag(c);
   j["id"]=meshRadio.idText(c.id);j["name"]=c.name;j["kind"]=!i?"public":open?"hashtag":"private";char hash[3];snprintf(hash,3,"%02X",channels::hashOf(c.secret));j["hash"]=hash;
   if(secrets||open)j["link"]=channels::link(c);}
  a=d.createNestedArray("heard");
  for(unsigned i=0;i<meshRadio.heardCount;i++){const auto& h=meshRadio.heard[i];JsonObject j=a.createNestedObject();char hash[3];snprintf(hash,3,"%02X",h.hash);j["hash"]=hash;j["packets"]=h.packets;j["age"]=(millis()-h.at)/1000;if(h.name[0])j["name"]=h.name;}
  d["samples"]=meshRadio.heardSamples;String s;serializeJson(d,s);return s;
}
// Replies: "OK channel added|exists ID", "OK ..." or "ERR <reason>: ..." (the page translates the reason).
String channelCommand(JsonObjectConst v){
  String action=v["action"]|"";
  auto id=[](const char* text,uint64_t& out){String s=text?text:"";if(s=="ALL"){out=meshmesh::Broadcast;return true;}char* e=nullptr;out=strtoull(s.c_str(),&e,16);return s.length()&&s.length()<=16&&e&&!*e&&out;};
  if(action=="add"){
    if(config.role!=RoleNormal||!meshRadio.ready)return "ERR mode: channels need the normal mode and a working radio";
    MeshRadio::ChannelResult r;uint64_t added=0;
    if(v.containsKey("hashtag"))r=meshRadio.joinHashtag(v["hashtag"]|"",&added);
    else if(v.containsKey("link"))r=meshRadio.joinLink(v["link"]|"",&added);
    else if(v.containsKey("create"))r=meshRadio.createChannel(v["create"]|"",&added);
    else if(v.containsKey("key")){uint8_t key[16];if(!channels::parseKey(v["key"]|"",key))return "ERR key: 32 hexadecimal digits or base64 of 16 bytes";r=meshRadio.addChannel(v["name"]|"",key,&added);memset(key,0,16);}
    else return "ERR channel add: hashtag, link, create or name+key";
    switch(r){
    case MeshRadio::ChannelAdded:return "OK channel added "+meshRadio.idText(added);
    case MeshRadio::ChannelExists:return "OK channel exists "+meshRadio.idText(added);
    case MeshRadio::ChannelFull:return "ERR full: "+String(channels::Max)+" channels at most, Public included";
    case MeshRadio::ChannelBadName:return "ERR name: 1-31 bytes of UTF-8";
    case MeshRadio::ChannelBadKey:return "ERR key: 32 hexadecimal digits or base64 of 16 bytes";
    case MeshRadio::ChannelBadLink:return "ERR link: meshcore://channel/add?name=...&secret=<32 hex>";
    case MeshRadio::ChannelUnavailable:return "ERR mode: channels need the normal mode and a working radio";
    default:return "ERR storage: channel not saved";
    }
  }
  if(action=="remove"){uint64_t c;if(!id(v["channel"],c))return "ERR channel ID";if(c==meshmesh::Broadcast)return "ERR public: Public stays";if(!meshRadio.channel(c))return "ERR unknown channel";if(meshRadio.sending(c))return "ERR busy: a message to this channel is being sent";return meshRadio.removeChannel(c)?"OK channel removed":"ERR storage: channel list not saved";}
  if(action=="invite"){uint64_t c,to;if(!id(v["channel"],c)||!meshRadio.channel(c))return "ERR unknown channel";if(!id(v["to"],to)||channels::isChannel(to))return "ERR node ID";
    if(channels::link(*meshRadio.channel(c),true).length()>meshRadio.messageLimit(to))return "ERR long: the channel name is too long for an invitation";
    return meshRadio.sendInvite(to,c)?"OK invitation queued":"ERR send: contact unknown, queue full or radio offline";}
  if(action=="probe"){String tag=channels::hashtag(v["hashtag"]|"");if(!tag.length())return "ERR name: 1-31 bytes of UTF-8";int n=meshRadio.probeHashtag(tag);uint8_t key[16];channels::hashtagSecret(tag,key);
    StaticJsonDocument<192>d;d["name"]=tag;char hash[3];snprintf(hash,3,"%02X",channels::hashOf(key));d["hash"]=hash;d["opened"]=n;d["samples"]=meshRadio.heardSamples;d["joined"]=meshRadio.channel(channels::idOf(key))!=nullptr;String s;serializeJson(d,s);return s;}
  return "ERR channel action add|remove|invite|probe";
}
String configJson(bool includeKey) {
  StaticJsonDocument<768> d;d["name"]=config.name;d["frequency"]=config.frequency;d["bandwidth"]=config.bandwidth;d["sf"]=config.sf;d["cr"]=config.cr;d["power"]=config.power;
  d["hops"]=config.hops;d["relay"]=config.relay;d["gps"]=config.gps;d["sound"]=config.sound;d["battery_volts"]=config.batteryVolts;d["lang"]=langCodes[config.lang<LangCount?config.lang:0];d["russian"]=config.lang==LangRu;d["brightness"]=config.brightness;if(includeKey)d["key"]=config.keyHex();
  d["auto_lock"]=config.autoLock;d["dim_after"]=config.dimAfter;
  d["utc_offset"]=config.utcOffset;
  String s;serializeJson(d,s);return s;
}
String applySettings(JsonObjectConst v) {
  if(meshRadio.busy())return "ERR radio busy; retry";
  Config next=config;
  for(JsonPairConst kv:v) {
    String name=kv.key().c_str();JsonVariantConst value=kv.value();
    if(name=="key") {if(!value.is<const char*>()||!next.setKey(value.as<String>()))return "ERR key must be 64 hexadecimal digits";}
    else if(name=="name") {if(!value.is<const char*>() || value.as<String>().length()>24 || value.as<String>().length()<1)return "ERR name: 1..24 UTF-8 bytes";strlcpy(next.name,value.as<const char*>(),sizeof(next.name));}
    else if(name=="frequency" || name=="bandwidth") {
      if(!value.is<float>() && !value.is<int>())return "ERR numeric value required";
      if(name=="frequency")next.frequency=value.as<float>();else next.bandwidth=value.as<float>();
    } else if(name=="sf"||name=="cr"||name=="power"||name=="hops"||name=="brightness") {
      if(!value.is<int>())return "ERR integer value required";int n=value.as<int>();
      if(name=="sf") {if(n<7||n>12)return "ERR SF 7..12";next.sf=n;}
      if(name=="cr") {if(n<5||n>8)return "ERR CR 5..8";next.cr=n;}
      if(name=="power") {if(n<0||n>MM_MAX_POWER)return "ERR power 0.." + String(MM_MAX_POWER) + " dBm";next.power=n;}
      if(name=="hops") {if(n<0||n>7)return "ERR hops 0..7";next.hops=n;}
      if(name=="brightness") {if(n<10||n>255)return "ERR brightness 10..255";next.brightness=n;}
    } else if(name=="utc_offset") {
      if(!value.is<int>())return "ERR integer UTC offset required";int n=value.as<int>();if(n<-720||n>840||n%15)return "ERR UTC offset minutes: -720..840, step 15";next.utcOffset=n;
    } else if(name=="auto_lock"||name=="dim_after") {
      if(!value.is<int>())return "ERR integer value required";int n=value.as<int>();
      if(n!=0 && (n<(name=="auto_lock"?30:10)||n>600))return "ERR timeout 0 or 30..600 (dim: 10..600)";
      if(name=="auto_lock")next.autoLock=n;else next.dimAfter=n;
    } else if(name=="lang") {
      int l=value.is<const char*>()?langFromCode(value.as<String>()):-1;
      if(l<0){String all;for(int i=0;i<LangCount;i++)all+=String(i?"|":"")+langCodes[i];return "ERR lang: "+all;}next.lang=l;
    } else if(name=="relay"||name=="gps"||name=="sound"||name=="russian"||name=="battery_volts") {
      if(!value.is<bool>())return "ERR boolean required";bool n=value.as<bool>();
      if(name=="relay")next.relay=n;if(name=="gps")next.gps=n;if(name=="sound")next.sound=n;if(name=="battery_volts")next.batteryVolts=n;
      if(name=="russian"&&!v.containsKey("lang")&&(n||next.lang==LangRu))next.lang=n?LangRu:LangEn; // pages from before "lang": false leaves other languages alone
    } else return "ERR unknown setting: "+name;
  }
  if(!next.valid())return "ERR invalid settings; M9 868 MHz range is 863..870";
  Config old=config;config=next;
  if(!meshRadio.applyConfig()) {config=old;meshRadio.applyConfig();return "ERR radio rejected settings; restored previous";}
  config.save();meshServer.configChanged();hardware.brightness(config.brightness);if(old.gps!=config.gps)hardware.setGps(config.gps);
  meshRadio.event="Settings saved";meshRadio.dirty=true;return "OK settings saved";
}
namespace {uint32_t restartAt=0;}
const char* roleName(uint8_t role){return role==RoleRepeater?"repeater":role==RoleRoom?"room":"normal";}
String setRole(uint8_t role){
  if(role>=RoleCount)return "ERR role normal|repeater|room";if(role==config.role)return String("OK role ")+roleName(role)+" already running";
  if(!config.saveRole(role))return "ERR role not saved";restartAt=millis()+1500;return String("OK role ")+roleName(role)+"; restarting";
}
void restartTick(){if(restartAt&&int32_t(millis()-restartAt)>=0)ESP.restart();}
String executeCommand(const String& input) {
  String line=input;line.trim();
  if(line.startsWith("map "))return maps.command(line);
#if !defined(MM_COMPACT)
  if(line=="internet"||line.startsWith("internet "))return internet.command(line);
#endif
  if(line=="chess"||line.startsWith("chess "))return chessNet.command(line);
  if(line=="ui")return uiStatus();
  if(line=="navigation")return navigation.info();
  if(line=="radar")return radar.json();
  if(line=="radar web"||line.startsWith("radar do "))return webRadarCommand(line);
  if(line=="connections")return connectionCredentials(); // Wi-Fi password and BLE PIN: USB or a paired BLE client
  if(line=="calibrate start"){if(!hardware.compassOk)return "ERR compass unavailable";navigation.start();return "OK rotate device in all directions for at least 20 seconds";}
  if(line=="calibrate finish")return navigation.finish()?"OK compass calibration saved":"ERR calibration needs 20 samples and wider rotation";
  if(line=="clock")return hardware.clockInfo();
  if(line.startsWith("clock ")){StaticJsonDocument<128>d;if(deserializeJson(d,line.substring(6))||!d["unix"].is<uint32_t>())return "ERR clock JSON unix seconds";return hardware.setUtc(d["unix"],"manual",true)?"OK UTC clock synchronized":"ERR clock range 2025..2038";}
  if(line=="status")return statusJson();
  if(line=="role")return String("{\"role\":\"")+roleName(config.role)+"\",\"roles\":[\"normal\",\"repeater\",\"room\"]}";
  if(line.startsWith("role ")){String r=line.substring(5);return setRole(r=="normal"?RoleNormal:r=="repeater"?RoleRepeater:r=="room"?RoleRoom:RoleCount);}
  // Repeater or room server: status (no passwords), its passwords, its MeshCore CLI as the local admin, a room post.
  if(line=="server")return meshServer.json();
  if(line=="server secrets")return meshServer.json(true);
  if(line.startsWith("server cli ")){if(!meshServer.running())return "ERR server role is not running";int at=input.indexOf("server cli ");return meshServer.command(input.substring(at+11));} // untrimmed: "set guest.password " clears it
  if(line.startsWith("server post ")){if(!meshServer.room())return "ERR room server role is not running";return meshServer.post(line.substring(12))?"OK post stored":"ERR post: 1-151 UTF-8 bytes";}
  if(line=="config")return configJson();
  if(line=="key")return configJson(true); // explicitly requested; never put in ordinary diagnostics
  if(line=="messages")return messagesJson();
  if(line=="nodes")return nodesJson();
  if(line=="channels")return channelsJson(true); // USB and a paired BLE client: the private links too
  if(line.startsWith("channel do ")){StaticJsonDocument<512>d;if(deserializeJson(d,line.substring(11))||!d.is<JsonObject>())return "ERR channel do {JSON}";return channelCommand(d.as<JsonObjectConst>());}
  if(line=="hello")return meshRadio.sendHello()?"OK hello queued":"ERR hello failed";
  if(line=="position")return meshRadio.sendPosition()?"OK position queued":"ERR position needs GPS fix";
  if(line=="txframe")return meshRadio.diagnosticFrame();
  if(line.startsWith("ingest ")) {
    String hex=line.substring(7);if(hex.length()%2 || hex.length()>meshmesh::MaxPacket*2)return "ERR frame size";
    uint8_t bytes[meshmesh::MaxPacket];
    for(unsigned i=0;i<hex.length()/2;i++) {String part=hex.substring(i*2,i*2+2);char* end;unsigned long v=strtoul(part.c_str(),&end,16);if(*end || !isxdigit(part[0]) || !isxdigit(part[1]))return "ERR hex";bytes[i]=v;}
    return meshRadio.diagnosticIngest(bytes,hex.length()/2)?"OK diagnostic frame accepted (USB, not RF)":"ERR diagnostic frame rejected";
  }
  if(line=="recalibrate")return meshRadio.recalibrate()?"OK radio set up again":"ERR radio busy with a packet";
  if(line=="selftest")return meshRadio.selfTest()?"OK crypto/UTF-8/tamper selftest":"ERR selftest";
  if(line=="wifi") {portalToggle();return portalActive()?"OK Wi-Fi portal on; credentials on device":"OK Wi-Fi off";}
  if(line=="ble") {bleToggle();return bleActive()?"OK BLE on":"OK BLE off";}
  if(line=="restart"){restartAt=millis()+1000;return "OK restarting";} // e.g. after fsformat: the settings are read at boot
#if !defined(MM_NRF52)
  if(line=="flashstatus"){char s[96];snprintf(s,sizeof(s),"OK flash status %04x (SR2<<8 | SR1; SR1 bits 2-6 protect blocks)",unsigned(flashStatus(false)));return s;}
#endif
  if(line=="fsformat") {
    if(hardware.fsOk)return "ERR filesystem already mounted; no format";
    if(meshRadio.busy())return "ERR radio busy";
#if defined(MM_NRF52)
    hardware.fsOk=LittleFS.format() && LittleFS.begin(false,"/littlefs",10,"littlefs");
    return hardware.fsOk?"OK MeshMesh filesystem initialized":"ERR filesystem";
#else
    String reply=formatStorage();hardware.fsOk=reply.startsWith("OK");return reply;
#endif
  }
  if(line.startsWith("resetpath ")||line.startsWith("forget ")) { // node card actions, as on the M9 screen
    bool reset=line.startsWith("resetpath ");String hex=line.substring(reset?10:7);char* end=nullptr;uint64_t id=strtoull(hex.c_str(),&end,16);
    if(!id||!end||*end||hex.length()>16)return "ERR node ID";if(meshRadio.busy())return "ERR radio busy; retry";
    if(reset)return meshRadio.resetPath(id)?"OK path reset; next message floods":"ERR path reset failed";
    return meshRadio.removeContact(id)?"OK contact removed; its next advert adds it again":"ERR contact not removed";
  }
  // The app over USB or BLE: text with line breaks, which a command line cannot carry.
  if(line.startsWith("sendjson ")){StaticJsonDocument<1024>d;if(deserializeJson(d,line.substring(9))||!d["to"].is<const char*>()||!d["text"].is<const char*>())return "ERR sendjson {\"to\":\"ALL|NODE_ID\",\"text\":\"...\"}";return executeCommand("send "+d["to"].as<String>()+" "+d["text"].as<String>());}
  if(line.startsWith("send ")) {
    int at=line.indexOf(' ',5);if(at<0)return "ERR send ALL|NODE_ID text";
    String to=line.substring(5,at);uint64_t id=meshmesh::Broadcast;
    if(to!="ALL") {char* end=nullptr;id=strtoull(to.c_str(),&end,16);if(!id || !end || *end || to.length()>16)return "ERR node ID";}
    return meshRadio.sendMessage(line.substring(at+1),id)?"OK message queued":"ERR message rejected";
  }
  if(line.startsWith("set ")) {
    StaticJsonDocument<1024> d;if(deserializeJson(d,line.substring(4)) || !d.is<JsonObject>())return "ERR set {JSON object}";
    return applySettings(d.as<JsonObjectConst>());
  }
  return "Commands: status, role, role normal|repeater|room, server, server secrets, server cli TEXT, server post TEXT, config, key, connections, messages, radar, radar web, radar do {JSON}, set {JSON}, send ALL|NODE_ID|CHANNEL_ID text, sendjson {JSON}, channels, channel do {JSON}, chess, hello, position, resetpath NODE_ID, forget NODE_ID, selftest, wifi, internet, ble, recalibrate, fsformat, restart";
}
