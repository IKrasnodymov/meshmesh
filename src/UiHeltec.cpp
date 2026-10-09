#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: the screens and menus are not speed-critical (the rest of the nRF52 image is -O2)
#endif
#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "Remote.h"
#include "Radar.h"
#include "ChessNet.h"
#include "ChessTour.h"
#include "MeshServer.h"
#include "Pet.h"
#include "Dice.h"
#include "Power.h"
#include "Regions.h"
#include <math.h>
#include <time.h>
#if defined(MM_HIRES)
#include "Palette.h"
#include "UiIcons.h"
#endif
#if defined(MM_NRF52)
#include <esp_system.h> // ESP.getFreeHeap(); Arduino.h provides it on the ESP32
#endif
// One PRG button: click = next screen or next menu item; hold = the screen's action, or its menu
// (chess on the board: click = next choice, hold = take it; UiChessCompact.inc).
// Joystick boards (MM_JOYSTICK, GAT562): left/right change the screen, up/down scroll the screen or
// the menu, the centre runs the action or opens the menu, Back closes and goes home; messages are
// written on an on-screen keyboard (UiCompose.inc).
namespace {
enum Page {Home,Messages,Nodes,Chess,PetPage,DicePage,Signals,Gps,Wifi,Ble,Settings,Modules,PageCount};
const char* pageNames[]={"home","messages","nodes","chess","pet","dice","radar","gps","wifi","ble","settings","modules"};
int page=Home,menuIndex=0,messageOffset=0,nodeIndex=0;bool menuOpen=false,dirty=true,screenOff=false;
uint32_t drawAt=0,lastInput=0,menuAt=0,actionAt=0,popupAt=0,ledAt=0,pingAt=0,pingedSamples=0;String action;
inline __attribute__((always_inline)) void led(bool on){if(pins::led>=0)digitalWrite(pins::led,on?pins::ledOn:!pins::ledOn);}
uint32_t chessPopupAt=0,chessSeen=0,tourSeen=0;String chessPopupText; // chess and tournament news
unsigned unreadCount=0;struct {uint64_t source=0;uint32_t session=0,id=0;} newest;
const uint8_t* activeFont=nullptr;
const uint8_t* const small=u8g2_font_5x8_t_cyrillic;const uint8_t* const body=u8g2_font_6x13_t_cyrillic;const uint8_t* const bold=u8g2_font_6x13B_t_cyrillic;
#define t(en,ru) String(tr(en,ru)) // a macro: tr() keys its translation at compile time (I18n.h)
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
#if defined(MM_HIRES)
// TFT: each layout font has a larger one of the same column width at 240x135 (HiresCanvas.h), drawn
// on the screen itself; x and the baseline y are screen pixels. Bold: the glyph twice, 1 px apart.
struct HiFont{const uint8_t* main;const uint8_t* latin;bool bold;};
HiFont hiFont(const uint8_t* f){
 if(f==small)return{u8g2_font_9x15_t_cyrillic,u8g2_font_9x15_tf,false};
 if(f==u8g2_font_4x6_t_cyrillic)return{u8g2_font_7x13_t_cyrillic,u8g2_font_7x13_tf,false};
 if(f==u8g2_font_10x20_t_cyrillic)return{u8g2_font_inr24_t_cyrillic,nullptr,false};
 return{u8g2_font_10x20_t_cyrillic,u8g2_font_10x20_tf,f==bold};
}
int hiGlyph(int x,int y,uint32_t cp,const uint8_t* f,uint16_t color,bool paint){
 HiresCanvas& c=*hardware.canvas;HiFont h=hiFont(f);
 const uint8_t* use=cp>0xffff?nullptr:fontHasGlyph(h.main,cp)?h.main:h.latin&&fontHasGlyph(h.latin,cp)?h.latin:nullptr;
 if(use){hardware.font.begin(c.screen);useFont(use);int w=u8g2_GetGlyphWidth(&hardware.font.u8g2,cp)+h.bold;
  if(paint){hardware.font.setForegroundColor(color);hardware.font.drawGlyph(x,y,cp);if(h.bold)hardware.font.drawGlyph(x+1,y,cp);}
  return w;}
 // What the large fonts lack (CJK, Arabic, extended Latin): the glyph subsets of the 12-20 px layout fonts
 // at their own size, as tall as the large fonts' capitals (small text takes the 13 px ones).
 const uint8_t* layout=f==small?body:f==u8g2_font_4x6_t_cyrillic?u8g2_font_6x12_t_cyrillic:f;
 if(cp<=0xffff)for(const uint8_t* const* x=fallbackFonts(layout);*x;x++)if(fontHasGlyph(*x,cp)){use=*x;break;}
 if(!use)return 0;hardware.font.begin(c.screen);useFont(use);if(paint){hardware.font.setForegroundColor(color);return hardware.font.drawGlyph(x,y,cp);}
 return u8g2_GetGlyphWidth(&hardware.font.u8g2,cp);
}
#endif
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
// The latest answer of a repeater or room, or of a route trace, in two short lines (empty when none).
String remoteLogin(const Peer& p){
 remote::Session* s=remote::find(p.id);if(!s||s->login==remote::Idle)return "";
 switch(s->login){case remote::Waiting:return t("login: waiting","вход: ждём");case remote::Done:return s->admin?t("logged in: admin","вход: админ"):t("logged in","вход выполнен");case remote::Refused:return t("login refused","вход: отказ");default:return t("login: no answer","вход: нет ответа");}
}
String remoteResult(const Peer& p){
 auto& tr=remote::trace;
 if(tr.id==p.id&&tr.state!=remote::Idle){
  if(tr.state==remote::Waiting)return t("trace: waiting","трасса: ждём");if(tr.state!=remote::Done)return t("trace: no answer","трасса: нет ответа");
  String r;for(unsigned i=0;i<tr.hops;i++)r+=String(tr.snr[i]/4.0f,0)+">";return t("SNR ","SNR ")+r+String(tr.snr[tr.hops]/4.0f,0);
 }
 remote::Session* s=remote::find(p.id);if(!s||s->status==remote::Idle)return "";
 if(s->status!=remote::Done)return s->status==remote::Waiting?t("status: waiting","статус: ждём"):t("status: no answer","статус: нет ответа");
 return String(s->battery/1000.0f,2)+t("V up ","В ")+String(s->uptime/3600)+t("h rx ","ч прм ")+String(s->received);
}
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
 if((p==Chess&&!MM_CHESS)||(p==PetPage&&!MM_PET)||(p==DicePage&&!MM_DICE))return false; // modules left out of this image (Modules.h)
 return config.role==RoleNormal||p==Home||p==PetPage||p==DicePage||p==Gps||p==Wifi||p==Ble||p==Settings||p==Modules;} // the pet lives in every role
} // namespace
// The pages after Home, in the default order; the chosen order and the hidden ones: config.apps (App.h).
#if defined(MM_NO_WIFI)
#define MM_WIFI_APP(x)
#else
#define MM_WIFI_APP(x) x,
#endif
const char* const uiApps[]={"chats","nodes",
#if MM_CHESS
 "chess",
#endif
#if MM_PET
 "pet",
#endif
#if MM_DICE
 "dice",
#endif
 "radar","gps",MM_WIFI_APP("wifi")"ble","settings","health"};
