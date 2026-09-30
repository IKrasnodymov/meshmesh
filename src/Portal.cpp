#include "App.h"
#include "MeshRadio.h"
#include "BleDiagnostics.h"
#include "Maps.h"
#include "Hardware.h"
#include "Navigation.h"
#include "PortalPage.h"
#include "WifiDiagnostics.h"
#include <WiFi.h>
#include <WebServer.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include <bootloader_random.h>
#include <mbedtls/base64.h>

namespace {
WebServer server(80);
bool wifiOn=false,bluetoothOn=false;
String password;
uint32_t pinCode=123456;
NimBLECharacteristic* bleTx=nullptr;
QueueHandle_t commands=nullptr;
String bleResponse;
unsigned bleOffset=0;
uint32_t nextNotification=0;
struct BleCommand {char text[256];};
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
}
bool portalActive() {return wifiOn;}String portalPassword() {return password;}
bool bleActive() {return bluetoothOn;}
uint32_t blePin() {return pinCode;}
String connectionCredentials() {
  StaticJsonDocument<512> d;d["wifi"]=wifiOn;d["ssid"]="MM-"+meshRadio.idText(meshRadio.nodeId).substring(6);
  d["password"]=password;d["ip"]="192.168.4.1";d["ble"]=bluetoothOn;
  d["ble_name"]="MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6);d["pin"]=pinCode;
  if(bluetoothOn)d["ble_address"]=NimBLEDevice::getAddress().toString().c_str();
  String result;serializeJson(d,result);return result;
}
void portalBegin() {
  uint8_t entropy[12];bootloader_random_enable();esp_fill_random(entropy,sizeof(entropy));bootloader_random_disable();
  const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for(int i=0;i<12;i++) password+=alphabet[entropy[i]%32];pinCode=100000+(uint32_t(entropy[0])<<16|uint32_t(entropy[1])<<8|entropy[2])%900000;
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",portalPage);});
  server.on("/api/clock",HTTP_GET,[]{if(authorized())answer(hardware.clockInfo());});
  server.on("/api/status",HTTP_GET,[]{if(authorized())answer(statusJson());});
  server.on("/api/nodes",HTTP_GET,[]{if(authorized())answer(nodesJson());});
  server.on("/api/navigation",HTTP_GET,[]{if(authorized())answer(navigation.info());});
  server.on("/api/maps/areas",HTTP_GET,[]{if(authorized())answer(maps.areas());});
  server.on("/api/maps",HTTP_GET,[]{if(authorized())answer(maps.info());});
  server.on("/api/maps/chunk",HTTP_POST,[]{if(!authorized())return;DynamicJsonDocument d(4096);if(deserializeJson(d,server.arg("plain"))||!d["data"].is<const char*>()){answer("ERR map chunk JSON",false);return;}String encoded=d["data"];uint8_t bytes[2048];size_t n=0;if(mbedtls_base64_decode(bytes,sizeof(bytes),&n,(const uint8_t*)encoded.c_str(),encoded.length())){answer("ERR map base64",false);return;}bool ok=maps.uploadChunk(bytes,n);answer(ok?"OK map chunk":"ERR "+maps.error,ok);});
  server.on("/api/maps/tile",HTTP_GET,[]{if(!authorized())return;if(!server.hasArg("z")||!server.hasArg("x")||!server.hasArg("y")){answer("ERR tile coordinates",false);return;}File f=maps.openTile(server.arg("z").toInt(),server.arg("x").toInt(),server.arg("y").toInt());if(!f){server.send(404,"text/plain","Map tile not saved");return;}server.streamFile(f,"application/octet-stream");f.close();});
  server.on("/api/messages",HTTP_GET,[]{if(authorized())answer(messagesJson());});
  server.on("/api/config",HTTP_GET,[]{if(authorized())answer(configJson());});
  server.on("/api/key",HTTP_GET,[]{if(authorized())answer(configJson(true));});
  server.on("/api/config",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<1024>d;if(deserializeJson(d,server.arg("plain"))||!d.is<JsonObject>()){answer("Invalid JSON",false);return;}String reply=applySettings(d.as<JsonObjectConst>());answer(reply,reply.startsWith("OK"));});
  server.on("/api/command",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<2048>d;if(deserializeJson(d,server.arg("plain"))||!d["command"].is<const char*>()){answer("Invalid command",false);return;}String reply=executeCommand(d["command"].as<String>());answer(reply,!reply.startsWith("ERR"));});
  server.on("/api/send",HTTP_POST,[]{if(!authorized())return;StaticJsonDocument<1024>d;if(deserializeJson(d,server.arg("plain"))||!d["text"].is<const char*>()||!d["to"].is<const char*>()){answer("Invalid message",false);return;}String reply=executeCommand("send "+d["to"].as<String>()+" "+d["text"].as<String>());answer(reply,reply.startsWith("OK"));});
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
}
void portalToggle() {
  if(wifiProbeActive()){meshRadio.event="Wi-Fi probe busy";meshRadio.dirty=true;return;}
  if(wifiOn) {server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);wifiOn=false;meshRadio.event="Wi-Fi off";}
  else {
    String ssid="MM-"+meshRadio.idText(meshRadio.nodeId).substring(6);WiFi.mode(WIFI_AP);
    wifiOn=WiFi.softAP(ssid.c_str(),password.c_str(),1,false,2);if(wifiOn)server.begin();meshRadio.event=wifiOn?"Wi-Fi: 192.168.4.1":"Wi-Fi failed";
  }
  meshRadio.dirty=true;
}
void bleToggle() {
  if(bleProbeActive()) {meshRadio.event="BLE probe busy";meshRadio.dirty=true;return;}
  if(bluetoothOn) {NimBLEDevice::deinit(true);bluetoothOn=false;bleTx=nullptr;bleResponse="";bleOffset=0;if(commands){vQueueDelete(commands);commands=nullptr;}meshRadio.event="BLE off";}
  else {
    commands=xQueueCreate(4,sizeof(BleCommand));
    if(!commands){meshRadio.event="BLE: insufficient RAM";return;}
    String name="MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6);NimBLEDevice::init(name.c_str());
    NimBLEDevice::setSecurityAuth(true,true,true);NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);NimBLEDevice::setSecurityPasskey(pinCode);
    NimBLEServer* b=NimBLEDevice::createServer();NimBLEService* s=b->createService("7a9e0001-98bd-4d56-89a8-c4eab4179010");
    bleTx=s->createCharacteristic("7a9e0003-98bd-4d56-89a8-c4eab4179010",NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::READ_AUTHEN|NIMBLE_PROPERTY::NOTIFY);
    auto rx=s->createCharacteristic("7a9e0002-98bd-4d56-89a8-c4eab4179010",NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_AUTHEN);
    rx->setCallbacks(&bleCallbacks);s->start();NimBLEDevice::getAdvertising()->addServiceUUID(s->getUUID());NimBLEDevice::getAdvertising()->start();bluetoothOn=true;meshRadio.event="BLE PIN: "+String(pinCode);
  }
  meshRadio.dirty=true;
}
void portalTick() {
  if(wifiOn)server.handleClient();
  if(commands && !bleResponse.length()) {
    BleCommand cmd;
    if(xQueueReceive(commands,&cmd,0)==pdTRUE) {
      String response=executeCommand(cmd.text);
      if(bleTx) {bleResponse=response+'\n';bleOffset=0;nextNotification=millis();}
    }
  }
  if(bleTx && bleResponse.length() && int32_t(millis()-nextNotification)>=0) {
    if(!bleTx->getSubscribedCount()) {bleResponse="";bleOffset=0;return;}
    unsigned length=min(unsigned(20),bleResponse.length()-bleOffset);
    bleTx->setValue((const uint8_t*)bleResponse.c_str()+bleOffset,length);
    bleTx->notify();bleOffset+=length;nextNotification=millis()+15;
    if(bleOffset==bleResponse.length()) {bleResponse="";bleOffset=0;}
  }
}
