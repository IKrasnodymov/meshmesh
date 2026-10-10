// Radar on nRF52 boards: directly heard LoRa nodes and Bluetooth LE devices (the SoftDevice
// scanner). No Wi-Fi: no access points, no CSI motion sensing. Table, homing statistics and
// JSON are shared with the ESP32 boards (RadarModel.cpp).
#include "Radar.h"
#include "People.h"
#include "App.h"
#include "MeshRadio.h"
#include <Mm1Packet.h>
#include <bluefruit.h>
Radar radar;
bool bleStack(); // PortalNrf.cpp: starts the SoftDevice once
namespace {
// Advertisements arrive on the Bluefruit task; the loop drains this ring.
struct Heard{uint64_t address;int8_t rssi;RadarTarget::Device device;uint16_t vendor;char name[33];};
Heard heard[32];volatile unsigned heardHead=0,heardTail=0,heardDropped=0;
void onReport(ble_gap_evt_adv_report_t* r){
  Heard h{};for(int i=5;i>=0;i--)h.address=h.address<<8|r->peer_addr.addr[i];h.rssi=constrain(int(r->rssi),-127,0);
  uint8_t maker[32];uint8_t makerSize=Bluefruit.Scanner.parseReportByType(r,BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,maker,sizeof(maker));
  uint8_t look[2];uint16_t appearance=Bluefruit.Scanner.parseReportByType(r,BLE_GAP_AD_TYPE_APPEARANCE,look,2)==2?uint16_t(look[0]|look[1]<<8):0;
  h.device=Radar::classify(appearance,maker,makerSize);h.vendor=makerSize>=2?uint16_t(maker[0]|maker[1]<<8):0xffff;
  uint8_t n=Bluefruit.Scanner.parseReportByType(r,BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,(uint8_t*)h.name,sizeof(h.name)-1);
  if(!n)n=Bluefruit.Scanner.parseReportByType(r,BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,(uint8_t*)h.name,sizeof(h.name)-1);
  h.name[n]=0;if(!meshmesh::validUtf8((const uint8_t*)h.name,n))for(unsigned k=0;k<n;k++)if(uint8_t(h.name[k])>=0x80)h.name[k]='?';
  taskENTER_CRITICAL();unsigned next=(heardHead+1)%32;if(next!=heardTail){heard[heardHead]=h;heardHead=next;}else heardDropped++;taskEXIT_CRITICAL();
  Bluefruit.Scanner.resume();
}
bool scanning=false;
}
void Radar::open(){if(active)return;active=true;count=0;sweeps=0;tracking=false;reset();wifi=WifiOff;ble=BleOff;lastSweep=0;loraAt=0;dirty=true;tick();}
void Radar::close(){if(!active)return;untrack();releaseBle();ble=BleOff;active=false;count=0;}
void Radar::release(){}
void Radar::releaseBle(){
  if(scanning){Bluefruit.Scanner.stop();scanning=false;}
  taskENTER_CRITICAL();heardHead=heardTail=0;taskEXIT_CRITICAL();
  if(ble==BleReady)ble=BleOff;
}
void Radar::refreshLora(uint32_t now){
  unsigned n=0;for(unsigned i=0;i<count;i++)if(targets[i].kind!=RadarTarget::Lora)targets[n++]=targets[i];count=n;
  for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];
    if(!p.heard||p.hops||now-p.seen>1800000)continue;RadarTarget* t=upsert(RadarTarget::Lora,p.id);if(!t)break;
    strlcpy(t->name,p.name,sizeof(t->name));t->rssi=constrain(int(p.rssi),-127,0);t->seen=p.seen;}
}
void Radar::drainBle(uint32_t now){
 taskENTER_CRITICAL();unsigned lost=heardDropped;heardDropped=0;taskEXIT_CRITICAL();people::noteDropped(false,lost);
  bool homing=tracking&&focus.kind==RadarTarget::Ble;
  for(unsigned k=0;k<32;k++){
    Heard h;taskENTER_CRITICAL();bool any=heardTail!=heardHead;if(any){h=heard[heardTail];heardTail=(heardTail+1)%32;}taskEXIT_CRITICAL();if(!any)break;
    people::hear(false,h.address,h.rssi,(h.device==RadarTarget::Phone||h.device==RadarTarget::Watch||h.device==RadarTarget::Personal));if(!active)continue;
  if(homing&&h.address==focus.id){windowSum+=h.rssi;windowCount++;rateFrames++;}
    RadarTarget* t=upsert(RadarTarget::Ble,h.address);if(!t)continue;
    if(h.name[0])strlcpy(t->name,h.name,sizeof(t->name));if(h.device!=RadarTarget::Unknown)t->device=h.device;if(h.vendor!=0xffff)t->vendor=h.vendor;
    if(!(homing&&h.address==focus.id)){t->rssi=h.rssi;t->seen=now;}
  }
}
void Radar::bleTick(uint32_t now){
  if(ble==BleFailed)return;
  if(!scanning){
    if(!bleStack()){ble=BleFailed;dirty=true;return;}
    Bluefruit.Scanner.setRxCallback(onReport);Bluefruit.Scanner.restartOnDisconnect(true);Bluefruit.Scanner.useActiveScan(active);
    Bluefruit.Scanner.setInterval(160,active?144:48); // 100 ms window of 90 ms, as on the ESP32 boards
    scanning=Bluefruit.Scanner.start(0);ble=scanning?BleReady:BleFailed;dirty=true;
  }
  drainBle(now);
}
bool Radar::track(unsigned index){
  if(index>=count)return false;untrack();focus=targets[index];reset();tracking=true;
  if(focus.kind==RadarTarget::Lora){const Peer* p=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==focus.id)p=&meshRadio.peers[i];loraSeen=p?p->seen:0;}
  addSample(focus.rssi,focus.seen);windowSum=0;windowCount=0;windowAt=rateAt=millis();rateFrames=0;return true;
}
void Radar::untrack(){if(!tracking)return;tracking=false;reset();dirty=true;}
void Radar::tick(){
  if(!active){if(people::settings.ble)bleTick(millis());else if(scanning)releaseBle();return;}uint32_t now=millis();
  bleTick(now);
  if(tracking&&focus.kind==RadarTarget::Ble){
    if(now-windowAt>=100){windowAt=now;if(windowCount)addSample(windowSum/int32_t(windowCount),now);windowSum=0;windowCount=0;}
    if(now-rateAt>=1000){rate=rateFrames*1000/(now-rateAt);rateFrames=0;rateAt=now;dirty=true;}
  }
  if(tracking&&focus.kind==RadarTarget::Lora){
    for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(p.id!=focus.id||p.seen==loraSeen)continue;
      loraSeen=p.seen;if(p.heard&&!p.hops){addSample(int(p.rssi),p.seen);rate++;}}
  }
  if(now-loraAt>=500){loraAt=now;refreshLora(now);prune(now);sort();sweeps++;}
}
// No Wi-Fi radio: CSI roles stay off.
void Radar::setCsi(CsiRole role){(void)role;csi=CsiOff;}
bool Radar::streamLine(String&){return false;}
void Radar::setBeaconHz(uint16_t hz){beaconHz=constrain(hz,10,200);}
void Radar::csiStart(){}
void Radar::csiStop(){}
void Radar::csiTick(uint32_t){}
void Radar::wifiStart(){}
void Radar::sniff(bool){}
void Radar::mergeScan(int){}