const uint8_t uiAppCount=sizeof uiApps/sizeof *uiApps;
namespace {
const uint8_t appPages[]={Messages,Nodes,
#if MM_CHESS
 Chess,
#endif
#if MM_PET
 PetPage,
#endif
#if MM_DICE
 DicePage,
#endif
 Signals,Gps,MM_WIFI_APP(Wifi)Ble,Settings,Modules};
static_assert(sizeof appPages==sizeof uiApps/sizeof *uiApps,"a page for every app ID");
// What a click goes through: Home, then the shown apps in the chosen order.
unsigned pageCycle(uint8_t* cycle){unsigned n=0;cycle[n++]=Home;uint8_t order[AppsMax];unsigned k=appsShown(order);for(unsigned i=0;i<k;i++)if(pageShown(appPages[order[i]]))cycle[n++]=appPages[order[i]];return n;}
int stepPage(int p,int step){uint8_t c[AppsMax+1];unsigned n=pageCycle(c);for(unsigned i=0;i<n;i++)if(c[i]==p)return c[(i+n+step)%n];return Home;} // a hidden page opened by an event: back to Home
int nextPage(int p){return stepPage(p,1);}
int previousPage(int p){return stepPage(p,-1);}
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
#endif
#if MM_CHESS
#include "UiChessCompact.inc"
#endif
#if MM_PET
#include "UiPetCompact.inc"
#endif
template<class T> String applyOne(const char* key,T value){StaticJsonDocument<96>d;d[key]=value;return applySettings(d.as<JsonObjectConst>());}
// Radio settings on the device: a draft saved at once, as on the M9. One button: click - next row,
// hold - change it (click - next value, hold - done); the frequency goes digit by digit (click - the
// digit +1, hold - the next digit). Joystick: up/down - rows, left/right - values, OK on the frequency -
// its digits (left/right - digit, up/down - value), back - leave without saving.
// Region: the default one of our floods (Regions.h), chosen among the regions found; Find regions: asks the
// repeaters that hear this node (hold).
enum RadioRow {RowFreq,RowBw,RowSf,RowCr,RowPower,RowHops,RowRelay,RowHash,RowRegion,RowFind,RowSave,RowCancel,RadioRows};
bool radioEdit=false,radioEditing=false;int radioRow=0,radioDigit=0;Config radioDraft;
void openRadio(){radioDraft=config;radioEdit=true;radioEditing=false;radioRow=0;radioDigit=0;dirty=true;}
String radioName(int i){
 switch(i){case RowFreq:return t("Frequency","Частота");case RowBw:return t("Bandwidth","Полоса");case RowSf:return "SF";case RowCr:return "CR";
 case RowPower:return t("Power","Мощность");case RowHops:return t("Relay limit","Предел перес.");case RowRelay:return t("Relaying","Ретрансляция");case RowHash:return t("Path hash","Хэш пути");
 case RowRegion:return t("Region","Регион");case RowFind:return t("Find regions","Найти регионы");
 case RowSave:return t("Save","Сохранить");default:return t("Cancel","Отмена");}
}
String radioValue(const Config& c,int i){
 switch(i){case RowFreq:return String(c.frequency,3);case RowBw:return String(c.bandwidth,1);case RowSf:return String(c.sf);case RowCr:return "4/"+String(c.cr);
 case RowPower:return String(c.power)+" dBm";case RowHops:return String(c.hops);case RowRelay:return c.relay?t("on","вкл"):t("off","выкл");
 case RowHash:return plural(c.pathHash,"byte","bytes","байт","байта","байт");
 case RowRegion:return c.region[0]?String(c.region):String(t("none","нет"));
 case RowFind:{auto& s=regions::search;return regions::searching()?String(s.repeaters)+t(" rpt..."," ретр..."):s.state==regions::Search::Done?plural(s.count,"found","found","найден","найдено","найдено"):String("");}}return "";
}
bool radioChanged(){for(int i=0;i<RowSave;i++)if(radioValue(radioDraft,i)!=radioValue(config,i))return true;return false;}
// The frequency digit being set: 0 - megahertz (863..870), 1..3 - the kilohertz digits.
void radioDigitSpan(const String& v,unsigned& from,unsigned& to){if(radioDigit==0){from=0;to=v.indexOf('.');}else{from=v.indexOf('.')+radioDigit;to=from+1;}}
void radioStep(int dir){
 auto& d=radioDraft;
 switch(radioRow){
 case RowFreq:{long k=lroundf(d.frequency*1000);
  if(radioDigit==0){k+=dir*1000L;if(k>=871000)k-=8000;if(k<863000)k+=8000;}
  else{long unit=radioDigit==1?100:radioDigit==2?10:1;int digit=k/unit%10;k+=((digit+dir+10)%10-digit)*unit;}
  d.frequency=constrain(k,863000L,870000L)/1000.f;break;}
 case RowBw:{const float bw[]={62.5f,125,250,500};int i=0;while(i<3&&d.bandwidth!=bw[i])i++;d.bandwidth=bw[(i+dir+4)%4];break;}
 case RowSf:d.sf=7+(d.sf-7+dir+6)%6;break;
 case RowCr:d.cr=5+(d.cr-5+dir+4)%4;break;
 case RowPower:d.power=(d.power+dir+MM_MAX_POWER+1)%(MM_MAX_POWER+1);break;
 case RowHops:d.hops=(d.hops+dir+8)%8;break;
 case RowRelay:d.relay=!d.relay;break;
 case RowHash:d.pathHash=1+(d.pathHash-1+dir+3)%3;break;
 case RowRegion:strlcpy(d.region,regions::cycle(d.region,dir,false).c_str(),sizeof d.region);break;
 case RowFind:if(!regions::find())notice(t("Radio offline","Радио недоступно"));break;
 }dirty=true;
}
void radioSave(){
 if(!radioChanged()){radioEdit=false;notice(t("Nothing to save","Изменений нет"));return;}
 StaticJsonDocument<256> j;auto& d=radioDraft;j["frequency"]=d.frequency;j["bandwidth"]=d.bandwidth;j["sf"]=int(d.sf);j["cr"]=int(d.cr);j["power"]=int(d.power);j["hops"]=int(d.hops);j["relay"]=d.relay;j["path_hash"]=int(d.pathHash);j["region"]=d.region;
 String r=applySettings(j.as<JsonObjectConst>());bool saved=r.startsWith("OK");if(saved)radioEdit=false;
 notice(saved?t("Settings saved","Настройки сохранены"):r.startsWith("ERR radio busy")?t("Radio busy, retry","Радио занято, повторите"):r.substring(4));
}
void radioButton(bool click,bool hold){
 if(radioEditing){if(click)radioStep(1);else if(hold){if(radioRow==RowFreq&&radioDigit<3)radioDigit++;else{radioEditing=false;radioDigit=0;}}return;}
 if(click){radioRow=(radioRow+1)%RadioRows;return;}
 if(!hold)return;
 if(radioRow==RowSave)radioSave();else if(radioRow==RowCancel)radioEdit=false;else if(radioRow==RowRelay||radioRow==RowFind)radioStep(1);else{radioEditing=true;radioDigit=0;}
}
#if defined(MM_JOYSTICK)
void radioJoystick(bool up,bool down,bool left,bool right,bool ok,bool back){
 if(radioEditing){if(up||down)radioStep(up?1:-1);else if(left||right)radioDigit=constrain(radioDigit+(right?1:-1),0,3);else if(ok||back)radioEditing=false;return;}
 if(up||down){radioRow=(radioRow+(up?RadioRows-1:1))%RadioRows;return;}
 if(left||right){if(radioRow==RowFreq){radioEditing=true;radioDigit=0;}else if(radioRow<RowSave&&radioRow!=RowFind)radioStep(right?1:-1);return;}
 if(ok){if(radioRow==RowSave)radioSave();else if(radioRow==RowCancel)radioEdit=false;else if(radioRow==RowFreq){radioEditing=true;radioDigit=0;}else radioStep(1);return;}
 if(back)radioEdit=false;
}
#endif
// The key hints of the radio screen: what a click and a hold (or the joystick) do now.
String radioHint(){
#if defined(MM_JOYSTICK)
 if(radioEditing)return t("<> digit, up/down value","<> цифра, вверх/вниз");
 return radioRow>=RowSave?t("up/down, OK","вверх/вниз, OK"):t("up/down, <> change","вверх/вниз, <> изменить");
#else
 if(radioEditing)return radioRow==RowFreq?t("click-digit+ hold-next","клик-цифра+ держ-далее"):t("click-change hold-done","клик-изм. держ-готово");
 return radioRow==RowSave?t("click-next hold-save","клик-далее держ-сохр."):radioRow==RowCancel?t("click-next hold-leave","клик-далее держ-выйти"):t("click-next hold-change","клик-далее держ-изм.");
#endif
}

// Actions: a screen with one action runs it on hold; several open a menu.
enum Act {ActFormat,ActJoin,ActJoinHeard,ActWrite,ActChess,ActChessOpen,ActChessNext,ActSound,ActRole,ActForward,ActAdvert,ActReplyOk,ActReplyAck,ActOlder,ActNewer,ActNextNode,ActNodeOk,ActResetPath,ActGps,ActPosition,ActWifi,ActBle,ActLanguage,ActBattery,ActScreen,ActContrast,ActSelfTest,ActHoming,ActNextSignal,ActStopHoming,ActResetPeak,ActCsiBeacon,ActCsiSensor,ActCalibrate,ActPetCuddle,ActPetFeed,ActPetHeal,ActPetEgg,ActPetDeath,ActPetAdopt,ActPetRelease,
  ActPowerOff,ActRadio,ActDiceRoll,ActDiceSaved,ActDiceNextSaved,ActDiceCount,ActDiceType,ActDiceMod,ActDiceHero,ActDiceMode,ActDiceGridMore,ActDiceGridFive,ActDiceGridRow,ActDiceGridType,ActDiceThreshold,
  ActDicePlus1,ActDiceMinus1,ActDicePlus5,ActDiceMinus5,ActDiceNextCounter,ActDiceAddCounter,ActDiceTap,ActRemoteLogin,ActRemoteStatus,ActTrace,ActClose};
constexpr unsigned MenuMax=10;
#if MM_DICE
#include "UiDiceCompact.inc"
#endif
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
  if(p->type==1){out[n++]=ActWrite;if(MM_CHESS)out[n++]=ActChess;}
#else
  out[n++]=ActNextNode;if(MM_CHESS&&p->type==1)out[n++]=ActChess;
#endif
  if(p->type==1)out[n++]=ActNodeOk;
  if(p->type==2||p->type==3){out[n++]=ActRemoteLogin;out[n++]=ActRemoteStatus;} // with the saved password, else as a guest
  if(remote::traceable(p->id))out[n++]=ActTrace;
  if(p->pathLength!=255)out[n++]=ActResetPath;}out[n++]=ActAdvert;break;
 case Chess:
