#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "Radar.h"
#include "ChessNet.h"
#include "MeshServer.h"
#include <math.h>
#include <time.h>
#if defined(MM_NRF52)
#include <esp_system.h> // ESP.getFreeHeap(); Arduino.h provides it on the ESP32
#endif
// One PRG button: click = next screen or next menu item; hold = the screen's action, or its menu.
// Joystick boards (MM_JOYSTICK, GAT562): left/right change the screen, up/down scroll the screen or
// the menu, the centre runs the action or opens the menu, Back closes and goes home; messages are
// written on an on-screen keyboard (UiCompose.inc).
namespace {
#if defined(MM_JOYSTICK)
enum Page {Home,Messages,Nodes,Chess,Signals,Gps,Wifi,Ble,Settings,Modules,PageCount};
const char* pageNames[]={"home","messages","nodes","chess","radar","gps","wifi","ble","settings","modules"};
#else
enum Page {Home,Messages,Nodes,Signals,Gps,Wifi,Ble,Settings,Modules,PageCount};
const char* pageNames[]={"home","messages","nodes","radar","gps","wifi","ble","settings","modules"};
#endif
int page=Home,menuIndex=0,messageOffset=0,nodeIndex=0;bool menuOpen=false,dirty=true,screenOff=false;
uint32_t drawAt=0,lastInput=0,menuAt=0,actionAt=0,popupAt=0,ledAt=0,pingAt=0,pingedSamples=0;String action;
inline __attribute__((always_inline)) void led(bool on){if(pins::led>=0)digitalWrite(pins::led,on?pins::ledOn:!pins::ledOn);}
uint32_t chessPopupAt=0,chessSeen=0; // chess news: Heltec has no board, the game is played on the Wi-Fi page
unsigned unreadCount=0;struct {uint64_t source=0;uint32_t session=0,id=0;} newest;
const uint8_t* activeFont=nullptr;
const uint8_t* const small=u8g2_font_5x8_t_cyrillic;const uint8_t* const body=u8g2_font_6x13_t_cyrillic;const uint8_t* const bold=u8g2_font_6x13B_t_cyrillic;
String t(const char* en,const char* ru){return tr(en,ru);}
// Lengths in 6 px columns: CJK glyphs take two.
unsigned chars(const String& value){unsigned n=0;for(unsigned i=0;i<value.length();)n+=glyphCells(utf8Next(value,i));return n;}
String clipped(const String& value,unsigned count){unsigned i=0,n=0,cut=0;while(i<value.length()){unsigned at=i,w=glyphCells(utf8Next(value,i));if(n+w>count){i=at;break;}if(n+w+2<=count)cut=i;n+=w;}return i<value.length()&&count>2?value.substring(0,cut)+"..":value.substring(0,i);}
void notice(const String& value){action=value;actionAt=millis();dirty=true;}
// SetFont resets transparency; the monochrome canvas treats any non-zero colour as lit.
void useFont(const uint8_t* f){if(f!=activeFont){hardware.font.setFont(f);hardware.font.setFontMode(1);activeFont=f;}}
// Glyph by glyph: what the font lacks comes from its fallback fonts (I18n.h); Arabic in visual order.
int glyph(int x,int y,uint32_t cp,const uint8_t* f,uint16_t color,bool paint){
 const uint8_t* use=cp<=0xffff&&fontHasGlyph(f,cp)?f:nullptr;
 if(!use&&cp<=0xffff)for(const uint8_t* const* x=fallbackFonts(f);*x;x++)if(fontHasGlyph(*x,cp)){use=*x;break;}
 if(!use)return 0;useFont(use);if(!paint)return u8g2_GetGlyphWidth(&hardware.font.u8g2,cp);hardware.font.setForegroundColor(color);return hardware.font.drawGlyph(x,y,cp);
}
int width(const String& value,const uint8_t* f){String s=visualText(value);int w=0;for(unsigned i=0;i<s.length();)w+=glyph(0,0,utf8Next(s,i),f,0,false);return w;}
void say(int x,int y,const String& value,const uint8_t* f=body,uint16_t color=1){String s=visualText(value);for(unsigned i=0;i<s.length();)x+=glyph(x,y,utf8Next(s,i),f,color,true);}
void sayRight(int x,int y,const String& value,const uint8_t* f=small,uint16_t color=1){say(x-width(value,f),y,value,f,color);}
// Up to `maximum` 21-column rows, wrapped at spaces; longer text pages every four seconds.
void textLines(const String& value,int y,unsigned maximum){
 String rows[12];unsigned at=0,count=0;
 while(at<value.length()&&count<12){unsigned n=0,end=at,space=0;while(end<value.length()&&value[end]!='\n'){unsigned next=end;uint32_t c=utf8Next(value,next);if(n+glyphCells(c)>21)break;n+=glyphCells(c);end=next;if(c==' ')space=end;}
  if(end<value.length()&&value[end]!='\n'&&space>at)end=space;rows[count++]=value.substring(at,end);at=end;if(at<value.length()&&value[at]=='\n')at++;}
 unsigned first=count>maximum?(millis()/4000)%(count-maximum+1):0;for(unsigned i=first;i<count&&i<first+maximum;i++)say(0,y+(i-first)*12,rows[i]);
}
String clockText(time_t at){if(at<1700000000)return "--:--";at+=config.utcOffset*60;tm* v=gmtime(&at);char b[6];snprintf(b,sizeof b,"%02d:%02d",v->tm_hour,v->tm_min);return b;}
String ago(uint32_t ms){uint32_t s=ms/1000;if(s<60)return String(s)+t("s","с");if(s<3600)return String(s/60)+t("m","м");if(s<86400)return String(s/3600)+t("h","ч");return String(s/86400)+t("d","д");}
String pathText(const Peer& p){if(p.pathLength==255)return t("path ?","путь ?");if(!(p.pathLength&63))return t("direct","напрямую");return String(p.pathLength&63)+t(" hops"," хоп.");}
String typeText(uint8_t type){switch(type){case 2:return t("repeater","ретранслятор");case 3:return t("room","комната");case 4:return t("sensor","датчик");}return t("chat","чат");}
bool distanceTo(const Peer& p,float& metres,float& bearing){
 if(!p.position||!hardware.gpsFix())return false;double la1=hardware.gps.location.lat()*M_PI/180,la2=p.latitude*M_PI/180,dl=(p.longitude-hardware.gps.location.lng())*M_PI/180;
 double a=sin((la2-la1)/2)*sin((la2-la1)/2)+cos(la1)*cos(la2)*sin(dl/2)*sin(dl/2);metres=12742000*atan2(sqrt(a),sqrt(1-a));bearing=fmod(atan2(sin(dl)*cos(la2),cos(la1)*sin(la2)-sin(la1)*cos(la2)*cos(dl))*180/M_PI+360,360);return true;
}
// Nodes, most recently heard first.
unsigned sortedNodes(unsigned* order){unsigned n=meshRadio.peerCount;uint32_t now=millis();for(unsigned i=0;i<n;i++)order[i]=i;
 auto before=[&](const Peer& a,const Peer& b){if(a.heard!=b.heard)return a.heard;return a.heard&&now-a.seen<now-b.seen;};
 for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&before(meshRadio.peers[order[j]],meshRadio.peers[order[j-1]]);j--)std::swap(order[j],order[j-1]);return n;}
