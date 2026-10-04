// Host renderer for the production screen UI. It links the real src/Ui.cpp
// (or src/UiHeltec.cpp) against in-memory state and writes PPM frames, so
// layout can be reviewed without a device. It is not a hardware check.
#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "Maps.h"
#include "Navigation.h"
#include "Wire.h"
#include <time.h>
#include <string>
#include <vector>
HardwareSerialShim Serial;EspShim ESP;SPIClass SPI;TwoWire Wire,Wire1;
size_t Print::print(const String& s){return write((const uint8_t*)s.c_str(),s.length());}
static uint32_t fakeMillis=100000;
uint32_t millis(){return fakeMillis;}
Config config;Hardware hardware;MeshRadio meshRadio;Maps maps;Navigation navigation;
void Config::load(){}void Config::save(){}bool Config::valid() const{return true;}
String Config::keyHex() const{return String("00");}bool Config::setKey(const String&){return false;}
static bool wifiOn=false,bleOn=false;
bool portalActive(){return wifiOn;}String portalPassword(){return "preview-pass";}void portalToggle(){wifiOn=!wifiOn;}
bool bleActive(){return bleOn;}void bleToggle(){bleOn=!bleOn;}uint32_t blePin(){return 123456;}
String configJson(bool){return "{}";}
String applySettings(JsonObjectConst){return "OK settings saved";}
// Hardware
bool Hardware::gpsFix(){return gps.location.isValid();}
void Hardware::brightness(uint8_t){}void Hardware::beep(){}void Hardware::setGps(bool){}
void Hardware::flush(){}
void Hardware::text(int x,int y,const String& v,uint16_t color){font.setForegroundColor(color);font.setCursor(x,y);font.print(v);}
void Hardware::line(int y,const String& v,uint16_t color){text(
#if defined(MM_HELTEC_V4)
0
#else
12
#endif
,y,v,color);}
// Radio
static uint32_t nextId=900;
bool MeshRadio::sendMessage(const String& text,uint64_t destination){ChatMessage m;m.source=nodeId;m.destination=destination;m.outgoing=true;m.status=ChatMessage::Queued;m.timestamp=time(nullptr);m.id=++nextId;strncpy(m.name,"M9",24);strncpy(m.text,text.c_str(),160);addMessage(m,false);event="Queued: waiting for delivery";return true;}
bool MeshRadio::sendHello(){txCount++;return true;}
bool MeshRadio::sendPosition(){return hardware.gpsFix();}
bool MeshRadio::selfTest(){return true;}
bool MeshRadio::busy() const{return false;}
String MeshRadio::idText(uint64_t id) const{if(id==meshmesh::Broadcast)return "ALL";char b[17];snprintf(b,sizeof b,"%012llX",(unsigned long long)id);return b;}
String MeshRadio::publicKeyText() const{return "5a1f0c9e77d24b0e8a41c3f2d9b6e0717a3c55e2b1d04f86c9e2a7b3d1f06e44";}
unsigned MeshRadio::messageLimit(uint64_t d) const{return d==meshmesh::Broadcast?151-4:151;}
void MeshRadio::addMessage(const ChatMessage& m,bool){if(historyCount==64){memmove(history,history+1,sizeof(ChatMessage)*63);historyCount=63;}history[historyCount++]=m;dirty=true;}
Peer* MeshRadio::contact(uint64_t id){for(unsigned i=0;i<peerCount;i++)if(peers[i].id==id)return &peers[i];return nullptr;}
#include "preview_hooks.inc"
// Maps: a synthetic street grid instead of SD tiles.
void Maps::begin(){}void Maps::tick(){}
void Maps::center(double lat,double lon){latitude=lat;longitude=lon;haveCenter=true;dirty=true;}
void Maps::pan(int dx,int dy){follow=false;double scale=256.0*(1<<zoom);longitude+=dx*360.0/scale;latitude-=dy*360.0/scale*cos(latitude*M_PI/180);dirty=true;}
void Maps::changeZoom(int d){zoom=constrain(int(zoom)+d,10,17);dirty=true;}
String Maps::areas(){return "[{\"id\":\"kazan\",\"name\":\"Казань\",\"tiles\":1022},{\"id\":\"center\",\"name\":\"Центр 4×4 км\",\"tiles\":218}]";}
bool Maps::selectArea(const String&){title="Казань";return true;}
String Maps::info(){return "{}";}
void Maps::draw(int x,int y,int w,int h){
 auto& c=*hardware.canvas;visibleTiles=available&&haveCenter?4:0;waiting=false;if(!visibleTiles)return;
 c.fillRect(x,y,w,h,0xef5b);
 for(int i=0;i<14;i++){int ry=i*29+(zoom*7)%29,rx=i*37+(zoom*5)%37;if(ry<h-4)c.fillRect(x,y+ry,w,4,0xffff);if(rx<w-3)c.fillRect(x+rx,y,3,h,0xffff);}
 c.fillRect(x+40,y+60,70,40,0xb7d6);c.fillRect(x+180,y+20,60,90,0xc618);c.fillRect(x+220,y+110,90,30,0x9e7f);
 c.fillRect(x,y+h-40,w,14,0xfee0);
}
void Navigation::begin(){}void Navigation::tick(){}void Navigation::start(){calibrating=true;}bool Navigation::finish(){calibrating=false;return true;}String Navigation::info(){return "{}";}
static void save(const std::string& path){
 auto& c=*hardware.canvas;FILE* f=fopen(path.c_str(),"wb");fprintf(f,"P6 %d %d 255\n",c.width(),c.height());
 for(int y=0;y<c.height();y++)for(int x=0;x<c.width();x++){uint16_t p=c.getPixel(x,y);
#if defined(MM_HELTEC_V4)
  uint8_t v=p?255:0;uint8_t rgb[3]={v,v,v};
#else
  uint8_t rgb[3]={uint8_t((p>>11&31)*255/31),uint8_t((p>>5&63)*255/63),uint8_t((p&31)*255/31)};
#endif
  fwrite(rgb,1,3,f);}
 fclose(f);
}
static std::string outDir;
static void tick(){fakeMillis+=1000;radar.tick();uiTick();}
static void key(int k){uiKey(k);tick();}
static void shot(const char* name){tick();save(outDir+"/"+name+".ppm");printf("%s\n",name);}
static uint64_t peerId(int i){return 0xA1B2C3D40000ULL+i*0x1111;}
static void scenario();
int main(int argc,char** argv){
 outDir=argc>1?argv[1]:".";
 config.lang=argc>2?max(0,langFromCode(argv[2])):LangRu; // a language code, Russian by default
#if defined(MM_HELTEC_V4)
 hardware.canvas=new GFXcanvas16(128,64);strcpy(config.name,"Heltec V4");
#if defined(MM_JOYSTICK)
 strcpy(config.name,"GAT562");
#endif
#else
 hardware.canvas=new GFXcanvas16(320,240);strcpy(config.name,"M9");
#endif
 hardware.font.begin(*hardware.canvas);hardware.font.setFont(u8g2_font_6x13_t_cyrillic);hardware.font.setFontMode(1);
 hardware.keyboardOk=hardware.sdOk=hardware.fsOk=hardware.rtcOk=hardware.rtcValid=hardware.compassOk=hardware.imuOk=hardware.compassSample=hardware.imuSample=true;
 hardware.batteryMv=3980;hardware.gps.location={true,55.7963,49.1088};hardware.gps.satellites.v=9;hardware.gps.hdop.v=1.1;hardware.gps.date.valid=true;hardware.gps.sentences=1200;
 meshRadio.ready=true;meshRadio.nodeId=0x5A1F0C9E77D2ULL;meshRadio.rxCount=148;meshRadio.txCount=37;meshRadio.relayed=12;meshRadio.rejected=2;meshRadio.lastRssi=-71;meshRadio.lastSnr=9.5;meshRadio.lastRxAt=fakeMillis-20000;
 // Node names fit Peer::name (24 bytes, as the firmware allows).
 const char* names[]={"Heltec V4","Kazan RPT-1","Комн. Казань","Игорь T-Deck","Sensor-12","Марат"};uint8_t types[]={1,2,3,1,4,1};
 for(int i=0;i<6;i++){auto& p=meshRadio.peers[meshRadio.peerCount++];p.id=peerId(i);strcpy(p.name,names[i]);p.type=types[i];p.heard=i!=4;p.seen=fakeMillis-(i*47000+3000);p.rssi=-58-i*11;p.snr=11-i*3.5f;p.hops=i==0?0:i;p.pathLength=i==0?0:i==2?255:i;p.position=i==0||i==1||i==3;p.latitude=55.7963+0.0011*(i+1)*(i%2?1:-1);p.longitude=49.1088+0.0013*(i+1)*(i%3?1:-1);for(int k=0;k<8;k++)p.publicKey[k]=uint8_t(p.id>>(56-8*k));p.publicKey[8]=0x10*i+3;}
 time_t now=time(nullptr);
 auto add=[&](uint64_t src,uint64_t dst,const char* name,const char* text,bool out,ChatMessage::Status st,int ago){ChatMessage m;m.source=src;m.destination=dst;strcpy(m.name,name);strcpy(m.text,text);m.outgoing=out;m.status=st;m.timestamp=now-ago;m.id=++nextId;meshRadio.history[meshRadio.historyCount++]=m;};
 uint64_t B=meshmesh::Broadcast;
 add(peerId(5),B,"Марат","Всем привет! Кто на связи в районе Кремля?",false,ChatMessage::Received,5400);
 add(meshRadio.nodeId,B,"M9","Я на связи, M9 у Кремлёвской",true,ChatMessage::Sent,5300);
 add(peerId(0),meshRadio.nodeId,"Heltec V4","Радиотест V4 → M9: 1790746076",false,ChatMessage::Received,3600);
 add(meshRadio.nodeId,peerId(0),"M9","Принято, слышу хорошо. SNR около 9 дБ",true,ChatMessage::Delivered,3500);
 add(meshRadio.nodeId,peerId(0),"M9","Проверка длинного сообщения: переносы строк должны аккуратно помещаться внутри пузыря",true,ChatMessage::Failed,1200);
 add(peerId(0),meshRadio.nodeId,"Heltec V4","OK",false,ChatMessage::Received,300);
 add(peerId(3),meshRadio.nodeId,"Игорь T-Deck","Буду через 10 минут",false,ChatMessage::Received,120);
 add(meshRadio.nodeId,peerId(3),"M9","Жду у входа",true,ChatMessage::Sent,60);
 // Channels: Public, a hashtag and a private one with messages; two more heard on air.
 meshRadio.channelCount=1;strcpy(meshRadio.channelList[0].name,"Public");memcpy(meshRadio.channelList[0].secret,channels::publicSecret,16);meshRadio.channelList[0].id=meshmesh::Broadcast;
 {uint64_t tag,priv;meshRadio.joinHashtag("kazan",&tag);uint8_t k[16];for(int i=0;i<16;i++)k[i]=uint8_t(200-i*9);meshRadio.addChannel("Друзья",k,&priv);
  add(peerId(3),tag,"Игорь T-Deck","Кто едет на Кабан в субботу?",false,ChatMessage::Received,2400);
  add(peerId(5),priv,"Марат","Встречаемся в 19:00",false,ChatMessage::Received,900);
  meshRadio.heard[0].hash=0x5A;meshRadio.heard[0].packets=3;meshRadio.heard[0].at=fakeMillis-120000;strcpy(meshRadio.heard[0].name,"#ru");
  meshRadio.heard[1].hash=0x07;meshRadio.heard[1].packets=1;meshRadio.heard[1].at=fakeMillis-600000;meshRadio.heardCount=2;meshRadio.heardSamples=4;
  channels::Channel invite{};strcpy(invite.name,"Походы");for(int i=0;i<16;i++)invite.secret[i]=uint8_t(i*13+5);
  add(peerId(5),meshRadio.nodeId,"Марат",channels::link(invite).c_str(),false,ChatMessage::Received,200);}
 maps.available=true;maps.tileCount=1240;maps.title="Казань";maps.zoom=15;maps.center(55.7963,49.1088);
 navigation.headingValid=true;navigation.heading=37;navigation.calibrated=true;
 uiBegin();scenario();return 0;
}
#include "scenario.inc"
