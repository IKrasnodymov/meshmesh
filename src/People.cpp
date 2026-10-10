#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "People.h"
#if !defined(MM_UI_PREVIEW)
#include <esp_system.h>
#endif
#include "BlobStore.h"
#include "App.h"
#include "Radar.h"
namespace people {
Settings settings;
namespace {constexpr uint32_t Magic=0x4d4d7001;constexpr unsigned Max=128;
struct Seen {uint32_t hash=0,at=0;int8_t rssi=0;};Seen seen[2][Max];uint32_t fullAt[2]={},lost[2]={},salt=0,saveAt=0;
bool valid(const Settings& s){return s.manual>=0&&s.manual<=999999&&(s.window==30||s.window==60||s.window==300)&&s.rssi>=-100&&s.rssi<=-30;}
uint32_t hash(uint64_t address){uint32_t h=2166136261u^salt;for(unsigned i=0;i<8;i++){h^=uint8_t(address>>(i*8));h*=16777619u;}return h?h:1;}
bool save(const Settings& s){return blob::save("/meshmesh/people",Magic,s);}
}
void begin(){Settings saved{};if(blob::load("/meshmesh/people",Magic,saved)&&valid(saved))settings=saved;resetWindow();}
void resetWindow(){memset(seen,0,sizeof seen);memset(fullAt,0,sizeof fullAt);memset(lost,0,sizeof lost);salt=
#if defined(MM_UI_PREVIEW)
 uint32_t(rand());
#else
 esp_random();
#endif
}
bool change(int delta){int64_t value=int64_t(settings.manual)+delta;if(value<0||value>999999)return false;settings.manual=value;saveAt=millis()+2000;return true;}
void flush(){if(saveAt&&save(settings))saveAt=0;}
void hear(bool wifi,uint64_t address,int rssi,bool personal){if(!(wifi?settings.wifi:settings.ble)||rssi<settings.rssi||(!wifi&&settings.personal&&!personal))return;unsigned kind=wifi?1:0;uint32_t now=millis(),id=hash(address);Seen* slot=nullptr;for(auto& entry:seen[kind]){if(entry.hash==id){slot=&entry;break;}if(!slot&&(!entry.hash||now-entry.at>=settings.window*1000u))slot=&entry;}if(!slot){noteDropped(wifi,1);return;}*slot={id,now,int8_t(rssi)};}
unsigned count(bool wifi){unsigned n=0;uint32_t now=millis();for(const auto& entry:seen[wifi?1:0])if(entry.hash&&now-entry.at<settings.window*1000u&&entry.rssi>=settings.rssi)n++;return n;}
void noteDropped(bool wifi,unsigned n){if(!(wifi?settings.wifi:settings.ble)||!n)return;unsigned kind=wifi?1:0;fullAt[kind]=millis()?millis():1;lost[kind]+=n;}
bool saturated(bool wifi){unsigned kind=wifi?1:0;return fullAt[kind]&&millis()-fullAt[kind]<settings.window*1000u;}uint32_t dropped(bool wifi){return saturated(wifi)?lost[wifi?1:0]:0;}
void tick(){if(saveAt&&int32_t(millis()-saveAt)>=0)flush();wifiTick();}
String json(){StaticJsonDocument<768>d;d["manual"]=settings.manual;d["ble"]=settings.ble;d["wifi"]=settings.wifi;d["personal_only"]=settings.personal;d["window"]=settings.window;d["rssi"]=settings.rssi;d["ble_devices"]=count(false);d["wifi_devices"]=count(true);d["ble_saturated"]=saturated(false);d["wifi_saturated"]=saturated(true);d["ble_dropped"]=dropped(false);d["wifi_dropped"]=dropped(true);d["ble_state"]=!settings.ble?"off":radar.ble==Radar::BleReady?"listening":radar.ble==Radar::BleFailed?"failed":"paused";d["wifi_state"]=wifiState();d["capacity"]=Max;
#if defined(MM_NO_WIFI)
 d["wifi_supported"]=false;
#else
 d["wifi_supported"]=true;
#endif
 String out;serializeJson(d,out);return out;}
String command(JsonObjectConst values){String action=values["action"]|"";if(action=="change"){if(!values["delta"].is<int>()||(values["delta"].as<int>()<-100||values["delta"].as<int>()>100)||!change(values["delta"]))return "ERR people counter range";return "OK people counter changed";}if(action=="window_reset"){resetWindow();return "OK people window reset";}
 Settings next=settings;if(action=="reset")next.manual=0;else if(action=="settings"){for(JsonPairConst pair:values){String key=pair.key().c_str();JsonVariantConst v=pair.value();if(key=="action")continue;if(key=="ble"||key=="wifi"||key=="personal_only"){if(!v.is<bool>())return "ERR people boolean";if(key=="ble")next.ble=v;if(key=="wifi")next.wifi=v;if(key=="personal_only")next.personal=v;}else if(key=="window"||key=="rssi"){if(!v.is<int>())return "ERR people integer";int n=v;if(key=="window"){if(n<0||n>300)return "ERR people window";next.window=n;}else{if(n<-100||n>-30)return "ERR people RSSI";next.rssi=n;}}else return "ERR people setting";}}else return "ERR people action change|reset|window_reset|settings";
#if defined(MM_NO_WIFI)
 if(next.wifi)return "ERR board has no Wi-Fi";
#endif
 if(!valid(next))return "ERR people: window 30|60|300, RSSI -100..-30";if(!save(next))return "ERR people storage";bool reset=next.window!=settings.window||next.rssi!=settings.rssi||next.personal!=settings.personal;bool stopBle=settings.ble&&!next.ble;settings=next;saveAt=0;if(reset)resetWindow();if(stopBle&&!radar.active)radar.releaseBle();if(!next.wifi)releaseWifi();return "OK people settings saved";
}
}