Peer* shownNode(){unsigned order[24];unsigned n=sortedNodes(order);if(!n)return nullptr;nodeIndex%=n;return &meshRadio.peers[order[nodeIndex]];}
const ChatMessage* shownMessage(){if(!meshRadio.historyCount)return nullptr;messageOffset=constrain(messageOffset,0,int(meshRadio.historyCount)-1);return &meshRadio.history[meshRadio.historyCount-1-messageOffset];}
// Signal radar (see Radar.h): the selection follows the target, not its row.
uint64_t signalId=0;RadarTarget::Kind signalKind=RadarTarget::Wifi;bool signalManual=false; // until "Next signal", the strongest
int shownSignal(){if(!signalManual&&radar.count){signalId=radar.targets[0].id;signalKind=radar.targets[0].kind;return 0;}for(unsigned i=0;i<radar.count;i++)if(radar.targets[i].id==signalId&&radar.targets[i].kind==signalKind)return i;if(!radar.count)return -1;signalId=radar.targets[0].id;signalKind=radar.targets[0].kind;return 0;}
String signalName(const RadarTarget& r){
 if(r.name[0])return r.name;
 if(r.kind==RadarTarget::Ble)switch(r.device){case RadarTarget::Phone:return t("phone","телефон");case RadarTarget::Watch:return t("watch","часы");case RadarTarget::Audio:return t("headphones","наушники");case RadarTarget::Personal:return t("phone/watch","телефон/часы");default:return t("BLE device","BLE-устройство");}
 return r.kind==RadarTarget::Wifi?t("hidden network","скрытая сеть"):meshRadio.idText(r.id);}
const char* kindLetter(const RadarTarget& r){return r.kind==RadarTarget::Lora?"L ":r.kind==RadarTarget::Ble?"B ":"W ";}
float signalLevel(int rssi){return constrain((rssi+100)/65.f,0.f,1.f);} // -100 .. -35 dBm
bool homingFresh(){return radar.fresh();}
String wifiState(){
#if defined(MM_NO_WIFI)
 return ""; // no Wi-Fi radio: the radar lists BLE and LoRa
#endif
 switch(radar.wifi){case Radar::WifiPortal:return t("Wi-Fi: access point","Wi-Fi: точка доступа");case Radar::WifiBusy:return t("Wi-Fi busy","Wi-Fi занят");case Radar::WifiFailed:return t("Wi-Fi error","Ошибка Wi-Fi");default:return radar.sweeps?"":t("scanning...","сканирую...");}}
String bleState(){return radar.ble==Radar::BleBusy?t("BLE busy","BLE занят"):radar.ble==Radar::BleFailed?t("BLE error","Ошибка BLE"):"";}
void showPage(int next){page=next;if(page==Signals){signalManual=false;radar.open();}else if(!webRadarActive())radar.close();if(page==Messages){messageOffset=0;unreadCount=0;}}
// Server roles show the pages that still mean something: no chats, nodes or radar.
bool pageShown(int p){
#if defined(MM_NO_WIFI)
 if(p==Wifi)return false; // no Wi-Fi radio
#endif
 return config.role==RoleNormal||p==Home||p==Gps||p==Wifi||p==Ble||p==Settings||p==Modules;}
int nextPage(int p){do p=(p+1)%PageCount;while(!pageShown(p));return p;}
int previousPage(int p){do p=(p+PageCount-1)%PageCount;while(!pageShown(p));return p;}
// Device role: offered for 5 s after boot (click: next, hold: choose) and from the menus.
bool rolePick=false,rolePickBoot=false;int roleSel=0;uint32_t rolePickAt=0;
String roleShort(int r){return r==RoleRepeater?t("Repeater","Репитер"):r==RoleRoom?t("Room server","Комната"):t("Normal","Обычный");}
// Boards without a screen (XIAO, a T-Beam without OLED) skip the boot choice: a press there would
// change the mode unseen. The web page, BLE and USB change it instead.
bool screenPresent(){
#if defined(MM_COMPACT) && !defined(MM_HELTEC_V4)
 return hardware.panel!=nullptr;
#else
 return true;
#endif
}
void openRolePick(bool atBoot){if(atBoot&&!screenPresent())return;rolePick=true;rolePickBoot=atBoot;rolePickAt=millis();roleSel=config.role;dirty=true;}
#if defined(MM_JOYSTICK)
#include "UiCompose.inc"
#include "UiChessCompact.inc"
#endif
template<class T> String applyOne(const char* key,T value){StaticJsonDocument<96>d;d[key]=value;return applySettings(d.as<JsonObjectConst>());}

