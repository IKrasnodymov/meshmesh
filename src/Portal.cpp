#include "App.h"
#include "Pet.h"
#include "Dice.h"
#include "MeshRadio.h"
#include "BleDiagnostics.h"
#include "Maps.h"
#include "Hardware.h"
#include "Power.h"
#include "Navigation.h"
#include "PortalPage.h"
#include "WifiDiagnostics.h"
#include "Radar.h"
#include "Internet.h"
#include "ChessNet.h"
#include "ChessTour.h"
#include "Companion.h"
#include <WiFi.h>
#include <WebServer.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include <bootloader_random.h>
#include <mbedtls/base64.h>
#include "nimble/porting/nimble/include/os/os_mbuf.h"

namespace {
WebServer server(80);
bool wifiOn=false,bluetoothOn=false;
// The page is served on the access point and, while the Wi-Fi client is online, on the home network.
bool serving=false;
String password;
uint32_t pinCode=123456;
NimBLECharacteristic* bleTx=nullptr;
QueueHandle_t commands=nullptr;
String bleResponse;
unsigned bleOffset=0;
uint32_t nextNotification=0;
struct BleCommand {char text[256];};
// MeshCore companion apps: the Nordic UART service of stock MeshCore, one frame per write and notification.
const char* const NusService="6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
NimBLECharacteristic* appTx=nullptr;
QueueHandle_t appFrames=nullptr;
struct AppFrame {uint8_t length;uint8_t data[companion::MaxFrame];};
uint32_t nextAppNotification=0;
// The web radar page holds the radar while it polls; the screen may hold it too (uiRadarPage).
bool webRadar=false;uint32_t webRadarAt=0;
bool wifiOffPending=false; // the web page turns the access point off after its reply is sent
void webRadarRelease(){if(!webRadar)return;webRadar=false;if(!uiRadarPage())radar.close();}
int radarIndex(uint32_t ref,const String& kind){const char* kinds[]={"wifi","ble","lora"};for(unsigned i=0;i<radar.count;i++)if(Radar::placement(radar.targets[i])==ref&&kind==kinds[radar.targets[i].kind])return i;return -1;}
String radarAction(JsonObjectConst v){
  String action=v["action"]|"";
  if(action=="close"){webRadarRelease();return "OK radar released";}
  if(!radar.active)return "ERR radar is not open";
  if(action=="track"){int i=radarIndex(v["ref"]|0u,v["kind"]|"");if(i<0||!radar.track(i))return "ERR signal is gone";return "OK homing";}
  if(action=="untrack"){radar.untrack();return "OK homing stopped";}
  if(action=="peak"){radar.resetPeak();return "OK peak reset";}
  if(action=="csi"){String role=v["role"]|"";if(role!="off"&&role!="beacon"&&role!="sensor")return "ERR CSI role off|beacon|sensor";radar.setCsi(role=="beacon"?Radar::CsiBeacon:role=="sensor"?Radar::CsiSensor:Radar::CsiOff);return "OK CSI "+role;}
  if(action=="calibrate"){if(!radar.beaconHeard())return "ERR needs a heard beacon";radar.calibrate();return "OK calibrating 10 s";}
  return "ERR radar action";
}
bool authorized() {
  if(server.authenticate("meshmesh",password.c_str()))return true;
  server.send(401,"text/plain","Authentication required");return false;
}
void answer(const String& value,bool success=true) {server.send(success?200:400,"application/json",value);}
class BleCallbacks:public NimBLECharacteristicCallbacks {
 void onWrite(NimBLECharacteristic* c) override {
   auto value=c->getValue();if(value.size()>255 || !commands)return;
   BleCommand command{};memcpy(command.text,value.data(),value.size());xQueueSend(commands,&command,0);
 }
};
BleCallbacks bleCallbacks;
class AppCallbacks:public NimBLECharacteristicCallbacks {
 void onWrite(NimBLECharacteristic* c) override {
   auto value=c->getValue();if(!value.size()||value.size()>companion::MaxFrame||!appFrames)return;
   AppFrame frame{};frame.length=value.size();memcpy(frame.data,value.data(),value.size());xQueueSend(appFrames,&frame,0);
 }
};
AppCallbacks appCallbacks;
// The advert of stock MeshCore (its service, "MeshCore-<name>" in the scan response): MeshCore apps find the board
// by the name. The MeshMesh app finds it by the "MM" mark in the manufacturer data (company ID 0xFFFF: no company);
// our own service still answers.
void advertise(){
  String name=bleName();NimBLEDevice::setDeviceName(name.c_str());
  NimBLEAdvertisementData adv,scan;adv.setFlags(BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP);
  adv.setCompleteServices(NimBLEUUID(NusService));adv.setManufacturerData(std::string("\xff\xffMM",4));scan.setName(name.c_str());
  auto* a=NimBLEDevice::getAdvertising();a->setAdvertisementData(adv);a->setScanResponseData(scan);
}
}
String bleName(){
  String name=config.name;while(name.length()>20){unsigned cut=name.length()-1;while(cut&&(uint8_t(name[cut])&0xc0)==0x80)cut--;name.remove(cut);} // the scan response holds 29 bytes
  return "MeshCore-"+name;
}
bool webRadarActive() {return webRadar;}
// The page's radar over USB or BLE (the Android app): the same hold, JSON and actions as /api/radar.
// Unlike the diagnostic "radar", it carries network and device names, as the web page does.
String webRadarCommand(const String& line){
  if(line=="radar web"){if(!radar.active)radar.open();webRadar=true;webRadarAt=millis();return radar.webJson();}
  StaticJsonDocument<256>d;if(deserializeJson(d,line.substring(9))||!d.is<JsonObject>())return "ERR radar do {JSON}";
  if(webRadar)webRadarAt=millis();return radarAction(d.as<JsonObjectConst>());
}
bool portalActive() {return wifiOn;}String portalPassword() {return password;}
bool bleActive() {return bluetoothOn;}
uint32_t blePin() {return pinCode;}
String connectionCredentials() {
  StaticJsonDocument<512> d;d["wifi"]=wifiOn;d["ssid"]="MM-"+meshRadio.idText(meshRadio.nodeId).substring(6);
  d["password"]=password;d["ip"]="192.168.4.1";if(internet.online())d["lan_ip"]=internet.address();d["ble"]=bluetoothOn;
  d["ble_name"]=bleName();d["pin"]=pinCode;
  if(bluetoothOn)d["ble_address"]=NimBLEDevice::getAddress().toString().c_str();
  String result;serializeJson(d,result);return result;
}
void portalBegin() {
  uint8_t entropy[12];bootloader_random_enable();esp_fill_random(entropy,sizeof(entropy));bootloader_random_disable();
  const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for(int i=0;i<12;i++) password+=alphabet[entropy[i]%32];pinCode=config.blePin;
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",portalPage);});
  server.on("/api/clock",HTTP_GET,[]{if(authorized())answer(hardware.clockInfo());});
  server.on("/api/status",HTTP_GET,[]{if(authorized())answer(statusJson());});
  server.on("/api/nodes",HTTP_GET,[]{if(authorized())answer(nodesJson());});
  server.on("/api/navigation",HTTP_GET,[]{if(authorized())answer(navigation.info());});
  server.on("/api/maps/areas",HTTP_GET,[]{if(authorized())answer(maps.areas());});
  server.on("/api/maps",HTTP_GET,[]{if(authorized())answer(maps.info());});
  server.on("/api/maps/chunk",HTTP_POST,[]{if(!authorized())return;DynamicJsonDocument d(4096);if(deserializeJson(d,server.arg("plain"))||!d["data"].is<const char*>()){answer("ERR map chunk JSON",false);return;}String encoded=d["data"];uint8_t bytes[2048];size_t n=0;if(mbedtls_base64_decode(bytes,sizeof(bytes),&n,(const uint8_t*)encoded.c_str(),encoded.length())){answer("ERR map base64",false);return;}bool ok=maps.uploadChunk(bytes,n);answer(ok?"OK map chunk":"ERR "+maps.error,ok);});
  server.on("/api/maps/tile",HTTP_GET,[]{if(!authorized())return;if(!server.hasArg("z")||!server.hasArg("x")||!server.hasArg("y")){answer("ERR tile coordinates",false);return;}File f=maps.openTile(server.arg("z").toInt(),server.arg("x").toInt(),server.arg("y").toInt());if(!f){server.send(404,"text/plain","Map tile not saved");return;}server.streamFile(f,"application/octet-stream");f.close();});
  server.on("/api/connections",HTTP_GET,[]{if(authorized())answer(connectionCredentials());});
  server.on("/api/radar",HTTP_GET,[]{if(!authorized())return;if(server.arg("open")=="1"){if(!radar.active)radar.open();webRadar=true;webRadarAt=millis();}answer(radar.webJson());});
  server.on("/api/radar",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<256>d;if(deserializeJson(d,server.arg("plain"))||!d.is<JsonObject>()){answer("Invalid JSON",false);return;}if(webRadar)webRadarAt=millis();String reply=radarAction(d.as<JsonObjectConst>());answer(reply,reply.startsWith("OK"));});
  // Chess: the list with the latest news, or one game (?id=3F2A); moves go through /api/command.
  server.on("/api/pet",HTTP_GET,[]{if(authorized())answer(creature.json());}); // actions through /api/command ("pet ...")
  server.on("/api/dice",HTTP_GET,[]{if(authorized())answer(dicer.json());}); // actions through /api/command ("dice ...")
  server.on("/api/tour",HTTP_GET,[]{if(!authorized())return;answer(tour::net.json());});
  server.on("/api/chess",HTTP_GET,[]{if(!authorized())return;if(server.hasArg("rating")){answer(chessNet.command("chess rating"));return;}if(!server.hasArg("id")){answer(chessNet.web());return;}char* e=nullptr;unsigned long id=strtoul(server.arg("id").c_str(),&e,16);ChessMatch* m=id&&e&&!*e?chessNet.find(uint16_t(id)):nullptr;if(!m){answer("Unknown game",false);return;}answer(chessNet.detail(*m));});
  // Channels: the private keys only over the access point, as /api/key (the home network carries plain HTTP).
  server.on("/api/channels",HTTP_GET,[]{if(authorized())answer(channelsJson(wifiOn));});
  server.on("/api/channels",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<512>d;if(deserializeJson(d,server.arg("plain"))||!d.is<JsonObject>()){answer("Invalid JSON",false);return;}String reply=channelCommand(d.as<JsonObjectConst>());answer(reply,!reply.startsWith("ERR"));});
  server.on("/api/messages",HTTP_GET,[]{if(authorized())answer(messagesJson());});
  server.on("/api/config",HTTP_GET,[]{if(authorized())answer(configJson());});
  // The private key only over the access point: on the home network the page is plain HTTP.
  server.on("/api/key",HTTP_GET,[]{if(!authorized())return;if(!wifiOn){answer("ERR the key is given out on the device access point only",false);return;}answer(configJson(true));});
  server.on("/api/config",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<1024>d;if(deserializeJson(d,server.arg("plain"))||!d.is<JsonObject>()){answer("Invalid JSON",false);return;}String reply=applySettings(d.as<JsonObjectConst>());answer(reply,reply.startsWith("OK"));});
  server.on("/api/command",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<2048>d;if(deserializeJson(d,server.arg("plain"))||!d["command"].is<const char*>()){answer("Invalid command",false);return;}if(d["command"]=="wifi"){wifiOffPending=true;answer("OK Wi-Fi off after this reply");return;}String reply=executeCommand(d["command"].as<String>());answer(reply,!reply.startsWith("ERR"));});
  server.on("/api/send",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<1024>d;if(deserializeJson(d,server.arg("plain"))||!d["text"].is<const char*>()||!d["to"].is<const char*>()){answer("Invalid message",false);return;}String reply=executeCommand("send "+d["to"].as<String>()+" "+d["text"].as<String>());answer(reply,reply.startsWith("OK"));});
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
}
void portalToggle() {
  if(wifiProbeActive()){meshRadio.event="Wi-Fi probe busy";meshRadio.dirty=true;return;}
  radar.release(); // the radar stops its Wi-Fi use (sweeps, homing, CSI beacon on the access point)
  if(wifiOn) {server.stop();serving=false;WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);wifiOn=false;webRadarRelease();meshRadio.event="Wi-Fi off";}
  else {
    server.stop();serving=false; // portalTick starts it again on the access point
    internet.yieldRadio(); // the Wi-Fi client resumes when the access point is off
    String ssid="MM-"+meshRadio.idText(meshRadio.nodeId).substring(6);WiFi.mode(WIFI_AP);
    wifiOn=WiFi.softAP(ssid.c_str(),password.c_str(),1,false,2);meshRadio.event=wifiOn?"Wi-Fi: 192.168.4.1":"Wi-Fi failed";
  }
  meshRadio.dirty=true;
}
void bleToggle() {
  if(bleProbeActive()) {meshRadio.event="BLE probe busy";meshRadio.dirty=true;return;}
  radar.releaseBle(); // stops the radar scan; it scans on the stack again next tick
  // The Bluetooth controller starts once per boot and is never deinitialised: a deinit and a later
  // init left Wi-Fi CSI data frozen until reboot (ESP32-S3, IDF 4.4). "Off" means no advertising,
  // no connections and no service commands.
  NimBLEServer* b=NimBLEDevice::getInitialized()?NimBLEDevice::getServer():nullptr;
  if(bluetoothOn) {
    if(b){b->advertiseOnDisconnect(false);NimBLEDevice::getAdvertising()->stop();for(uint16_t id:b->getPeerDevices())b->disconnect(id);}
    bluetoothOn=false;bleResponse="";bleOffset=0;if(commands){vQueueDelete(commands);commands=nullptr;}
    if(appFrames){vQueueDelete(appFrames);appFrames=nullptr;}companion::disconnected(companion::LinkBle);meshRadio.event="BLE off";
  }
  else {
    commands=xQueueCreate(4,sizeof(BleCommand));appFrames=xQueueCreate(4,sizeof(AppFrame));
    if(!commands||!appFrames){if(commands)vQueueDelete(commands);if(appFrames)vQueueDelete(appFrames);commands=nullptr;appFrames=nullptr;meshRadio.event="BLE: insufficient RAM";return;}
    String name="MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6);NimBLEDevice::init(name.c_str());NimBLEDevice::setDeviceName(name.c_str()); // the radar may have started the stack unnamed
    NimBLEDevice::setSecurityAuth(true,true,true);NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);NimBLEDevice::setSecurityPasskey(pinCode);
    if(!b){
      b=NimBLEDevice::createServer();NimBLEService* s=b->createService("7a9e0001-98bd-4d56-89a8-c4eab4179010");
      bleTx=s->createCharacteristic("7a9e0003-98bd-4d56-89a8-c4eab4179010",NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::READ_AUTHEN|NIMBLE_PROPERTY::NOTIFY);
      auto rx=s->createCharacteristic("7a9e0002-98bd-4d56-89a8-c4eab4179010",NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_AUTHEN);
      rx->setCallbacks(&bleCallbacks);s->start();
      NimBLEService* nus=b->createService(NusService);
      appTx=nus->createCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E",NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::READ_AUTHEN|NIMBLE_PROPERTY::NOTIFY);
      auto appRx=nus->createCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_AUTHEN);
      appRx->setCallbacks(&appCallbacks);nus->start();
    }
    advertise();b->advertiseOnDisconnect(true);NimBLEDevice::getAdvertising()->start();bluetoothOn=true;meshRadio.event="BLE PIN: "+String(pinCode);
  }
  config.saveBle(bluetoothOn);meshRadio.dirty=true;
}
void portalTick() {
  bool serve=wifiOn||internet.online();
  if(serve!=serving){if(serve)server.begin();else server.stop();serving=serve;}
  if(serving)server.handleClient();
  if(wifiOffPending){wifiOffPending=false;if(wifiOn)portalToggle();}
  if(webRadar&&millis()-webRadarAt>10000)webRadarRelease(); // the page was closed or the phone left
  if(commands && !bleResponse.length()) {
    BleCommand cmd;
    if(xQueueReceive(commands,&cmd,0)==pdTRUE) {
      powerWake();String response=executeCommand(cmd.text);
      if(bleTx) {bleResponse=response+'\n';bleOffset=0;nextNotification=millis();}
    }
  }
  // Companion frames: one per write, one per notification, while the host has spare buffers.
  if(appFrames){AppFrame frame;if(xQueueReceive(appFrames,&frame,0)==pdTRUE){powerWake();companion::command(frame.data,frame.length,companion::LinkBle);}}
  if(appTx&&companion::connected()&&!appTx->getSubscribedCount())companion::disconnected(companion::LinkBle);
  else if(appTx&&int32_t(millis()-nextAppNotification)>=0&&os_msys_num_free()>=6){
    uint8_t frame[companion::MaxFrame];size_t length=companion::next(frame,companion::LinkBle);
    if(length){appTx->notify(frame,length);nextAppNotification=millis()+(length>20?8:15);}
  }
  if(bleTx && bleResponse.length() && int32_t(millis()-nextNotification)>=0) {
    if(!bleTx->getSubscribedCount()) {bleResponse="";bleOffset=0;return;}
    // Notifications as large as the smallest negotiated MTU allows (20 bytes without an exchange),
    // sent only while the host has spare buffers: a dropped notification would corrupt the reply.
    if(os_msys_num_free()<6){nextNotification=millis()+4;return;}
    unsigned room=244;NimBLEServer* s=NimBLEDevice::getServer();
    for(uint16_t id:s->getPeerDevices()){unsigned mtu=s->getPeerMTU(id);if(mtu>=23&&mtu-3<room)room=mtu-3;}
    unsigned length=min(room,bleResponse.length()-bleOffset);
    bleTx->notify((const uint8_t*)bleResponse.c_str()+bleOffset,length);bleOffset+=length;nextNotification=millis()+(length>20?8:15);
    if(bleOffset==bleResponse.length()) {bleResponse="";bleOffset=0;}
  }
}
