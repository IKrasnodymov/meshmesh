#include "People.h"
#if defined(MM_NO_WIFI)
namespace people {void wifiTick(){}void releaseWifi(){}bool wifiRunning(){return false;}const char* wifiState(){return "unsupported";}}
#else
#include "App.h"
#include "Radar.h"
#include "Internet.h"
#include "WifiDiagnostics.h"
#include <WiFi.h>
#include <esp_wifi.h>
namespace people {
namespace {
bool running=false;uint8_t channel=1;uint32_t hopAt=0;const char* state="off";
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
struct Frame {uint64_t address;int8_t rssi;};Frame queue[32];unsigned head=0,tail=0;uint32_t overflow=0;
void receive(void* buffer,wifi_promiscuous_pkt_type_t type){auto* p=static_cast<wifi_promiscuous_pkt_t*>(buffer);if(p->rx_ctrl.sig_len<24)return;const uint8_t* b=p->payload;
 // Probe requests come from clients. Data is useful only with To DS set and From DS clear.
 if(!((type==WIFI_PKT_MGMT&&(b[0]&0xfc)==0x40)||(type==WIFI_PKT_DATA&&(b[1]&3)==1)))return;
 const uint8_t* source=b+10;if(source[0]&1)return;uint64_t address=0;for(unsigned i=0;i<6;i++)address=address<<8|source[i];if(!address)return;
 taskENTER_CRITICAL(&guard);unsigned next=(head+1)%32;if(next==tail)overflow++;else{queue[head]={address,p->rx_ctrl.rssi};head=next;}taskEXIT_CRITICAL(&guard);
}
bool busy(){return portalActive()||radar.active||radar.csi!=Radar::CsiOff||wifiProbeActive()||internet.enabled||internet.periodicActive();}
}
bool wifiRunning(){return running;}const char* wifiState(){return state;}
void releaseWifi(){if(!running)return;esp_wifi_set_promiscuous(false);esp_wifi_set_promiscuous_rx_cb(nullptr);running=false;taskENTER_CRITICAL(&guard);head=tail=0;taskEXIT_CRITICAL(&guard);if(!portalActive()&&!radar.active&&!wifiProbeActive()&&!internet.online())WiFi.mode(WIFI_OFF);state=settings.wifi?"paused":"off";}
void wifiTick(){if(!settings.wifi||busy()){releaseWifi();state=!settings.wifi?"off":"paused";return;}if(!running){if(!WiFi.mode(WIFI_STA)){state="failed";return;}WiFi.setSleep(true);wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA};esp_wifi_set_promiscuous_filter(&filter);esp_wifi_set_promiscuous_rx_cb(receive);if(esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE)!=ESP_OK||esp_wifi_set_promiscuous(true)!=ESP_OK){esp_wifi_set_promiscuous(false);WiFi.mode(WIFI_OFF);state="failed";return;}running=true;channel=1;hopAt=millis();state="listening";}
 uint32_t missed;taskENTER_CRITICAL(&guard);missed=overflow;overflow=0;taskEXIT_CRITICAL(&guard);noteDropped(true,missed);
 for(unsigned i=0;i<32;i++){Frame frame;taskENTER_CRITICAL(&guard);bool any=head!=tail;if(any){frame=queue[tail];tail=(tail+1)%32;}taskEXIT_CRITICAL(&guard);if(!any)break;hear(true,frame.address,frame.rssi);}
 if(millis()-hopAt>=1000){hopAt=millis();wifi_country_t country{};unsigned first=1,last=11;if(esp_wifi_get_country(&country)==ESP_OK&&country.nchan){first=country.schan;last=country.schan+country.nchan-1;}channel=channel>=last?first:channel+1;esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);}
}
}
#endif