// Actions: a screen with one action runs it on hold; several open a menu.
enum Act {ActFormat,ActJoin,ActJoinHeard,ActWrite,ActChess,ActSound,ActRole,ActForward,ActAdvert,ActReplyOk,ActReplyAck,ActOlder,ActNewer,ActNextNode,ActNodeOk,ActResetPath,ActGps,ActPosition,ActWifi,ActBle,ActLanguage,ActBattery,ActScreen,ActContrast,ActSelfTest,ActHoming,ActNextSignal,ActStopHoming,ActResetPeak,ActCsiBeacon,ActCsiSensor,ActCalibrate,ActClose};
// Channels are added on the web page or in the app; here: an invitation in a message and a hashtag heard on air.
const HeardChannel* heardTag(){for(unsigned i=0;i<meshRadio.heardCount;i++)if(meshRadio.heard[i].name[0])return &meshRadio.heard[i];return nullptr;}
String channelTag(uint64_t id){const channels::Channel* c=meshRadio.channel(id);return channels::isPublic(c?c->secret:channels::publicSecret)?String(" #"):" "+String(c->name[0]=='#'?"":"#")+c->name;}
String messageText(const ChatMessage& m){String name;uint8_t key[16];return channels::parseLink(m.text,name,key)?t("Invitation to channel ","Приглашение в канал ")+name:String(m.text);}
void joinedNotice(MeshRadio::ChannelResult r,const String& name){notice(r==MeshRadio::ChannelAdded?t("Joined ","Вступили: ")+name:r==MeshRadio::ChannelExists?t("Already in ","Уже в канале ")+name:r==MeshRadio::ChannelFull?t("8 channels at most","Не больше 8 каналов"):t("Not joined","Не удалось вступить"));}
unsigned actions(Act* out){
 unsigned n=0;switch(page){
 case Home:
  // Storage left by another firmware: on the nRF52 the settings live there (nothing is saved or sent
  // without it); on the ESP32 the key, settings and contacts are in NVS, the history and games are not kept.
  if(!hardware.fsOk)out[n++]=ActFormat;
#if defined(MM_JOYSTICK)
  if(config.role==RoleNormal)out[n++]=ActWrite;
#endif
  out[n++]=ActAdvert;if(config.role!=RoleNormal){out[n++]=ActForward;out[n++]=ActRole;}break;
 case Messages:
  {String name;uint8_t key[16];const ChatMessage* m=shownMessage();if(m&&channels::parseLink(m->text,name,key)&&!meshRadio.channel(channels::idOf(key)))out[n++]=ActJoin;}
  if(heardTag())out[n++]=ActJoinHeard;
#if defined(MM_JOYSTICK)
  out[n++]=ActWrite;
#endif
  if(meshRadio.historyCount){out[n++]=ActReplyOk;out[n++]=ActReplyAck;out[n++]=ActOlder;out[n++]=ActNewer;}break;
 case Nodes:if(Peer* p=shownNode()){
#if defined(MM_JOYSTICK)
  if(p->type==1){out[n++]=ActWrite;out[n++]=ActChess;}
#else
  out[n++]=ActNextNode;
#endif
  if(p->type==1)out[n++]=ActNodeOk;if(p->pathLength!=255)out[n++]=ActResetPath;}out[n++]=ActAdvert;break;
 case Signals:
#if defined(MM_NO_WIFI)
  if(radar.tracking){out[n++]=ActStopHoming;out[n++]=ActResetPeak;}else if(radar.count)out[n++]=ActHoming;break; // no Wi-Fi CSI
#endif
  if(radar.csi==Radar::CsiSensor){out[n++]=ActCalibrate;out[n++]=ActCsiSensor;}
  else if(radar.tracking){out[n++]=ActStopHoming;out[n++]=ActResetPeak;}
  else{out[n++]=ActCsiBeacon;if(radar.csi==Radar::CsiOff){out[n++]=ActCsiSensor;if(radar.count){out[n++]=ActHoming;out[n++]=ActNextSignal;}}}break; // CSI first: the beacon is the usual Heltec role
 case Gps:out[n++]=ActGps;out[n++]=ActPosition;break;
 case Wifi:out[n++]=ActWifi;break;case Ble:out[n++]=ActBle;break;
 case Settings:
#if defined(MM_JOYSTICK)
  out[n++]=ActSound; // the GAT562 buzzer
#endif
  out[n++]=ActLanguage;out[n++]=ActBattery;out[n++]=ActScreen;out[n++]=ActContrast;out[n++]=ActRole;break;
 case Modules:if(!hardware.fsOk)out[n++]=ActFormat;out[n++]=ActSelfTest;break; // FS ERR is shown here
 }if(n>1)out[n++]=ActClose;return n;
}
bool keepsMenu(Act a){return a==ActFormat||a==ActSound||a==ActNextSignal||a==ActOlder||a==ActNewer||a==ActNextNode||a==ActLanguage||a==ActBattery||a==ActScreen||a==ActContrast;}
String actName(Act a){
 const ChatMessage* m=shownMessage();bool publicChat=m&&channels::isChannel(m->destination);
 switch(a){
 case ActWrite:return page==Nodes?t("Write message...","Написать...")
  :page==Messages&&m&&!publicChat?t("Write reply...","Ответить текстом..."):t("Write to channel...","Написать в канал...");
 case ActFormat:return t("Create storage...","Создать хранилище...");
 case ActJoin:{String name;uint8_t key[16];channels::parseLink(m?m->text:"",name,key);return t("Join ","Вступить: ")+name;}
 case ActJoinHeard:{const HeardChannel* h=heardTag();return t("Join heard ","Вступить: ")+(h?h->name:"");}
 case ActChess:return t("Chess: invite","Шахматы: вызвать");
 case ActSound:return config.sound?t("Sound: on","Звук: вкл."):t("Sound: off","Звук: выкл.");
 case ActRole:return t("Device mode...","Режим работы...");case ActForward:return meshServer.view().forwarding?t("Forwarding: off","Пересылка: выкл."):t("Forwarding: on","Пересылка: вкл.");
 case ActAdvert:return t("Announce node","Объявить узел");case ActReplyOk:return t("Reply: OK","Ответить: OK")+String(publicChat?" #":"");case ActReplyAck:return t("Reply: Got it","Ответить: Принято");
 case ActOlder:return t("Older message","Предыдущее");case ActNewer:return t("Newer message","Следующее");case ActNextNode:return t("Next node","Следующий узел");case ActNodeOk:return t("Send: OK","Написать: OK");case ActResetPath:return t("Reset path","Сбросить путь");
 case ActGps:return config.gps?t("Turn GPS off","Выключить GPS"):t("Turn GPS on","Включить GPS");case ActPosition:return t("Share position","Передать позицию");
 case ActWifi:return portalActive()?t("Turn Wi-Fi off","Выключить Wi-Fi"):t("Turn Wi-Fi on","Включить Wi-Fi");case ActBle:return bleActive()?t("Turn BLE off","Выключить BLE"):t("Turn BLE on","Включить BLE");
 case ActLanguage:return t("Language: ","Язык: ")+langNames[config.lang<LangCount?config.lang:0];case ActBattery:return config.batteryVolts?t("Battery: volts","Батарея: вольты"):t("Battery: percent","Батарея: проценты");
 case ActScreen:return t("Screen off: ","Гасить: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда"));case ActContrast:return t("Contrast: ","Контраст: ")+String(config.brightness);
 case ActHoming:{int i=shownSignal();return t("Home in: ","Пеленг: ")+(i>=0?signalName(radar.targets[i]):String("-"));}case ActNextSignal:return t("Next signal","Следующий сигнал");
 case ActCsiBeacon:return radar.csi==Radar::CsiBeacon?t("Stop beacon","Выключить маяк"):t("CSI beacon: on","Маяк CSI: включить");
 case ActCsiSensor:return radar.csi==Radar::CsiSensor?t("CSI motion: off","Движение CSI: выкл."):t("CSI sensor","Приёмник CSI");
 case ActCalibrate:return t("Calibrate (10 s still)","Калибровка (10 с тихо)");
 case ActStopHoming:return t("Stop homing","Остановить пеленг");case ActResetPeak:return t("Reset peak","Сбросить пик");
 case ActSelfTest:return t("Encryption test","Тест шифрования");case ActClose:return t("< Close menu","< Закрыть меню");
 }return "";
}
void reply(const String& text){const ChatMessage* m=shownMessage();if(!m)return;uint64_t to=channels::isChannel(m->destination)?m->destination:m->outgoing?m->destination:m->source;bool sent=to!=meshRadio.nodeId&&meshRadio.sendMessage(text,to);notice(sent?t("Reply ","Ответ ")+text+t(" queued"," в очереди"):t("Reply not queued","Ответ не отправлен"));}
void run(Act a){
 switch(a){
 case ActFormat:{static uint32_t armed=0;
#if defined(MM_JOYSTICK)
  if(!armed||millis()-armed>5000){armed=millis();notice(t("OK again: erase old data","Ещё раз OK: стереть"));break;}
#else
  if(!armed||millis()-armed>5000){armed=millis();notice(t("Hold again: erase","Удерж. ещё: стереть"));break;}
#endif
  armed=0;String r=executeCommand("fsformat");if(r.startsWith("OK"))executeCommand("restart");notice(r.startsWith("OK")?t("Storage created, restart","Создано, перезапуск"):r);break;}
 case ActJoin:{const ChatMessage* m=shownMessage();String name;uint8_t key[16];if(m&&channels::parseLink(m->text,name,key))joinedNotice(meshRadio.joinLink(m->text),name);break;}
 case ActJoinHeard:{const HeardChannel* h=heardTag();if(h){String name=h->name;joinedNotice(meshRadio.joinHashtag(name),name);}break;}
 case ActWrite:
#if defined(MM_JOYSTICK)
  if(page==Nodes){if(Peer* p=shownNode())openCompose(p->id,p->name);}
  else if(page==Messages&&shownMessage()&&channels::isChannel(shownMessage()->destination)){const ChatMessage* m=shownMessage();const channels::Channel* c=meshRadio.channel(m->destination);if(c)openCompose(c->id,c->name);else openCompose(meshmesh::Broadcast,t("Public channel","Общий канал"));}
  else if(page==Messages&&shownMessage()){const ChatMessage* m=shownMessage();uint64_t to=m->outgoing?m->destination:m->source;String name=m->name;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==to)name=meshRadio.peers[i].name;openCompose(to,name);}
  else openCompose(meshmesh::Broadcast,t("Public channel","Общий канал"));
#endif
  break;
 case ActChess:
#if defined(MM_JOYSTICK)
  if(Peer* p=shownNode()){ // the board shows the invitation; no news popup for it
   uint32_t before=chessNet.events;ChessMatch* m=chessNet.invite(p->id,2);if(m){showPage(Chess);openChess(m);notice(t("Challenge sent","Вызов отправлен"));}else if(chessNet.events!=before)notice(chessNet.event);chessSeen=chessNet.events;}
#endif
  break;
 case ActRole:openRolePick(false);break;
 case ActForward:{String r=meshServer.command(meshServer.view().forwarding?"set repeat off":"set repeat on");notice(r.startsWith("OK")?(meshServer.view().forwarding?t("Forwarding on","Пересылка вкл."):t("Forwarding off","Пересылка выкл.")):r);break;}
 case ActAdvert:notice(meshRadio.sendHello()?t("Node announced","Узел объявлен"):t("Announcement failed","Объявление не отправлено"));break;
 case ActReplyOk:reply("OK");break;case ActReplyAck:reply(t("Got it","Принято"));break;
 case ActOlder:messageOffset++;shownMessage();break;case ActNewer:messageOffset=max(0,messageOffset-1);break;case ActNextNode:nodeIndex++;menuIndex=0;break;
 case ActNodeOk:{Peer* p=shownNode();bool sent=p&&meshRadio.sendMessage("OK",p->id);notice(sent?t("OK queued","OK в очереди"):t("Not queued","Не отправлено"));break;}
 case ActResetPath:{Peer* p=shownNode();notice(p&&!meshRadio.busy()&&meshRadio.resetPath(p->id)?t("Path reset","Путь сброшен"):t("Path reset failed","Путь не сброшен"));break;}
 case ActGps:notice(applyOne("gps",!config.gps).startsWith("OK")?"GPS: "+String(config.gps?t("on","вкл"):t("off","выкл")):t("Radio busy, retry","Радио занято, повторите"));break;
 case ActPosition:notice(meshRadio.sendPosition()?t("Position shared","Позиция передана"):t("Needs a GPS fix","Нужна позиция GPS"));break;
 case ActWifi:portalToggle();break;case ActBle:bleToggle();break;
 case ActSound:applyOne("sound",!config.sound);hardware.beep();break;
 case ActLanguage:applyOne("lang",langCodes[(config.lang+1)%LangCount]);break;case ActBattery:applyOne("battery_volts",!config.batteryVolts);break;
 case ActScreen:{const uint16_t steps[]={0,15,30,60,120,300};int i=0;while(i<5&&steps[i]!=config.dimAfter)i++;applyOne("dim_after",steps[(i+1)%6]);break;}
 case ActContrast:{const uint8_t steps[]={10,60,150,255};int i=0;while(i<3&&steps[i]<config.brightness)i++;applyOne("brightness",steps[(i+1)%4]);break;}
 case ActHoming:{int i=shownSignal();signalManual=true;if(i>=0&&radar.track(i)){pingedSamples=radar.samples;notice(t("Homing started","Пеленг начат"));}break;}
 case ActNextSignal:{int i=shownSignal();signalManual=true;if(i>=0){i=(i+1)%radar.count;signalId=radar.targets[i].id;signalKind=radar.targets[i].kind;}menuIndex=0;break;}
 case ActStopHoming:radar.untrack();break;case ActResetPeak:radar.resetPeak();notice(t("Peak reset","Пик сброшен"));break;
 case ActCsiBeacon:radar.setCsi(radar.csi==Radar::CsiBeacon?Radar::CsiOff:Radar::CsiBeacon);break;
 case ActCsiSensor:radar.setCsi(radar.csi==Radar::CsiSensor?Radar::CsiOff:Radar::CsiSensor);break;
 case ActCalibrate:if(radar.beaconHeard()){radar.calibrate();notice(t("Calibrating 10 s","Калибровка 10 с"));}else notice(t("No beacon heard","Маяк не слышен"));break;
 case ActClose:break;
 case ActSelfTest:{bool valid=meshRadio.selfTest();meshRadio.event=valid?"Encryption test OK":"Encryption test FAILED";notice(valid?t("Encryption: OK","Шифрование: OK"):t("Encryption: ERROR","Шифрование: ошибка"));break;}
 }
}