#if !defined(MM_JOYSTICK)
#if MM_CHESS
  {ChessMatch* games[ChessNet::MaxMatches];unsigned g=chessGames(games);if(g){out[n++]=ActChessOpen;if(g>1)out[n++]=ActChessNext;}} // the joystick opens with OK
#endif
#endif
  break;
 case Signals:
#if defined(MM_NO_WIFI)
  if(radar.tracking){out[n++]=ActStopHoming;out[n++]=ActResetPeak;}else if(radar.count)out[n++]=ActHoming;break; // no Wi-Fi CSI
#endif
  if(radar.csi==Radar::CsiSensor){out[n++]=ActCalibrate;out[n++]=ActCsiSensor;}
  else if(radar.tracking){out[n++]=ActStopHoming;out[n++]=ActResetPeak;}
  else{out[n++]=ActCsiBeacon;if(radar.csi==Radar::CsiOff){out[n++]=ActCsiSensor;if(radar.count){out[n++]=ActHoming;out[n++]=ActNextSignal;}}}break; // CSI first: the beacon is the usual Heltec role
#if MM_PET
 case PetPage:
  if(!creature.has()){out[n++]=ActPetAdopt;break;} // a pet is optional
  if(!creature.alive())out[n++]=ActPetEgg;else{out[n++]=ActPetCuddle;if(creature.s.stage!=pet::Egg){out[n++]=ActPetFeed;if(creature.s.health<800)out[n++]=ActPetHeal;}}
  out[n++]=ActPetDeath;out[n++]=ActPetRelease;break;
