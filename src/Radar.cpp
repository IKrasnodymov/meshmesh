#include "Radar.h"
#include "App.h"
#include "MeshRadio.h"
#include "WifiDiagnostics.h"
#include "BleDiagnostics.h"
#include "Internet.h"
#include <Mm1Packet.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
Radar radar;
namespace {
// Homing frames arrive on the Wi-Fi task; the loop drains them in 100 ms windows.
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
uint8_t bssid[6];int32_t frameSum=0;uint32_t frameCount=0;
void onFrame(void* buffer,wifi_promiscuous_pkt_type_t type){
 if(type!=WIFI_PKT_MGMT)return;auto* p=(const wifi_promiscuous_pkt_t*)buffer;
 // Beacons and probe responses only: the access point sends them at a fixed rate and power.
 if(p->rx_ctrl.sig_len<24||(p->payload[0]!=0x80&&p->payload[0]!=0x50)||memcmp(p->payload+10,bssid,6))return;
 taskENTER_CRITICAL(&guard);frameSum+=p->rx_ctrl.rssi;frameCount++;taskEXIT_CRITICAL(&guard);
}
void onScanDone(void*,esp_event_base_t,int32_t,void* data){auto* e=(wifi_event_sta_scan_done_t*)data;radar.doneStatus=e->status;radar.doneNumber=e->number;}
bool scanHandler=false;
// Arduino 2.0.17 scanNetworks() leaves wifi_scan_config_t::home_chan_dwell_time uninitialised;
// with stack garbage there the driver rejects the scan (seen on M9: SCAN_DONE status 1 after 3 ms).
esp_err_t startScan(){WiFi.scanDelete();wifi_scan_config_t c={};c.show_hidden=true;c.scan_type=WIFI_SCAN_TYPE_ACTIVE;c.scan_time.active.min=100;c.scan_time.active.max=110;return esp_wifi_scan_start(&c,false);}
uint64_t macId(const uint8_t* m){uint64_t v=0;for(int i=0;i<6;i++)v=v<<8|m[i];return v;}
// Bluetooth advertisements arrive on the NimBLE host task; the loop drains this ring.
struct Heard{uint64_t address;int8_t rssi;RadarTarget::Device device;uint16_t vendor;char name[33];};
Heard heard[32];unsigned heardHead=0,heardTail=0;
class Scanner:public NimBLEAdvertisedDeviceCallbacks{
 void onResult(NimBLEAdvertisedDevice* d) override{
  Heard h{};const uint8_t* a=d->getAddress().getNative();for(int i=5;i>=0;i--)h.address=h.address<<8|a[i];h.rssi=constrain(d->getRSSI(),-127,0);
  std::string maker=d->haveManufacturerData()?d->getManufacturerData():std::string();
  h.device=Radar::classify(d->haveAppearance()?d->getAppearance():0,(const uint8_t*)maker.data(),maker.size());h.vendor=maker.size()>=2?uint8_t(maker[0])|uint8_t(maker[1])<<8:0xffff;
  if(d->haveName()){std::string n=d->getName();size_t len=min(n.size(),sizeof(h.name)-1);memcpy(h.name,n.data(),len);
   if(!meshmesh::validUtf8((const uint8_t*)h.name,len))for(size_t k=0;k<len;k++)if(uint8_t(h.name[k])>=0x80)h.name[k]='?';}
  taskENTER_CRITICAL(&guard);unsigned next=(heardHead+1)%32;if(next!=heardTail){heard[heardHead]=h;heardHead=next;}taskEXIT_CRITICAL(&guard);
 }
} scanner;
}
void Radar::open(){if(active)return;active=true;count=0;sweeps=0;tracking=false;reset();wifi=WifiOff;ble=BleOff;lastSweep=0;loraAt=0;dirty=true;tick();}
void Radar::close(){
 if(!active)return;untrack();setCsi(CsiOff);bool ours=wifi==WifiReady;release();
 // The Bluetooth stack is never deinitialised: a deinit right after a scan stop ran a stale host
 // event (M9 panic, PC 0 in nimble_port_run), and any deinit + init froze Wi-Fi CSI until reboot.
 releaseBle();
 if(ours&&!portalActive()&&!wifiProbeActive())WiFi.mode(WIFI_OFF);
 wifi=WifiOff;ble=BleOff;active=false;count=0;
}
void Radar::release(){
 csiStop();sniff(false);if(scanning){esp_wifi_scan_stop();scanning=false;}WiFi.scanDelete();
 if(wifi==WifiReady)wifi=WifiOff;
}
// Stops our scan and restores the scan defaults the BLE probe relies on.
void Radar::releaseBle(){
 // The stack stays up (see bleToggle in Portal.cpp): a deinit and a later init froze Wi-Fi CSI data.
 if(NimBLEDevice::getInitialized()){NimBLEScan* s=NimBLEDevice::getScan();if(bleScanning)s->stop();s->setAdvertisedDeviceCallbacks(nullptr);s->setMaxResults(0xff);s->setDuplicateFilter(true);s->clearResults();}
 bleScanning=false;
 taskENTER_CRITICAL(&guard);heardHead=heardTail=0;taskEXIT_CRITICAL(&guard);
 if(ble==BleReady)ble=BleOff;
}
// The radar never shares Wi-Fi with the access point or the USB Wi-Fi probe.
void Radar::wifiStart(){
 internet.yieldRadio(); // scans, homing and CSI need the radio off any access point
 if(!WiFi.mode(WIFI_STA)){wifi=WifiFailed;return;}
 WiFi.setSleep(true); // required for Wi-Fi/BLE coexistence on ESP32-S3; see AGENTS.md
 if(!scanHandler)scanHandler=esp_event_handler_register(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,onScanDone,nullptr)==ESP_OK;
 wifi=WifiReady;lastSweep=0;
}
void Radar::sniff(bool on){
 if(on==sniffing)return;
 if(!on){esp_wifi_set_promiscuous(false);sniffing=false;return;}
 for(int i=5;i>=0;i--)bssid[i]=focus.id>>(8*(5-i));
 taskENTER_CRITICAL(&guard);frameSum=0;frameCount=0;taskEXIT_CRITICAL(&guard);
 wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT};
 esp_wifi_set_promiscuous_filter(&filter);esp_wifi_set_promiscuous_rx_cb(onFrame);
 if(esp_wifi_set_promiscuous(true)!=ESP_OK||esp_wifi_set_channel(focus.channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK){esp_wifi_set_promiscuous(false);wifi=WifiFailed;return;}
 sniffing=true;
}
void Radar::mergeScan(int n){
 uint32_t now=millis();
 for(int i=0;i<n;i++){
  String ssid;uint8_t security;int32_t rssi,channel;uint8_t* mac;
  if(!WiFi.getNetworkInfo(i,ssid,security,rssi,mac,channel)||!mac)continue;
  RadarTarget* t=upsert(RadarTarget::Wifi,macId(mac));if(!t)continue;
  // SSIDs are raw bytes: keep valid UTF-8, replace anything else by '?'.
  strlcpy(t->name,ssid.c_str(),sizeof(t->name));size_t len=strlen(t->name);
  if(!meshmesh::validUtf8((const uint8_t*)t->name,len))for(size_t k=0;k<len;k++)if(uint8_t(t->name[k])>=0x80)t->name[k]='?';
  t->channel=channel;t->open=security==WIFI_AUTH_OPEN;t->rssi=constrain(rssi,-127,0);t->seen=now;
 }
}
// LoRa strength means the node itself only for packets heard directly (zero hops).
void Radar::refreshLora(uint32_t now){
 unsigned n=0;for(unsigned i=0;i<count;i++)if(targets[i].kind!=RadarTarget::Lora)targets[n++]=targets[i];count=n;
 for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];
  if(!p.heard||p.hops||now-p.seen>1800000)continue;RadarTarget* t=upsert(RadarTarget::Lora,p.id);if(!t)break;
  strlcpy(t->name,p.name,sizeof(t->name));t->rssi=constrain(int(p.rssi),-127,0);t->seen=p.seen;}
}
void Radar::drainBle(uint32_t now){
 bool homing=tracking&&focus.kind==RadarTarget::Ble;
 for(unsigned k=0;k<32;k++){
  Heard h;taskENTER_CRITICAL(&guard);bool any=heardTail!=heardHead;if(any){h=heard[heardTail];heardTail=(heardTail+1)%32;}taskEXIT_CRITICAL(&guard);if(!any)break;
  if(homing&&h.address==focus.id){windowSum+=h.rssi;windowCount++;rateFrames++;}
  RadarTarget* t=upsert(RadarTarget::Ble,h.address);if(!t)continue;
  if(h.name[0])strlcpy(t->name,h.name,sizeof(t->name));if(h.device!=RadarTarget::Unknown)t->device=h.device;if(h.vendor!=0xffff)t->vendor=h.vendor;
  if(!(homing&&h.address==focus.id)){t->rssi=h.rssi;t->seen=now;}
 }
}
// Bluetooth: our own scan on the MeshMesh BLE service stack, or on a stack the radar starts itself.
void Radar::bleTick(uint32_t now){
 if(bleProbeActive()){if(ble==BleReady)releaseBle();if(ble!=BleBusy)dirty=true;ble=BleBusy;return;}
 if(ble==BleFailed)return;
 bool wanted=csi==CsiOff&&(!tracking||focus.kind!=RadarTarget::Wifi); // CSI and homing on Wi-Fi get all the 2.4 GHz airtime
 if(!wanted){if(bleScanning&&NimBLEDevice::getInitialized())NimBLEDevice::getScan()->stop();bleScanning=false;return;}
 if(!NimBLEDevice::getInitialized()){NimBLEDevice::init("");bleScanning=false;} // stays up for the rest of the boot
 ble=BleReady;NimBLEScan* s=NimBLEDevice::getScan();
 if(!bleScanning||!s->isScanning()){
  s->setAdvertisedDeviceCallbacks(&scanner,true);s->setMaxResults(0);s->setDuplicateFilter(false);s->setActiveScan(true);s->setInterval(100);s->setWindow(90);
  bleScanning=s->start(0,nullptr,false);if(!bleScanning){ble=BleFailed;dirty=true;}
 }
 drainBle(now);
}
bool Radar::track(unsigned index){
 if(index>=count)return false;untrack();focus=targets[index];reset();tracking=true;
 if(focus.kind==RadarTarget::Lora){const Peer* p=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==focus.id)p=&meshRadio.peers[i];loraSeen=p?p->seen:0;}
 addSample(focus.rssi,focus.seen); // the sweep reading, with its real age
 windowSum=0;windowCount=0;windowAt=rateAt=millis();rateFrames=0;
 return true;
}
void Radar::untrack(){if(!tracking)return;sniff(false);tracking=false;reset();lastSweep=0;dirty=true;}
void Radar::tick(){
 if(!active)return;uint32_t now=millis();
 WifiState want=portalActive()?WifiPortal:wifiProbeActive()?WifiBusy:WifiReady;
 if(want!=WifiReady){if(wifi==WifiReady)release();if(wifi!=want)dirty=true;wifi=want;}
 else if(wifi!=WifiReady&&wifi!=WifiFailed)wifiStart();
 bool homingWifi=tracking&&focus.kind==RadarTarget::Wifi,sweep=!tracking||focus.kind==RadarTarget::Lora;
 // CSI keeps the radio on one channel: finish no sweep, just stop it.
 if(csi!=CsiOff){if(scanning){esp_wifi_scan_stop();WiFi.scanDelete();scanning=false;}csiTick(now);}
 else if(wifi==WifiReady){
  if(scanning){
   if(WiFiGenericClass::getStatusBits()&WIFI_SCAN_DONE_BIT){int n=WiFi.scanComplete();scanResult=n;scanMs=now-scanAt;if(n>0)mergeScan(n);WiFi.scanDelete();scanning=false;lastSweep=now;sweeps++;dirty=true;}
   else if(now-scanAt>4000){esp_wifi_scan_stop();WiFi.scanDelete();scanning=false;scanResult=WIFI_SCAN_FAILED;lastSweep=now;}
  }
  // Homing listens on the target's channel; a sweep would hop away from it.
  else if(homingWifi){if(!sniffing)sniff(true);if(sniffing){taskENTER_CRITICAL(&guard);windowSum+=frameSum;windowCount+=frameCount;rateFrames+=frameCount;frameSum=0;frameCount=0;taskEXIT_CRITICAL(&guard);}}
  else if(sweep&&now-lastSweep>=500){scanAt=now;scanStart=startScan();if(scanStart==ESP_OK)scanning=true;else lastSweep=now;}
 }
 bleTick(now);
 // Wi-Fi and BLE homing: one sample per 100 ms window that received anything.
 if(tracking&&focus.kind!=RadarTarget::Lora){
  if(now-windowAt>=100){windowAt=now;if(windowCount)addSample(windowSum/int32_t(windowCount),now);windowSum=0;windowCount=0;}
  if(now-rateAt>=1000){rate=rateFrames*1000/(now-rateAt);rateFrames=0;rateAt=now;dirty=true;}
 }
 if(tracking&&focus.kind==RadarTarget::Lora){
  for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(p.id!=focus.id||p.seen==loraSeen)continue;
   loraSeen=p.seen;if(p.heard&&!p.hops){addSample(int(p.rssi),p.seen);rate++;}}
 }
 if(now-loraAt>=500){loraAt=now;refreshLora(now);prune(now);sort();}
}
