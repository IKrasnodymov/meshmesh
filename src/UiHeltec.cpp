#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include <math.h>
#include <time.h>
// One PRG button: click = next screen or next menu item; hold = the screen's action, or its menu.
namespace {
enum Page {Home,Messages,Nodes,Gps,Wifi,Ble,Settings,Modules,PageCount};
const char* pageNames[]={"home","messages","nodes","gps","wifi","ble","settings","modules"};
int page=Home,menuIndex=0,messageOffset=0,nodeIndex=0;bool menuOpen=false,dirty=true,screenOff=false;
uint32_t drawAt=0,lastInput=0,menuAt=0,actionAt=0,popupAt=0,ledAt=0;String action;
unsigned unreadCount=0;struct {uint64_t source=0;uint32_t session=0,id=0;} newest;
const uint8_t* activeFont=nullptr;
const uint8_t* const small=u8g2_font_5x8_t_cyrillic;const uint8_t* const body=u8g2_font_6x13_t_cyrillic;const uint8_t* const bold=u8g2_font_6x13B_t_cyrillic;
String t(const char* en,const char* ru){return config.russian?ru:en;}
unsigned chars(const String& value){unsigned n=0;for(unsigned i=0;i<value.length();i++)n+=(uint8_t(value[i])&0xc0)!=0x80;return n;}
String clipped(const String& value,unsigned count){unsigned i=0,n=0,cut=0;while(i<value.length()&&n<count){if(n+2==count)cut=i;uint8_t c=value[i];i+=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;n++;}return i<value.length()&&count>2?value.substring(0,cut)+"..":value.substring(0,i);}
void notice(const String& value){action=value;actionAt=millis();dirty=true;}
// SetFont resets transparency; the monochrome canvas treats any non-zero colour as lit.
void useFont(const uint8_t* f){if(f!=activeFont){hardware.font.setFont(f);hardware.font.setFontMode(1);activeFont=f;}}
int width(const String& value,const uint8_t* f){useFont(f);return hardware.font.getUTF8Width(value.c_str());}
void say(int x,int y,const String& value,const uint8_t* f=body,uint16_t color=1){useFont(f);hardware.text(x,y,value,color);}
void sayRight(int x,int y,const String& value,const uint8_t* f=small,uint16_t color=1){say(x-width(value,f),y,value,f,color);}
// Up to `maximum` 21-column rows, wrapped at spaces; longer text pages every four seconds.
void textLines(const String& value,int y,unsigned maximum){
 String rows[12];unsigned at=0,count=0;
 while(at<value.length()&&count<12){unsigned n=0,end=at,space=0;while(end<value.length()&&n<21&&value[end]!='\n'){uint8_t c=value[end];end+=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;n++;if(c==' ')space=end;}
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
template<class T> String applyOne(const char* key,T value){StaticJsonDocument<96>d;d[key]=value;return applySettings(d.as<JsonObjectConst>());}

// Actions: a screen with one action runs it on hold; several open a menu.
enum Act {ActAdvert,ActReplyOk,ActReplyAck,ActOlder,ActNewer,ActNextNode,ActNodeOk,ActResetPath,ActGps,ActPosition,ActWifi,ActBle,ActLanguage,ActBattery,ActScreen,ActContrast,ActSelfTest,ActClose};
unsigned actions(Act* out){
 unsigned n=0;switch(page){
 case Home:out[n++]=ActAdvert;break;
 case Messages:if(meshRadio.historyCount){out[n++]=ActReplyOk;out[n++]=ActReplyAck;out[n++]=ActOlder;out[n++]=ActNewer;}break;
 case Nodes:if(Peer* p=shownNode()){out[n++]=ActNextNode;if(p->type==1)out[n++]=ActNodeOk;if(p->pathLength!=255)out[n++]=ActResetPath;}out[n++]=ActAdvert;break;
 case Gps:out[n++]=ActGps;out[n++]=ActPosition;break;
 case Wifi:out[n++]=ActWifi;break;case Ble:out[n++]=ActBle;break;
 case Settings:out[n++]=ActLanguage;out[n++]=ActBattery;out[n++]=ActScreen;out[n++]=ActContrast;break;
 case Modules:out[n++]=ActSelfTest;break;
 }if(n>1)out[n++]=ActClose;return n;
}
bool keepsMenu(Act a){return a==ActOlder||a==ActNewer||a==ActNextNode||a==ActLanguage||a==ActBattery||a==ActScreen||a==ActContrast;}
String actName(Act a){
 const ChatMessage* m=shownMessage();bool publicChat=m&&m->destination==meshmesh::Broadcast;
 switch(a){
 case ActAdvert:return t("Announce node","Объявить узел");case ActReplyOk:return t("Reply: OK","Ответить: OK")+String(publicChat?" #":"");case ActReplyAck:return t("Reply: Got it","Ответить: Принято");
 case ActOlder:return t("Older message","Предыдущее");case ActNewer:return t("Newer message","Следующее");case ActNextNode:return t("Next node","Следующий узел");case ActNodeOk:return t("Send: OK","Написать: OK");case ActResetPath:return t("Reset path","Сбросить путь");
 case ActGps:return config.gps?t("Turn GPS off","Выключить GPS"):t("Turn GPS on","Включить GPS");case ActPosition:return t("Share position","Передать позицию");
 case ActWifi:return portalActive()?t("Turn Wi-Fi off","Выключить Wi-Fi"):t("Turn Wi-Fi on","Включить Wi-Fi");case ActBle:return bleActive()?t("Turn BLE off","Выключить BLE"):t("Turn BLE on","Включить BLE");
 case ActLanguage:return t("Language: English","Язык: русский");case ActBattery:return config.batteryVolts?t("Battery: volts","Батарея: вольты"):t("Battery: percent","Батарея: проценты");
 case ActScreen:return t("Screen off: ","Гасить: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда"));case ActContrast:return t("Contrast: ","Контраст: ")+String(config.brightness);
 case ActSelfTest:return t("Encryption test","Тест шифрования");case ActClose:return t("< Close menu","< Закрыть меню");
 }return "";
}
void reply(const String& text){const ChatMessage* m=shownMessage();if(!m)return;uint64_t to=m->destination==meshmesh::Broadcast?meshmesh::Broadcast:m->outgoing?m->destination:m->source;bool sent=to!=meshRadio.nodeId&&meshRadio.sendMessage(text,to);notice(sent?t("Reply ","Ответ ")+text+t(" queued"," в очереди"):t("Reply not queued","Ответ не отправлен"));}
void run(Act a){
 switch(a){
 case ActAdvert:notice(meshRadio.sendHello()?t("Node announced","Узел объявлен"):t("Announcement failed","Объявление не отправлено"));break;
 case ActReplyOk:reply("OK");break;case ActReplyAck:reply(t("Got it","Принято"));break;
 case ActOlder:messageOffset++;shownMessage();break;case ActNewer:messageOffset=max(0,messageOffset-1);break;case ActNextNode:nodeIndex++;menuIndex=0;break;
 case ActNodeOk:{Peer* p=shownNode();bool sent=p&&meshRadio.sendMessage("OK",p->id);notice(sent?t("OK queued","OK в очереди"):t("Not queued","Не отправлено"));break;}
 case ActResetPath:{Peer* p=shownNode();notice(p&&!meshRadio.busy()&&meshRadio.resetPath(p->id)?t("Path reset","Путь сброшен"):t("Path reset failed","Путь не сброшен"));break;}
 case ActGps:notice(applyOne("gps",!config.gps).startsWith("OK")?"GPS: "+String(config.gps?t("on","вкл"):t("off","выкл")):t("Radio busy, retry","Радио занято, повторите"));break;
 case ActPosition:notice(meshRadio.sendPosition()?t("Position shared","Позиция передана"):t("Needs a GPS fix","Нужна позиция GPS"));break;
 case ActWifi:portalToggle();break;case ActBle:bleToggle();break;
 case ActLanguage:applyOne("russian",!config.russian);break;case ActBattery:applyOne("battery_volts",!config.batteryVolts);break;
 case ActScreen:{const uint16_t steps[]={0,15,30,60,120,300};int i=0;while(i<5&&steps[i]!=config.dimAfter)i++;applyOne("dim_after",steps[(i+1)%6]);break;}
 case ActContrast:{const uint8_t steps[]={40,120,200,255};int i=0;while(i<3&&steps[i]<config.brightness)i++;applyOne("brightness",steps[(i+1)%4]);break;}
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
void footer(const String& hint){
 auto& c=*hardware.canvas;for(int i=0;i<PageCount;i++){int x=1+i*3;if(i==page)c.fillRect(x,58,2,4,1);else c.drawPixel(x,61,1);}
 if(hint.length())sayRight(128,63,clipped(hint,20));
}
String hint(){Act acts[8];unsigned n=actions(acts);if(!n)return "";return n==1?t("hold: ","держ: ")+actName(acts[0]):t("hold: menu","держ: меню");}
void drawMenu(){
 auto& c=*hardware.canvas;Act acts[8];unsigned n=actions(acts);if(!n){menuOpen=false;return;}menuIndex%=n;int first=max(0,min(menuIndex-1,int(n)-3));
 c.fillRect(0,12,128,45,0);c.drawRect(0,12,128,45,1);
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){int y=14+(i-first)*14;bool focus=int(i)==menuIndex;if(focus)c.fillRect(2,y,124,13,1);say(5,y+10,clipped(actName(acts[i]),20),body,focus?0:1);}
 c.fillRect(0,57,128,7,0);footer(t("click-next hold-run","клик-далее держ-ОК"));
}
void drawPopup(const ChatMessage& m){
 auto& c=*hardware.canvas;c.fillScreen(0);c.drawRect(0,0,128,64,1);c.drawRect(3,3,11,8,1);c.drawLine(3,3,8,7,1);c.drawLine(13,3,8,7,1);
 say(18,11,clipped(String(m.name)+(m.destination==meshmesh::Broadcast?" #":""),18),bold);textLines(m.text,25,3);sayRight(126,62,t("click: close","клик: закрыть"));
}
void draw(){
 auto& c=*hardware.canvas;c.fillScreen(0);const ChatMessage* last=meshRadio.historyCount?&meshRadio.history[meshRadio.historyCount-1]:nullptr;
 if(popupAt&&millis()-popupAt<8000&&last&&!last->outgoing){drawPopup(*last);hardware.flush();return;}
 popupAt=0;String title;
 switch(page){
 case Home:{title=config.name;say(0,31,clockText(time(nullptr)),u8g2_font_10x20_t_cyrillic);
  if(meshRadio.ready){sayRight(128,20,String(config.frequency,3)+t(" MHz"," МГц"));sayRight(128,30,"SF"+String(config.sf)+" BW"+String(config.bandwidth,1));}else sayRight(128,24,t("Radio error ","Ошибка радио ")+String(meshRadio.radioError));
  unsigned near=0;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].heard&&millis()-meshRadio.peers[i].seen<1800000)near++;
  say(0,43,"RX "+String(meshRadio.rxCount)+"  TX "+String(meshRadio.txCount)+t("  near "," рядом ")+String(near),small);
  unsigned mv=hardware.batteryMv;say(0,52,(mv>4250?t("USB power","Питание USB"):String(mv/1000.f,2)+"V")+(config.relay?t("  relay on","  ретрансляция"):""),small);break;}
 case Messages:{const ChatMessage* m=shownMessage();title=t("Messages","Сообщения")+(m?" "+String(meshRadio.historyCount-messageOffset)+"/"+String(meshRadio.historyCount):"");
  if(m){String who=m->outgoing?t("You","Вы"):String(m->name);if(m->destination==meshmesh::Broadcast)who+=" #";const char* en[]={"","queued","sent","delivered","no ACK"},*ru[]={"","очередь","отправл.","доставл.","нет ACK"};
   String st=m->outgoing?String(config.russian?ru[m->status]:en[m->status]):clockText(m->timestamp);say(0,23,clipped(who,20-chars(st)),bold);sayRight(128,22,st);textLines(m->text,36,2);}
  else{say(0,30,t("No messages yet","Сообщений ещё нет"));say(0,44,t("They appear here","Здесь появятся входящие"),small);}break;}
 case Nodes:{unsigned order[24];unsigned n=sortedNodes(order);Peer* p=shownNode();title=t("Nodes","Узлы")+(n?" "+String(nodeIndex%n+1)+"/"+String(n):"");
  if(p){say(0,23,clipped(p->name,21),bold);say(0,33,typeText(p->type)+", "+pathText(*p),small);
   say(0,42,p->heard?String(int(p->rssi))+" dBm SNR "+String(p->snr,1)+", "+ago(millis()-p->seen):t("saved, not heard","сохранён, не слышен"),small);
   float metres,bearing;if(distanceTo(*p,metres,bearing)){const char* ru[]={"С","СВ","В","ЮВ","Ю","ЮЗ","З","СЗ"},*en[]={"N","NE","E","SE","S","SW","W","NW"};int k=int((bearing+22.5f)/45)%8;say(0,51,(metres<1000?String(int(metres))+t(" m "," м "):String(metres/1000,1)+t(" km "," км "))+(config.russian?ru[k]:en[k]),small);}
   else if(p->position)say(0,51,t("has GPS position","есть GPS-позиция"),small);}
  else{say(0,30,t("No nodes heard","Узлы пока не найдены"));say(0,44,t("hold: announce","держите: объявить"),small);}break;}
 case Gps:{title="GPS";bool fix=hardware.gpsFix();
  say(0,23,!config.gps?t("GPS off","GPS выключен"):fix?t("Position fix","Позиция есть"):hardware.clockConflict?t("Old GPS date","Старая дата GPS"):hardware.gps.passedChecksum()?t("Searching sky","Поиск спутников"):t("No data from GPS","Нет данных GPS"),bold);
  say(0,33,t("Satellites ","Спутники ")+String(hardware.gps.satellites.value())+"  NMEA "+String(hardware.gps.passedChecksum()),small);
  if(fix)say(0,42,String(hardware.gps.location.lat(),5)+", "+String(hardware.gps.location.lng(),5),small);
  say(0,51,t("Clock ","Часы ")+clockText(time(nullptr))+" "+(hardware.clockSource=="unset"?t("not set","не задано"):hardware.clockSource),small);break;}
 case Wifi:title="Wi-Fi";if(portalActive()){say(0,23,"MM-"+meshRadio.idText(meshRadio.nodeId).substring(6),bold);say(0,36,portalPassword());say(0,48,"192.168.4.1",small);}else{say(0,27,t("Access point off","Точка доступа выкл."));say(0,41,t("Web chat and settings","Веб-чат и настройки"),small);}break;
 case Ble:title="Bluetooth";if(bleActive()){say(0,23,"MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6),bold);say(0,37,"PIN "+String(blePin()),bold);}else{say(0,27,t("Bluetooth off","Bluetooth выкл."));say(0,41,t("Secure pairing","Защищённое сопряжение"),small);}break;
 case Settings:title=t("Settings","Настройки");say(0,20,t("Language: ","Язык: ")+(config.russian?"русский":"English"),small);say(0,28,t("Battery: ","Батарея: ")+(config.batteryVolts?t("volts","вольты"):t("percent","проценты")),small);
  say(0,36,t("Screen off: ","Гасить экран: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда")),small);say(0,44,t("Contrast: ","Контраст: ")+String(config.brightness),small);
  say(0,52,String(config.frequency,3)+" SF"+String(config.sf)+" CR4/"+String(config.cr)+" "+String(config.power)+"dBm",small);break;
 default:title=t("Modules","Модули");say(0,21,"LoRa "+String(meshRadio.ready?"OK":"ERR")+"  FS "+String(hardware.fsOk?"OK":"ERR")+"  GPS "+String(!config.gps?"-":hardware.gps.passedChecksum()?"OK":"?"),small);
  say(0,30,String(ESP.getFreeHeap()/1024)+"K RAM  "+String(ESP.getFreePsram()/1024)+"K PSRAM",small);say(0,39,t("relayed ","переслано ")+String(meshRadio.relayed)+t("  rejected ","  откл. ")+String(meshRadio.rejected),small);
  say(0,48,t("up ","работа ")+ago(millis())+t("  boot ","  загр. ")+String(config.bootCounter),small);
 }
 header(title);
 if(menuOpen)drawMenu();else footer(hint());
 if(action.length()&&millis()-actionAt<3500){c.fillRect(0,36,128,20,0);c.drawRect(0,36,128,20,1);say(3,50,clipped(action,20));}
 hardware.flush();
}
}
void uiKey(int key){
 lastInput=millis();dirty=true;
 if(screenOff){screenOff=false;hardware.brightness(config.brightness);return;} // the first press only wakes the panel
 if(popupAt){popupAt=0;if(key==13)return;}
 if(menuOpen){menuAt=millis();Act acts[8];unsigned n=actions(acts);if(!n){menuOpen=false;return;}
  if(key==13||key==0x82){menuIndex=(menuIndex+1)%n;return;}
  if(key==0xa3){Act a=acts[menuIndex%n];run(a);if(!keepsMenu(a))menuOpen=false;}return;}
 if(key==13||key==0x82){page=(page+1)%PageCount;if(page==Messages){messageOffset=0;unreadCount=0;}return;}
 if(key==0xa3){Act acts[8];unsigned n=actions(acts);if(n==1)run(acts[0]);else if(n>1){menuOpen=true;menuIndex=0;menuAt=millis();}}
}
void uiBegin(){pinMode(pins::led,OUTPUT);digitalWrite(pins::led,LOW);lastInput=millis();if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];newest={m.source,m.session,m.id};}draw();}
String uiStatus(){StaticJsonDocument<384>d;d["action"]=millis()-actionAt<3500?action:String();d["page"]=pageNames[page];d["locked"]=false;d["menu"]=menuOpen;d["menu_index"]=menuIndex;d["screen_off"]=screenOff;d["popup"]=popupAt!=0;d["unread"]=unreadCount;String s;serializeJson(d,s);return s;}
void uiTick(){
 uint32_t now=millis();
 // New incoming message: popup, wake the panel and blink the LED three times.
 if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];if(m.source!=newest.source||m.session!=newest.session||m.id!=newest.id){newest={m.source,m.session,m.id};if(!m.outgoing){if(page!=Messages)unreadCount++;popupAt=now;ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}}
 if(ledAt){uint32_t e=now-ledAt;digitalWrite(pins::led,e<1500&&(e/250)%2==0);if(e>=1500){ledAt=0;digitalWrite(pins::led,LOW);}}
 if(menuOpen&&now-menuAt>10000){menuOpen=false;dirty=true;}
 if(config.dimAfter&&!screenOff&&now-lastInput>=config.dimAfter*1000UL){screenOff=true;hardware.brightness(0);}
 if(screenOff)return;
 if((dirty||meshRadio.dirty||now-drawAt>1000)&&now-drawAt>150){draw();drawAt=now;dirty=false;meshRadio.dirty=false;}
}
