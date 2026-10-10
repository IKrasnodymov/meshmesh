#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: not speed-critical (AGENTS.md, "Языки")
#endif
#include "Radar.h"
#include <ArduinoJson.h>
// Target table and homing statistics. No radio access here, so the host UI preview links this file.
RadarTarget* Radar::upsert(RadarTarget::Kind kind,uint64_t id){
 for(unsigned i=0;i<count;i++)if(targets[i].kind==kind&&targets[i].id==id)return &targets[i];
 if(count==MaxTargets)return nullptr;RadarTarget& t=targets[count++];t={};t.kind=kind;t.id=id;return &t;
}
// Access points and Bluetooth devices missing for 20 s leave the table; the homing target stays.
void Radar::prune(uint32_t now){
 unsigned n=0;for(unsigned i=0;i<count;i++){const RadarTarget& t=targets[i];bool focused=tracking&&t.kind==focus.kind&&t.id==focus.id;
  if(t.kind==RadarTarget::Lora||focused||now-t.seen<20000)targets[n++]=t;}
 count=n;
}
void Radar::sort(){for(unsigned i=1;i<count;i++)for(unsigned j=i;j>0&&targets[j].rssi>targets[j-1].rssi;j--)std::swap(targets[j],targets[j-1]);}
void Radar::reset(){samples=0;lastSample=0;fast=slow=0;peak=-127;rate=0;trendState=candidate=0;candidateAt=0;}
void Radar::addSample(int rssi,uint32_t at){
 int8_t r=constrain(rssi,-127,0);bool lora=focus.kind==RadarTarget::Lora;
 // Wi-Fi and BLE: the median of the last five 100 ms windows rejects multipath spikes.
 // LoRa samples arrive per packet, seconds or minutes apart, and are used as they are.
 recent[samples%5]=r;int8_t value=r;
 if(!lora){unsigned n=samples<5?samples+1:5;int8_t v[5];memcpy(v,recent,n);for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&v[j]<v[j-1];j--)std::swap(v[j],v[j-1]);value=v[n/2];}
 if(!samples){fast=slow=value;}else{fast+=(value-fast)*(lora?.6f:.35f);slow+=(value-slow)*(lora?.25f:.06f);}
 if(!samples||fast>peak)peak=fast;
 history[samples%HistorySize]=value;samples++;lastSample=at;focus.rssi=value;focus.seen=at;dirty=true;
 for(unsigned i=0;i<count;i++)if(targets[i].kind==focus.kind&&targets[i].id==focus.id){targets[i].rssi=value;targets[i].seen=at;}
 // Trend: enter at 2.5 dB, leave below 1.5 dB; a new state must hold for 1.2 s (LoRa: at once).
 float d=fast-slow;int8_t raw=d>=2.5f?1:d<=-2.5f?-1:0;if(trendState&&raw==0&&d*trendState>1.5f)raw=trendState;
 if(samples<(lora?3u:6u)){trendState=candidate=0;return;}
 if(lora||raw==trendState){trendState=candidate=raw;return;}
 if(raw!=candidate){candidate=raw;candidateAt=at;}else if(at-candidateAt>=1200)trendState=raw;
}
int8_t Radar::sample(unsigned i) const{return history[(samples-historyCount()+i)%HistorySize];}
bool Radar::fresh() const{return tracking&&samples&&millis()-lastSample<(focus.kind==RadarTarget::Lora?600000UL:1500UL);}
int Radar::trend() const{return fresh()?trendState:0;}
unsigned Radar::counted(RadarTarget::Kind kind) const{unsigned n=0;for(unsigned i=0;i<count;i++)n+=targets[i].kind==kind;return n;}
unsigned Radar::personal(uint32_t within) const{unsigned n=0;uint32_t now=millis();for(unsigned i=0;i<count;i++){const RadarTarget& t=targets[i];n+=t.kind==RadarTarget::Ble&&t.device!=RadarTarget::Unknown&&now-t.seen<within;}return n;}
// Appearance categories (Bluetooth Assigned Numbers): 1 phone, 3 watch, 37 audio sink/earbuds.
// Maker formats: Apple 0x004C (0x07 proximity pairing = AirPods, 0x10/0x0C nearby/handoff = iPhone
// or Apple Watch), Garmin 0x0087 watches; Samsung, Google, Huawei and Xiaomi are phones or wearables.
RadarTarget::Device Radar::classify(uint16_t appearance,const uint8_t* maker,size_t makerSize){
 switch(appearance>>6){case 1:return RadarTarget::Phone;case 3:return RadarTarget::Watch;case 37:return RadarTarget::Audio;}
 if(makerSize<2)return RadarTarget::Unknown;uint16_t company=maker[0]|maker[1]<<8;
 if(company==0x004c&&makerSize>2){uint8_t type=maker[2];if(type==0x07)return RadarTarget::Audio;if(type==0x10||type==0x0c)return RadarTarget::Personal;return RadarTarget::Unknown;}
 if(company==0x0087)return RadarTarget::Watch;
 if(company==0x0075||company==0x00e0||company==0x027d||company==0x038f)return RadarTarget::Personal;
 return RadarTarget::Unknown;
}
// CSI motion: activity is the mean jitter (1 - correlation of consecutive channel responses) per
// 0.5 s. Ten seconds of an empty, still area give the baseline; motion is 2.2 times it (at least
// +0.004), and it stays reported for 2 s after the last window above the threshold. Measured on
// M9 <- Heltec at -40 dBm: still 0.005-0.011 (a person standing elsewhere raises it), walking
// between the boards 0.013-0.039.
float Radar::motionThreshold() const{return baseline>0?max(baseline*2.2f,baseline+.004f):.015f;}
bool Radar::beaconHeard() const{return csi==CsiSensor&&csiLast&&millis()-csiLast<2000;}
float Radar::motion(unsigned i) const{return motionLog[(motionSamples-motionCount()+i)%MotionHistory];}
void Radar::calibrate(){calibrateUntil=millis()+10000;if(!calibrateUntil)calibrateUntil=1;calibrationSum=0;calibrationCount=0;moving=false;dirty=true;}
void Radar::addActivity(float value,uint32_t at){
 motionLog[motionSamples%MotionHistory]=value;motionSamples++;activity=value;dirty=true;
 if(calibrateUntil){calibrationSum+=value;calibrationCount++;if(int32_t(at-calibrateUntil)>=0){baseline=calibrationSum/calibrationCount;calibrateUntil=0;}moving=false;return;}
 if(value>motionThreshold()){moving=true;movingAt=at;}else if(moving&&at-movingAt>=2000)moving=false;
}
// USB diagnostics: counts, strengths, channels and mesh node IDs; nearby networks and
// Bluetooth addresses stay on the screen and are never exported.
String Radar::json() const{
 StaticJsonDocument<2048> d;const char* states[]={"off","ready","portal","busy","failed"},*bleStates[]={"off","ready","busy","failed"},*kinds[]={"wifi","ble","lora"},*devices[]={"unknown","phone","watch","audio","personal"};uint32_t now=millis();
 d["active"]=active;d["wifi"]=states[wifi];d["ble"]=bleStates[ble];d["sweeps"]=sweeps;d["scan_start"]=scanStart;d["scan_result"]=scanResult;d["scan_ms"]=scanMs;d["done_status"]=doneStatus;d["done_number"]=doneNumber;
 d["wifi_targets"]=counted(RadarTarget::Wifi);d["ble_targets"]=counted(RadarTarget::Ble);d["lora_targets"]=counted(RadarTarget::Lora);d["personal"]=personal(30000);
 auto describe=[&](JsonObject o,const RadarTarget& t){o["kind"]=kinds[t.kind];o["rssi"]=t.rssi;
  if(t.kind==RadarTarget::Wifi)o["channel"]=t.channel;if(t.kind==RadarTarget::Ble)o["device"]=devices[t.device];
  if(t.kind==RadarTarget::Lora){char node[17];snprintf(node,sizeof node,"%04lX%08lX",(unsigned long)(t.id>>32),(unsigned long)(t.id&0xffffffffu));o["node"]=node;}
  o["meshmesh"]=t.kind==RadarTarget::Lora||(t.kind==RadarTarget::Ble?!strncmp(t.name,"MeshCore-",9)||!strncmp(t.name,"MeshMesh ",9):!strncmp(t.name,"MM-",3));o["age_ms"]=now-t.seen;};
 JsonArray a=d.createNestedArray("strongest");for(unsigned i=0;i<count&&i<10;i++)describe(a.createNestedObject(),targets[i]);
 d["tracking"]=tracking;
 if(tracking){JsonObject f=d.createNestedObject("focus");describe(f,focus);f["samples"]=samples;f["rate"]=rate;f["smoothed"]=serialized(String(fast,1));f["peak"]=serialized(String(peak,1));f["trend"]=trend();f["fresh"]=fresh();}
 const char* roles[]={"off","beacon","sensor"};JsonObject c=d.createNestedObject("csi");c["role"]=roles[csi];c["running"]=csiRunning;c["rate"]=csiRate;c["channel"]=csiChannel;c["now_frames"]=csiNowFrames;c["action_frames"]=csiActionFrames;c["mgmt_frames"]=csiMgmtFrames;c["sent_ok"]=csiSentOk;c["sent_fail"]=csiSentFail;c["csi_frames"]=csiAnyFrames;
 if(csi==CsiSensor){c["heard"]=beaconHeard();c["rssi"]=csiRssi;c["activity"]=serialized(String(activity,6));c["stale"]=csiStale;c["restarts"]=csiRestarts;c["beacon_hz"]=beaconHz;c["stream"]=csiStream;c["stream_dropped"]=csiStreamDropped;c["baseline"]=serialized(String(baseline,4));c["threshold"]=serialized(String(motionThreshold(),4));c["moving"]=moving;c["calibrating"]=calibrateUntil!=0;c["windows"]=motionSamples;}
 String s;serializeJson(d,s);return s;
}
String Radar::webJson() const{
 DynamicJsonDocument d(16384);const char* states[]={"off","ready","portal","busy","failed"},*bleStates[]={"off","ready","busy","failed"},*kinds[]={"wifi","ble","lora"},*devices[]={"unknown","phone","watch","audio","personal"},*roles[]={"off","beacon","sensor"};uint32_t now=millis();
 d["active"]=active;d["wifi"]=states[wifi];d["ble"]=bleStates[ble];d["sweeps"]=sweeps;d["personal"]=personal(30000);d["tracking"]=tracking;
 auto describe=[&](JsonObject o,const RadarTarget& t){o["kind"]=kinds[t.kind];o["ref"]=placement(t);o["name"]=t.name;o["rssi"]=t.rssi;o["age_ms"]=now-t.seen;
  if(t.kind==RadarTarget::Wifi){o["channel"]=t.channel;o["open"]=t.open;}
  if(t.kind==RadarTarget::Ble){o["device"]=devices[t.device];if(t.vendor!=0xffff)o["vendor"]=t.vendor;}
  if(t.kind==RadarTarget::Lora){char node[17];snprintf(node,sizeof node,"%04lX%08lX",(unsigned long)(t.id>>32),(unsigned long)(t.id&0xffffffffu));o["node"]=node;}};
 JsonArray a=d.createNestedArray("targets");for(unsigned i=0;i<count&&i<40;i++)describe(a.createNestedObject(),targets[i]);
 if(tracking){JsonObject f=d.createNestedObject("focus");describe(f,focus);f["samples"]=samples;f["rate"]=rate;f["fast"]=serialized(String(fast,1));f["peak"]=serialized(String(peak,1));f["trend"]=trend();f["fresh"]=fresh();f["sample_age_ms"]=samples?now-lastSample:0;
  JsonArray h=f.createNestedArray("history");for(unsigned i=0;i<historyCount();i++)h.add(sample(i));}
 JsonObject c=d.createNestedObject("csi");c["role"]=roles[csi];c["running"]=csiRunning;c["rate"]=csiRate;
 if(csi==CsiSensor){c["heard"]=beaconHeard();c["rssi"]=csiRssi;c["stale"]=csiStale;c["moving"]=moving;c["activity"]=serialized(String(activity*1000,2));c["threshold"]=serialized(String(motionThreshold()*1000,2));c["calibrated"]=baseline>0;
  if(calibrateUntil)c["calibrating_ms"]=int32_t(calibrateUntil-now)>0?calibrateUntil-now:0;
  JsonArray m=c.createNestedArray("history");for(unsigned i=0;i<motionCount();i++)m.add(serialized(String(motion(i)*1000,2)));}
 String s;serializeJson(d,s);return s;
}
