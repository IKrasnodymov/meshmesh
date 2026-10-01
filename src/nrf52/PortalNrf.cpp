// nRF52 boards: no Wi-Fi, so the device page reaches the phone over Bluetooth (the Android app) or
// USB. The BLE service is the one of src/Portal.cpp: commands written to 7a9e0002, replies with a
// trailing newline notified on 7a9e0003, both behind MITM pairing with the PIN shown on the screen.
#include "App.h"
#include "MeshRadio.h"
#include "Radar.h"
#include <bluefruit.h>
#include <esp_system.h>

namespace {
// UUIDs in the little-endian byte order Bluefruit takes: 7a9e000x-98bd-4d56-89a8-c4eab4179010.
#define MM_UUID(n) {0x10,0x90,0x17,0xb4,0xea,0xc4,0xa8,0x89,0x56,0x4d,0xbd,0x98,n,0x00,0x9e,0x7a}
const uint8_t serviceUuid[]=MM_UUID(0x01),rxUuid[]=MM_UUID(0x02),txUuid[]=MM_UUID(0x03);
BLEService service(serviceUuid);
BLECharacteristic rx(rxUuid),tx(txUuid);
bool started=false,bluetoothOn=false;
uint32_t pinCode=123456;
struct BleCommand {char text[256];};
QueueHandle_t commands=nullptr;
uint16_t client=BLE_CONN_HANDLE_INVALID;
String bleResponse;unsigned bleOffset=0;uint32_t nextNotification=0;
bool webRadar=false;uint32_t webRadarAt=0;
void webRadarRelease(){if(!webRadar)return;webRadar=false;if(!uiRadarPage())radar.close();}
int radarIndex(uint32_t ref,const String& kind){const char* kinds[]={"wifi","ble","lora"};for(unsigned i=0;i<radar.count;i++)if(Radar::placement(radar.targets[i])==ref&&kind==kinds[radar.targets[i].kind])return i;return -1;}
String radarAction(JsonObjectConst v){
  String action=v["action"]|"";
  if(action=="close"){webRadarRelease();return "OK radar released";}
  if(!radar.active)return "ERR radar is not open";
  if(action=="track"){int i=radarIndex(v["ref"]|0u,v["kind"]|"");if(i<0||!radar.track(i))return "ERR signal is gone";return "OK homing";}
  if(action=="untrack"){radar.untrack();return "OK homing stopped";}
  if(action=="peak"){radar.resetPeak();return "OK peak reset";}
  if(action=="csi"||action=="calibrate")return "ERR Wi-Fi CSI needs an ESP32 board";
  return "ERR radar action";
}
// Writes arrive on the Bluefruit callback task; the loop runs them.
void onWrite(uint16_t,BLECharacteristic*,uint8_t* data,uint16_t size){
  if(!bluetoothOn||!commands||size>255)return;BleCommand c{};memcpy(c.text,data,size);xQueueSend(commands,&c,0);
}
void onConnect(uint16_t handle){client=handle;}
void onDisconnect(uint16_t handle,uint8_t){if(handle==client){client=BLE_CONN_HANDLE_INVALID;bleResponse="";bleOffset=0;}}
void start(){
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin(1,1); // one phone; the central role scans for the radar
  Bluefruit.autoConnLed(false);Bluefruit.setTxPower(4);
  String name="MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6);Bluefruit.setName(name.c_str());
  char pin[8];snprintf(pin,sizeof(pin),"%06lu",(unsigned long)pinCode);
  Bluefruit.Security.setMITM(true);Bluefruit.Security.setIOCaps(true,false,false);Bluefruit.Security.setPIN(pin);
  Bluefruit.Periph.setConnectCallback(onConnect);Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  service.begin();
  rx.setProperties(CHR_PROPS_WRITE);rx.setPermission(SECMODE_NO_ACCESS,SECMODE_ENC_WITH_MITM);rx.setMaxLen(255);rx.setWriteCallback(onWrite);rx.begin();
  tx.setProperties(CHR_PROPS_READ|CHR_PROPS_NOTIFY);tx.setPermission(SECMODE_ENC_WITH_MITM,SECMODE_NO_ACCESS);tx.setMaxLen(244);tx.begin();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(service);Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.setInterval(32,244);Bluefruit.Advertising.setFastTimeout(30);
  commands=xQueueCreate(4,sizeof(BleCommand));started=true;
}
}
bool webRadarActive(){return webRadar;}
String webRadarCommand(const String& line){
  if(line=="radar web"){if(!radar.active)radar.open();webRadar=true;webRadarAt=millis();return radar.webJson();}
  StaticJsonDocument<256>d;if(deserializeJson(d,line.substring(9))||!d.is<JsonObject>())return "ERR radar do {JSON}";
  if(webRadar)webRadarAt=millis();return radarAction(d.as<JsonObjectConst>());
}
bool portalActive(){return false;}
String portalPassword(){return "";}
bool bleActive(){return bluetoothOn;}
uint32_t blePin(){return pinCode;}
String connectionCredentials(){
  StaticJsonDocument<384>d;d["wifi"]=false;d["ble"]=bluetoothOn;d["ble_name"]="MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6);d["pin"]=pinCode;
  if(bluetoothOn){uint8_t a[6];Bluefruit.getAddr(a);char s[18];snprintf(s,sizeof(s),"%02x:%02x:%02x:%02x:%02x:%02x",a[5],a[4],a[3],a[2],a[1],a[0]);d["ble_address"]=s;}
  String out;serializeJson(d,out);return out;
}
void portalBegin(){uint8_t entropy[3];esp_fill_random(entropy,sizeof(entropy));pinCode=100000+(uint32_t(entropy[0])<<16|uint32_t(entropy[1])<<8|entropy[2])%900000;}
void portalToggle(){meshRadio.event="No Wi-Fi on this board";meshRadio.dirty=true;}
// The SoftDevice starts once (the radar may have started it); "off" means no advertising and no connection.
bool bleStack(){if(!started)start();return started;}
void bleToggle(){
  bleStack();
  if(bluetoothOn){
    Bluefruit.Advertising.restartOnDisconnect(false);Bluefruit.Advertising.stop();
    if(client!=BLE_CONN_HANDLE_INVALID)Bluefruit.disconnect(client);
    bluetoothOn=false;bleResponse="";bleOffset=0;xQueueReset(commands);meshRadio.event="BLE off";
  } else {
    Bluefruit.Advertising.restartOnDisconnect(true);Bluefruit.Advertising.start(0);bluetoothOn=true;meshRadio.event="BLE PIN: "+String(pinCode);
  }
  meshRadio.dirty=true;
}
void portalTick(){
  if(webRadar&&millis()-webRadarAt>10000)webRadarRelease();
  if(!started)return;
  if(commands&&!bleResponse.length()){BleCommand c;if(xQueueReceive(commands,&c,0)==pdTRUE){bleResponse=executeCommand(c.text)+'\n';bleOffset=0;nextNotification=millis();}}
  if(bleResponse.length()&&int32_t(millis()-nextNotification)>=0){
    BLEConnection* link=client!=BLE_CONN_HANDLE_INVALID?Bluefruit.Connection(client):nullptr;
    if(!link||!tx.notifyEnabled(client)){bleResponse="";bleOffset=0;return;}
    unsigned room=min(244u,unsigned(max(23,int(link->getMtu())))-3);
    unsigned length=min(room,bleResponse.length()-bleOffset);
    // notify() waits for a free SoftDevice buffer; a refusal is retried shortly, never skipped.
    if(tx.notify(client,(const uint8_t*)bleResponse.c_str()+bleOffset,length)){bleOffset+=length;nextNotification=millis()+2;}else nextNotification=millis()+10;
    if(bleOffset==bleResponse.length()){bleResponse="";bleOffset=0;}
  }
}