// HUD: title on the left; unread, GPS, links, signal and battery on the right.
void header(const String& title){
 auto& c=*hardware.canvas;int x=127;unsigned mv=hardware.batteryMv;
 if(config.batteryVolts&&mv){String v=String(mv/1000.f,2)+"V";sayRight(x+1,7,v);x-=width(v,small)+3;}
 else{unsigned pct=mv<=3300?0:mv>=4200?100:(mv-3300)/9;c.drawRect(x-12,1,12,7,1);c.drawFastVLine(x,3,3,1);if(mv>4250){c.drawLine(x-8,2,x-6,4,1);c.drawLine(x-6,4,x-4,4,1);c.drawLine(x-4,4,x-2,6,1);}else if(mv)c.fillRect(x-10,3,max(1u,8*pct/100),3,1);x-=16;}
 bool fresh=meshRadio.lastRxAt&&millis()-meshRadio.lastRxAt<600000;unsigned level=!meshRadio.ready||!fresh?0:meshRadio.lastSnr>=5?4:meshRadio.lastSnr>=0?3:meshRadio.lastSnr>=-5?2:1;
 for(unsigned k=0;k<4;k++){int h=2+k*2;if(k<level)c.fillRect(x-14+k*4,8-h,3,h,1);else c.drawPixel(x-13+k*4,7,1);}if(!meshRadio.ready)c.drawLine(x-15,0,x-1,8,1);x-=18;
 if(config.gps){c.drawCircle(x-3,3,2,1);if(hardware.gpsFix())c.fillCircle(x-3,3,2,1);c.drawLine(x-5,4,x-3,8,1);c.drawLine(x-1,4,x-3,8,1);x-=8;}
 if(portalActive()){for(int r:{2,4,6})for(int a=-45;a<=45;a+=15)c.drawPixel(x-4+roundf(r*sinf(a*M_PI/180)),8-roundf(r*cosf(a*M_PI/180)),1);x-=10;}
 if(bleActive()){c.drawLine(x-3,0,x-3,8,1);c.drawLine(x-3,0,x-1,2,1);c.drawLine(x-1,2,x-5,6,1);c.drawLine(x-3,8,x-1,6,1);c.drawLine(x-1,6,x-5,2,1);x-=8;}
 if(unreadCount){String n=String(unreadCount);sayRight(x,7,n);x-=width(n,small)+1;c.drawRect(x-9,1,9,7,1);c.drawLine(x-9,1,x-5,5,1);c.drawLine(x-1,1,x-5,5,1);x-=12;}
 say(0,7,clipped(title,max(3,(x-2)/5)),small);c.drawFastHLine(0,10,128,1);
}
// The longest start of value that fits px with "..", measured in the font (fallback glyphs are wider than a column).
String fitted(const String& value,int px,const uint8_t* f){
 if(width(value,f)<=px)return value;int room=px-width("..",f);unsigned i=0,cut=0;
 while(i<value.length()){unsigned next=i;utf8Next(value,next);if(width(value.substring(0,next),f)>room)break;cut=i=next;}
 return value.substring(0,cut)+"..";
}
void footer(const String& hint){
 auto& c=*hardware.canvas;int x=1;for(int i=0;i<PageCount;i++){if(!pageShown(i))continue;if(i==page)c.fillRect(x,58,2,4,1);else c.drawPixel(x,61,1);x+=3;}
 if(hint.length())sayRight(128,63,fitted(clipped(hint,20),128-x,small));
}
#if defined(MM_JOYSTICK)
String hint(){Act acts[8];unsigned n=actions(acts);if(!n)return "<  >";return n==1?t("OK: ","OK: ")+actName(acts[0]):t("OK: menu  < >","OK: меню  < >");}
#else
String hint(){Act acts[8];unsigned n=actions(acts);if(!n)return "";return n==1?t("hold: ","держ: ")+actName(acts[0]):t("hold: menu","держ: меню");}
#endif
void drawMenu(){
 auto& c=*hardware.canvas;Act acts[8];unsigned n=actions(acts);if(!n){menuOpen=false;return;}menuIndex%=n;int first=max(0,min(menuIndex-1,int(n)-3));
 c.fillRect(0,12,128,45,0);c.drawRect(0,12,128,45,1);
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){int y=14+(i-first)*14;bool focus=int(i)==menuIndex;if(focus)c.fillRect(2,y,124,13,1);say(5,y+10,clipped(actName(acts[i]),20),body,focus?0:1);}
 c.fillRect(0,57,128,7,0);
