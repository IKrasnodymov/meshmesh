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
#include <LittleFS.h>
#include <time.h>
String statusJson() {
  StaticJsonDocument<3072> d;
#if defined(MM_HELTEC_V4)
  d["board"]="heltec_v4";
#else
  d["board"]="m9";
#endif
  d["firmware"]=MESHMM_FIRMWARE;d["node"]=meshRadio.idText(meshRadio.nodeId);d["name"]=config.name;d["network"]=meshRadio.networkId;
  char buildHash[65];mesh::Utils::toHex(buildHash,esp_ota_get_app_description()->app_elf_sha256,32);d["build_sha256"]=buildHash;d["protocol"]="MeshCore";d["public_key"]=meshRadio.publicKeyText();d["channel"]="Public";d["public_message_limit"]=meshRadio.messageLimit();d["unix_time"]=int64_t(time(nullptr));d["clock_source"]=hardware.clockSource;d["clock_conflict"]=hardware.clockConflict;d["uptime"]=millis()/1000;d["boot"]=config.bootCounter;d["reset_reason"]=int(esp_reset_reason());d["heap"]=ESP.getFreeHeap();d["psram"]=ESP.getFreePsram();
  d["radio"]=meshRadio.ready;d["radio_error"]=meshRadio.radioError;d["tx"]=meshRadio.txCount;d["rx"]=meshRadio.rxCount;d["rejected"]=meshRadio.rejected;d["relayed"]=meshRadio.relayed;d["contacts_replaced"]=meshRadio.replaced;
  d["diagnostic_rx"]=meshRadio.diagnosticRx;d["rssi"]=meshRadio.lastRssi;d["snr"]=meshRadio.lastSnr;d["keyboard"]=hardware.keyboardOk;d["key_count"]=hardware.keyCount;d["last_key"]=hardware.lastKey;
  d["battery_mv"]=hardware.batteryMv;d["sd"]=hardware.sdOk;d["storage"]=hardware.fsOk;d["rtc"]=hardware.rtcOk;d["rtc_valid"]=hardware.rtcValid;
  d["compass"]=hardware.compassOk;d["compass_sample"]=hardware.compassSample;d["imu"]=hardware.imuOk;d["imu_sample"]=hardware.imuSample;
  d["gps_bytes"]=hardware.gpsBytes;d["gps_sentences"]=hardware.gps.passedChecksum();d["gps_fix"]=hardware.gpsFix();d["satellites"]=hardware.gps.satellites.value();
  if(d["gps_fix"].as<bool>()) {d["latitude"]=hardware.gps.location.lat();d["longitude"]=hardware.gps.location.lng();}
  JsonArray a=d.createNestedArray("mag");for(float n:hardware.mag)a.add(n);a=d.createNestedArray("accel");for(float n:hardware.accel)a.add(n);
  d["clock_trusted"]=hardware.clockTrusted;d["busy"]=meshRadio.busy();if(meshRadio.lastRxAt)d["rx_age"]=(millis()-meshRadio.lastRxAt)/1000;
  d["event"]=meshRadio.event;d["wifi"]=portalActive();d["internet"]=internet.online();d["ble"]=bleActive();String s;serializeJson(d,s);return s;
}
String messagesJson() {
  DynamicJsonDocument d(32768);JsonArray a=d.to<JsonArray>();
  for(unsigned i=0;i<meshRadio.historyCount;i++) {const auto& m=meshRadio.history[i];JsonObject j=a.createNestedObject();j["protocol"]=m.protocol;j["source"]=meshRadio.idText(m.source);j["destination"]=meshRadio.idText(m.destination);j["session"]=m.session;j["id"]=m.id;j["name"]=m.name;j["text"]=m.text;j["time"]=m.timestamp;j["outgoing"]=m.outgoing;j["status"]=int(m.status);
   if(m.route){j["route"]=m.route==ChatMessage::RouteDirect?"direct":"flood";if(m.hops!=255)j["hops"]=m.hops;if(m.tries)j["tries"]=m.tries;}}
  String s;serializeJson(d,s);return s;
}
String nodesJson(){DynamicJsonDocument d(16384);JsonArray a=d.to<JsonArray>();for(unsigned i=0;i<meshRadio.peerCount;i++){auto& p=meshRadio.peers[i];JsonObject j=a.createNestedObject();char key[65];mesh::Utils::toHex(key,p.publicKey,32);j["public_key"]=key;j["type"]=p.type;j["heard"]=p.heard;j["path_length"]=p.pathLength;j["id"]=meshRadio.idText(p.id);j["name"]=p.name;j["rssi"]=p.rssi;j["snr"]=p.snr;if(p.heard)j["age_seconds"]=(millis()-p.seen)/1000;else j["age_seconds"]=nullptr;j["hops"]=p.hops;j["position"]=p.position;if(p.position){j["latitude"]=p.latitude;j["longitude"]=p.longitude;}}String s;serializeJson(d,s);return s;}
String configJson(bool includeKey) {
  StaticJsonDocument<768> d;d["name"]=config.name;d["frequency"]=config.frequency;d["bandwidth"]=config.bandwidth;d["sf"]=config.sf;d["cr"]=config.cr;d["power"]=config.power;
  d["hops"]=config.hops;d["relay"]=config.relay;d["gps"]=config.gps;d["sound"]=config.sound;d["battery_volts"]=config.batteryVolts;d["russian"]=config.russian;d["brightness"]=config.brightness;if(includeKey)d["key"]=config.keyHex();
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
      if(name=="power") {if(n<0||n>22)return "ERR power 0..22 dBm";next.power=n;}
      if(name=="hops") {if(n<0||n>7)return "ERR hops 0..7";next.hops=n;}
      if(name=="brightness") {if(n<10||n>255)return "ERR brightness 10..255";next.brightness=n;}
    } else if(name=="utc_offset") {
      if(!value.is<int>())return "ERR integer UTC offset required";int n=value.as<int>();if(n<-720||n>840||n%15)return "ERR UTC offset minutes: -720..840, step 15";next.utcOffset=n;
    } else if(name=="auto_lock"||name=="dim_after") {
      if(!value.is<int>())return "ERR integer value required";int n=value.as<int>();
      if(n!=0 && (n<(name=="auto_lock"?30:10)||n>600))return "ERR timeout 0 or 30..600 (dim: 10..600)";
      if(name=="auto_lock")next.autoLock=n;else next.dimAfter=n;
    } else if(name=="relay"||name=="gps"||name=="sound"||name=="russian"||name=="battery_volts") {
      if(!value.is<bool>())return "ERR boolean required";bool n=value.as<bool>();
      if(name=="relay")next.relay=n;if(name=="gps")next.gps=n;if(name=="sound")next.sound=n;if(name=="russian")next.russian=n;if(name=="battery_volts")next.batteryVolts=n;
    } else return "ERR unknown setting: "+name;
  }
  if(!next.valid())return "ERR invalid settings; M9 868 MHz range is 863..870";
  Config old=config;config=next;
  if(!meshRadio.applyConfig()) {config=old;meshRadio.applyConfig();return "ERR radio rejected settings; restored previous";}
  config.save();hardware.brightness(config.brightness);if(old.gps!=config.gps)hardware.setGps(config.gps);
  meshRadio.event="Settings saved";meshRadio.dirty=true;return "OK settings saved";
}
String executeCommand(const String& input) {
  String line=input;line.trim();
  if(line.startsWith("map "))return maps.command(line);
#if !defined(MM_HELTEC_V4)
  if(line=="internet"||line.startsWith("internet "))return internet.command(line);
#endif
  if(line=="chess"||line.startsWith("chess "))return chessNet.command(line);
  if(line=="ui")return uiStatus();
  if(line=="navigation")return navigation.info();
  if(line=="radar")return radar.json();
  if(line=="calibrate start"){if(!hardware.compassOk)return "ERR compass unavailable";navigation.start();return "OK rotate device in all directions for at least 20 seconds";}
  if(line=="calibrate finish")return navigation.finish()?"OK compass calibration saved":"ERR calibration needs 20 samples and wider rotation";
  if(line=="clock")return hardware.clockInfo();
  if(line.startsWith("clock ")){StaticJsonDocument<128>d;if(deserializeJson(d,line.substring(6))||!d["unix"].is<uint32_t>())return "ERR clock JSON unix seconds";return hardware.setUtc(d["unix"],"manual",true)?"OK UTC clock synchronized":"ERR clock range 2025..2038";}
  if(line=="status")return statusJson();
  if(line=="config")return configJson();
  if(line=="key")return configJson(true); // explicitly requested; never put in ordinary diagnostics
  if(line=="messages")return messagesJson();
  if(line=="nodes")return nodesJson();
  if(line=="hello")return meshRadio.sendHello()?"OK hello queued":"ERR hello failed";
  if(line=="position")return meshRadio.sendPosition()?"OK position queued":"ERR position needs GPS fix";
  if(line=="txframe")return meshRadio.diagnosticFrame();
  if(line.startsWith("ingest ")) {
    String hex=line.substring(7);if(hex.length()%2 || hex.length()>meshmesh::MaxPacket*2)return "ERR frame size";
    uint8_t bytes[meshmesh::MaxPacket];
    for(unsigned i=0;i<hex.length()/2;i++) {String part=hex.substring(i*2,i*2+2);char* end;unsigned long v=strtoul(part.c_str(),&end,16);if(*end || !isxdigit(part[0]) || !isxdigit(part[1]))return "ERR hex";bytes[i]=v;}
    return meshRadio.diagnosticIngest(bytes,hex.length()/2)?"OK diagnostic frame accepted (USB, not RF)":"ERR diagnostic frame rejected";
  }
  if(line=="selftest")return meshRadio.selfTest()?"OK crypto/UTF-8/tamper selftest":"ERR selftest";
  if(line=="wifi") {portalToggle();return portalActive()?"OK Wi-Fi portal on; credentials on device":"OK Wi-Fi off";}
  if(line=="ble") {bleToggle();return bleActive()?"OK BLE on":"OK BLE off";}
  if(line=="fsformat") {
    if(hardware.fsOk)return "ERR filesystem already mounted; no format";
    if(meshRadio.busy())return "ERR radio busy";
    hardware.fsOk=LittleFS.format() && LittleFS.begin(false,"/littlefs",10,"littlefs");
    return hardware.fsOk?"OK MeshMesh filesystem initialized":"ERR filesystem";
  }
  if(line.startsWith("resetpath ")||line.startsWith("forget ")) { // node card actions, as on the M9 screen
    bool reset=line.startsWith("resetpath ");String hex=line.substring(reset?10:7);char* end=nullptr;uint64_t id=strtoull(hex.c_str(),&end,16);
    if(!id||!end||*end||hex.length()>16)return "ERR node ID";if(meshRadio.busy())return "ERR radio busy; retry";
    if(reset)return meshRadio.resetPath(id)?"OK path reset; next message floods":"ERR path reset failed";
    return meshRadio.removeContact(id)?"OK contact removed; its next advert adds it again":"ERR contact not removed";
  }
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
  return "Commands: status, config, key, messages, radar, set {JSON}, send ALL|NODE_ID text, chess, hello, position, resetpath NODE_ID, forget NODE_ID, selftest, wifi, internet, ble, fsformat";
}