#endif
#if MM_DICE
 case DicePage:n=diceActions(out);break;
#endif
 case Gps:out[n++]=ActGps;out[n++]=ActPosition;break;
 case Wifi:out[n++]=ActWifi;break;case Ble:out[n++]=ActBle;break;
 case Settings:
#if defined(MM_JOYSTICK)
  out[n++]=ActSound; // the GAT562 buzzer
#endif
  out[n++]=ActRadio;out[n++]=ActLanguage;out[n++]=ActBattery;out[n++]=ActScreen;out[n++]=ActContrast;out[n++]=ActRole;out[n++]=ActPowerOff;break;
 case Modules:if(!hardware.fsOk)out[n++]=ActFormat;out[n++]=ActSelfTest;break; // FS ERR is shown here
 }if(n>1)out[n++]=ActClose;return n;
}
bool keepsMenu(Act a){
#if MM_DICE
 if(a>=ActDiceRoll&&a<ActClose)return diceKeeps(a);
#endif
 return a==ActFormat||a==ActPowerOff||a==ActSound||a==ActNextSignal||a==ActOlder||a==ActNewer||a==ActNextNode||a==ActChessNext||a==ActLanguage||a==ActBattery||a==ActScreen||a==ActContrast||a==ActPetDeath||a==ActPetRelease;}
