// Radio actions added by the UI; kept here so the preview links.
bool MeshRadio::resetPath(uint64_t id){Peer* p=contact(id);if(!p)return false;p->pathLength=255;return true;}
bool MeshRadio::removeContact(uint64_t id){Peer* p=contact(id);if(!p)return false;unsigned i=p-peers;memmove(peers+i,peers+i+1,sizeof(Peer)*(peerCount-i-1));peerCount--;return true;}
void Hardware::ping(uint16_t,uint16_t){}
// Signal radar: synthetic sweep results; the table and trend logic are the real src/RadarModel.cpp.
#include "Radar.h"
Radar radar;
void Radar::open(){if(active)return;active=true;wifi=WifiReady;sweeps=3;count=0;
 struct {uint64_t id;const char* name;uint8_t ch;bool open;int rssi;} aps[]={{0x5A1F0C9E1101ULL,"MM-9E77D2",1,false,-41},{0x34E8941A2B3CULL,"Keenetic-4471",6,false,-58},{0xA0F3C1223344ULL,"TP-Link_5GHz",11,false,-66},{0x10FEED000001ULL,"",3,false,-72},{0x28CDC1ABCDEFULL,"Кафе Бахетле",1,true,-79},{0xB827EB000102ULL,"DIRECT-HP-M28",6,false,-88}};
 for(auto& a:aps){RadarTarget* t=upsert(RadarTarget::Wifi,a.id);strcpy(t->name,a.name);t->channel=a.ch;t->open=a.open;t->rssi=a.rssi;t->seen=millis();}
 struct {uint64_t id;const char* name;RadarTarget::Device device;uint16_t vendor;int rssi;} bles[]={{0x6A11,"",RadarTarget::Personal,0x004c,-52},{0x6A12,"Galaxy Watch6",RadarTarget::Watch,0x0075,-61},{0x6A13,"",RadarTarget::Audio,0x004c,-70},{0x6A14,"MeshMesh 0E44",RadarTarget::Unknown,0xffff,-47},{0x6A15,"",RadarTarget::Unknown,0x0006,-83}};
 for(auto& b:bles){RadarTarget* t=upsert(RadarTarget::Ble,b.id);strcpy(t->name,b.name);t->device=b.device;t->vendor=b.vendor;t->rssi=b.rssi;t->seen=millis()-(b.id&3)*7000;}
 ble=BleReady;
 for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(!p.heard||p.hops)continue;RadarTarget* t=upsert(RadarTarget::Lora,p.id);strcpy(t->name,p.name);t->rssi=p.rssi;t->seen=p.seen;}
 sort();}
void Radar::close(){setCsi(CsiOff);untrack();active=false;count=0;wifi=WifiOff;ble=BleOff;}
void Radar::release(){}
void Radar::releaseBle(){}
void Radar::tick(){if(csi==CsiSensor)csiLast=millis();}
// CSI: a still room, then someone walking between the boards.
void Radar::setCsi(CsiRole role){if(role==csi)return;csi=role;csiRunning=role!=CsiOff;csiRate=role==CsiOff?0:role==CsiBeacon?50:48;motionSamples=0;moving=false;
 if(role==CsiSensor){csiRssi=-41;csiLast=millis();baseline=.004f;for(int i=0;i<100;i++){float v=i<70?.0035f+.0015f*((i*37)%7)/6:.018f+.03f*((i*53)%9)/8;addActivity(v,millis()-(100-i)*500);}}}
bool Radar::track(unsigned index){if(index>=count)return false;focus=targets[index];reset();tracking=true;
 // A walk towards the access point: noisy approach, then a short step back.
 for(int i=0;i<90;i++){int r=-78+i/3+int((i*37)%7)-3-(i>80?(i-80)*2:0);addSample(r,millis()-(90-i)*100);}
 lastSample=millis()-300;rate=21;return true;}
void Radar::untrack(){tracking=false;reset();}
