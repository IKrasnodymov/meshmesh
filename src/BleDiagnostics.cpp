#include "Version.h"
#include "BleDiagnostics.h"
#include "App.h"
#include "MeshRadio.h"
#include "Radar.h"
#include <NimBLEDevice.h>
#include <freertos/semphr.h>

namespace {
constexpr const char* serviceId="7a9e0001-98bd-4d56-89a8-c4eab4179010";
constexpr size_t capacity=40960;
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
struct State {
  bool running=false,done=false,encrypted=false,authenticated=false;
  bool selftest=false,status=false,config=false,messages=false,sent=false,overflow=false;
  char stage[32]="idle",error[96]={},name[32]={},message[152]={},address[24]={};
  uint32_t pin=0;size_t responseBytes=0,statusBytes=0,historyBytes=0;
  float frequency=0,bandwidth=0;int sf=0,cr=0,power=0,hops=0;
} state;
char* response=nullptr;size_t used=0;
SemaphoreHandle_t received=nullptr;
State snapshot() {portENTER_CRITICAL(&guard);State value=state;portEXIT_CRITICAL(&guard);return value;}
bool flag(bool State::*field,bool value) {portENTER_CRITICAL(&guard);state.*field=value;portEXIT_CRITICAL(&guard);return value;}

void stage(const char* value) {portENTER_CRITICAL(&guard);strlcpy(state.stage,value,sizeof(state.stage));portEXIT_CRITICAL(&guard);}
void error(const char* value) {portENTER_CRITICAL(&guard);strlcpy(state.error,value,sizeof(state.error));portEXIT_CRITICAL(&guard);}
class Callbacks:public NimBLEClientCallbacks {
  uint32_t onPassKeyRequest() override {return state.pin;}
  bool onConfirmPIN(uint32_t) override {return false;} // This probe must use passkey entry.
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    portENTER_CRITICAL(&guard);
    state.encrypted=desc->sec_state.encrypted;state.authenticated=desc->sec_state.authenticated;
    portEXIT_CRITICAL(&guard);
  }
} callbacks;
void notification(NimBLERemoteCharacteristic*,uint8_t* data,size_t length,bool) {
  bool complete=false;
  portENTER_CRITICAL(&guard);
  if(response) {
    if(used+length>=capacity)state.overflow=true;
    else {memcpy(response+used,data,length);used+=length;state.responseBytes=used;response[used]=0;complete=memchr(data,'\n',length)!=nullptr;}
  }
  portEXIT_CRITICAL(&guard);
  if(complete && received)xSemaphoreGive(received);
}
bool request(NimBLERemoteCharacteristic* rx,const String& command) {
  while(xSemaphoreTake(received,0)==pdTRUE) {}
  portENTER_CRITICAL(&guard);used=0;response[0]=0;state.overflow=false;portEXIT_CRITICAL(&guard);
  if(!rx->writeValue(command.c_str(),true)) {error("Protected GATT write failed");return false;}
  if(xSemaphoreTake(received,pdMS_TO_TICKS(45000))!=pdTRUE) {error("Incomplete notification response");return false;}
  portENTER_CRITICAL(&guard);state.responseBytes=used;bool overflow=state.overflow;portEXIT_CRITICAL(&guard);
  if(overflow) {error("Notification response overflow");return false;}
  return true;
}
void probe(void*) {
  NimBLEClient* client=nullptr;
  do {
    response=(char*)malloc(capacity);received=xSemaphoreCreateBinary();
    if(!response || !received) {error("Insufficient probe memory");break;}
    stage("scan");auto* scan=NimBLEDevice::getScan();scan->setActiveScan(true);
    auto found=scan->start(5,false);bool matched=false;NimBLEAddress address;
    for(int i=0;i<found.getCount();i++) {
      auto device=found.getDevice(i);
      const auto mark=device.getManufacturerData();
      bool ours=device.isAdvertisingService(NimBLEUUID(serviceId))||(mark.size()>=4&&mark.compare(0,4,std::string("\xff\xffMM",4))==0);
      if(device.getName()==state.name && ours) {address=device.getAddress();matched=true;break;}
    }
    if(!matched) {error("MeshMesh peer not found");break;}
    portENTER_CRITICAL(&guard);strlcpy(state.address,address.toString().c_str(),sizeof(state.address));portEXIT_CRITICAL(&guard);
    client=NimBLEDevice::createClient();if(!client) {error("BLE client unavailable");break;}
    client->setClientCallbacks(&callbacks,false);client->setConnectTimeout(15);
    client->setConnectionParams(12,24,0,200);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    stage("connect");if(!client->connect(address)) {error("BLE connection failed");break;}
    stage("pair");
    bool secured=client->secureConnection();auto auth=snapshot();
    if(!secured || !auth.encrypted || !auth.authenticated) {error("Authenticated pairing failed");break;}
    auto* service=client->getService(serviceId);
    auto* tx=service?service->getCharacteristic("7a9e0003-98bd-4d56-89a8-c4eab4179010"):nullptr;
    auto* rx=service?service->getCharacteristic("7a9e0002-98bd-4d56-89a8-c4eab4179010"):nullptr;
    if(!tx || !rx || !tx->subscribe(true,notification,true)) {error("GATT service subscription failed");break;}
    stage("selftest");if(!request(rx,"selftest"))break;
    if(!flag(&State::selftest,String(response).startsWith("OK crypto/UTF-8/tamper selftest"))) {error("Unexpected self-test response");break;}
    stage("status");if(!request(rx,"status"))break;
    portENTER_CRITICAL(&guard);state.statusBytes=used;portEXIT_CRITICAL(&guard);
    DynamicJsonDocument document(capacity);
    if(!flag(&State::status,!deserializeJson(document,response) && document["firmware"]==MESHMM_FIRMWARE && document["radio"].as<bool>())) {error("Invalid fragmented status JSON");break;}
    stage("config");if(!request(rx,"config"))break;document.clear();
    // The radio profile that makes two nodes hear each other; power and hops are each node's own choice.
    bool matchedConfig=!deserializeJson(document,response) && abs(document["frequency"].as<float>()-auth.frequency)<.0001f &&
      document["bandwidth"].as<float>()==auth.bandwidth && document["sf"].as<int>()==auth.sf && document["cr"].as<int>()==auth.cr;
    if(!flag(&State::config,matchedConfig)) {error("Radio profile mismatch over BLE");break;}
    stage("messages");if(!request(rx,"messages"))break;document.clear();
    portENTER_CRITICAL(&guard);state.historyBytes=used;portEXIT_CRITICAL(&guard);
    if(!flag(&State::messages,!deserializeJson(document,response) && document.is<JsonArray>())) {error("Invalid fragmented message history JSON");break;}
    if(state.message[0]) {
      stage("send");if(!request(rx,"send "+meshRadio.idText(meshRadio.nodeId)+" "+String(state.message)))break;
      if(!flag(&State::sent,String(response).startsWith("OK message queued"))) {error("BLE chat command rejected");break;}
    }
  } while(false);
  stage("cleanup");
  if(client) {if(client->isConnected())client->disconnect();NimBLEDevice::deleteClient(client);}
  NimBLEDevice::getScan()->clearResults();NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  // The BLE host no longer has a subscribed connection before freeing its buffer.
  portENTER_CRITICAL(&guard);char* memory=response;response=nullptr;portEXIT_CRITICAL(&guard);free(memory);
  if(received) {vSemaphoreDelete(received);received=nullptr;}
  portENTER_CRITICAL(&guard);state.running=false;state.done=true;strlcpy(state.stage,"done",sizeof(state.stage));portEXIT_CRITICAL(&guard);
  vTaskDelete(nullptr);
}
}
bool bleProbeActive() {portENTER_CRITICAL(&guard);bool value=state.running;portEXIT_CRITICAL(&guard);return value;}
String startBleProbe(JsonObjectConst options) {
  if(bleProbeActive()||radar.active)return "ERR BLE probe busy or radar open";
  if(!options["name"].is<const char*>() || !options["pin"].is<uint32_t>())return "ERR bleprobe needs name and PIN";
  String name=options["name"].as<String>(),message=options["message"]|"";
  uint32_t pin=options["pin"].as<uint32_t>();
  if((!name.startsWith("MeshMesh ")&&!name.startsWith("MeshCore-")) || name.length()>31 || pin>999999 || message.length()>151 ||
     !meshmesh::validUtf8((const uint8_t*)message.c_str(),message.length()))return "ERR probe parameters";
  if(!bleActive())bleToggle();if(!bleActive())return "ERR BLE unavailable";
  radar.releaseBle(); // stop the background counter's scanner before the probe task starts it
  portENTER_CRITICAL(&guard);state=State();state.running=true;state.pin=pin;
  strlcpy(state.name,name.c_str(),sizeof(state.name));strlcpy(state.message,message.c_str(),sizeof(state.message));portEXIT_CRITICAL(&guard);
  state.frequency=config.frequency;state.bandwidth=config.bandwidth;state.sf=config.sf;state.cr=config.cr;state.power=config.power;state.hops=config.hops;
  if(xTaskCreate(probe,"ble-probe",8192,nullptr,1,nullptr)!=pdPASS) {
    portENTER_CRITICAL(&guard);state.running=false;portEXIT_CRITICAL(&guard);return "ERR probe task allocation";
  }
  return "OK BLE probe started";
}
String bleProbeResult() {
  State value=snapshot();
  StaticJsonDocument<768> d;d["running"]=value.running;d["done"]=value.done;d["stage"]=value.stage;d["error"]=value.error;
  d["peer"]=value.name;d["address"]=value.address;d["encrypted"]=value.encrypted;d["authenticated"]=value.authenticated;
  d["selftest"]=value.selftest;d["status"]=value.status;d["config"]=value.config;d["messages"]=value.messages;
  d["sent"]=value.sent;d["response_bytes"]=value.responseBytes;d["overflow"]=value.overflow;
  d["status_bytes"]=value.statusBytes;d["history_bytes"]=value.historyBytes;
  String result;serializeJson(d,result);return result;
}
