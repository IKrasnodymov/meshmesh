#include "Config.h"
#include <esp_system.h>
#include <bootloader_random.h>
#include <Mm1Packet.h>
Config config;
bool Config::valid() const {
  return isfinite(frequency) && frequency>=863.0f && frequency<=870.0f &&
    (bandwidth==62.5f || bandwidth==125.0f || bandwidth==250.0f || bandwidth==500.0f) &&
    sf>=7 && sf<=12 && cr>=5 && cr<=8 && power>=0 && power<=MM_MAX_POWER && hops<=7 && brightness>=10 &&
    (autoLock==0 || (autoLock>=30&&autoLock<=600)) && (dimAfter==0||(dimAfter>=10&&dimAfter<=600)) && utcOffset>=-720&&utcOffset<=840&&utcOffset%15==0 &&
    strnlen(name,sizeof(name))>0 && strnlen(name,sizeof(name))<sizeof(name) &&
    meshmesh::validUtf8((const uint8_t*)name,strlen(name));
}
void Config::load() {
  Preferences p; if(!p.begin("meshmesh",false)) {bootCounter=0;Serial.println("ERR NVS unavailable; TX disabled");return;}
  p.getString("name",name,sizeof(name));
  if(!name[0]) strcpy(name,MM_NODE_NAME);
  frequency=p.getFloat("freq",868.731f); bandwidth=p.getFloat("bw",62.5f);
  sf=p.getUChar("sf",8); cr=p.getUChar("cr",6); power=p.getChar("power",10);
  hops=p.getUChar("hops",3); relay=p.getBool("relay",true); gps=p.getBool("gps",MM_GPS_DEFAULT);
  sound=p.getBool("sound",true); batteryVolts=p.getBool("bat_v",false); brightness=p.getUChar("light",180);
  autoLock=p.getUShort("lock",90);dimAfter=p.getUShort("dim",30);
  utcOffset=p.getShort("utc_offset",180);
  // Language: "lang" since 0.3.7; older versions kept only "russian".
  lang=p.isKey("lang")?p.getUChar("lang",LangEn):p.getBool("russian",false)?LangRu:LangEn;if(lang>=LangCount)lang=LangEn;
  // The site installer's choice applies once per installation; afterwards Settings decide.
  String chosen=installLanguage();if(chosen.length()&&p.getString("inst_lang","")!=chosen){lang=langFromCode(chosen);p.putUChar("lang",lang);p.putString("inst_lang",chosen);}
  role=p.getUChar("role",RoleNormal);if(role>=RoleCount)role=RoleNormal;
  if(p.getBytesLength("key")==32) p.getBytes("key",key,32);
  else {bootloader_random_enable();esp_fill_random(key,32);bootloader_random_disable();p.putBytes("key",key,32);}
  bootCounter=p.getUInt("boot",0)+1;
  if(!bootCounter || p.putUInt("boot",bootCounter)!=sizeof(bootCounter)) {bootCounter=0;Serial.println("ERR boot counter; TX disabled");}
  p.end();
  if(!valid()) {frequency=868.731f;bandwidth=62.5f;sf=8;cr=6;power=10;hops=3;strcpy(name,"MeshMesh");}
}
void Config::save() {
  Preferences p; if(!p.begin("meshmesh",false)) return;
  p.putString("name",name); p.putFloat("freq",frequency); p.putFloat("bw",bandwidth);
  p.putUChar("sf",sf); p.putUChar("cr",cr); p.putChar("power",power); p.putUChar("hops",hops);
  p.putBool("relay",relay);p.putBool("gps",gps);p.putBool("sound",sound);p.putBool("bat_v",batteryVolts);p.putUChar("lang",lang);p.putBool("russian",lang==LangRu);
  p.putUChar("light",brightness);p.putUShort("lock",autoLock);p.putUShort("dim",dimAfter);p.putShort("utc_offset",utcOffset);p.putBytes("key",key,32);p.end();
}
bool Config::saveRole(uint8_t next){if(next>=RoleCount)return false;Preferences p;if(!p.begin("meshmesh",false))return false;bool saved=p.putUChar("role",next)==1;p.end();return saved;} // config.role keeps the running role
String Config::keyHex() const {String s; s.reserve(64);char b[3];for(auto v:key) {snprintf(b,3,"%02x",v);s+=b;}return s;}
bool Config::setKey(const String& text) {
  if(text.length()!=64) return false;
  uint8_t next[32];
  for(int i=0;i<32;i++) {
    unsigned value=0;
    for(int j=0;j<2;j++) {char c=text[i*2+j];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return false;value=value*16+n;}
    next[i]=value;
  }
  memcpy(key,next,32);return true;
}