String actName(Act a){
#if MM_DICE
 if(a>=ActDiceRoll&&a<ActClose)return diceActName(a);
#endif
 const ChatMessage* m=shownMessage();bool publicChat=m&&channels::isChannel(m->destination);
 switch(a){
 case ActWrite:return page==Nodes?t("Write message...","Написать...")
  :page==Messages&&m&&!publicChat?t("Write reply...","Ответить текстом..."):t("Write to channel...","Написать в канал...");
 case ActFormat:return t("Create storage...","Создать хранилище...");
 case ActJoin:{String name;uint8_t key[16];channels::parseLink(m?m->text:"",name,key);return t("Join ","Вступить: ")+name;}
 case ActJoinHeard:{const HeardChannel* h=heardTag();return t("Join heard ","Вступить: ")+(h?h->name:"");}
 case ActChess:return t("Chess: invite","Шахматы: вызвать");
 case ActChessOpen:return t("Open game","Открыть");
 case ActChessNext:return t("Next game","Следующая партия");
 case ActSound:return config.sound?t("Sound: on","Звук: вкл."):t("Sound: off","Звук: выкл.");
 case ActRole:return t("Device mode...","Режим работы...");case ActForward:return meshServer.view().forwarding?t("Forwarding: off","Пересылка: выкл."):t("Forwarding: on","Пересылка: вкл.");
 case ActAdvert:return t("Announce node","Объявить узел");case ActReplyOk:return t("Reply: OK","Ответить: OK")+String(publicChat?" #":"");case ActReplyAck:return t("Reply: Got it","Ответить: Принято");
 case ActOlder:return t("Older message","Предыдущее");case ActNewer:return t("Newer message","Следующее");case ActNextNode:return t("Next node","Следующий узел");case ActNodeOk:return t("Send: OK","Написать: OK");case ActResetPath:return t("Reset path","Сбросить путь");
 case ActRemoteLogin:{Peer* p=shownNode();return p&&remote::saved(p->id)?t("Log in","Войти"):t("Log in as guest","Войти гостем");}case ActRemoteStatus:return t("Server status","Статус сервера");case ActTrace:return t("Trace route","Трассировка");
 case ActGps:return config.gps?t("Turn GPS off","Выключить GPS"):t("Turn GPS on","Включить GPS");case ActPosition:return t("Share position","Передать позицию");
 case ActWifi:return portalActive()?t("Turn Wi-Fi off","Выключить Wi-Fi"):t("Turn Wi-Fi on","Включить Wi-Fi");case ActBle:return bleActive()?t("Turn BLE off","Выключить BLE"):t("Turn BLE on","Включить BLE");
 case ActLanguage:return t("Language: ","Язык: ")+langNames[config.lang<LangCount?config.lang:0];case ActBattery:return config.batteryVolts?t("Battery: volts","Батарея: вольты"):t("Battery: percent","Батарея: проценты");
 case ActScreen:return t("Screen off: ","Гасить: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда"));case ActContrast:return t("Contrast: ","Контраст: ")+String(config.brightness);
 case ActHoming:{int i=shownSignal();return t("Home in: ","Пеленг: ")+(i>=0?signalName(radar.targets[i]):String("-"));}case ActNextSignal:return t("Next signal","Следующий сигнал");
 case ActCsiBeacon:return radar.csi==Radar::CsiBeacon?t("Stop beacon","Выключить маяк"):t("CSI beacon: on","Маяк CSI: включить");
 case ActCsiSensor:return radar.csi==Radar::CsiSensor?t("CSI motion: off","Движение CSI: выкл."):t("CSI sensor","Приёмник CSI");
 case ActCalibrate:return t("Calibrate (10 s still)","Калибровка (10 с тихо)");
 case ActStopHoming:return t("Stop homing","Остановить пеленг");case ActResetPeak:return t("Reset peak","Сбросить пик");
 case ActPetCuddle:return creature.s.stage==pet::Egg?t("Knock","Постучать"):t("Pet","Погладить");
 case ActPetFeed:return t("Feed a snack: ","Кормить, вкусн.: ")+String(creature.s.snacks);case ActPetHeal:return t("Heal","Лечить");case ActPetEgg:return t("New egg","Новое яйцо");
 case ActPetAdopt:return t("Start a pet","Завести питомца");case ActPetRelease:return t("Let it go...","Отпустить...");
 case ActPetDeath:return creature.s.mortal?t("Death: on","Смерть: вкл."):t("Death: off","Смерть: выкл.");
 case ActPowerOff:return t("Turn off...","Выключить...");case ActRadio:return t("Radio...","Радио...");
 case ActSelfTest:return t("Encryption test","Тест шифрования");case ActClose:return t("< Close menu","< Закрыть меню");
 default:break;
 }return "";
}
void reply(const String& text){const ChatMessage* m=shownMessage();if(!m)return;uint64_t to=channels::isChannel(m->destination)?m->destination:m->outgoing?m->destination:m->source;bool sent=to!=meshRadio.nodeId&&meshRadio.sendMessage(text,to);notice(sent?t("Reply ","Ответ ")+text+t(" queued"," в очереди"):t("Reply not queued","Ответ не отправлен"));}
void run(Act a){
#if MM_DICE
 if(a>=ActDiceRoll&&a<ActClose){diceRun(a);return;}
#endif
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
#if MM_CHESS
 case ActChess:
  if(Peer* p=shownNode()){ // the board shows the invitation; no news popup for it
   uint32_t before=chessNet.events;ChessMatch* m=chessNet.invite(p->id,2);if(m){showPage(Chess);openChess(m);notice(t("Challenge sent","Вызов отправлен"));}else if(chessNet.events!=before)notice(chessNet.event);chessSeen=chessNet.events;}
  break;
 case ActChessOpen:{ChessMatch* games[ChessNet::MaxMatches];unsigned g=chessGames(games);if(g)openChess(games[chessSel%g]);break;}
 case ActChessNext:chessSel++;menuIndex=0;break;
#endif
 case ActRole:openRolePick(false);break;case ActRadio:openRadio();break;
 case ActForward:{String r=meshServer.command(meshServer.view().forwarding?"set repeat off":"set repeat on");notice(r.startsWith("OK")?(meshServer.view().forwarding?t("Forwarding on","Пересылка вкл."):t("Forwarding off","Пересылка выкл.")):r);break;}
 case ActAdvert:notice(meshRadio.sendHello()?t("Node announced","Узел объявлен"):t("Announcement failed","Объявление не отправлено"));break;
 case ActReplyOk:reply("OK");break;case ActReplyAck:reply(t("Got it","Принято"));break;
 case ActOlder:messageOffset++;shownMessage();break;case ActNewer:messageOffset=max(0,messageOffset-1);break;case ActNextNode:nodeIndex++;menuIndex=0;break;
 case ActNodeOk:{Peer* p=shownNode();bool sent=p&&meshRadio.sendMessage("OK",p->id);notice(sent?t("OK queued","OK в очереди"):t("Not queued","Не отправлено"));break;}
 case ActResetPath:{Peer* p=shownNode();notice(p&&!meshRadio.busy()&&meshRadio.resetPath(p->id)?t("Path reset","Путь сброшен"):t("Path reset failed","Путь не сброшен"));break;}
 case ActRemoteLogin:case ActRemoteStatus:case ActTrace:{Peer* p=shownNode();bool sent=p&&(a==ActRemoteLogin?remote::login(p->id,""):a==ActRemoteStatus?remote::status(p->id):remote::traceTo(p->id));notice(sent?t("Sent, waiting","Отправлено, ждём"):t("Not sent","Не отправлено"));break;}
 case ActGps:notice(applyOne("gps",!config.gps).startsWith("OK")?"GPS: "+String(config.gps?t("on","вкл"):t("off","выкл")):t("Radio busy, retry","Радио занято, повторите"));break;
 case ActPosition:notice(meshRadio.sendPosition()?t("Position shared","Позиция передана"):t("Needs a GPS fix","Нужна позиция GPS"));break;
 case ActWifi:portalToggle();break;case ActBle:bleToggle();break;
 case ActSound:applyOne("sound",!config.sound);hardware.beep();break;
 case ActLanguage:applyOne("lang",langCodes[langStep(config.lang,1)]);break;case ActBattery:applyOne("battery_volts",!config.batteryVolts);break;
 case ActScreen:{const uint16_t steps[]={0,15,30,60,120,300};int i=0;while(i<5&&steps[i]!=config.dimAfter)i++;applyOne("dim_after",steps[(i+1)%6]);break;}
 case ActContrast:{const uint8_t steps[]={10,60,150,255};int i=0;while(i<3&&steps[i]<config.brightness)i++;applyOne("brightness",steps[(i+1)%4]);break;}
 case ActHoming:{int i=shownSignal();signalManual=true;if(i>=0&&radar.track(i)){pingedSamples=radar.samples;notice(t("Homing started","Пеленг начат"));}break;}
 case ActNextSignal:{int i=shownSignal();signalManual=true;if(i>=0){i=(i+1)%radar.count;signalId=radar.targets[i].id;signalKind=radar.targets[i].kind;}menuIndex=0;break;}
 case ActStopHoming:radar.untrack();break;case ActResetPeak:radar.resetPeak();notice(t("Peak reset","Пик сброшен"));break;
 case ActCsiBeacon:radar.setCsi(radar.csi==Radar::CsiBeacon?Radar::CsiOff:Radar::CsiBeacon);break;
 case ActCsiSensor:radar.setCsi(radar.csi==Radar::CsiSensor?Radar::CsiOff:Radar::CsiSensor);break;
 case ActCalibrate:if(radar.beaconHeard()){radar.calibrate();notice(t("Calibrating 10 s","Калибровка 10 с"));}else notice(t("No beacon heard","Маяк не слышен"));break;
#if MM_PET
 case ActPetCuddle:creature.cuddle();break;case ActPetFeed:creature.feed();break;case ActPetHeal:creature.heal();break;case ActPetEgg:creature.newEgg();break; // it answers in its bubble
 case ActPetDeath:notice(creature.setMortal(!creature.s.mortal));break;
 case ActPetAdopt:creature.newEgg();menuOpen=false;break;
 case ActPetRelease:{static uint32_t armed=0;if(!armed||millis()-armed>5000){armed=millis();
#if defined(MM_JOYSTICK)
  notice(t("OK again: let it go","Ещё раз OK: отпустить"));
#else
  notice(t("Hold again: let it go","Удерж. ещё: отпустить"));
#endif
  break;}armed=0;notice(creature.release());menuOpen=false;break;}
#endif
 case ActPowerOff:{static uint32_t armed=0;if(!armed||millis()-armed>5000){armed=millis();
#if defined(MM_JOYSTICK)
  notice(t("OK again: turn off","Ещё раз OK: выключить"));
#else
  notice(t("Hold again: turn off","Удерж. ещё: выключить"));
#endif
  break;}armed=0;powerOff();menuOpen=false;break;}
 case ActClose:default:break;
 case ActSelfTest:{bool valid=meshRadio.selfTest();meshRadio.event=valid?"Encryption test OK":"Encryption test FAILED";notice(valid?t("Encryption: OK","Шифрование: OK"):t("Encryption: ERROR","Шифрование: ошибка"));break;}
 }
}