#if defined(MM_JOYSTICK)
 footer(t("up/down, OK, back","выбор, OK, назад"));
#else
 footer(t("click-next hold-run","клик-далее держ-ОК"));
#endif
}
void drawPopup(const ChatMessage& m){
 auto& c=*hardware.canvas;c.fillScreen(0);c.drawRect(0,0,128,64,1);c.drawRect(3,3,11,8,1);c.drawLine(3,3,8,7,1);c.drawLine(13,3,8,7,1);
 say(18,11,clipped(String(m.name)+(channels::isChannel(m.destination)?channelTag(m.destination):String()),18),bold);textLines(messageText(m),25,3);sayRight(126,62,t("click: close","клик: закрыть"));
}
void drawChessPopup(){
 auto& c=*hardware.canvas;c.fillScreen(0);c.drawRect(0,0,128,64,1);
 c.fillCircle(8,5,2,1);c.fillTriangle(8,5,5,10,11,10,1);c.fillRect(4,10,9,2,1); // a pawn
 say(18,11,t("Chess","Шахматы"),bold);textLines(chessNet.event,25,3);
#if defined(MM_NO_WIFI)
 sayRight(126,62,t("play in the app (BLE)","играть: приложение (BLE)"),small);
#else
 sayRight(126,62,portalActive()?t("play on the Wi-Fi page","играть: Wi-Fi-страница"):t("Wi-Fi page to play","играть: включите Wi-Fi"),small);
#endif
}
void drawRolePick(){
 auto& c=*hardware.canvas;c.fillScreen(0);
 for(int r=0;r<RoleCount;r++){int y=21+r*12;bool focus=r==roleSel;if(focus)c.fillRect(0,y-10,128,12,1);say(3,y,roleShort(r),body,!focus);if(r==config.role)sayRight(125,y,t("now","сейчас"),small,!focus);}
 uint32_t gone=millis()-rolePickAt;unsigned left=gone>=5000?0:(5000-gone+999)/1000;
 header(t("Device mode","Режим работы"));
 sayRight(128,63,rolePickBoot?t("as now in ","как сейчас через ")+String(left)+t("s","с"):
#if defined(MM_JOYSTICK)
 t("up/down, OK","вверх/вниз, OK")
#else
 t("click-next hold-choose","клик-далее держ-выбор")
#endif
 ,small);
 if(action.length()&&millis()-actionAt<3500){c.fillRect(0,36,128,20,0);c.drawRect(0,36,128,20,1);say(3,50,clipped(action,20));}
 hardware.flush();
}
// Server home: role, radio, traffic and the passwords an owner needs for the MeshCore app.
void drawServerHome(){
 ServerView v=meshServer.view();bool room=config.role==RoleRoom;
 say(0,21,roleShort(config.role),bold);if(meshRadio.ready)sayRight(128,20,String(config.frequency,3)+t(" MHz"," МГц"));else sayRight(128,20,t("Radio error","Ошибка радио"));
 say(0,31,room?t("posts ","постов ")+String(v.posts)+t("  in ","  вошли ")+String(v.clients):"RX "+String(meshRadio.rxCount)+" TX "+String(meshRadio.txCount)+t(" fwd "," перес. ")+String(meshRadio.relayed),small);
 say(0,40,t("admin ","админ ")+v.password,small);
 say(0,49,(room?t("room ","комната "):t("guest ","гость "))+(v.guest.length()?v.guest:t("none","нет"))+(v.forwarding?"":t("  no fwd","  без перес.")),small);
}
void draw(){
 auto& c=*hardware.canvas;if(rolePick){drawRolePick();return;}
#if defined(MM_JOYSTICK)
 if(composing){drawCompose();return;}
#endif
 c.fillScreen(0);const ChatMessage* last=meshRadio.historyCount?&meshRadio.history[meshRadio.historyCount-1]:nullptr;
 if(popupAt&&millis()-popupAt<8000&&last&&!last->outgoing){drawPopup(*last);hardware.flush();return;}
 popupAt=0;
 if(chessPopupAt&&millis()-chessPopupAt<8000){drawChessPopup();hardware.flush();return;}
 chessPopupAt=0;String title;
 switch(page){
 case Home:{title=config.name;if(config.role!=RoleNormal){drawServerHome();break;}say(0,31,clockText(time(nullptr)),u8g2_font_10x20_t_cyrillic);
  if(meshRadio.ready){sayRight(128,20,String(config.frequency,3)+t(" MHz"," МГц"));sayRight(128,30,"SF"+String(config.sf)+" BW"+String(config.bandwidth,1));}else sayRight(128,24,t("Radio error ","Ошибка радио ")+String(meshRadio.radioError));
  unsigned near=0;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].heard&&millis()-meshRadio.peers[i].seen<1800000)near++;
  say(0,43,"RX "+String(meshRadio.rxCount)+"  TX "+String(meshRadio.txCount)+t("  near "," рядом ")+String(near),small);
  unsigned mv=hardware.batteryMv;say(0,52,(mv>4250?t("USB power","Питание USB"):String(mv/1000.f,2)+"V")+(config.relay?t("  relay on","  ретрансляция"):""),small);break;}
 case Messages:{const ChatMessage* m=shownMessage();title=t("Messages","Сообщения")+(m?" "+String(meshRadio.historyCount-messageOffset)+"/"+String(meshRadio.historyCount):"");
  if(m){String who=m->outgoing?t("You","Вы"):String(m->name);if(channels::isChannel(m->destination))who+=channelTag(m->destination);const char* states[]={"",tr("queued","очередь"),tr("sent","отправл."),tr("delivered","доставл."),tr("no ACK","нет ACK")};
   String route=meshRadio.routeText(*m,true),st=m->outgoing?String(states[m->status])+(route.length()?" "+route:String()):(route.length()?route+" ":String())+clockText(m->timestamp);say(0,23,clipped(who,20-chars(st)),bold);sayRight(128,22,st);textLines(messageText(*m),36,2);}
  else{say(0,30,t("No messages yet","Сообщений ещё нет"));say(0,44,t("They appear here","Здесь появятся входящие"),small);}break;}
 case Nodes:{unsigned order[24];unsigned n=sortedNodes(order);Peer* p=shownNode();title=t("Nodes","Узлы")+(n?" "+String(nodeIndex%n+1)+"/"+String(n):"");
  if(p){say(0,23,clipped(p->name,21),bold);say(0,33,typeText(p->type)+", "+pathText(*p),small);
   say(0,42,p->heard?String(int(p->rssi))+" dBm SNR "+String(p->snr,1)+", "+ago(millis()-p->seen):t("saved, not heard","сохранён, не слышен"),small);
   float metres,bearing;if(distanceTo(*p,metres,bearing)){const char* dirs[]={tr("N","С"),tr("NE","СВ"),tr("E","В"),tr("SE","ЮВ"),tr("S","Ю"),tr("SW","ЮЗ"),tr("W","З"),tr("NW","СЗ")};int k=int((bearing+22.5f)/45)%8;say(0,51,(metres<1000?String(int(metres))+t(" m "," м "):String(metres/1000,1)+t(" km "," км "))+dirs[k],small);}
   else if(p->position)say(0,51,t("has GPS position","есть GPS-позиция"),small);}
  else{say(0,30,t("No nodes heard","Узлы пока не найдены"));say(0,44,t("hold: announce","держите: объявить"),small);}break;}
 case Signals:{
  if(radar.csi==Radar::CsiSensor){title=t("Motion CSI","Движение CSI");bool heard=radar.beaconHeard();
   String state=radar.wifi!=Radar::WifiReady?t("no Wi-Fi: AP on?","нет Wi-Fi: точка?"):!heard?t("no beacon","нет маяка"):radar.csiStale?t("CSI frozen","CSI стоит"):radar.calibrateUntil?t("calibrating","калибровка"):radar.moving?t("MOTION","ДВИЖЕНИЕ"):t("still","тихо");
   say(0,31,state,u8g2_font_10x20_t_cyrillic);
   float threshold=radar.motionThreshold(),scale=max(threshold*2.5f,radar.activity*1.1f);
   c.drawRect(0,36,128,6,1);if(heard)c.fillRect(1,37,max(1,int(126*min(1.f,radar.activity/scale))),4,1);c.drawFastVLine(1+int(125*threshold/scale),34,10,1);
   say(0,52,String(radar.activity*1000,1)+t(" / thr "," / порог ")+String(threshold*1000,1)+(radar.baseline>0?"":"*"),small);
   say(30,62,String(radar.csiRate)+t("/s","/с"),small);}
  else if(radar.tracking){const RadarTarget& f=radar.focus;bool fresh=homingFresh();int trend=radar.trend();title=t("Homing","Пеленг");
   say(0,21,clipped(String(kindLetter(f))+signalName(f),21),bold);
   int shown=lroundf(radar.fast);say(0,41,radar.samples?String(shown):String("--"),u8g2_font_10x20_t_cyrillic);say(radar.samples&&shown<=-100?42:34,41,"dBm",small);
   bool wifiLost=(f.kind==RadarTarget::Wifi&&radar.wifi!=Radar::WifiReady)||(f.kind==RadarTarget::Ble&&radar.ble!=Radar::BleReady);
   sayRight(128,31,wifiLost?(f.kind==RadarTarget::Ble?bleState():wifiState()):!fresh?(radar.samples>1?t("lost","потерян"):t("no signal","нет сигнала")):trend>0?t("stronger","теплее"):trend<0?t("weaker","холоднее"):t("steady","ровно"),bold);
   if(fresh&&!wifiLost){int x=70,y=25;if(trend>0)c.fillTriangle(x,y+4,x+8,y+4,x+4,y-2,1);else if(trend<0)c.fillTriangle(x,y-2,x+8,y-2,x+4,y+4,1);}
   sayRight(128,41,t("peak ","пик ")+(radar.samples?String(int(lroundf(radar.peak))):String("-")),small);
   c.drawRect(0,45,128,6,1);if(radar.samples)c.fillRect(1,46,max(1,int(126*signalLevel(shown))),4,1);if(radar.samples){int px=1+int(125*signalLevel(lroundf(radar.peak)));c.drawFastVLine(px,43,10,1);}
   String st=f.kind==RadarTarget::Lora?String(radar.samples)+t(" pkts"," пак."):String(radar.rate)+t("/s","/с");say(30,62,st,small);}
  else{bool beacon=radar.csi==Radar::CsiBeacon;int sel=shownSignal();
   // Beacon: sweeps and BLE pause, so the title shows the beacon instead of the counts.
   title=beacon?t("CSI beacon ","Маяк CSI ")+String(radar.csiRate)+t("/s","/с"):t("Radar","Радар")+
#if !defined(MM_NO_WIFI)
   " W"+String(radar.counted(RadarTarget::Wifi))+
#endif
   " B"+String(radar.counted(RadarTarget::Ble))+" L"+String(radar.counted(RadarTarget::Lora));
   String st=beacon?String():wifiState();if(!st.length()&&!beacon)st=bleState();
   if(sel<0){say(0,30,beacon?t("Beacon on","Маяк включён"):t("No signals yet","Сигналов пока нет"));say(0,44,st.length()?st:beacon?t("hold: switch off","держите: выключить"):
#if defined(MM_NO_WIFI)
    t("BLE and LoRa","BLE и LoRa")
#else
    t("Wi-Fi and LoRa","Wi-Fi и LoRa")
#endif
   ,small);}
   else{unsigned rows=st.length()?3:4; // a status line takes the 4th row, clear of the bottom hint
    int first=max(0,min(sel-1,int(radar.count)-int(rows)));for(int i=first;i<int(radar.count)&&i<first+int(rows);i++){const RadarTarget& r=radar.targets[i];int y=19+(i-first)*10;
     if(i==sel)c.fillRect(0,y-8,128,10,1);say(1,y,String(kindLetter(r))+clipped(signalName(r),17),small,i!=sel);sayRight(127,y,String(int(r.rssi)),small,i!=sel);}
    if(st.length())say(0,49,clipped(st,25),small);}}
  break;}
 case Gps:{title="GPS";bool fix=hardware.gpsFix();
  say(0,23,!config.gps?t("GPS off","GPS выключен"):fix?t("Position fix","Позиция есть"):hardware.clockConflict?t("Old GPS date","Старая дата GPS"):hardware.gps.passedChecksum()?t("Searching sky","Поиск спутников"):t("No data from GPS","Нет данных GPS"),bold);
  say(0,33,t("Satellites ","Спутники ")+String(hardware.gps.satellites.value())+"  NMEA "+String(hardware.gps.passedChecksum()),small);
  if(fix)say(0,42,String(hardware.gps.location.lat(),5)+", "+String(hardware.gps.location.lng(),5),small);
  say(0,51,t("Clock ","Часы ")+clockText(time(nullptr))+" "+(hardware.clockSource=="unset"?t("not set","не задано"):hardware.clockSource),small);break;}
 case Wifi:title="Wi-Fi";if(portalActive()){say(0,23,"MM-"+meshRadio.idText(meshRadio.nodeId).substring(6),bold);say(0,36,portalPassword());say(0,48,"192.168.4.1",small);}else{say(0,27,t("Access point off","Точка доступа выкл."));say(0,41,t("Web chat and settings","Веб-чат и настройки"),small);}break;
 case Ble:title="Bluetooth";if(bleActive()){say(0,23,"MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6),bold);say(0,37,"PIN "+String(blePin()),bold);}else{say(0,27,t("Bluetooth off","Bluetooth выкл."));say(0,41,t("Secure pairing","Защищённое сопряжение"),small);}break;
 case Settings:title=t("Settings","Настройки");say(0,20,t("Language: ","Язык: ")+langNames[config.lang<LangCount?config.lang:0],small);say(0,28,t("Battery: ","Батарея: ")+(config.batteryVolts?t("volts","вольты"):t("percent","проценты")),small);
  say(0,36,t("Screen off: ","Гасить экран: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда")),small);say(0,44,t("Contrast: ","Контраст: ")+String(config.brightness),small);
  say(0,52,String(config.frequency,3)+" SF"+String(config.sf)+" CR4/"+String(config.cr)+" "+String(config.power)+"dBm",small);break;
 #if defined(MM_JOYSTICK)
 case Chess:if(chessOpen&&chessOpen->state!=ChessMatch::Free){drawChessBoard(*chessOpen);return;}chessOpen=nullptr;drawChessList(title);break;
 #endif
 default:title=t("Modules","Модули");say(0,21,"LoRa "+String(meshRadio.ready?"OK":"ERR")+"  FS "+String(hardware.fsOk?"OK":"ERR")+"  GPS "+String(!config.gps?"-":hardware.gps.passedChecksum()?"OK":"?"),small);
  say(0,30,String(ESP.getFreeHeap()/1024)+"K RAM"+(ESP.getPsramSize()?"  "+String(ESP.getFreePsram()/1024)+"K PSRAM":String()),small);say(0,39,t("relayed ","переслано ")+String(meshRadio.relayed)+t("  rejected ","  откл. ")+String(meshRadio.rejected),small);
  say(0,48,t("up ","работа ")+ago(millis())+t("  boot ","  загр. ")+String(config.bootCounter),small);
 }
 header(title);
 if(menuOpen)drawMenu();else footer(hint());
 if(action.length()&&millis()-actionAt<3500){c.fillRect(0,36,128,20,0);c.drawRect(0,36,128,20,1);say(3,50,clipped(action,20));}
 hardware.flush();
}
}
#if defined(MM_JOYSTICK)
// Up/down inside a screen: older/newer message, previous/next node or signal.
void scroll(int step){
 if(page==Messages){messageOffset=max(0,messageOffset-step);shownMessage();}
 else if(page==Nodes){unsigned n=meshRadio.peerCount;if(n)nodeIndex=(nodeIndex+n+step)%n;}
 else if(page==Signals&&radar.count&&!radar.tracking){int i=shownSignal();signalManual=true;if(i>=0){i=(i+radar.count+step)%radar.count;signalId=radar.targets[i].id;signalKind=radar.targets[i].kind;}}
}
void joystickKey(int key){
 bool up=key==0xb5,down=key==0xb6,left=key==0xb4,right=key==0xb7,ok=key==13||key==0xa3,back=key==0x86||key==0x82;
 if(composing){composeKey(key);return;}
 if(rolePick){
  if(up||down){roleSel=(roleSel+(up?RoleCount-1:1))%RoleCount;rolePickBoot=false;return;}
  if(ok){if(roleSel==config.role){rolePick=false;return;}String r=setRole(roleSel);notice(r.startsWith("OK")?t("Restarting: ","Перезапуск: ")+roleShort(roleSel):r);if(!r.startsWith("OK"))rolePick=false;return;}
  if(back)rolePick=false;return;}
 if(popupAt){popupAt=0;if(ok)showPage(Messages);return;}
 if(chessPopupAt){chessPopupAt=0;if(ok&&config.role==RoleNormal){ChessMatch* m=chessNet.eventMatch;showPage(Chess);openChess(m&&m->state!=ChessMatch::Free?m:nullptr);}return;}
 if(menuOpen){menuAt=millis();Act acts[8];unsigned n=actions(acts);if(!n){menuOpen=false;return;}
  if(up||down){menuIndex=(menuIndex+(up?n-1:1))%n;return;}
  if(ok){Act a=acts[menuIndex%n];run(a);if(!keepsMenu(a))menuOpen=false;return;}
  menuOpen=false;return;}
 if(page==Chess&&chessKey(key))return;
 if(left||right){showPage(left?previousPage(page):nextPage(page));return;}
 if(back){if(page!=Home)showPage(Home);return;}
 if(up||down){scroll(up?-1:1);return;}
 if(ok){Act acts[8];unsigned n=actions(acts);if(n==1)run(acts[0]);else if(n>1){menuOpen=true;menuIndex=0;menuAt=millis();}}
}
#endif
void uiKey(int key){
 lastInput=millis();dirty=true;
 if(screenOff){screenOff=false;hardware.brightness(config.brightness);return;} // the first press only wakes the panel
#if defined(MM_JOYSTICK)
 joystickKey(key);return;
#endif
 if(rolePick){if(key==13||key==0x82){roleSel=(roleSel+1)%RoleCount;rolePickBoot=false;return;}
  if(key==0xa3){if(roleSel==config.role){rolePick=false;return;}String r=setRole(roleSel);notice(r.startsWith("OK")?t("Restarting: ","Перезапуск: ")+roleShort(roleSel):r);if(!r.startsWith("OK"))rolePick=false;}return;}
 if(popupAt){popupAt=0;if(key==13)return;}
 if(chessPopupAt){chessPopupAt=0;if(key==13)return;}
 if(menuOpen){menuAt=millis();Act acts[8];unsigned n=actions(acts);if(!n){menuOpen=false;return;}
  if(key==13||key==0x82){menuIndex=(menuIndex+1)%n;return;}
  if(key==0xa3){Act a=acts[menuIndex%n];run(a);if(!keepsMenu(a))menuOpen=false;}return;}
 if(key==13||key==0x82){showPage(nextPage(page));return;}
 if(key==0xa3){Act acts[8];unsigned n=actions(acts);if(n==1)run(acts[0]);else if(n>1){menuOpen=true;menuIndex=0;menuAt=millis();}}
}
bool uiRadarPage(){return page==Signals;}
void uiBegin(){openRolePick(true);chessSeen=chessNet.events;if(pins::led>=0)pinMode(pins::led,OUTPUT);led(false);lastInput=millis();if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];newest={m.source,m.session,m.id};}draw();}
String uiStatus(){StaticJsonDocument<448>d;d["action"]=millis()-actionAt<3500?action:String();d["page"]=rolePick?"role":pageNames[page];d["role"]=roleName(config.role);if(rolePick){d["role_selected"]=roleSel;d["boot_pick"]=rolePickBoot;}d["locked"]=false;d["menu"]=menuOpen;d["menu_index"]=menuIndex;d["screen_off"]=screenOff;d["popup"]=popupAt!=0;d["chess_popup"]=chessPopupAt!=0;d["unread"]=unreadCount;
#if defined(MM_JOYSTICK)
 if(page==Chess&&chessOpen){char id[5];snprintf(id,sizeof id,"%04X",chessOpen->id);d["chess_game"]=id;d["chess_cursor"]=chessCursor;d["chess_held"]=chessHeld;d["chess_menu"]=chessMenu;}
 if(composing){d["compose"]=true;d["draft"]=draft;d["keyboard"]=layoutNames[kbLayout];d["key_row"]=kbRow;d["key_col"]=kbCol;}
#endif
if(page==Signals){d["radar_selected"]=shownSignal();d["csi_role"]=radar.csi;}String s;serializeJson(d,s);return s;}
void uiTick(){
 uint32_t now=millis();
 // New incoming message: popup, wake the panel and blink the LED three times.
 if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];if(m.source!=newest.source||m.session!=newest.session||m.id!=newest.id){newest={m.source,m.session,m.id};if(!m.outgoing){if(page!=Messages)unreadCount++;popupAt=now;ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}}
 if(chessNet.events!=chessSeen){chessSeen=chessNet.events;if(chessNet.event.length()){chessPopupAt=now;ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}
 if(ledAt){uint32_t e=now-ledAt;led(e<1500&&(e/250)%2==0);if(e>=1500){ledAt=0;led(false);}}
 // Homing ping on the LED (the V4 has no buzzer): faster as the signal strengthens.
 if(!ledAt&&page==Signals&&radar.tracking){bool fresh=homingFresh();float level=constrain((radar.fast+85)/55.f,0.f,1.f);lastInput=now;
  if(fresh&&radar.focus.kind==RadarTarget::Lora){if(radar.samples!=pingedSamples){pingedSamples=radar.samples;pingAt=now;}}
  else if(fresh&&now-pingAt>=uint32_t(1200-1140*level*level))pingAt=now;
  led(pingAt&&now-pingAt<40);}
 else if(!ledAt&&page==Signals&&radar.csi==Radar::CsiSensor){lastInput=now;pingAt=1;led(radar.moving);} // LED on while motion is sensed
 else if(!ledAt&&pingAt){pingAt=0;led(false);}
 if(rolePickBoot&&now-rolePickAt>=5000){rolePick=rolePickBoot=false;dirty=true;}
 if(rolePick)lastInput=now; // the choice stays lit
 if(menuOpen&&now-menuAt>10000){menuOpen=false;dirty=true;}
 if(config.dimAfter&&!screenOff&&now-lastInput>=config.dimAfter*1000UL){screenOff=true;hardware.brightness(0);}
 if(screenOff)return;
 #if defined(MM_JOYSTICK)
 if(composing||(page==Chess&&chessOpen&&chessHeld>=0))dirty=true; // blinking cursor or picked piece
 if(chessNet.dirty&&page==Chess){dirty=true;chessNet.dirty=false;}
 #endif
 if((dirty||meshRadio.dirty||radar.dirty||now-drawAt>1000)&&now-drawAt>150){draw();drawAt=now;dirty=false;meshRadio.dirty=false;radar.dirty=false;}
}
