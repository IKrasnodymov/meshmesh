// nRF52 boards: no Wi-Fi, so the device page reaches the phone over Bluetooth (the Android app) or
// USB. The BLE service is the one of src/Portal.cpp: commands written to 7a9e0002, replies with a
// trailing newline notified on 7a9e0003, both behind MITM pairing with the PIN shown on the screen.
#include "App.h"
#include "MeshRadio.h"
#include "Radar.h"
#include "Power.h"
#include "Companion.h"
#include <bluefruit.h>
#include <esp_system.h>

namespace {
// UUIDs in the little-endian byte order Bluefruit takes: 7a9e000x-98bd-4d56-89a8-c4eab4179010.
#define MM_UUID(n) {0x10,0x90,0x17,0xb4,0xea,0xc4,0xa8,0x89,0x56,0x4d,0xbd,0x98,n,0x00,0x9e,0x7a}
const uint8_t serviceUuid[]=MM_UUID(0x01),rxUuid[]=MM_UUID(0x02),txUuid[]=MM_UUID(0x03);
BLEService service(serviceUuid);
BLECharacteristic rx(rxUuid),tx(txUuid);
// MeshCore companion apps: the Nordic UART service of stock MeshCore (6E40000x-B5A3-F393-E0A9-E50E24DCCA9E).
#define NUS_UUID(n) {0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,n,0x00,0x40,0x6e}
const uint8_t nusUuid[]=NUS_UUID(0x01),nusRxUuid[]=NUS_UUID(0x02),nusTxUuid[]=NUS_UUID(0x03);
BLEService nus(nusUuid);
BLECharacteristic appRx(nusRxUuid),appTx(nusTxUuid);
struct AppFrame {uint8_t length;uint8_t data[companion::MaxFrame];};
QueueHandle_t appFrames=nullptr;
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
void onAppWrite(uint16_t,BLECharacteristic*,uint8_t* data,uint16_t size){
  if(!bluetoothOn||!appFrames||!size||size>companion::MaxFrame)return;AppFrame f{};f.length=size;memcpy(f.data,data,size);xQueueSend(appFrames,&f,0);
}
void onConnect(uint16_t handle){client=handle;}
void onDisconnect(uint16_t handle,uint8_t){if(handle==client){client=BLE_CONN_HANDLE_INVALID;bleResponse="";bleOffset=0;companion::disconnected(companion::LinkBle);}}
// The advert of stock MeshCore (its service, "MeshCore-<name>" in the scan response): MeshCore apps find the board
// by the name. The MeshMesh app finds it by the "MM" mark in the manufacturer data (company ID 0xFFFF: no company);
// our own service still answers.
void advertise(){
  static const uint8_t mark[]={0xff,0xff,'M','M'};
  Bluefruit.setName(bleName().c_str());
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);Bluefruit.Advertising.addService(nus);
  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,mark,sizeof(mark));Bluefruit.ScanResponse.addName();
}
void start(){
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin(1,1); // one phone; the central role scans for the radar
  Bluefruit.autoConnLed(false);Bluefruit.setTxPower(4);
  Bluefruit.setName(bleName().c_str());
  char pin[8];snprintf(pin,sizeof(pin),"%06lu",(unsigned long)pinCode);
  Bluefruit.Security.setMITM(true);Bluefruit.Security.setIOCaps(true,false,false);Bluefruit.Security.setPIN(pin);
  Bluefruit.Periph.setConnectCallback(onConnect);Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  service.begin();
  rx.setProperties(CHR_PROPS_WRITE);rx.setPermission(SECMODE_NO_ACCESS,SECMODE_ENC_WITH_MITM);rx.setMaxLen(255);rx.setWriteCallback(onWrite);rx.begin();
  tx.setProperties(CHR_PROPS_READ|CHR_PROPS_NOTIFY);tx.setPermission(SECMODE_ENC_WITH_MITM,SECMODE_NO_ACCESS);tx.setMaxLen(244);tx.begin();
  nus.begin();
  appRx.setProperties(CHR_PROPS_WRITE|CHR_PROPS_WRITE_WO_RESP);appRx.setPermission(SECMODE_NO_ACCESS,SECMODE_ENC_WITH_MITM);appRx.setMaxLen(companion::MaxFrame);appRx.setWriteCallback(onAppWrite);appRx.begin();
  appTx.setProperties(CHR_PROPS_READ|CHR_PROPS_NOTIFY);appTx.setPermission(SECMODE_ENC_WITH_MITM,SECMODE_NO_ACCESS);appTx.setMaxLen(companion::MaxFrame);appTx.begin();
  advertise();
  Bluefruit.Advertising.setInterval(32,244);Bluefruit.Advertising.setFastTimeout(30);
  commands=xQueueCreate(4,sizeof(BleCommand));appFrames=xQueueCreate(4,sizeof(AppFrame));started=true;
}
}
String bleName(){
  // The scan response holds 29 bytes: the name cut to 15, then 4 hex digits of the node key, so boards
  // with the same name differ in the list of devices.
  String name=config.name;while(name.length()>15){unsigned cut=name.length()-1;while(cut&&(uint8_t(name[cut])&0xc0)==0x80)cut--;name.remove(cut);}
  return "MeshCore-"+name+" "+meshRadio.idText(meshRadio.nodeId).substring(0,4);
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
  StaticJsonDocument<384>d;d["wifi"]=false;d["ble"]=bluetoothOn;d["ble_name"]=bleName();d["pin"]=pinCode;
  if(bluetoothOn){uint8_t a[6];Bluefruit.getAddr(a);char s[18];snprintf(s,sizeof(s),"%02x:%02x:%02x:%02x:%02x:%02x",a[5],a[4],a[3],a[2],a[1],a[0]);d["ble_address"]=s;}
  String out;serializeJson(d,out);return out;
}
void portalBegin(){pinCode=config.blePin;}
void portalToggle(){meshRadio.event="No Wi-Fi on this board";meshRadio.dirty=true;}
// The SoftDevice starts once (the radar may have started it); "off" means no advertising and no connection.
bool bleStack(){if(!started)start();return started;}
void bleRename(){if(!started)return;bool on=bluetoothOn;if(on)Bluefruit.Advertising.stop();Bluefruit.Advertising.clearData();Bluefruit.ScanResponse.clearData();advertise();if(on)Bluefruit.Advertising.start(0);}
void bleToggle(){
  bleStack();
  if(bluetoothOn){
    Bluefruit.Advertising.restartOnDisconnect(false);Bluefruit.Advertising.stop();
    if(client!=BLE_CONN_HANDLE_INVALID)Bluefruit.disconnect(client);
    bluetoothOn=false;bleResponse="";bleOffset=0;xQueueReset(commands);xQueueReset(appFrames);companion::disconnected(companion::LinkBle);meshRadio.event="BLE off";
  } else {
    Bluefruit.Advertising.restartOnDisconnect(true);Bluefruit.Advertising.start(0);bluetoothOn=true;meshRadio.event="BLE PIN: "+String(pinCode);
  }
  config.saveBle(bluetoothOn);meshRadio.dirty=true;
}
// Power off (Power.cpp): no advertising and no connection, without changing the saved setting.
void bleSilence(){
  if(!bluetoothOn)return;
  Bluefruit.Advertising.restartOnDisconnect(false);Bluefruit.Advertising.stop();
  if(client!=BLE_CONN_HANDLE_INVALID)Bluefruit.disconnect(client);
  bluetoothOn=false;
}
void portalTick(){
  if(webRadar&&millis()-webRadarAt>10000)webRadarRelease();
  if(!started)return;
  // Companion frames: one per write, one per notification.
  if(appFrames){AppFrame f;if(xQueueReceive(appFrames,&f,0)==pdTRUE){powerWake();companion::command(f.data,f.length,companion::LinkBle);}}
  static uint8_t held[companion::MaxFrame];static size_t heldLength=0;static uint32_t nextApp=0; // a refused frame is sent again, never skipped
  if(client==BLE_CONN_HANDLE_INVALID||!appTx.notifyEnabled(client))heldLength=0;
  else if(int32_t(millis()-nextApp)>=0){
    if(!heldLength)heldLength=companion::next(held,companion::LinkBle);
    if(heldLength){if(appTx.notify(client,held,heldLength)){heldLength=0;nextApp=millis()+2;}else nextApp=millis()+10;}
  }
  if(commands&&!bleResponse.length()){BleCommand c;if(xQueueReceive(commands,&c,0)==pdTRUE){powerWake();bleResponse=executeCommand(c.text)+'\n';bleOffset=0;nextNotification=millis();}}
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