// HUD: title on the left; unread, GPS, links, signal and battery on the right.
void header(const String& title){
 auto& c=*hardware.canvas;int x=127;unsigned mv=hardware.batteryMv;
 // Measured battery: volts or the charge estimate as text; the icon stays for no battery and for USB power.
 if(mv&&(config.batteryVolts||mv<=4250)){String v=config.batteryVolts?String(mv/1000.f,2)+"V":String(mv<=3300?0:mv>=4200?100:(mv-3300)/9)+"%";sayRight(x+1,7,v);x-=width(v,small)+3;}
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
 auto& c=*hardware.canvas;int x=1;uint8_t cycle[AppsMax+1];unsigned n=pageCycle(cycle);
#if MM_DICE
 if(diceTapOn())n=0; // counting: the button does not change the screen, the hint takes the row
#endif
 for(unsigned i=0;i<n;i++){if(cycle[i]==page)c.fillRect(x,58,2,4,1);else c.drawPixel(x,61,1);x+=3;}
 if(hint.length())sayRight(128,63,fitted(clipped(hint,20),128-x,small));
}
#if defined(MM_JOYSTICK)
String hint(){Act acts[MenuMax];unsigned n=actions(acts);if(!n)return "<  >";return n==1?t("OK: ","OK: ")+actName(acts[0]):t("OK: menu  < >","OK: меню  < >");}
#else
String hint(){
#if MM_DICE
 if(diceTapOn())return t("click+ hold- 5s:exit","клик+ держ- 5с:выход");
#endif
 Act acts[MenuMax];unsigned n=actions(acts);if(!n)return "";return n==1?t("hold: ","держ: ")+actName(acts[0]):t("hold: menu","держ: меню");}
