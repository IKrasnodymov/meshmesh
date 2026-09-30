#include "Radar.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <math.h>
// Wi-Fi CSI motion sensing (see Radar.h). The beacon is identified by its payload, so any MeshMesh
// board can be the beacon; the sensor learns the sender address from the first frame it receives.
namespace {
constexpr uint8_t channel=1; // the access point channel, so a board serving Wi-Fi can still be the beacon
const uint8_t broadcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};
const char signature[]="MMCSI";
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
esp_timer_handle_t sender=nullptr;uint32_t sequence=0;wifi_interface_t senderIf=WIFI_IF_STA;
uint8_t beaconMac[6];bool beaconKnown=false;
float previous[52],accumulated[52];unsigned frames=0;bool havePrevious=false;
float jitterSum=0;uint32_t jitterCount=0,packets=0,changes=0;int32_t rssiSum=0;int8_t lastRaw[128];
void send(void*){uint8_t payload[12];memcpy(payload,signature,5);memcpy(payload+5,&sequence,4);sequence++;
 if(esp_now_send(broadcast,payload,sizeof payload)==ESP_OK){taskENTER_CRITICAL(&guard);packets++;taskEXIT_CRITICAL(&guard);}}
void received(const uint8_t* mac,const uint8_t* data,int size){
 radar.csiNowFrames++;if(size<5||memcmp(data,signature,5))return;
 taskENTER_CRITICAL(&guard);if(!beaconKnown||memcmp(beaconMac,mac,6)){memcpy(beaconMac,mac,6);beaconKnown=true;havePrevious=false;frames=0;memset(accumulated,0,sizeof accumulated);}taskEXIT_CRITICAL(&guard);
}
void sent(const uint8_t*,esp_now_send_status_t status){if(status==ESP_NOW_SEND_SUCCESS)radar.csiSentOk++;else radar.csiSentFail++;}
void sniffed(void* buffer,wifi_promiscuous_pkt_type_t type){auto* p=(const wifi_promiscuous_pkt_t*)buffer;if(type==WIFI_PKT_MGMT)radar.csiMgmtFrames++;if(type==WIFI_PKT_MGMT&&p->rx_ctrl.sig_len>24&&p->payload[0]==0xd0)radar.csiActionFrames++;}
// LLTF: 64 subcarriers as (imaginary, real) int8 pairs, indices 0..31 then -32..-1. The 52 data
// subcarriers are 1..26 and 38..63; the first word (subcarriers 0 and 1) may be invalid.
void onCsi(void*,wifi_csi_info_t* info){
 radar.csiAnyFrames++;if(!info||!info->buf||info->len<128)return;
 bool known;taskENTER_CRITICAL(&guard);known=beaconKnown&&!memcmp(info->mac,beaconMac,6);taskEXIT_CRITICAL(&guard);if(!known)return;
 bool changed=memcmp(lastRaw,info->buf,sizeof lastRaw)!=0;memcpy(lastRaw,info->buf,sizeof lastRaw);
 float a[52];unsigned n=0;
 for(int k=1;k<64;k++){if(k>26&&k<38)continue;if(k==1&&info->first_word_invalid){a[n++]=0;continue;}float im=info->buf[2*k],re=info->buf[2*k+1];a[n++]=sqrtf(im*im+re*re);}
 // Five frames (100 ms) are averaged: single frames carry 8..18-unit amplitudes whose rounding noise
 // alone gives a jitter near 0.02; motion changes the channel well within 100 ms.
 for(unsigned i=0;i<52;i++)accumulated[i]+=a[i];float jitter=-1;
 if(++frames==5){for(unsigned i=0;i<52;i++)a[i]=accumulated[i]/5;memset(accumulated,0,sizeof accumulated);frames=0;
  if(havePrevious){float ma=0,mb=0;for(unsigned i=0;i<n;i++){ma+=a[i];mb+=previous[i];}ma/=n;mb/=n;
   float sab=0,saa=0,sbb=0;for(unsigned i=0;i<n;i++){float x=a[i]-ma,y=previous[i]-mb;sab+=x*y;saa+=x*x;sbb+=y*y;}
   if(saa>0&&sbb>0)jitter=1-sab/sqrtf(saa*sbb);}
  memcpy(previous,a,sizeof a);havePrevious=true;}
 taskENTER_CRITICAL(&guard);packets++;changes+=changed;rssiSum+=info->rx_ctrl.rssi;if(jitter>=0){jitterSum+=jitter;jitterCount++;}taskEXIT_CRITICAL(&guard);
}
}
void Radar::setCsi(CsiRole role){if(role==csi)return;csiStop();if(role!=CsiOff)untrack();csi=role;activity=0;moving=false;csiRate=0;csiLast=0;dirty=true;}
void Radar::csiStart(){
 if(csiRunning)return;bool ap=wifi==WifiPortal;
 if(csi==CsiSensor&&wifi!=WifiReady)return;
 if(csi==CsiBeacon&&wifi!=WifiReady&&!ap)return;
 taskENTER_CRITICAL(&guard);jitterSum=0;jitterCount=0;packets=0;changes=0;rssiSum=0;beaconKnown=false;havePrevious=false;frames=0;memset(accumulated,0,sizeof accumulated);taskEXIT_CRITICAL(&guard);
 if(!ap){ // a station not connected to any network may change channel while promiscuous
  wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT};esp_wifi_set_promiscuous_filter(&filter);esp_wifi_set_promiscuous_rx_cb(csi==CsiSensor?sniffed:nullptr);
  if(esp_wifi_set_promiscuous(true)!=ESP_OK||esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK){esp_wifi_set_promiscuous(false);return;}}
 if(esp_now_init()!=ESP_OK){if(!ap)esp_wifi_set_promiscuous(false);return;}
 csiRunning=true; // from here csiStop() undoes a partial start
 if(csi==CsiBeacon){
  senderIf=ap?WIFI_IF_AP:WIFI_IF_STA;esp_now_peer_info_t peer{};memcpy(peer.peer_addr,broadcast,6);peer.channel=0;peer.ifidx=senderIf;
  esp_now_register_send_cb(sent);esp_now_add_peer(&peer);esp_wifi_config_espnow_rate(senderIf,WIFI_PHY_RATE_6M); // OFDM (802.11b frames carry no CSI); MCS0 frames never reached the air
  esp_timer_create_args_t args{};args.callback=send;args.name="csi-beacon";
  if(esp_timer_create(&args,&sender)!=ESP_OK||esp_timer_start_periodic(sender,20000)!=ESP_OK){csiStop();return;}
 }else{
  esp_now_register_recv_cb(received);
  wifi_csi_config_t config{};config.lltf_en=true;config.htltf_en=false;config.stbc_htltf2_en=false;config.ltf_merge_en=false;config.channel_filter_en=false;config.manu_scale=false;
  if(esp_wifi_set_csi_config(&config)!=ESP_OK||esp_wifi_set_csi_rx_cb(onCsi,nullptr)!=ESP_OK||esp_wifi_set_csi(true)!=ESP_OK){csiStop();return;}
 }
 csiWindowAt=csiRateAt=millis();csiPackets=0;
}
void Radar::csiStop(){
 if(sender){esp_timer_stop(sender);esp_timer_delete(sender);sender=nullptr;esp_wifi_config_espnow_rate(senderIf,WIFI_PHY_RATE_1M_L);} // back to the default rate
 if(csiRunning){esp_wifi_set_csi(false);esp_wifi_set_csi_rx_cb(nullptr,nullptr);esp_now_unregister_recv_cb();esp_now_deinit();if(!sniffing&&wifi==WifiReady)esp_wifi_set_promiscuous(false);}
 csiRunning=false;
}
void Radar::csiTick(uint32_t now){
 bool usable=csi==CsiSensor?wifi==WifiReady:(wifi==WifiReady||wifi==WifiPortal);
 if(!usable){if(csiRunning){csiStop();dirty=true;}return;}
 if(!csiRunning){csiStart();return;}
 if(now-csiWindowAt>=500){csiWindowAt=now;{uint8_t primary=0;wifi_second_chan_t second;if(esp_wifi_get_channel(&primary,&second)==ESP_OK)csiChannel=primary;}float sum;uint32_t count,got,changed;int32_t rssi;
  taskENTER_CRITICAL(&guard);sum=jitterSum;count=jitterCount;got=packets;changed=changes;rssi=rssiSum;jitterSum=0;jitterCount=0;packets=0;changes=0;rssiSum=0;taskEXIT_CRITICAL(&guard);
  csiPackets+=got;
  if(csi==CsiSensor&&got){csiLast=now;csiRssi=constrain(rssi/int32_t(got),-127,0);}
  // Frozen CSI (frames arrive, data never changes) is not "still": skip it and restart capture after 3 s.
  bool frozen=csi==CsiSensor&&got>=10&&!changed;if(frozen!=csiStale)dirty=true;csiStale=frozen;staleWindows=frozen?staleWindows+1:0;
  if(staleWindows>=6){staleWindows=0;csiRestarts++;csiStop();csiStart();return;}
  if(csi==CsiSensor&&count&&!frozen)addActivity(sum/count,now);
 }
 if(now-csiRateAt>=1000){csiRate=csiPackets*1000/(now-csiRateAt);csiPackets=0;csiRateAt=now;dirty=true;}
}