#endif
void drawMenu(){
 auto& c=*hardware.canvas;Act acts[MenuMax];unsigned n=actions(acts);if(!n){menuOpen=false;return;}menuIndex%=n;int first=max(0,min(menuIndex-1,int(n)-3));
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
 say(18,11,t("Chess","Шахматы"),bold);textLines(chessPopupText,25,3);
#if defined(MM_JOYSTICK)
 if(config.role==RoleNormal)sayRight(126,62,t("OK: open the game","OK: открыть партию"),small);
#else
 if(config.role==RoleNormal)sayRight(126,62,t("hold: open the game","держ: открыть партию"),small);
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
// Radio settings: three rows; the value being changed (or the frequency digit) drawn inverted.
void drawRadioEdit(){
 auto& c=*hardware.canvas;c.fillScreen(0);int first=max(0,min(radioRow-1,RadioRows-3));
 for(int r=first;r<first+3;r++){int y=21+(r-first)*12;bool focus=r==radioRow;if(focus)c.fillRect(0,y-10,128,12,1);say(3,y,radioName(r),small,!focus);
  if(r>=RowSave)continue;String v=radioValue(radioDraft,r);int x=125-width(v,small);sayRight(125,y,v,small,!focus);
  if(radioValue(config,r)!=v)c.fillRect(x-4,y-5,2,2,!focus); // changed, not saved yet
  if(focus&&radioEditing){unsigned a=0,b=v.length();if(r==RowFreq)radioDigitSpan(v,a,b);int u=x+width(v.substring(0,a),small);String part=v.substring(a,b);
   c.fillRect(u-1,y-9,width(part,small)+2,11,0);say(u,y,part,small,1);}}
 header(t("Radio","Радио")+(radioChanged()?" *":""));sayRight(128,63,radioHint(),small);
 if(action.length()&&millis()-actionAt<3500){c.fillRect(0,36,128,20,0);c.drawRect(0,36,128,20,1);say(3,50,clipped(action,20));}
 hardware.flush();
}
// Power off pending (Power.cpp): what turns the board on again.
String powerOnHint(){int way=powerOnWay();
 if(way==-2)return t("the power key","кнопка питания");if(way<0)return t("RESET or power switch","RESET или выключатель");
#if defined(MM_JOYSTICK)
 return t("hold the joystick","удержать джойстик");
#else
 return t("hold ","удержать ")+MM_BUTTON;
#endif
}
bool farewell=false; // the board is about to turn off: the last frame
void drawOff(){
 auto& c=*hardware.canvas;c.fillScreen(0);say(0,24,farewell?t("Device is off","Устройство выключено"):t("Turning off...","Выключение..."),bold);
 say(0,44,t("To turn on:","Включить:"),small);say(0,54,powerOnHint(),small);hardware.flush();
}
// Server home: role, radio, traffic and the passwords an owner needs for the MeshCore app.
void drawServerHome(){
 ServerView v=meshServer.view();bool room=config.role==RoleRoom;
 say(0,21,roleShort(config.role),bold);if(meshRadio.ready)sayRight(128,20,String(config.frequency,3)+t(" MHz"," МГц"));else sayRight(128,20,t("Radio error","Ошибка радио"));
 say(0,31,room?t("posts ","постов ")+String(v.posts)+t("  in ","  вошли ")+String(v.clients):"RX "+String(meshRadio.rxCount)+" TX "+String(meshRadio.txCount)+t(" fwd "," перес. ")+String(meshRadio.relayed),small);
 say(0,40,t("admin ","админ ")+v.password,small);
 say(0,49,(room?t("room ","комната "):t("guest ","гость "))+(v.guest.length()?v.guest:t("none","нет"))+(v.forwarding?"":t("  no fwd","  без перес.")),small);
}
#if defined(MM_HIRES)
#include "UiHires.inc"
#endif
void draw(){
#if defined(MM_HIRES)
 hi::draw();return; // T114: its own drawing at 240x135
#endif
 auto& c=*hardware.canvas;if(powerOffPending()){drawOff();return;}if(rolePick){drawRolePick();return;}if(radioEdit){drawRadioEdit();return;}
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
   String login=remoteLogin(*p),result=remoteResult(*p);
   if(login.length()||result.length()){say(0,42,login.length()?login:String(int(p->rssi))+" dBm SNR "+String(p->snr,1),small);say(0,51,result,small);break;}
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
 case Ble:title="Bluetooth";if(bleActive()){String n=bleName();say(0,23,n,width(n,bold)<=128?bold:small);say(0,37,"PIN "+String(blePin()),bold);}else{say(0,27,t("Bluetooth off","Bluetooth выкл."));say(0,41,t("Secure pairing","Защищённое сопряжение"),small);}break;
 case Settings:title=t("Settings","Настройки");say(0,20,t("Language: ","Язык: ")+langNames[config.lang<LangCount?config.lang:0],small);say(0,28,t("Battery: ","Батарея: ")+(config.batteryVolts?t("volts","вольты"):t("percent","проценты")),small);
  say(0,36,t("Screen off: ","Гасить экран: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда")),small);say(0,44,t("Contrast: ","Контраст: ")+String(config.brightness),small);
  say(0,52,String(config.frequency,3)+" SF"+String(config.sf)+" CR4/"+String(config.cr)+" "+String(config.power)+"dBm",small);break;
#if MM_CHESS
 case Chess:if(chessOpen&&chessOpen->state!=ChessMatch::Free){drawChessBoard(*chessOpen);return;}chessOpen=nullptr;drawChessList(title);break;
#endif
#if MM_PET
 case PetPage:title=petTitle();drawPetMono();break;
#endif
#if MM_DICE
 case DicePage:title=diceTitle();drawDiceMono();break;
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
#if MM_DICE
 else if(page==DicePage)diceScroll(step);
#endif
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
 if(chessPopupAt){chessPopupAt=0;if(ok&&config.role==RoleNormal){
#if MM_CHESS
 ChessMatch* m=chessNet.eventMatch;showPage(Chess);openChess(m&&m->state!=ChessMatch::Free?m:nullptr);
#endif
 }return;}
 if(menuOpen){menuAt=millis();Act acts[MenuMax];unsigned n=actions(acts);if(!n){menuOpen=false;return;}
  if(up||down){menuIndex=(menuIndex+(up?n-1:1))%n;return;}
  if(ok){Act a=acts[menuIndex%n];run(a);if(!keepsMenu(a))menuOpen=false;return;}
  menuOpen=false;return;}
#if MM_CHESS
 if(page==Chess&&chessKey(key))return;
#endif
 if(left||right){showPage(left?previousPage(page):nextPage(page));return;}
 if(back){if(page!=Home)showPage(Home);return;}
 if(up||down){scroll(up?-1:1);return;}
 if(ok){Act acts[MenuMax];unsigned n=actions(acts);if(n==1)run(acts[0]);else if(n>1){menuOpen=true;menuIndex=0;menuAt=millis();}}
}
#endif
void uiKey(int key){
 if(powerOffPending())return; // the screen says it is turning off
 lastInput=millis();dirty=true;
 if(screenOff){screenOff=false;hardware.brightness(config.brightness); // the first press only wakes the panel
#if MM_DICE && !defined(MM_JOYSTICK)
  if(!diceTapOn()) // ... but counts while counting with the button
#endif
  return;}
#if defined(MM_JOYSTICK)
 if(radioEdit){radioJoystick(key==0xb5,key==0xb6,key==0xb4,key==0xb7,key==13||key==0xa3,key==0x86||key==0x82);return;}
 joystickKey(key);return;
#endif
 if(radioEdit){radioButton(key==13||key==0x82,key==0xa3);return;}
 if(rolePick){if(key==13||key==0x82){roleSel=(roleSel+1)%RoleCount;rolePickBoot=false;return;}
  if(key==0xa3){if(roleSel==config.role){rolePick=false;return;}String r=setRole(roleSel);notice(r.startsWith("OK")?t("Restarting: ","Перезапуск: ")+roleShort(roleSel):r);if(!r.startsWith("OK"))rolePick=false;}return;}
 if(popupAt){popupAt=0;if(key==13)return;}
 if(chessPopupAt){chessPopupAt=0;if(key==0xa3&&config.role==RoleNormal){
#if MM_CHESS
 ChessMatch* m=chessNet.eventMatch;showPage(Chess);openChess(m&&m->state!=ChessMatch::Free?m:nullptr);
#endif
 }return;}
 if(menuOpen){menuAt=millis();Act acts[MenuMax];unsigned n=actions(acts);if(!n){menuOpen=false;return;}
  if(key==13||key==0x82){menuIndex=(menuIndex+1)%n;return;}
  if(key==0xa3){Act a=acts[menuIndex%n];run(a);if(!keepsMenu(a))menuOpen=false;}return;}
#if !defined(MM_JOYSTICK)
#if MM_CHESS
 if(page==Chess&&chessButton(key))return;
#endif
#if MM_DICE
 if(diceTapKey(key))return;
#endif
#endif
 if(key==13||key==0x82){showPage(nextPage(page));return;}
 if(key==0xa3){Act acts[MenuMax];unsigned n=actions(acts);if(n==1)run(acts[0]);else if(n>1){menuOpen=true;menuIndex=0;menuAt=millis();}}
}
bool uiRadarPage(){return page==Signals;}
void uiBegin(){openRolePick(true);chessSeen=chessNet.events;if(pins::led>=0)pinMode(pins::led,OUTPUT);led(false);lastInput=millis();if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];newest={m.source,m.session,m.id};}draw();}
String uiStatus(){StaticJsonDocument<640>d;d["action"]=millis()-actionAt<3500?action:String();d["page"]=rolePick?"role":pageNames[page];d["role"]=roleName(config.role);if(rolePick){d["role_selected"]=roleSel;d["boot_pick"]=rolePickBoot;}if(radioEdit){d["page"]="radio";d["radio_row"]=radioRow;d["radio_editing"]=radioEditing;d["radio_digit"]=radioDigit;d["radio_draft"]=radioValue(radioDraft,radioRow);}d["locked"]=false;d["menu"]=menuOpen;d["menu_index"]=menuIndex;d["screen_off"]=screenOff;d["popup"]=popupAt!=0;d["chess_popup"]=chessPopupAt!=0;d["unread"]=unreadCount;
#if MM_CHESS
 if(page==Chess&&chessOpen){char id[5];snprintf(id,sizeof id,"%04X",chessOpen->id);d["chess_game"]=id;d["chess_cursor"]=chessCursor;d["chess_held"]=chessHeld;d["chess_menu"]=chessMenu;d["chess_menu_index"]=chessMenuIndex;
  if(chessMenu){ChessAct acts[8];unsigned k=chessActions(*chessOpen,acts);d["chess_act"]=int(acts[chessMenuIndex%k]);}}
#endif
#if defined(MM_JOYSTICK)
 if(composing){d["compose"]=true;d["draft"]=draft;d["keyboard"]=layoutNames[kbLayout];d["key_row"]=kbRow;d["key_col"]=kbCol;}
#endif
if(page==Signals){d["radar_selected"]=shownSignal();d["csi_role"]=radar.csi;}
#if MM_DICE
 if(page==DicePage){d["dice_counter"]=diceCounterSel;d["dice_saved"]=diceSavedSel;d["dice_tapping"]=diceTapOn();}
#endif
 String s;serializeJson(d,s);return s;}
bool uiScreenOff(){return screenOff;}
void uiFarewell(){farewell=true;screenOff=false;hardware.brightness(config.brightness);draw();}
void uiTick(){
 uint32_t now=millis();
 if(powerOffPending()){static bool shown=false;if(!shown){shown=true;screenOff=false;hardware.brightness(config.brightness);draw();}return;} // drawn once: the board turns off
 // New incoming message: popup, wake the panel and blink the LED three times.
 if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];if(m.source!=newest.source||m.session!=newest.session||m.id!=newest.id){newest={m.source,m.session,m.id};if(!m.outgoing){if(page!=Messages)unreadCount++;popupAt=now;ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}}
 if(tour::net.events!=tourSeen){tourSeen=tour::net.events;if(tour::net.event.length()){chessPopupText=tour::net.event;chessPopupAt=now;ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}
 if(chessNet.events!=chessSeen){chessSeen=chessNet.events;if(chessNet.event.length()){
  // News of the game on the screen updates the board instead of covering it.
  
#if MM_CHESS
  if(page==Chess&&chessOpen&&chessNet.eventMatch==chessOpen&&!menuOpen)chessNet.viewed(*chessOpen);else
#endif
  {chessPopupText=chessNet.event;chessPopupAt=now;}ledAt=now;menuOpen=false;if(screenOff){screenOff=false;hardware.brightness(config.brightness);}lastInput=now;dirty=true;}}
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
 if(composing)dirty=true; // blinking cursor
 #endif
 if(page==PetPage)dirty=true; // it moves
#if MM_DICE
 if(page==DicePage&&dicer.events!=diceSeen){diceSeen=dicer.events;dirty=true;} // rolls from the web page or the app
#endif
#if MM_CHESS
 if(page==Chess&&chessOpen&&(chessHeld>=0||(chessOpen->state==ChessMatch::Playing&&chessOpen->myTurn())))dirty=true; // the picked piece or the cursor blinks
#endif
 if(chessNet.dirty&&page==Chess){dirty=true;chessNet.dirty=false;}
 if((dirty||meshRadio.dirty||radar.dirty||now-drawAt>1000)&&now-drawAt>150){draw();drawAt=now;dirty=false;meshRadio.dirty=false;radar.dirty=false;}
}
