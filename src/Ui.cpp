#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "Solitaire.h"
#include <Preferences.h>
#include <Mm1Packet.h>
#include <time.h>
#include <math.h>
#include <initializer_list>
// Defined in U8g2_for_Adafruit_GFX.cpp; used to fall back to Latin-1 and placeholders for missing glyphs.
uint8_t u8g2_IsGlyph(u8g2_font_t* u8g2,uint16_t encoding);
int8_t u8g2_GetGlyphWidth(u8g2_font_t* u8g2,uint16_t encoding);
namespace {
enum Page {Home,Threads,Chat,Map,Nodes,Sensors,Settings,Radio,Display,Network,Diagnostics,Help,Library,Node,Scope,Homing,Motion,Game};
const char* pageNames[]={"home","threads","chat","map","nodes","sensors","settings","radio","display","network","diagnostics","help","library","node","radar","homing","motion","solitaire"};
enum Key {Enter=13,Erase=8,KeyMsg=0x81,KeyHome=0x82,KeyAt=0x83,KeyAdv=0x84,KeyMap=0x85,KeyBack=0x86,KeyGps=0x87,KeyMic=0x88,KeySet=0x90,KeyHold=0xa3,KeyLeft=0xb4,KeyUp=0xb5,KeyDown=0xb6,KeyRight=0xb7};
Page page=Home,chatReturn=Threads;
bool scopeManual=false,csiBeaconRole=false; // radar: selection moved by the user; CSI page role
 // radar list: the user moved the selection off the strongest signal
uint32_t lastDraw=0,lastInput=0,toastAt=0;String toast,eventSeen;uint16_t toastColor=0;
int selected=0,chatOffset=0,action=0;
uint64_t recipient=meshmesh::Broadcast,focusNode=0;
String composer,edit;bool editing=false,dirty=true,locked=false,wakeOnly=false,keyboardRussian=true,deleteArmed=false,layoutHelp=false;uint32_t lastSpace=0;
Config draft;
struct ComposerDraft{uint64_t recipient=0;String text;uint32_t touched=0;} drafts[16];
struct ReadMark{uint64_t id;uint32_t at;} marks[65];unsigned markCount=0;
DynamicJsonDocument library(8192);
uint64_t conversations[65];unsigned conversationCount=1;
unsigned nodeOrder[24],nodeTotal=0;

// Palette: neutral dark surfaces, colour reserved for focus and state.
constexpr uint16_t rgb(uint32_t v){return ((v>>8)&0xf800)|((v>>5)&0x07e0)|((v>>3)&0x1f);}
const uint16_t bg=rgb(0x080c11),bar=rgb(0x000000),card=rgb(0x131a22),cardHi=rgb(0x1c2835),line=rgb(0x2a3542),ink=rgb(0xe4e9ee),dim=rgb(0x8d99a6),faint=rgb(0x56626e);
const uint16_t accent=rgb(0x1fc2ae),ok=rgb(0x4cc26b),warn=rgb(0xe9b13b),bad=rgb(0xe5564d),info=rgb(0x4f9df7),violet=rgb(0xa78bfa),outBubble=rgb(0x103a35),inBubble=rgb(0x1a232d);
const uint16_t avatarHues[]={rgb(0x2f7d6f),rgb(0x3f6fb5),rgb(0x8a5cc2),rgb(0xb5693f),rgb(0x4f8a3a),rgb(0xa8466a),rgb(0x3a8aa0),rgb(0x8f7a2e)};

// Text: the Cyrillic fonts lack Latin-1 and punctuation such as "·", "°", "№".
struct Face{const uint8_t* main;const uint8_t* latin;};
const Face body{u8g2_font_6x13_t_cyrillic,u8g2_font_6x13_tf},bold{u8g2_font_6x13B_t_cyrillic,u8g2_font_6x13B_tf},small{u8g2_font_5x8_t_cyrillic,u8g2_font_5x8_tf};
const Face big{u8g2_font_10x20_t_cyrillic,u8g2_font_10x20_tf},digits{u8g2_font_logisoso42_tn,nullptr};
const uint8_t* activeFont=nullptr;
void useFont(const uint8_t* f){if(f!=activeFont){hardware.font.setFont(f);hardware.font.setFontMode(1);activeFont=f;}} // SetFont resets transparency
uint32_t nextCp(const String& s,unsigned& i){uint8_t c=s[i];unsigned n=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;uint32_t cp=n==1?c:n==2?c&0x1f:n==3?c&0x0f:c&0x07;for(unsigned k=1;k<n&&i+k<s.length();k++)cp=cp<<6|(uint8_t(s[i+k])&0x3f);i+=n;return cp;}
const char* substitute(uint32_t cp){switch(cp){case 0x2026:return "...";case 0x2013:case 0x2014:case 0x2212:return "-";case 0x2116:return "No";case 0x2018:case 0x2019:return "'";case 0x201c:case 0x201d:case 0x201e:return "\"";case 0x2192:return "->";case 0x2190:return "<-";case 0x2022:return "\xc2\xb7";}return nullptr;}
int glyph(int x,int y,uint32_t cp,const Face& f,uint16_t color,bool paint){
 auto& u=hardware.font;const uint8_t* use=nullptr;
 if(cp<=0xffff){useFont(f.main);if(u8g2_IsGlyph(&u.u8g2,cp))use=f.main;else if(f.latin){useFont(f.latin);if(u8g2_IsGlyph(&u.u8g2,cp))use=f.latin;}}
 if(!use){
  if(const char* sub=substitute(cp)){String s(sub);int w=0;for(unsigned i=0;i<s.length();)w+=glyph(x+w,y,nextCp(s,i),f,color,paint);return w;}
  useFont(f.main);int w=u8g2_GetGlyphWidth(&u.u8g2,'0'),a=u.getFontAscent();if(paint)hardware.canvas->drawRect(x+1,y-a,max(2,w-2),a,color);return w; // visible placeholder, e.g. emoji
 }
 useFont(use);if(!paint)return u8g2_GetGlyphWidth(&u.u8g2,cp);u.setForegroundColor(color);return u.drawGlyph(x,y,cp);
}
int measure(const String& s,const Face& f=body){int w=0;for(unsigned i=0;i<s.length();)w+=glyph(0,0,nextCp(s,i),f,0,false);return w;}
int text(int x,int y,const String& s,uint16_t color=ink,const Face& f=body){for(unsigned i=0;i<s.length();)x+=glyph(x,y,nextCp(s,i),f,color,true);return x;}
void textRight(int x,int y,const String& s,uint16_t color=ink,const Face& f=body){text(x-measure(s,f),y,s,color,f);}
void textCenter(int x,int y,const String& s,uint16_t color=ink,const Face& f=body){text(x-measure(s,f)/2,y,s,color,f);}
String fit(const String& s,int px,const Face& f=body){
 if(measure(s,f)<=px)return s;int dots=measure("...",f),w=0;unsigned i=0,end=0;
 while(i<s.length()){int g=glyph(0,0,nextCp(s,i),f,0,false);if(w+g+dots>px)break;w+=g;end=i;}return s.substring(0,end)+"...";
}
// Word wrap by pixel width; long words break anywhere.
unsigned wrap(const String& s,String* rows,unsigned maxRows,int px,const Face& f=body){
 unsigned count=0,i=0;String row;int rowW=0;auto push=[&]{if(count<maxRows)rows[count++]=row;row="";rowW=0;};
 while(i<s.length()&&count<maxRows){
  if(s[i]=='\n'){push();i++;continue;}
  unsigned start=i;int w=0;while(i<s.length()&&s[i]!='\n'){uint32_t cp=nextCp(s,i);w+=glyph(0,0,cp,f,0,false);if(cp==' ')break;}
  String word=s.substring(start,i);
  if(rowW+w<=px){row+=word;rowW+=w;continue;}
  if(rowW){push();if(count>=maxRows)break;}
  if(w<=px){row=word;rowW=w;continue;}
  for(unsigned j=0;j<word.length();){unsigned at=j;int g=glyph(0,0,nextCp(word,j),f,0,false);if(rowW+g>px){push();if(count>=maxRows)break;}row+=word.substring(at,j);rowW+=g;}
 }
 if(row.length()&&count<maxRows)rows[count++]=row;return count;
}
String t(const char* en,const char* ru){return config.russian?ru:en;}
String flag(bool v){return v?t("On","Вкл"):t("Off","Выкл");}
String count(unsigned n,const char* one,const char* many,const char* ru1,const char* ru2,const char* ru5){if(!config.russian)return String(n)+" "+(n==1?one:many);unsigned m=n%100,d=n%10;return String(n)+" "+(m>=11&&m<=14?ru5:d==1?ru1:d>=2&&d<=4?ru2:ru5);}
void notice(const String& value,uint16_t color=accent){toast=value;toastAt=millis();toastColor=color;dirty=true;}

// Drawing primitives and icons (drawn, not font glyphs, so they scale and take state colours).
GFXcanvas16& g(){return *hardware.canvas;}
void panel(int x,int y,int w,int h,uint16_t fill,int r=6){g().fillRoundRect(x,y,w,h,r,fill);}
void ring(int x,int y,int w,int h,int r=6,uint16_t color=accent){g().drawRoundRect(x,y,w,h,r,color);g().drawRoundRect(x+1,y+1,w-2,h-2,max(r-1,1),color);}
void tri(int x,int y,int dir,int s,uint16_t c){if(dir==0)g().fillTriangle(x-s,y+s/2,x+s,y+s/2,x,y-s/2-1,c);else if(dir==2)g().fillTriangle(x-s,y-s/2,x+s,y-s/2,x,y+s/2+1,c);else if(dir==1)g().fillTriangle(x-s/2,y-s,x-s/2,y+s,x+s/2+1,y,c);else g().fillTriangle(x+s/2,y-s,x+s/2,y+s,x-s/2-1,y,c);}
void arc(int cx,int cy,int r,int a0,int a1,uint16_t c){for(int a=a0;a<=a1;a+=3){float rad=a*M_PI/180;g().drawPixel(cx+roundf(r*sinf(rad)),cy-roundf(r*cosf(rad)),c);}}
void thickLine(int x0,int y0,int x1,int y1,uint16_t c){g().drawLine(x0,y0,x1,y1,c);g().drawLine(x0+1,y0,x1+1,y1,c);}
enum Icon {IcChat,IcPin,IcMesh,IcCompass,IcWifi,IcBle,IcGear,IcTower,IcRoom,IcSensor,IcPerson,IcLock,IcMail,IcHash,IcPulse,IcHelp,IcRadio,IcScreen,IcKey,IcRadar,IcCards};
void icon(Icon id,int cx,int cy,int s,uint16_t c,uint16_t hole=bg){
 auto& d=g();int h=s/2,q=max(1,s/4),w=max(1,s/8);
 switch(id){
 case IcChat:d.fillRoundRect(cx-s,cy-s*3/4,2*s,s*3/2,max(2,s/3),c);d.fillTriangle(cx-h,cy+s*3/4-1,cx,cy+s*3/4-1,cx-s*3/4,cy+s,c);for(int k=-1;k<=1;k++)d.fillCircle(cx+k*h,cy,w,hole);break;
 case IcPin:d.fillCircle(cx,cy-q,s*5/8,c);d.fillTriangle(cx-s*9/16,cy,cx+s*9/16,cy,cx,cy+s,c);d.fillCircle(cx,cy-q,max(1,s/4),hole);break;
 case IcMesh:{int ax=cx,ay=cy-s*3/4,bx=cx-s*3/4,by=cy+s*5/8,ex=cx+s*3/4;thickLine(ax,ay,bx,by,c);thickLine(ax,ay,ex,by,c);thickLine(bx,by,ex,by,c);for(int p:{0,1,2})d.fillCircle(p==0?ax:p==1?bx:ex,p==0?ay:by,max(2,s/4),c);break;}
 case IcCompass:d.drawCircle(cx,cy,s,c);if(s>7)d.drawCircle(cx,cy,s-1,c);d.fillTriangle(cx,cy-s*3/4,cx-max(2,s/4),cy,cx+max(2,s/4),cy,c);d.fillTriangle(cx,cy+s*3/4,cx-max(2,s/4),cy,cx+max(2,s/4),cy,faint);break;
 case IcWifi:{int oy=cy+s*5/8;for(int r:{s,s*2/3,s/3}){arc(cx,oy,r,-45,45,c);arc(cx,oy,r-1,-45,45,c);}d.fillCircle(cx,oy,max(1,s/6),c);break;}
 case IcBle:d.drawLine(cx,cy-s,cx,cy+s,c);d.drawLine(cx,cy-s,cx+h,cy-h,c);d.drawLine(cx+h,cy-h,cx-h,cy+h,c);d.drawLine(cx,cy+s,cx+h,cy+h,c);d.drawLine(cx+h,cy+h,cx-h,cy-h,c);if(s>6){d.drawLine(cx+1,cy-s,cx+1,cy+s,c);}break;
 case IcGear:d.fillCircle(cx,cy,s*5/8,c);for(int k=0;k<8;k++){float a=k*M_PI/4;d.fillCircle(cx+roundf(s*.8f*cosf(a)),cy+roundf(s*.8f*sinf(a)),max(1,s/4),c);}d.fillCircle(cx,cy,max(1,s/4),hole);break;
 case IcTower:d.drawLine(cx,cy-q,cx-h,cy+s,c);d.drawLine(cx,cy-q,cx+h,cy+s,c);d.drawLine(cx,cy-q,cx,cy+s,c);d.fillCircle(cx,cy-h,max(1,s/5),c);arc(cx,cy-h,h+1,-75,-30,c);arc(cx,cy-h,h+1,30,75,c);arc(cx,cy-h,s-1,-75,-35,c);arc(cx,cy-h,s-1,35,75,c);break;
 case IcRoom:d.fillTriangle(cx-s,cy,cx+s,cy,cx,cy-s,c);d.fillRect(cx-s*3/4,cy,s*3/2,s,c);d.fillRect(cx-max(1,s/5),cy+h-1,max(2,s*2/5),s-h+1,hole);break;
 case IcSensor:d.fillRoundRect(cx-max(1,s/5),cy-s,max(3,s*2/5),s*3/2,max(1,s/5),c);d.fillCircle(cx,cy+h+q/2,max(2,s*2/5),c);break;
 case IcPerson:d.fillCircle(cx,cy-h+1,max(2,s*3/8),c);d.fillRoundRect(cx-s*5/8,cy+1,s*5/4,s,max(2,s/2),c);break;
 case IcLock:arc(cx,cy-q,max(2,s*3/8),-90,90,c);arc(cx,cy-q,max(2,s*3/8)-1,-90,90,c);d.drawFastVLine(cx-max(2,s*3/8),cy-q,q+1,c);d.drawFastVLine(cx+max(2,s*3/8),cy-q,q+1,c);d.fillRoundRect(cx-s*5/8,cy,s*5/4,s*7/8,max(1,s/5),c);break;
 case IcMail:d.drawRect(cx-s,cy-s*5/8,2*s,s*5/4,c);d.drawLine(cx-s,cy-s*5/8,cx,cy+q/2,c);d.drawLine(cx+s-1,cy-s*5/8,cx,cy+q/2,c);break;
 case IcHash:thickLine(cx-q,cy-s,cx-h,cy+s,c);thickLine(cx+h,cy-s,cx+q,cy+s,c);d.fillRect(cx-s,cy-q-1,2*s,2,c);d.fillRect(cx-s,cy+q-1,2*s,2,c);break;
 case IcPulse:{int p[][2]={{-s,0},{-h,0},{-q,-s*3/4},{q,s*3/4},{h,0},{s,0}};for(int k=0;k<5;k++)thickLine(cx+p[k][0],cy+p[k][1],cx+p[k+1][0],cy+p[k+1][1],c);break;}
 case IcHelp:d.drawCircle(cx,cy,s,c);textCenter(cx,cy+5,"?",c,bold);break;
 case IcRadio:d.drawFastVLine(cx,cy-h,s+h,c);d.fillCircle(cx,cy-h,max(1,s/5),c);arc(cx,cy-h,h+1,-120,-60,c);arc(cx,cy-h,h+1,60,120,c);arc(cx,cy-h,s,-125,-55,c);arc(cx,cy-h,s,55,125,c);break;
 case IcRadar:d.drawCircle(cx,cy,s,c);d.drawCircle(cx,cy,max(2,s/2),c);thickLine(cx,cy,cx+roundf(s*.7f)-1,cy-roundf(s*.7f)+1,c);d.fillCircle(cx,cy,max(1,s/6),c);d.fillCircle(cx-h,cy+q+1,max(1,s/6),c);break;
 case IcCards:{d.fillRoundRect(cx-s,cy-s,s*9/8,s*3/2,2,c);int x0=cx-s/4,y0=cy-s/2,w=s*5/4,h=s*3/2,mx=x0+w/2,my=y0+h/2;d.fillRoundRect(x0-2,y0-2,w+4,h+4,3,hole);d.fillRoundRect(x0,y0,w,h,2,c);d.fillTriangle(mx,my-q-2,mx-q-1,my,mx+q+1,my,hole);d.fillTriangle(mx,my+q+2,mx-q-1,my,mx+q+1,my,hole);break;}
 case IcScreen:d.drawRoundRect(cx-s,cy-s*5/8,2*s,s*5/4,2,c);d.fillRect(cx-s+3,cy-s*5/8+3,2*s-6,s*5/4-6,c);break;
 case IcKey:d.drawCircle(cx-h,cy,max(2,s*3/8),c);d.drawFastHLine(cx-h+s*3/8,cy,s+1,c);d.drawFastVLine(cx+h,cy,max(2,s/3),c);d.drawFastVLine(cx+s-1,cy,max(2,s/3),c);break;
 }
}
void bars(int x,int y,unsigned level,uint16_t color){for(unsigned k=0;k<4;k++){int h=3+k*2;g().fillRect(x+k*4,y-h,3,h,k<level?color:line);}}
unsigned snrLevel(float snr){return snr>=5?4:snr>=0?3:snr>=-5?2:snr>=-10?1:0;}
// Avatars: initial of the name, colour from the ID.
String initial(const String& name){if(!name.length())return "?";unsigned i=0;uint32_t cp=nextCp(name,i);if(cp>=0x430&&cp<=0x44f)cp-=0x20;else if(cp==0x451)cp=0x401;else if(cp<128)cp=toupper(cp);else if(cp>=0x800)return "?";char b[4]={};if(cp<0x80)b[0]=cp;else{b[0]=0xc0|cp>>6;b[1]=0x80|(cp&0x3f);}return b;}
void avatar(int cx,int cy,int r,uint64_t id,const String& name,int type=-1){
 uint16_t hue=avatarHues[(id^id>>17^id>>41)%8];
 if(id==meshmesh::Broadcast){g().fillCircle(cx,cy,r,rgb(0x14524b));icon(IcHash,cx,cy,r/2,accent);return;}
 if(type>1){uint16_t tc=type==2?violet:type==3?warn:info;g().fillCircle(cx,cy,r,card);g().drawCircle(cx,cy,r,tc);icon(type==2?IcTower:type==3?IcRoom:IcSensor,cx,cy,r/2+1,tc,card);return;}
 g().fillCircle(cx,cy,r,hue);textCenter(cx,cy+(r>12?5:4),initial(name),ink,r>12?bold:small);
}
// Softkey footer: [KEY] action pairs; "<>" and "^v" draw arrow pairs.
struct Hint{const char* key;String action;};
void footer(std::initializer_list<Hint> hints){
 auto& d=g();d.fillRect(0,221,320,19,bar);d.drawFastHLine(0,220,320,line);int x=6;
 for(auto& h:hints){
  bool arrows=!strcmp(h.key,"<>")||!strcmp(h.key,"^v");int kw=arrows?17:measure(h.key,small)+8,aw=measure(h.action,body);
  if(x+kw+4+aw>316)break;
  d.fillRoundRect(x,224,kw,13,3,line);
  if(!strcmp(h.key,"<>")){tri(x+5,230,3,3,ink);tri(x+12,230,1,3,ink);}else if(!strcmp(h.key,"^v")){tri(x+5,231,0,3,ink);tri(x+12,229,2,3,ink);}else text(x+4,234,h.key,ink,small);
  text(x+kw+4,235,h.action,dim);x+=kw+4+aw+10;
 }
}
// Conversations, read marks and drafts.
bool belongs(const ChatMessage& m,uint64_t id){return id==meshmesh::Broadcast?m.destination==id:(m.outgoing?m.destination==id:m.source==id&&(m.destination==meshRadio.nodeId||m.protocol==1));}
void threads(){conversationCount=1;conversations[0]=meshmesh::Broadcast;for(int i=int(meshRadio.historyCount)-1;i>=0;i--){auto& m=meshRadio.history[i];uint64_t id=m.outgoing?m.destination:m.source;if(m.destination==meshmesh::Broadcast)id=meshmesh::Broadcast;if(id==meshmesh::Broadcast)continue;bool found=false;for(unsigned j=1;j<conversationCount;j++)if(conversations[j]==id)found=true;if(!found&&conversationCount<65)conversations[conversationCount++]=id;}}
Peer* peerOf(uint64_t id){for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==id)return &meshRadio.peers[i];return nullptr;}
String nameOf(uint64_t id){if(id==meshmesh::Broadcast)return "Public";if(Peer* p=peerOf(id))return p->name;for(int i=meshRadio.historyCount-1;i>=0;i--)if(meshRadio.history[i].source==id)return meshRadio.history[i].name;return meshRadio.idText(id);}
String readKey(uint64_t id){if(id==meshmesh::Broadcast)return "all";char b[15]={};unsigned n=0;do{b[n++]="0123456789abcdefghijklmnopqrstuvwxyz"[id%36];id/=36;}while(id);String key="r";while(n)key+=b[--n];return key;}
uint32_t readAt(uint64_t id){
 for(unsigned i=0;i<markCount;i++)if(marks[i].id==id)return marks[i].at;
 uint32_t v=0;Preferences p;if(p.begin("meshmesh-ui",true)){v=p.getUInt(readKey(id).c_str(),0);p.end();}
 if(markCount<65)marks[markCount++]={id,v};return v; // cached: status bar needs unread totals every frame
}
unsigned unread(uint64_t id){uint32_t at=readAt(id);unsigned n=0;for(unsigned i=0;i<meshRadio.historyCount;i++){auto& m=meshRadio.history[i];if(belongs(m,id)&&!m.outgoing&&m.timestamp>at)n++;}return n;}
unsigned unreadTotal(){threads();unsigned n=0;for(unsigned i=0;i<conversationCount;i++)n+=unread(conversations[i]);return n;}
void markRead(){
 uint32_t at=0;for(unsigned i=0;i<meshRadio.historyCount;i++){auto& m=meshRadio.history[i];if(belongs(m,recipient)&&!m.outgoing)at=max(at,m.timestamp);}
 if(at<=readAt(recipient))return;Preferences p;if(p.begin("meshmesh-ui",false)){p.putUInt(readKey(recipient).c_str(),at);p.end();}
 for(unsigned i=0;i<markCount;i++)if(marks[i].id==recipient)marks[i].at=at;
}
void rememberComposer(){int chosen=-1;for(int i=0;i<16;i++)if(drafts[i].recipient==recipient){chosen=i;break;}if(chosen<0)for(int i=0;i<16;i++)if(!drafts[i].recipient){chosen=i;break;}if(chosen<0){chosen=0;for(int i=1;i<16;i++)if(drafts[i].touched<drafts[chosen].touched)chosen=i;}drafts[chosen].recipient=recipient;drafts[chosen].text=composer;drafts[chosen].touched=millis();}
String restoredComposer(){for(auto& d:drafts)if(d.recipient==recipient)return d.text;return "";}
// Nodes: most recently heard first; focus follows the node, not its row.
void sortNodes(){
 nodeTotal=meshRadio.peerCount;for(unsigned i=0;i<nodeTotal;i++)nodeOrder[i]=i;uint32_t now=millis();
 auto before=[&](const Peer& a,const Peer& b){if(a.heard!=b.heard)return a.heard;return a.heard&&now-a.seen<now-b.seen;};
 for(unsigned i=1;i<nodeTotal;i++)for(unsigned j=i;j>0&&before(meshRadio.peers[nodeOrder[j]],meshRadio.peers[nodeOrder[j-1]]);j--)std::swap(nodeOrder[j],nodeOrder[j-1]);
 if(!nodeTotal){selected=0;return;}
 for(unsigned i=0;i<nodeTotal;i++)if(meshRadio.peers[nodeOrder[i]].id==focusNode){selected=i;return;}
 selected=constrain(selected,0,int(nodeTotal)-1);focusNode=meshRadio.peers[nodeOrder[selected]].id;
}
Peer* focusedPeer(){return peerOf(focusNode);}
void gameOpen();void gameLeave();String gameTitle();String gameTileDetail();
// Leaving the radar pages keeps the radar running while the web page holds it (webRadarActive).
void change(Page next){if(page==Game&&next!=Game)gameLeave();if(next==Game&&page!=Game)gameOpen();if(page==Chat&&next!=Chat)rememberComposer();bool radarPage=next==Scope||next==Homing||next==Motion;if(radarPage&&page!=Scope&&page!=Homing&&page!=Motion)scopeManual=false;if(radarPage)radar.open();else if(!webRadarActive())radar.close();if(radarPage||!webRadarActive())radar.setCsi(next!=Motion?Radar::CsiOff:csiBeaconRole?Radar::CsiBeacon:Radar::CsiSensor);if(next==Scope)radar.untrack();page=next;selected=0;chatOffset=0;action=0;editing=false;deleteArmed=false;dirty=true;if(next==Radio||next==Display)draft=config;if(next==Threads)threads();if(next==Chat){composer=restoredComposer();markRead();}if(next==Library)deserializeJson(library,maps.areas());if(next==Nodes)sortNodes();}

// Time, distances and short labels.
bool localTime(time_t at,tm& out){if(at<1700000000)return false;at+=config.utcOffset*60;out=*gmtime(&at);return true;}
String clockText(){tm v;if(!localTime(time(nullptr),v))return "--:--";char b[6];snprintf(b,sizeof b,"%02d:%02d",v.tm_hour,v.tm_min);return b;}
String timeText(uint32_t at){tm v,today;if(!localTime(at,v))return "";char b[8];if(localTime(time(nullptr),today)&&(v.tm_yday!=today.tm_yday||v.tm_year!=today.tm_year))snprintf(b,sizeof b,"%02d.%02d",v.tm_mday,v.tm_mon+1);else snprintf(b,sizeof b,"%02d:%02d",v.tm_hour,v.tm_min);return b;}
String dateText(){tm v;if(!localTime(time(nullptr),v))return t("Clock not set","Время не установлено");const char* dayRu[]={"воскресенье","понедельник","вторник","среда","четверг","пятница","суббота"},*dayEn[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};const char* monRu[]={"января","февраля","марта","апреля","мая","июня","июля","августа","сентября","октября","ноября","декабря"},*monEn[]={"January","February","March","April","May","June","July","August","September","October","November","December"};return config.russian?String(dayRu[v.tm_wday])+", "+String(v.tm_mday)+" "+monRu[v.tm_mon]:String(dayEn[v.tm_wday])+", "+monEn[v.tm_mon]+" "+String(v.tm_mday);}
String ago(uint32_t ms){uint32_t s=ms/1000;if(s<60)return t("now","сейчас");if(s<3600)return String(s/60)+t(" min"," мин");if(s<86400)return String(s/3600)+t(" h"," ч");return String(s/86400)+t(" d"," д");}
String pathText(const Peer& p){if(p.pathLength==255)return t("path unknown","путь неизвестен");if(!(p.pathLength&63))return t("direct","напрямую");return count(p.pathLength&63,"hop","hops","хоп","хопа","хопов");}
String typeText(uint8_t type){switch(type){case 1:return t("Chat node","Чат-узел");case 2:return t("Repeater","Ретранслятор");case 3:return t("Room server","Сервер комнаты");case 4:return t("Sensor","Датчик");}return t("Node","Узел");}
uint16_t typeColor(uint8_t type){return type==2?violet:type==3?warn:type==4?info:accent;}
bool distanceTo(const Peer& p,float& metres,float& bearing){
 if(!p.position||!hardware.gpsFix())return false;double la1=hardware.gps.location.lat()*M_PI/180,la2=p.latitude*M_PI/180,dl=(p.longitude-hardware.gps.location.lng())*M_PI/180;
 double a=sin((la2-la1)/2)*sin((la2-la1)/2)+cos(la1)*cos(la2)*sin(dl/2)*sin(dl/2);metres=12742000*atan2(sqrt(a),sqrt(1-a));
 bearing=fmod(atan2(sin(dl)*cos(la2),cos(la1)*sin(la2)-sin(la1)*cos(la2)*cos(dl))*180/M_PI+360,360);return true;
}
String distanceText(float m,float bearing){const char* ru[]={"С","СВ","В","ЮВ","Ю","ЮЗ","З","СЗ"},*en[]={"N","NE","E","SE","S","SW","W","NW"};int k=int((bearing+22.5f)/45)%8;return (m<1000?String(int(m/10)*10)+t(" m"," м"):String(m/1000,m<10000?1:0)+t(" km"," км"))+" "+(config.russian?ru[k]:en[k]);}
unsigned batteryPercent(){static const uint16_t mv[]={3300,3500,3600,3700,3800,3900,4000,4100,4200};static const uint8_t pc[]={0,5,15,35,55,70,82,93,100};uint16_t v=hardware.batteryMv;if(v<=mv[0])return 0;for(int i=1;i<9;i++)if(v<=mv[i])return pc[i-1]+(pc[i]-pc[i-1])*(v-mv[i-1])/(mv[i]-mv[i-1]);return 100;}

// Status bar (HUD): title on the left; radio, GPS, links, clock and battery on the right.
String title(){
 if(locked)return config.name;
 switch(page){case Home:return "MeshMesh";case Threads:return t("Chats","Чаты");case Chat:return nameOf(recipient);case Map:return t("Map","Карта");case Nodes:return t("Nodes","Узлы");case Node:{Peer* p=focusedPeer();return p?String(p->name):t("Node","Узел");}
 case Sensors:return t("Navigation","Навигация");case Settings:return t("Settings","Настройки");case Radio:return t("Radio","Радио");case Display:return t("Screen & device","Экран и устройство");case Network:return t("Connections","Подключения");case Diagnostics:return t("Module health","Состояние модулей");case Help:return t("Keys","Клавиши");case Library:return t("Saved maps","Сохранённые карты");case Scope:return t("Radar: signals","Радар: сигналы");case Homing:return t("Homing","Пеленг");case Game:return gameTitle();case Motion:return t("Radar: motion (CSI)","Радар: движение (CSI)");}return "";
}
void statusBar(){
 auto& d=g();d.fillRect(0,0,320,20,bar);d.drawFastHLine(0,20,320,line);int x=313;
 uint16_t mv=hardware.batteryMv;bool external=mv>4250;unsigned pct=batteryPercent();uint16_t level=!mv?faint:external?accent:pct<=15?bad:pct<=30?warn:ok;
 d.drawRoundRect(x-19,5,19,10,2,dim);d.fillRect(x,8,2,4,dim);
 if(external){d.fillTriangle(x-8,6,x-13,11,x-9,11,accent);d.fillTriangle(x-10,9,x-6,9,x-11,14,accent);}else if(mv)d.fillRect(x-17,7,max(1u,15*pct/100),6,level);
 x-=23;if(mv&&(config.batteryVolts||!external)){String p=config.batteryVolts?String(mv/1000.f,2)+"V":String(pct)+"%";textRight(x,14,p,dim,small);x-=measure(p,small)+6;}
 String clock=clockText();textRight(x,15,clock,ink,bold);x-=measure(clock,bold)+8;
 bool fresh=meshRadio.lastRxAt&&millis()-meshRadio.lastRxAt<600000;bars(x-15,15,meshRadio.ready&&fresh?max(1u,snrLevel(meshRadio.lastSnr)):0,ok);
 if(!meshRadio.ready){d.drawLine(x-15,5,x-4,15,bad);d.drawLine(x-4,5,x-15,15,bad);}
 x-=21;if(meshRadio.busy()){tri(x+2,10,0,3,accent);x-=9;}
 if(config.gps){icon(IcPin,x-5,10,6,hardware.gpsFix()?ok:hardware.clockConflict?bad:warn,bar);x-=15;}
 if(portalActive()){icon(IcWifi,x-6,10,7,accent,bar);x-=17;}
 if(bleActive()){icon(IcBle,x-3,10,6,info,bar);x-=12;}
 unsigned n=page==Threads||page==Chat?0:unreadTotal();
 if(n){String c=String(n);textRight(x,14,c,warn,small);x-=measure(c,small)+2;icon(IcMail,x-7,10,7,warn,bar);x-=19;}
 String name=title();if(page==Chat||page==Node){tri(10,10,3,4,dim);text(17,15,fit(name,x-22,bold),ink,bold);}else text(8,15,fit(name,x-14,bold),page==Home?accent:ink,bold);
}
void drawToast(){
 if(!toast.length()||millis()-toastAt>=3500)return;String s=fit(toast,280);int w=measure(s)+26,x=160-w/2;
 panel(x,193,w,23,cardHi,6);g().drawRoundRect(x,193,w,23,6,line);g().fillRoundRect(x+5,198,3,13,1,toastColor);text(x+15,209,s,ink);
}
void scrollbar(int first,int visible,int total,int y,int h){if(total<=visible)return;int th=max(12,h*visible/total),ty=y+(h-th)*first/max(1,total-visible);g().fillRoundRect(315,y,3,h,1,card);g().fillRoundRect(315,ty,3,th,1,faint);}
void listRow(int y,int h,bool focus){panel(8,y,304,h,focus?cardHi:bg,7);if(focus)g().fillRoundRect(8,y+6,3,h-12,1,accent);}

// Module health: the same eight modules as the diagnostics page.
unsigned moduleStates(bool* state){bool s[]={meshRadio.ready,hardware.keyboardOk,hardware.sdOk,hardware.fsOk,hardware.rtcValid,hardware.gps.passedChecksum()>0,hardware.compassSample,hardware.imuSample};unsigned faults=0;for(int i=0;i<8;i++){if(state)state[i]=s[i];faults+=!s[i];}return faults;}
// Home: identity strip and a 3-column grid of destinations; two rows are visible, the rest scroll.
const int tileCount=9,tileColumns=3,tileRows=(tileCount+tileColumns-1)/tileColumns;
void drawHome(){
 auto& d=g();panel(8,26,304,38,card,8);icon(IcRadio,26,44,9,meshRadio.ready?accent:bad,card);
 text(44,41,fit(String(config.name),150,bold),ink,bold);
 text(44,56,fit(meshRadio.ready?String(config.frequency,3)+t(" MHz · SF"," МГц · SF")+String(config.sf)+" · BW "+String(config.bandwidth,1)+" · CR 4/"+String(config.cr):t("Radio error ","Ошибка радио ")+String(meshRadio.radioError),200,small),meshRadio.ready?dim:bad,small);
 textRight(302,41,String(meshRadio.txCount),ink,small);tri(302-measure(String(meshRadio.txCount),small)-7,38,0,3,accent);
 textRight(302,56,String(meshRadio.rxCount),ink,small);tri(302-measure(String(meshRadio.rxCount),small)-7,53,2,3,ok);
 unsigned unreadCount=unreadTotal(),near=0,total=meshRadio.peerCount;for(unsigned i=0;i<total;i++)if(meshRadio.peers[i].heard&&millis()-meshRadio.peers[i].seen<1800000)near++;
 String links=portalActive()&&bleActive()?"Wi-Fi + BLE":portalActive()?"Wi-Fi":bleActive()?"Bluetooth":t("All off","Всё выключено");unsigned faults=moduleStates(nullptr);
 struct {Icon ic;uint16_t hue;String name,detail;unsigned badge;} tiles[tileCount]={
  {IcChat,accent,t("Chats","Чаты"),unreadCount?count(unreadCount,"new","new","новое","новых","новых"):count(meshRadio.historyCount,"message","messages","сообщение","сообщения","сообщений"),unreadCount},
  {IcPin,ok,t("Map","Карта"),!maps.available?t("No SD card","Нет SD-карты"):maps.title.length()?maps.title:t("No maps yet","Карт пока нет"),0},
  {IcMesh,violet,t("Nodes","Узлы"),String(near)+t(" of "," из ")+String(total)+t(" nearby"," рядом"),0},
  {IcCompass,warn,t("Navigation","Навигация"),hardware.gpsFix()?"GPS: "+count(hardware.gps.satellites.value(),"sat","sats","спутник","спутника","спутников"):config.gps?t("GPS: searching","GPS: поиск"):t("GPS off","GPS выключен"),0},
  {IcWifi,info,t("Connect","Связь"),links,0},
  {IcRadar,accent,t("Radar","Радар"),"Wi-Fi, BLE, LoRa",0},
  {IcPulse,faults?bad:ok,t("Module health","Модули"),faults?count(faults,"fault","faults","ошибка","ошибки","ошибок"):t("All OK","Всё в норме"),0},
  {IcGear,dim,t("Settings","Настройки"),t("Radio, screen","Радио, экран"),0},
  {IcCards,warn,t("Solitaire","Косынка"),gameTileDetail(),0}};
 int firstRow=max(0,selected/tileColumns-1);
 for(int i=firstRow*tileColumns;i<tileCount&&i<(firstRow+2)*tileColumns;i++){
  int x=8+(i%tileColumns)*104,y=71+(i/tileColumns-firstRow)*74;bool focus=selected==i;panel(x,y,96,68,focus?cardHi:card,8);if(focus)ring(x,y,96,68,8);
  d.fillRoundRect(x+9,y+8,26,26,6,bg);icon(tiles[i].ic,x+22,y+21,8,tiles[i].hue,bg);
  if(tiles[i].badge){String b=tiles[i].badge>99?"99+":String(tiles[i].badge);int w=max(16,measure(b,small)+8);d.fillRoundRect(x+88-w,y+8,w,13,6,bad);textCenter(x+88-w/2,y+18,b,ink,small);}
  text(x+9,y+49,fit(tiles[i].name,80,bold),ink,bold);text(x+9,y+61,fit(tiles[i].detail,80,small),dim,small);
 }
 scrollbar(firstRow,2,tileRows,71,142);
 footer({{"OK",t("Open","Открыть")},{"MSG",t("Chats","Чаты")},{"MAP",t("Map","Карта")},{"MIC",t("Lock","Блок")}});
}
void drawThreads(){
 threads();if(selected>=int(conversationCount))selected=conversationCount-1;int first=max(0,selected-3);
 for(int i=first;i<int(conversationCount)&&i<first+4;i++){
  uint64_t id=conversations[i];int y=24+(i-first)*48;bool focus=selected==i;listRow(y,45,focus);
  Peer* p=peerOf(id);avatar(32,y+22,15,id,nameOf(id),p?p->type:-1);
  const ChatMessage* last=nullptr;for(int j=meshRadio.historyCount-1;j>=0;j--)if(belongs(meshRadio.history[j],id)){last=&meshRadio.history[j];break;}
  String when=last?timeText(last->timestamp):"";if(when.length())textRight(306,19+y,when,dim,small);
  text(56,y+18,fit(id==meshmesh::Broadcast?String("Public"):nameOf(id),240-measure(when,small),bold),ink,bold);
  String preview=last?(last->outgoing?t("You: ","Вы: "):id==meshmesh::Broadcast?String(last->name)+": ":String())+last->text:(id==meshmesh::Broadcast?t("Open MeshCore channel","Открытый канал MeshCore"):t("No messages yet","Сообщений ещё нет"));
  preview.replace("\n"," ");unsigned n=unread(id);int pillW=0;
  if(n){String b=String(n);pillW=max(18,measure(b,small)+10);g().fillRoundRect(306-pillW,y+26,pillW,14,7,accent);textCenter(306-pillW/2,y+36,b,bg,small);}
  text(56,y+36,fit(preview,244-pillW),n?ink:dim);
 }
 scrollbar(first,4,conversationCount,24,190);
 footer({{"OK",t("Open","Открыть")},{"^v",t("Select","Выбор")},{"ADV",t("Announce","Объявить")},{"HOME",t("Menu","Меню")}});
}
void statusMark(int x,int y,const ChatMessage& m){
 auto& d=g();switch(m.status){
 case ChatMessage::Queued:d.drawCircle(x+3,y-3,3,dim);d.drawFastVLine(x+3,y-5,3,dim);d.drawFastHLine(x+3,y-3,2,dim);break;
 case ChatMessage::Sent:d.drawLine(x,y-3,x+2,y-1,dim);d.drawLine(x+2,y-1,x+6,y-6,dim);break;
 case ChatMessage::Delivered:for(int k:{0,4}){d.drawLine(x+k,y-3,x+k+2,y-1,accent);d.drawLine(x+k+2,y-1,x+k+6,y-6,accent);}break;
 case ChatMessage::Failed:d.fillCircle(x+3,y-3,4,bad);d.drawFastVLine(x+3,y-5,3,ink);d.drawPixel(x+3,y-1,ink);break;
 default:break;
 }
}
void drawChat(){
 auto& d=g();Peer* p=peerOf(recipient);
 String sub=recipient==meshmesh::Broadcast?t("Open channel · senders unverified · no ACK","Открытый канал · без подтверждений"):p?typeText(p->type)+" · "+pathText(*p)+(p->heard?" · "+ago(millis()-p->seen):String()):t("Not in contacts","Нет в контактах");
 text(8,33,fit(sub,270,small),dim,small);String lang=keyboardRussian?"RU":"EN";d.fillRoundRect(292,24,22,13,3,keyboardRussian?rgb(0x14524b):line);textCenter(303,34,lang,ink,small);
 String rows[6];unsigned inputRows=max(1u,wrap(composer,rows,6,276));unsigned shown=min(inputRows,3u);int boxH=shown*13+11,boxY=216-boxH;
 int indices[64],total=0;for(unsigned i=0;i<meshRadio.historyCount;i++)if(belongs(meshRadio.history[i],recipient))indices[total++]=i;
 int end=max(0,total-chatOffset),bottom=boxY-5;const int top=41;
 for(int j=end-1;j>=0&&bottom>top+20;j--){
  auto& m=meshRadio.history[indices[j]];String lines[7];unsigned n=wrap(m.text,lines,7,214);bool named=!m.outgoing&&recipient==meshmesh::Broadcast;
  String when=timeText(m.timestamp),state=m.status==ChatMessage::Failed?t("not confirmed","не подтверждено"):String();if(m.protocol==1)state="MM/1 "+state;
  int metaW=measure(when,small)+(state.length()?measure(state,small)+4:0)+(m.outgoing?12:0),w=metaW;
  for(unsigned k=0;k<n;k++)w=max(w,measure(lines[k]));if(named)w=max(w,measure(m.name,small));w=max(44,w+16);
  int h=7+(named?11:0)+n*13+10;if(bottom-h<top){if(j!=end-1)break;n=max(1,(bottom-top-7-(named?11:0)-10)/13);h=7+(named?11:0)+n*13+10;}
  int x=m.outgoing?312-w:8,y=bottom-h,ty=y+14;panel(x,y,w,h,m.outgoing?outBubble:inBubble,8);
  if(named){text(x+8,y+10,fit(m.name,w-16,small),avatarHues[(m.source^m.source>>17^m.source>>41)%8]|0x4208,small);ty+=11;}
  for(unsigned k=0;k<n;k++)text(x+8,ty+k*13,lines[k]);
  int mx=x+w-7-(m.outgoing?11:0),my=y+h-4;textRight(mx,my,when,dim,small);if(state.length())textRight(mx-measure(when,small)-4,my,state,m.status==ChatMessage::Failed?bad:faint,small);if(m.outgoing)statusMark(x+w-15,my,m);
  bottom=y-5;
 }
 if(!total){icon(recipient==meshmesh::Broadcast?IcHash:IcChat,160,86,14,faint);textCenter(160,124,t("No messages yet","Сообщений пока нет"),dim);textCenter(160,142,t("Type and press OK to send","Наберите текст и нажмите OK"),faint,small);}
 if(chatOffset){String c=String(chatOffset);int w=measure(c,small)+18;d.fillRoundRect(304-w,boxY-19,w,14,7,accent);tri(304-w+7,boxY-12,2,3,bg);text(304-w+12,boxY-8,c,bg,small);}
 panel(8,boxY,304,boxH,card,7);d.drawRoundRect(8,boxY,304,boxH,7,composer.length()?accent:line);
 if(composer.length()){unsigned firstRow=inputRows>3?inputRows-3:0;int cx=14;for(unsigned i=firstRow;i<inputRows;i++)cx=text(14,boxY+14+(i-firstRow)*13,rows[i],ink);d.fillRect(cx+1,boxY+4+(shown-1)*13,2,12,accent);}
 else{d.fillRect(14,boxY+5,2,12,accent);text(20,boxY+15,t("Message...","Сообщение..."),faint);textRight(306,boxY+15,t("2x space: RU/EN","2×пробел: RU/EN"),faint,small);}
 unsigned used=composer.length(),limit=meshRadio.messageLimit(recipient);String counter=String(used)+"/"+String(limit);
 textRight(306,boxY+boxH-3,counter,used*10>=limit*9?(used>=limit?bad:warn):faint,small);
 footer({{"OK",t("Send","Отпр.")},{config.russian?"2×пробел":"2x space","RU/EN"},{t("hold OK","удерж. OK").c_str(),t("Layout","Раскладка")},{"BACK",t("Back","Назад")}});
}
// Map: north-up tiles with HUD chips, own position, node markers and a scale bar.
void drawMap(){
 auto& d=g();maps.draw(0,21,320,199);
 if(!maps.available||!maps.haveCenter){
  panel(40,70,240,86,card,10);icon(IcPin,160,96,12,faint,card);
  textCenter(160,128,!maps.available?t("Insert an SD card for maps","Для карты нужна SD-карта"):t("Load a map via Wi-Fi or USB","Загрузите карту по Wi-Fi или USB"),ink);
  textCenter(160,144,!maps.available?t("Maps are stored on SD","Карты хранятся на SD"):t("GPS: waiting for position","GPS: ожидание позиции"),dim,small);
 }else{
  double scale=256.0*(1UL<<maps.zoom);
  auto yy=[&](double l){double r=l*M_PI/180;return (1-log(tan(r)+1/cos(r))/M_PI)/2*scale;};
  auto point=[&](double lat,double lon,int& x,int& y){if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>85.05112878)return false;x=160+int((lon-maps.longitude)/360*scale);y=120+int(yy(lat)-yy(maps.latitude));return x>=6&&x<314&&y>=26&&y<216;};
  for(unsigned i=0;i<meshRadio.peerCount;i++){auto& p=meshRadio.peers[i];int x,y;if(!p.position||!point(p.latitude,p.longitude,x,y))continue;
   d.fillCircle(x,y,6,bar);d.fillCircle(x,y,4,typeColor(p.type));String name=fit(p.name,80,small);int w=measure(name,small)+6,lx=constrain(x+8,2,316-w);d.fillRoundRect(lx,y-6,w,11,3,bar);text(lx+3,y+2,name,ink,small);}
  if(hardware.gpsFix()){int x,y;if(point(hardware.gps.location.lat(),hardware.gps.location.lng(),x,y)){
   if(navigation.headingValid){float a=navigation.heading*M_PI/180;d.fillTriangle(x+roundf(18*sinf(a)),y-roundf(18*cosf(a)),x+roundf(8*sinf(a-0.6f)),y-roundf(8*cosf(a-0.6f)),x+roundf(8*sinf(a+0.6f)),y-roundf(8*cosf(a+0.6f)),info);}
   d.fillCircle(x,y,7,ink);d.fillCircle(x,y,5,info);}}
  if(!maps.follow){d.fillRect(151,119,19,3,bar);d.fillRect(159,111,3,19,bar);d.drawFastHLine(152,120,17,ink);d.drawFastVLine(160,112,17,ink);d.drawCircle(160,120,5,bar);}
  if(!maps.visibleTiles){panel(30,96,260,44,card,8);textCenter(160,114,maps.waiting?t("Reading map from SD...","Чтение карты с SD..."):t("This zoom is not downloaded","Этот масштаб не загружен"),ink);if(!maps.waiting)textCenter(160,130,t("+/- zoom · L saved maps","+/- масштаб · L список карт"),dim,small);}
  double metresPerPx=cos(maps.latitude*M_PI/180)*40075016.686/scale;int nice[]={5,10,20,50,100,200,500,1000,2000,5000,10000,20000,50000,100000},dist=nice[0];for(int v:nice)if(v/metresPerPx<=90)dist=v;int px=max(8,int(dist/metresPerPx));
  String scaleText=dist>=1000?String(dist/1000)+t(" km"," км"):String(dist)+t(" m"," м");int sw=max(px,measure(scaleText,small))+12;
  d.fillRoundRect(312-sw,196,sw,19,4,bar);d.fillRect(306-px,209,px,2,ink);d.drawFastVLine(306-px,206,5,ink);d.drawFastVLine(305,206,5,ink);textRight(306,204,scaleText,ink,small);
  d.fillRoundRect(4,203,82,12,3,bar);text(8,212,"© OpenStreetMap",dim,small);
 }
 String area=fit(maps.title.length()?maps.title:t("Offline map","Офлайн-карта"),120,small)+"  z"+String(maps.zoom);int aw=measure(area,small)+22;
 d.fillRoundRect(6,26,aw,16,4,bar);icon(IcPin,14,34,5,ok,bar);text(22,37,area,ink,small);
 bool fix=hardware.gpsFix();String gps=!config.gps?t("GPS off","GPS выкл"):fix?"GPS "+String(hardware.gps.satellites.value()):hardware.clockConflict?t("GPS: old date","GPS: старая дата"):t("GPS: searching","GPS: поиск");
 int gw=measure(gps,small)+10;d.fillRoundRect(6,45,gw,15,4,bar);text(11,56,gps,!config.gps?dim:fix?ok:warn,small);
 if(navigation.headingValid){int cx=298,cy=42;d.fillCircle(cx,cy,15,bar);d.drawCircle(cx,cy,15,line);float a=navigation.heading*M_PI/180;
  d.fillTriangle(cx+roundf(11*sinf(a)),cy-roundf(11*cosf(a)),cx+roundf(4*cosf(a)),cy+roundf(4*sinf(a)),cx-roundf(4*cosf(a)),cy-roundf(4*sinf(a)),bad);
  d.fillTriangle(cx-roundf(11*sinf(a)),cy+roundf(11*cosf(a)),cx+roundf(4*cosf(a)),cy+roundf(4*sinf(a)),cx-roundf(4*cosf(a)),cy-roundf(4*sinf(a)),dim);
  String deg=String(int(navigation.heading+.5f)%360)+"°";d.fillRoundRect(cx-15,60,30,12,3,bar);textCenter(cx,69,deg,ink,small);}
 footer({{"<>",t("Pan","Сдвиг")},{"+/-",t("Zoom","Масштаб")},{"OK",maps.follow?t("Following","Следую"):t("To GPS","К GPS")},{"L",t("Maps","Карты")}});
}
void drawLibrary(){
 unsigned total=library.size();if(!total){icon(IcPin,160,90,14,faint);textCenter(160,128,t("No saved maps","Сохранённых карт нет"),ink);textCenter(160,146,t("Upload a map via Wi-Fi or USB","Загрузите карту по Wi-Fi или USB"),dim,small);}
 int first=max(0,selected-3);for(unsigned i=first;i<total&&i<unsigned(first+4);i++){int y=24+(i-first)*48;bool focus=selected==int(i);listRow(y,45,focus);
  g().fillRoundRect(18,y+8,30,30,7,card);icon(IcPin,33,y+22,9,ok,card);String name=library[i]["name"].as<String>();
  text(58,y+19,fit(name,200,bold),ink,bold);text(58,y+36,count(library[i]["tiles"].as<unsigned>(),"tile on SD","tiles on SD","тайл на SD","тайла на SD","тайлов на SD"),dim,small);
  if(name==maps.title){g().drawLine(284,y+22,288,y+26,accent);g().drawLine(288,y+26,296,y+17,accent);g().drawLine(284,y+23,288,y+27,accent);g().drawLine(288,y+27,296,y+18,accent);}}
 scrollbar(first,4,total,24,190);footer({{"OK",t("Open","Открыть")},{"^v",t("Select","Выбор")},{"BACK",t("Map","Карта")}});
}
void drawNodes(){
 sortNodes();
 if(!nodeTotal){icon(IcMesh,160,86,16,faint);textCenter(160,126,t("No nodes discovered yet","Узлы пока не обнаружены"),ink);textCenter(160,144,t("ADV sends your announcement","ADV отправит ваше объявление"),dim,small);}
 int first=max(0,selected-3);uint32_t now=millis();
 for(unsigned i=first;i<nodeTotal&&i<unsigned(first+4);i++){
  auto& p=meshRadio.peers[nodeOrder[i]];int y=24+(i-first)*48;bool focus=selected==int(i);listRow(y,45,focus);avatar(32,y+22,15,p.id,p.name,p.type);
  String age=p.heard?ago(now-p.seen):t("saved","сохранён");textRight(306,y+18,age,p.heard&&now-p.seen<1800000?ok:dim,small);
  if(p.heard)bars(286-measure(age,small),y+18,snrLevel(p.snr),ok);
  text(56,y+18,fit(p.name,200-measure(age,small),bold),ink,bold);
  float metres,bearing;String detail=typeText(p.type)+" · "+pathText(p);
  if(p.heard)detail+=" · "+String(int(p.rssi))+" dBm";if(distanceTo(p,metres,bearing))detail+=" · "+distanceText(metres,bearing);else if(p.position)detail+=" · GPS";
  text(56,y+36,fit(detail,250,small),typeColor(p.type)==accent?dim:typeColor(p.type),small);
 }
 scrollbar(first,4,nodeTotal,24,190);
 footer({{"OK",t("Details","Карточка")},{"P",t("On map","На карте")},{"ADV",t("Announce","Объявить")},{"BACK",t("Back","Назад")}});
}
enum NodeAction {ActWrite,ActMap,ActResetPath,ActForget};
unsigned nodeActions(const Peer& p,NodeAction* out){unsigned n=0;if(p.type==1)out[n++]=ActWrite;if(p.position)out[n++]=ActMap;if(p.pathLength!=255)out[n++]=ActResetPath;out[n++]=ActForget;return n;}
void drawNode(){
 Peer* p=focusedPeer();if(!p){textCenter(160,110,t("Node was removed","Узел удалён"),dim);footer({{"BACK",t("Nodes","Узлы")}});return;}
 auto& d=g();avatar(36,50,20,p->id,p->name,p->type);text(66,48,fit(p->name,240,big),ink,big);text(66,64,typeText(p->type),typeColor(p->type),small);
 float metres,bearing;String rows[][2]={
  {t("Last heard","Последний приём"),p->heard?(millis()-p->seen<60000?ago(0):ago(millis()-p->seen)+t(" ago"," назад")):t("not since boot","после запуска не слышен")},
  {t("Signal","Сигнал"),p->heard?String(int(p->rssi))+" dBm · SNR "+String(p->snr,1):String("-")},
  {t("Route","Маршрут"),pathText(*p)+(p->pathLength==255?t(" · flood"," · flood"):String())},
  {t("Position","Позиция"),distanceTo(*p,metres,bearing)?distanceText(metres,bearing):p->position?String(p->latitude,5)+", "+String(p->longitude,5):t("not shared","не передана")},
  {"ID",meshRadio.idText(p->id)}};
 for(int i=0;i<5;i++){int y=90+i*18;text(14,y,rows[i][0],dim);textRight(306,y,fit(rows[i][1],180),ink);if(i<4)d.drawFastHLine(14,y+5,292,card);}
 NodeAction acts[4];unsigned n=nodeActions(*p,acts);action=constrain(action,0,int(n)-1);
 const String names[]={t("Message","Написать"),t("On map","На карте"),t("Reset path","Сброс пути"),deleteArmed?t("Sure?","Удалить?"):t("Forget","Удалить")};
 int w=(304-(n-1)*6)/n;for(unsigned i=0;i<n;i++){int x=8+i*(w+6);bool focus=action==int(i),danger=acts[i]==ActForget;panel(x,184,w,28,focus?(danger&&deleteArmed?bad:cardHi):card,7);if(focus)ring(x,184,w,28,7,danger?bad:accent);textCenter(x+w/2,202,fit(names[acts[i]],w-8),danger?(focus?ink:bad):ink);}
 footer({{"<>",t("Action","Действие")},{"OK",t("Run","Выполнить")},{"BACK",t("Nodes","Узлы")}});
}
void compassRose(int cx,int cy,int r){
 auto& d=g();d.fillCircle(cx,cy,r,card);d.drawCircle(cx,cy,r,line);d.drawCircle(cx,cy,r-1,line);
 for(int a=0;a<360;a+=30){float rad=a*M_PI/180;int l=a%90?4:7;d.drawLine(cx+roundf((r-2)*sinf(rad)),cy-roundf((r-2)*cosf(rad)),cx+roundf((r-2-l)*sinf(rad)),cy-roundf((r-2-l)*cosf(rad)),a%90?faint:dim);}
 const char* ru[]={"С","В","Ю","З"},*en[]={"N","E","S","W"};for(int k=0;k<4;k++){float rad=k*M_PI/2;textCenter(cx+roundf((r-16)*sinf(rad)),cy-roundf((r-16)*cosf(rad))+4,config.russian?ru[k]:en[k],k?dim:bad,bold);}
 if(navigation.headingValid){float a=navigation.heading*M_PI/180;int tx=cx+roundf((r-8)*sinf(a)),ty=cy-roundf((r-8)*cosf(a));d.fillTriangle(tx,ty,cx+roundf(5*cosf(a)),cy+roundf(5*sinf(a)),cx-roundf(5*cosf(a)),cy-roundf(5*sinf(a)),accent);d.fillCircle(cx,cy,3,ink);}
 else textCenter(cx,cy+4,"--",faint,bold);
}
void drawSensors(){
 compassRose(62,84,54);String heading=navigation.headingValid?String(int(navigation.heading+.5f)%360)+"°":navigation.calibrated?t("hold flat","держите ровно"):t("not calibrated","не откалиброван");
 textCenter(62,153,heading,navigation.headingValid?ink:warn,navigation.headingValid?bold:small);
 bool fix=hardware.gpsFix();int y=38;
 auto row=[&](const String& name,const String& value,uint16_t color){text(130,y,name,dim,small);textRight(306,y,fit(value,120),color);y+=19;};
 row("GPS",!config.gps?t("off","выключен"):fix?t("fix","позиция есть"):hardware.clockConflict?t("old date","старая дата"):t("searching","поиск"),!config.gps?dim:fix?ok:warn);
 row(t("Satellites","Спутники"),String(hardware.gps.satellites.value()),ink);
 row(t("Latitude","Широта"),fix?String(hardware.gps.location.lat(),5):String("-"),ink);
 row(t("Longitude","Долгота"),fix?String(hardware.gps.location.lng(),5):String("-"),ink);
 row(t("Clock","Часы"),(hardware.clockSource=="unset"?t("not set","не задано"):hardware.clockSource=="manual"?t("manual","вручную"):hardware.clockSource)+(hardware.clockTrusted?t(" · trusted"," · доверено"):String()),ink);
 row(t("Compass","Компас"),navigation.calibrating?count(navigation.samples,"sample","samples","отсчёт","отсчёта","отсчётов"):navigation.calibrated?t("calibrated","откалиброван"):t("needs calibration","нужна калибровка"),navigation.calibrating?warn:ink);
 String actions[]={t("Share position","Передать позицию"),navigation.calibrating?t("Finish calibration","Завершить калибровку"):t("Calibrate compass","Калибровать компас")};
 String hints[]={fix?t("Advert with GPS","Объявление с GPS"):t("Needs a fresh GPS fix","Нужна свежая позиция GPS"),navigation.calibrating?t("Rotate in all directions","Вращайте во все стороны"):t("Rotate for 20+ seconds","Вращение 20+ секунд")};
 for(int i=0;i<2;i++){int x=8+i*154;bool focus=selected==i;panel(x,168,150,46,focus?cardHi:card,7);if(focus)ring(x,168,150,46,7);text(x+10,186,fit(actions[i],132,bold),ink,bold);text(x+10,203,fit(hints[i],132,small),dim,small);}
 footer({{"<>",t("Select","Выбор")},{"OK",t("Run","Выполнить")},{"BACK",t("Back","Назад")}});
}
// Signal radar: Wi-Fi access points, Bluetooth devices and directly heard LoRa nodes. Nearer the centre means a
// stronger signal; the angle only keeps targets apart, because RSSI carries no bearing.
uint64_t scopeId=0;RadarTarget::Kind scopeKind=RadarTarget::Wifi;bool radarSound=true;uint32_t pingAt=0,pingedSamples=0;
void scopeSelect(int i){if(i<0||i>=int(radar.count))return;scopeId=radar.targets[i].id;scopeKind=radar.targets[i].kind;}
// Until the user moves it, the selection stays on the strongest signal; then it follows its target
// through re-sorting, and a vanished target passes it back to the strongest.
int scopeSelected(){if(!scopeManual&&radar.count){scopeSelect(0);return 0;}for(unsigned i=0;i<radar.count;i++)if(radar.targets[i].id==scopeId&&radar.targets[i].kind==scopeKind)return i;if(!radar.count)return -1;scopeSelect(0);return 0;}
float signalLevel(int rssi){return constrain((rssi+100)/65.f,0.f,1.f);} // -100 .. -35 dBm
uint16_t mix(uint32_t a,uint32_t b,float f){auto ch=[&](int s){return int(((a>>s)&255)*(1-f)+((b>>s)&255)*f);};return rgb(ch(16)<<16|ch(8)<<8|ch(0));}
uint32_t targetHue(const RadarTarget& r){return r.kind==RadarTarget::Lora?0xa78bfa:r.kind==RadarTarget::Ble?(r.device?0xf472b6:0x8d99a6):r.open?0xe9b13b:0x4f9df7;}
String deviceText(const RadarTarget& r){switch(r.device){case RadarTarget::Phone:return t("Phone","Телефон");case RadarTarget::Watch:return t("Watch","Часы");case RadarTarget::Audio:return t("Headphones","Наушники");case RadarTarget::Personal:return t("Phone/watch","Телефон/часы");default:return t("BLE device","BLE-устройство");}}
String vendorText(uint16_t v){switch(v){case 0x004c:return "Apple";case 0x0075:return "Samsung";case 0x00e0:return "Google";case 0x027d:return "Huawei";case 0x038f:return "Xiaomi";case 0x0087:return "Garmin";case 0x0006:return "Microsoft";}return "";}
String targetName(const RadarTarget& r){if(r.name[0])return r.name;if(r.kind==RadarTarget::Ble){String v=vendorText(r.vendor);return deviceText(r)+(v.length()?" "+v:String());}return r.kind==RadarTarget::Wifi?t("Hidden network","Скрытая сеть"):meshRadio.idText(r.id);}
String macText(uint64_t id){char b[18];snprintf(b,sizeof b,"%02X:%02X:%02X:%02X:%02X:%02X",unsigned(id>>40&255),unsigned(id>>32&255),unsigned(id>>24&255),unsigned(id>>16&255),unsigned(id>>8&255),unsigned(id&255));return b;}
bool radarFresh(){return radar.fresh();}
String bleState(){switch(radar.ble){case Radar::BleBusy:return t("Bluetooth busy: probe","Bluetooth занят проверкой");case Radar::BleFailed:return t("Bluetooth error","Ошибка Bluetooth");default:return "";}}
String wifiState(){switch(radar.wifi){case Radar::WifiPortal:return t("Wi-Fi busy: access point","Wi-Fi занят точкой доступа");case Radar::WifiBusy:return t("Wi-Fi busy: probe","Wi-Fi занят проверкой");case Radar::WifiFailed:return t("Wi-Fi error","Ошибка Wi-Fi");default:return radar.sweeps?"":t("Scanning Wi-Fi...","Сканирую Wi-Fi...");}}
void drawScope(){
 auto& d=g();const int cx=96,cy=116,R=86;const uint32_t cardRgb=0x131a22,gridRgb=0x223040;uint32_t now=millis();
 d.fillCircle(cx,cy,R,card);for(int k=1;k<=4;k++)d.drawCircle(cx,cy,R*k/4,k==4?line:rgb(gridRgb));
 d.drawFastHLine(cx-R,cy,2*R+1,rgb(gridRgb));d.drawFastVLine(cx,cy-R,2*R+1,rgb(gridRgb));
 float sweep=(now%3000)*2*M_PI/3000; // one turn per 3 s, fading trail behind the beam
 for(int k=24;k>=0;k--){float a=sweep-k*.03f;d.drawLine(cx,cy,cx+roundf(R*sinf(a)),cy-roundf(R*cosf(a)),mix(0x1fc2ae,cardRgb,.25f+k*.03f));}
 int sel=scopeSelected();
 for(int i=radar.count-1;i>=0;i--){const RadarTarget& r=radar.targets[i];
  uint32_t h=uint32_t(r.id^(r.id>>29))*2654435761u;float a=(h>>8)%360*M_PI/180;float dist=R*(.1f+.84f*(1-signalLevel(r.rssi)));
  int x=cx+roundf(dist*sinf(a)),y=cy-roundf(dist*cosf(a));float since=fmodf(sweep-a+4*M_PI,2*M_PI)/(2*M_PI); // blips glow as the beam passes
  float faded=r.kind==RadarTarget::Wifi?0:constrain((now-r.seen)/30000.f,0.f,.5f); // quiet targets fade
  d.fillCircle(x,y,i==sel?4:3,mix(targetHue(r),cardRgb,min(.85f,.15f+.6f*since+faded)));if(i==sel){d.drawCircle(x,y,7,ink);d.drawCircle(x,y,8,accent);}}
 d.fillCircle(cx,cy,3,ink);
 textCenter(cx,216,t("closer to centre = stronger","ближе к центру - сильнее"),faint,small);
 text(194,34,fit("Wi-Fi "+String(radar.counted(RadarTarget::Wifi))+"·BLE "+String(radar.counted(RadarTarget::Ble))+"·LoRa "+String(radar.counted(RadarTarget::Lora)),120,small),dim,small);
 int first=max(0,sel-6);
 for(int i=first;i<int(radar.count)&&i<first+7;i++){const RadarTarget& r=radar.targets[i];int y=40+(i-first)*22;bool focus=i==sel;
  panel(190,y,124,20,focus?cardHi:bg,5);if(focus)d.fillRoundRect(190,y+4,3,12,1,accent);d.fillCircle(199,y+10,3,rgb(targetHue(r)));
  text(206,y+14,fit(targetName(r),76),focus?ink:dim);textRight(310,y+14,String(int(r.rssi)),focus?ink:dim,bold);}
 String state=wifiState();if(!state.length())state=bleState();
 unsigned people=radar.personal(30000);if(!state.length()&&people)state=count(people,"phone/watch nearby","phones/watches nearby","телефон/часы рядом","телефона/часов рядом","телефонов/часов рядом");
 if(!radar.count)textCenter(252,110,state.length()?state:t("No signals yet","Сигналов пока нет"),radar.wifi==Radar::WifiReady?dim:warn,small);
 else if(state.length())text(194,212,fit(state,120,small),radar.wifi==Radar::WifiReady&&radar.ble!=Radar::BleFailed&&radar.ble!=Radar::BleBusy?dim:warn,small);
 footer({{"OK",t("Home in","Пеленг")},{"<>",t("Motion","Движение")},{"^v",t("Select","Выбор")},{"BACK",t("Menu","Меню")}});
}
void drawHoming(){
 auto& d=g();const RadarTarget& f=radar.focus;uint16_t hue=rgb(targetHue(f));bool fresh=radarFresh(),lora=f.kind==RadarTarget::Lora,bt=f.kind==RadarTarget::Ble;
 panel(8,26,304,34,card,8);d.fillRoundRect(14,31,24,24,6,bg);icon(lora?IcTower:bt?(f.device?IcPerson:IcBle):IcWifi,26,43,7,hue,bg);
 text(46,40,fit(targetName(f),258,bold),ink,bold);
 String vendor=vendorText(f.vendor);
 text(46,54,fit(lora?"LoRa · "+meshRadio.idText(f.id)+t(" · direct packets only"," · только прямые пакеты"):bt?"Bluetooth · "+deviceText(f)+(vendor.length()?" · "+vendor:String())+t(" · address changes over time"," · адрес со временем меняется"):"Wi-Fi · "+t("channel ","канал ")+String(f.channel)+" · "+macText(f.id),258,small),dim,small);
 int x=text(14,112,radar.samples?String(int(lroundf(radar.fast))):String("--"),fresh?ink:faint,digits);text(x+4,112,"dBm",dim,small);
 // Homing indicator: the fast average against the slow one, as in the RSSI tracker.
 int trend=radar.trend();panel(186,66,126,46,card,8);
 if(f.kind==RadarTarget::Wifi&&radar.wifi!=Radar::WifiReady)text(196,93,fit(wifiState(),108,small),warn,small);
 else if(bt&&radar.ble!=Radar::BleReady)text(196,93,fit(bleState(),108,small),warn,small);
 else if(!fresh)text(196,93,radar.samples>1?t("Lost","Потерян"):t("No signal","Нет сигнала"),warn,bold);
 else{uint16_t c=trend>0?ok:trend<0?bad:dim;if(trend)tri(206,89,trend>0?0:2,10,c);else{d.fillRect(197,83,18,3,c);d.fillRect(197,91,18,3,c);}
  text(224,86,trend>0?t("Stronger","Теплее"):trend<0?t("Weaker","Холоднее"):t("Steady","Ровно"),c,bold);text(224,101,t("keep moving","двигайтесь"),faint,small);}
 // Strength meter with the peak marker.
 float level=signalLevel(lroundf(radar.fast));d.fillRoundRect(14,120,292,8,3,line);if(radar.samples)d.fillRoundRect(14,120,max(3,int(292*level)),8,3,!fresh?faint:level<.3f?bad:level<.6f?warn:ok);
 if(radar.samples){int px=14+int(291*signalLevel(radar.peak));d.fillRect(px-1,116,3,16,ink);}
 text(14,142,"-100",faint,small);textRight(306,142,"-35 dBm",faint,small);textCenter(160,142,t("peak ","пик ")+(radar.samples?String(int(lroundf(radar.peak)))+" dBm ("+String(radar.fast-radar.peak,0)+")":String("-")),dim,small);
 // History: the last 120 samples, oldest on the left.
 panel(8,148,304,58,card,6);for(int dbm:{-90,-70,-50}){int y=202-int(50*signalLevel(dbm));d.drawFastHLine(12,y,296,rgb(0x223040));}
 unsigned n=radar.historyCount();int px=0,py=0;
 for(unsigned i=0;i<n;i++){int x=12+int(i*296/(Radar::HistorySize-1)),y=202-int(50*signalLevel(radar.sample(i)));if(i)d.drawLine(px,py,x,y,accent);else d.drawPixel(x,y,accent);px=x;py=y;}
 if(!n)textCenter(160,181,t("Waiting for samples","Ожидание отсчётов"),faint,small);
 uint32_t age=radar.samples?millis()-radar.lastSample:0;
 String stats=lora?count(radar.samples,"packet","packets","пакет","пакета","пакетов"):String(radar.rate)+(bt?t(" adverts/s"," объявл./с"):t(" beacons/s"," маяков/с"));
 int sx=text(12,216,stats,dim,small);
 // Sample age: green up to 0.5 s, yellow up to 1.5 s, then red (LoRa counts in minutes).
 if(radar.samples)text(sx,216," · "+(age<60000?String(age/1000.f,1)+t(" s ago"," с назад"):ago(age)),lora?dim:age<500?ok:age<1500?warn:bad,small);
 footer({{"OK",radarSound?t("Mute","Без звука"):t("Sound","Звук")},{"<>",t("Reset peak","Сброс пика")},{"BACK",t("List","Список")}});
}
// CSI motion: one board is the beacon, this one the sensor (or the other way round).
uint32_t motionBeepAt=0;bool wasMoving=false;
void drawMotion(){
 auto& d=g();bool beacon=radar.csi==Radar::CsiBeacon,heard=radar.beaconHeard();uint32_t now=millis();
 panel(8,26,304,34,card,8);d.fillRoundRect(14,31,24,24,6,bg);icon(beacon?IcRadio:IcPulse,26,43,7,beacon?accent:heard?ok:warn,bg);
 String blocked=radar.wifi==Radar::WifiBusy?t("Wi-Fi busy: probe","Wi-Fi занят проверкой"):radar.wifi==Radar::WifiFailed?t("Wi-Fi error","Ошибка Wi-Fi"):(!beacon&&radar.wifi==Radar::WifiPortal)?t("Turn the access point off","Выключите точку доступа"):"";
 text(46,40,beacon?t("Beacon for another board","Маяк для другой платы"):t("Sensor","Приёмник"),ink,bold);
 text(46,54,fit(blocked.length()?blocked:beacon?String(radar.csiRate)+t(" frames/s on channel 1"," кадров/с на канале 1"):heard?t("beacon ","маяк ")+String(radar.csiRate)+t(" frames/s · "," кадров/с · ")+String(int(radar.csiRssi))+" dBm":t("waiting: set the other board to CSI beacon","ждём маяк: включите «Маяк CSI» на другой плате"),258,small),blocked.length()?warn:dim,small);
 String state;uint16_t color;
 if(blocked.length()){state=t("NO WI-FI","НЕТ WI-FI");color=warn;}
 else if(beacon){state=t("BEACON ON","МАЯК РАБОТАЕТ");color=accent;}
 else if(!heard){state=t("NO BEACON","НЕТ МАЯКА");color=warn;}
 else if(radar.csiStale){state=t("CSI FROZEN","CSI НЕ МЕНЯЕТСЯ");color=warn;}
 else if(radar.calibrateUntil){state=t("CALIBRATING ","КАЛИБРОВКА ")+String((radar.calibrateUntil-now+999)/1000)+t(" s"," с");color=info;}
 else if(radar.moving){state=t("MOTION","ДВИЖЕНИЕ");color=bad;}
 else{state=t("STILL","ТИХО");color=ok;}
 textCenter(160,92,state,color,big);
 if(!beacon){
  float threshold=radar.motionThreshold(),scale=max(threshold*2.5f,radar.activity*1.1f);
  d.fillRoundRect(14,104,292,8,3,line);if(heard)d.fillRoundRect(14,104,max(3,int(292*min(1.f,radar.activity/scale))),8,3,radar.moving?bad:ok);
  int tx=14+int(291*threshold/scale);d.fillRect(tx-1,100,3,16,ink);
  text(14,128,t("activity ","активность ")+String(radar.activity*1000,1),dim,small);
  textRight(306,128,t("threshold ","порог ")+String(threshold*1000,1)+(radar.baseline>0?String():t(" (default)"," (по умолч.)")),radar.baseline>0?dim:warn,small);
  // History: one point per 0.5 s, threshold dotted.
  panel(8,136,304,70,card,6);unsigned n=radar.motionCount();float top=threshold*2.5f;for(unsigned i=0;i<n;i++)top=max(top,radar.motion(i));
  int ty=202-int(62*threshold/top);for(int x=12;x<308;x+=6)d.drawFastHLine(x,ty,3,faint);
  int px=0,py=0;for(unsigned i=0;i<n;i++){int x=12+int(i*296/(Radar::MotionHistory-1)),y=202-int(62*radar.motion(i)/top);if(i)d.drawLine(px,py,x,y,radar.motion(i)>threshold?bad:accent);px=x;py=y;}
  if(!n)textCenter(160,175,t("Walk between the boards to see the trace","Пройдите между платами — появится след"),faint,small);
  text(12,216,fit(t("Calibrate with the area empty and still (10 s)","Калибровка: 10 с, в зоне никого и ничего не движется"),296,small),faint,small);
 }else text(12,130,fit(t("Place the boards 2-5 m apart; motion between them is sensed","Расставьте платы в 2-5 м; движение между ними будет видно"),296,small),dim,small);
 if(beacon)footer({{"<>",t("Signals","Сигналы")},{"B",t("Sensor","Приёмник")},{"BACK",t("Menu","Меню")}});
 else footer({{"OK",t("Calibrate","Калибровка")},{"<>",t("Signals","Сигналы")},{"B",t("Beacon","Маяк")},{"BACK",t("Menu","Меню")}});
}
const int settingsCount=7;
void drawSettings(){
 Icon icons[]={IcRadio,IcScreen,IcCompass,IcWifi,IcPulse,IcHelp,IcPin};uint16_t hues[]={accent,info,warn,info,ok,dim,ok};
 String names[]={t("Radio","Радио"),t("Screen & device","Экран и устройство"),t("GPS & compass","GPS и компас"),t("Connections","Подключения"),t("Module health","Состояние модулей"),t("Keys & help","Клавиши и подсказки"),t("Saved maps","Сохранённые карты")};
 String details[]={String(config.frequency,3)+t(" MHz · SF"," МГц · SF")+String(config.sf)+" · "+String(config.power)+" dBm",
  t("Brightness ","Яркость ")+String(config.brightness)+" · "+(config.russian?"Русский":"English"),
  "GPS "+flag(config.gps)+" · "+(navigation.calibrated?t("compass calibrated","компас откалиброван"):t("compass not calibrated","компас не откалиброван")),
  "Wi-Fi "+flag(portalActive())+" · BLE "+flag(bleActive()),"RX "+String(meshRadio.rxCount)+" · TX "+String(meshRadio.txCount)+" · "+String(meshRadio.relayed)+t(" relayed"," переслано"),
  t("What every key does","Что делает каждая клавиша"),maps.title.length()?maps.title+" · "+count(maps.tileCount,"tile","tiles","тайл","тайла","тайлов"):t("No maps","Карт нет")};
 int first=max(0,selected-5);for(int i=first;i<settingsCount&&i<first+6;i++){int y=24+(i-first)*32;bool focus=selected==i;listRow(y,30,focus);
  g().fillRoundRect(16,y+4,22,22,5,card);icon(icons[i],27,y+15,7,hues[i],card);text(46,y+14,names[i],ink,bold);text(46,y+26,fit(details[i],250,small),dim,small);tri(302,y+15,1,4,focus?accent:faint);}
 scrollbar(first,6,settingsCount,24,190);footer({{"OK",t("Open","Открыть")},{"^v",t("Select","Выбор")},{"BACK",t("Menu","Меню")}});
}
int settingRows(){return page==Radio?8:10;}
String settingName(int i){
 if(page==Radio){const char* en[]={"Frequency","Bandwidth","Spreading factor","Coding rate","TX power","Our relay limit","Relay packets"};const char* ru[]={"Частота","Полоса","Фактор SF","Кодирование CR","Мощность","Предел наших пересылок","Ретрансляция"};return i<7?(config.russian?ru[i]:en[i]):"";}
 const char* en[]={"Name","Language","Brightness","Sounds","Auto lock","Dim screen","GPS receiver","UTC offset","Battery shows"};const char* ru[]={"Имя","Язык","Яркость","Звуки","Автоблокировка","Гасить экран","Приёмник GPS","Смещение UTC","Батарея в строке"};return i<9?(config.russian?ru[i]:en[i]):"";
}
String settingValue(const Config& c,int i){
 if(page==Radio){switch(i){case 0:return String(c.frequency,3)+t(" MHz"," МГц");case 1:return String(c.bandwidth,1)+t(" kHz"," кГц");case 2:return "SF"+String(c.sf);case 3:return "4/"+String(c.cr);case 4:return String(c.power)+" dBm";case 5:return String(c.hops);case 6:return flag(c.relay);}return "";}
 switch(i){case 0:return c.name;case 1:return c.russian?"Русский":"English";case 2:return String(c.brightness);case 3:return flag(c.sound);case 4:return c.autoLock?String(c.autoLock)+t(" s"," с"):t("Off","Выкл");case 5:return c.dimAfter?String(c.dimAfter)+t(" s"," с"):t("Off","Выкл");case 6:return flag(c.gps);case 7:{int m=abs(c.utcOffset);char b[12];snprintf(b,sizeof b,"%c%02d:%02d",c.utcOffset<0?'-':'+',m/60,m%60);return b;}case 8:return c.batteryVolts?t("Volts","Вольты"):t("Percent","Проценты");}return "";
}
String settingHint(int i){
 if(page==Radio){switch(i){case 0:return t("1 kHz steps; must match every node","Шаг 1 кГц; должна совпадать у всех узлов");case 1:return t("62.5 / 125 / 250 / 500 kHz","62.5 / 125 / 250 / 500 кГц");case 2:return t("Higher SF: longer range, slower","Больше SF - дальше, но медленнее");case 3:return t("Error correction 4/5 to 4/8","Избыточность кода 4/5 ... 4/8");case 4:return t("Radio chip setting, 0-22 dBm","Настройка чипа, 0-22 dBm");case 5:return t("Only packets relayed by this device","Только пакеты, пересылаемые этим узлом");case 6:return t("Forward other nodes' MeshCore packets","Пересылать чужие пакеты MeshCore");}return t("Applied to the radio when saved","Применяется к радио при сохранении");}
 switch(i){case 0:return t("OK: edit, 1-24 UTF-8 bytes","OK: изменить, 1-24 байта UTF-8");case 1:return t("Menu language; typing layout is the @ key","Язык меню; раскладка ввода - клавиша @");case 2:return "10-255";case 3:return t("Tone for messages and ACKs","Сигнал о сообщениях и подтверждениях");case 4:return t("0 = off, 30-600 s","0 = выкл, 30-600 с");case 5:return t("0 = off, 10-600 s","0 = выкл, 10-600 с");case 6:return t("Receiver power","Питание приёмника");case 7:return t("Local time, 15 minute steps","Местное время, шаг 15 минут");case 8:return t("Status bar: charge estimate or measured voltage","Строка состояния: оценка заряда или измеренное напряжение");}return t("Stored in device memory","Сохраняется в памяти устройства");
}
bool draftChanged(){for(int i=0;i<settingRows()-1;i++)if(settingValue(draft,i)!=settingValue(config,i))return true;return false;}
void drawEditor(){
 int rows=settingRows(),first=max(0,selected-6);bool changed=draftChanged();
 for(int i=first;i<rows&&i<first+7;i++){
  int y=25+(i-first)*23;bool focus=selected==i;
  if(i==rows-1){panel(8,y,304,21,changed?(focus?accent:rgb(0x14524b)):card,6);if(focus&&!changed)ring(8,y,304,21,6,line);textCenter(160,y+15,changed?t("Save changes","Сохранить изменения"):t("No changes","Нет изменений"),changed?(focus?bg:ink):dim,bold);continue;}
  listRow(y,21,focus);String value=settingValue(draft,i);bool diff=value!=settingValue(config,i);
  if(diff)g().fillCircle(17,y+10,2,accent);text(24,y+15,settingName(i),focus?ink:dim);
  if(focus&&!(page==Display&&i==0)){int w=measure(value,bold);tri(300,y+10,1,4,accent);text(292-w,y+15,value,accent,bold);tri(284-w,y+10,3,4,accent);}
  else textRight(page==Display&&i==0&&focus?306:300,y+15,fit(value,150),diff?accent:ink,focus?bold:body);
 }
 scrollbar(first,7,rows,25,160);text(10,207,fit(settingHint(selected),300,small),faint,small);
 if(page==Display&&selected==0)footer({{"OK",t("Edit name","Изменить имя")},{"^v",t("Select","Выбор")},{"BACK",t("Cancel","Отмена")}});
 else footer({{"<>",t("Change","Изменить")},{"OK",selected==rows-1?t("Save","Сохранить"):t("Next value","Дальше")},{"BACK",t("Cancel","Отмена")}});
}
void toggle(int x,int y,bool on){panel(x,y,30,16,on?accent:line,8);g().fillCircle(on?x+22:x+8,y+8,6,on?bg:dim);}
void drawNetwork(){
 auto& d=g();bool wifi=portalActive(),ble=bleActive();int y=25;
 auto cardAt=[&](int i,int h,Icon ic,const String& name,const String& state,bool on){bool focus=selected==i;panel(8,y,304,h,focus?cardHi:card,8);if(focus)ring(8,y,304,h,8);d.fillRoundRect(16,y+7,24,24,6,bg);icon(ic,28,y+19,7,on?info:faint,bg);text(48,y+17,name,ink,bold);text(48,y+30,state,dim,small);};
 int wh=wifi?78:38;cardAt(0,wh,IcWifi,t("Wi-Fi access point","Точка доступа Wi-Fi"),wifi?t("Web chat and map upload","Веб-чат и загрузка карт"):t("Off","Выключена"),wifi);toggle(274,y+11,wifi);
 if(wifi){text(48,y+48,"MM-"+meshRadio.idText(meshRadio.nodeId).substring(6),accent,bold);text(48,y+63,t("Password: ","Пароль: ")+portalPassword(),ink);textRight(302,y+63,"192.168.4.1",dim,small);}
 y+=wh+6;int bh=ble?54:38;cardAt(1,bh,IcBle,"Bluetooth LE",ble?t("Secure pairing, MeshMesh service","Защищённое сопряжение, сервис MeshMesh"):t("Off","Выключен"),ble);toggle(274,y+11,ble);
 if(ble)text(48,y+47,"PIN "+String(blePin()),accent,bold);
 y+=bh+6;if(y+38<=216)cardAt(2,38,IcKey,t("MeshCore identity","Ключ MeshCore"),meshRadio.publicKeyText().substring(0,24)+"...",true);
 footer({{"OK",t("Toggle","Переключить")},{"^v",t("Select","Выбор")},{"BACK",t("Back","Назад")}});
}
String keyName(uint32_t k){
 switch(k){case 0:return "-";case KeyMsg:return "MSG";case KeyHome:return "HOME";case KeyAt:return t("@ (input language)","@ (язык ввода)");case KeyAdv:return "ADV";case 0x87:return t("hold ADV (GPS)","удерж. ADV (GPS)");case KeyMap:return "MAP";case KeyBack:return "BACK";case KeyMic:return "MIC";case KeySet:return "CTRL";case KeyHold:return t("hold OK","удерж. OK");case KeyLeft:return t("Left","Влево");case KeyUp:return t("Up","Вверх");case KeyDown:return t("Down","Вниз");case KeyRight:return t("Right","Вправо");case Enter:return "OK";case Erase:return "DEL";case ' ':return t("space","пробел");}
 char b[16];if(k>32&&k<127)snprintf(b,sizeof b,"'%c'",char(k));else snprintf(b,sizeof b,"0x%02X",unsigned(k));return b;
}
void drawDiagnostics(){
 const char* names[]={"LoRa",nullptr,"SD","LittleFS","RTC","GPS / NMEA",nullptr,"IMU"};String label[8];for(int i=0;i<8;i++)label[i]=names[i]?String(names[i]):i==1?t("Keyboard","Клавиатура"):t("Compass","Компас");
 bool state[8];moduleStates(state);
 for(int i=0;i<8;i++){int x=8+(i%2)*154,y=25+(i/2)*22;panel(x,y,150,19,card,5);g().fillCircle(x+11,y+9,4,state[i]?ok:bad);text(x+21,y+14,label[i],ink);textRight(x+144,y+14,state[i]?"OK":t("n/a","нет"),state[i]?ok:bad,small);}
 panel(8,116,304,74,card,8);uint32_t up=millis()/1000;char uptime[16];snprintf(uptime,sizeof uptime,"%lu:%02lu:%02lu",(unsigned long)(up/3600),(unsigned long)(up/60%60),(unsigned long)(up%60));
 String cells[][2]={{"RX",String(meshRadio.rxCount)},{"TX",String(meshRadio.txCount)},{t("Relayed","Переслано"),String(meshRadio.relayed)},{t("Rejected","Отклонено"),String(meshRadio.rejected)},
  {"RSSI",String(int(meshRadio.lastRssi))+" dBm"},{"SNR",String(meshRadio.lastSnr,1)+" dB"},{"RAM",String(ESP.getFreeHeap()/1024)+" K"},{t("Uptime","Работа"),uptime}};
 for(int i=0;i<8;i++){int x=16+(i%4)*75,y=134+(i/4)*32;text(x,y,cells[i][0],dim,small);text(x,y+15,cells[i][1],ink,bold);}
 text(10,207,t("Boot ","Загрузка ")+String(config.bootCounter)+t(" · last key: "," · последняя клавиша: ")+keyName(hardware.lastKey),faint,small);
 footer({{"OK",t("Crypto test + sound","Тест шифрования и звука")},{"BACK",t("Back","Назад")}});
}
void drawHelp(){
 Hint keys[]={{"MSG",t("Chats","Чаты")},{"MAP",t("Map; L - saved maps","Карта; L - список карт")},{"HOME",t("Menu","Главное меню")},{"BACK",t("Previous screen","Предыдущий экран")},{"CTRL",t("Settings","Настройки")},
  {"ADV",t("Announce this node; hold: GPS on/off","Объявить узел; удерж.: GPS вкл/выкл")},{"@",t("function key, not Sym+@: RU/EN in chat","функц. клавиша (не Sym+@): RU/EN в чате")},{"MIC",t("Lock screen (there is no microphone)","Блокировка (микрофона нет)")},
  {"OK",t("Open / send; hold: unlock","Открыть / отправить; удерж.: вход")},{config.russian?"2×пробел":"2x space",t("in text: switch RU/EN; Right: capital","в тексте: RU/EN; -> : заглавная")}};
 for(int i=0;i<10;i++){int y=24+i*19;int kw=max(30,measure(keys[i].key,small)+10);g().fillRoundRect(10,y,kw,15,3,line);textCenter(10+kw/2,y+11,keys[i].key,ink,small);text(max(50,kw+16),y+12,fit(keys[i].action,258),i%2?dim:ink);}
 footer({{"OK",t("Russian layout","Русская раскладка")},{"BACK",t("Settings","Настройки")}});
}
void drawLayoutHelp();
void drawLocked(){
 textCenter(160,106,clockText(),ink,digits);textCenter(160,130,dateText(),dim);
 unsigned n=unreadTotal();if(n){const ChatMessage* last=nullptr;for(int i=meshRadio.historyCount-1;i>=0;i--)if(!meshRadio.history[i].outgoing){last=&meshRadio.history[i];break;}
  panel(40,144,240,40,card,8);icon(IcMail,62,164,9,warn,card);text(80,160,count(n,"new message","new messages","новое сообщение","новых сообщения","новых сообщений"),ink,bold);if(last)text(80,176,fit(t("Last from ","Последнее от ")+String(last->name),190,small),dim,small);}
 else textCenter(160,166,meshRadio.ready?t("Radio and GPS keep working","Радио и GPS продолжают работать"):t("Radio error","Ошибка радио"),meshRadio.ready?faint:bad,small);
 icon(IcLock,160,200,7,dim);footer({{"OK",t("Hold to unlock","Удерживайте для входа")},{"MIC",t("Lock","Блок")}});
}
void drawEditing(){
 uint16_t* px=g().getBuffer();for(int i=0;i<320*240;i++)px[i]=(px[i]>>1)&0x7bef; // dim the page under the dialog
 bool key=page==Network;panel(12,60,296,122,cardHi,10);g().drawRoundRect(12,60,296,122,10,line);
 text(26,82,key?t("Network key · 64 hex","Ключ сети · 64 hex"):t("Device name","Имя устройства"),ink,bold);
 panel(24,92,272,52,bg,6);g().drawRoundRect(24,92,272,52,6,accent);String rows[3];unsigned n=wrap(edit,rows,3,258);int cx=32;for(unsigned i=0;i<n;i++)cx=text(32,108+i*15,rows[i],ink);g().fillRect(cx+1,98+max(0,int(n)-1)*15,2,12,accent);
 textRight(294,160,String(edit.length())+"/"+String(key?64:24)+t(" bytes"," байт"),faint,small);if(!key)text(26,160,keyboardRussian?"RU":"EN",accent,small);
 text(26,175,t("OK: save   BACK: cancel","OK: сохранить   BACK: отменить"),dim,small);
}
#include "UiSolitaire.inc"
void draw(){
 auto& c=g();c.fillScreen(bg);
 if(locked)drawLocked();
 else switch(page){
 case Home:drawHome();break;case Threads:drawThreads();break;case Chat:drawChat();break;case Map:drawMap();break;case Library:drawLibrary();break;
 case Nodes:drawNodes();break;case Node:drawNode();break;case Sensors:drawSensors();break;case Settings:drawSettings();break;
 case Radio:case Display:drawEditor();break;case Network:drawNetwork();break;case Diagnostics:drawDiagnostics();break;case Help:drawHelp();break;
 case Scope:drawScope();break;case Homing:drawHoming();break;case Game:drawGame();break;case Motion:drawMotion();break;
 }
 statusBar();if(editing&&!locked)drawEditing();if(layoutHelp&&!locked)drawLayoutHelp();drawToast();hardware.flush();
}
// Russian input for Latin keycaps: phonetic, as in WadaMesh (privet -> привет). The seven letters
// without a Latin sound-alike are on 1-7 (ч щ ъ ь э ю ё). M9 Sym symbols such as @ # $ are separate
// keys, not shifted digits, so they always stay symbols; Right after a letter changes its case.
const char* const ruLower[]={"а","б","ц","д","е","ф","г","х","и","й","к","л","м","н","о","п","я","р","с","т","у","в","ш","ж","ы","з"};
const char* const ruUpper[]={"А","Б","Ц","Д","Е","Ф","Г","Х","И","Й","К","Л","М","Н","О","П","Я","Р","С","Т","У","В","Ш","Ж","Ы","З"};
const char* const ruExtra[]={"ч","щ","ъ","ь","э","ю","ё"};
String keyboard(int key){
 if(!keyboardRussian||key<=0)return String(char(key));
 if(key>='a'&&key<='z')return ruLower[key-'a'];if(key>='A'&&key<='Z')return ruUpper[key-'A'];if(key>='1'&&key<='7')return ruExtra[key-'1'];
 return String(char(key));
}
// Right after a letter toggles its case: capitals for letters typed on digit keys.
bool toggleLastCase(String& text){
 if(!text.length())return false;unsigned at=meshmesh::previousCharacter(text.c_str(),text.length()),i=at;uint32_t cp=nextCp(text,i),to=cp;
 if(cp>='a'&&cp<='z')to=cp-32;else if(cp>='A'&&cp<='Z')to=cp+32;else if(cp>=0x430&&cp<=0x44f)to=cp-0x20;else if(cp>=0x410&&cp<=0x42f)to=cp+0x20;else if(cp==0x451)to=0x401;else if(cp==0x401)to=0x451;
 if(to==cp)return false;char b[3]={};if(to<0x80)b[0]=to;else{b[0]=0xc0|to>>6;b[1]=0x80|(to&0x3f);}text.remove(at);text+=b;return true;
}
void toggleLanguage(){keyboardRussian=!keyboardRussian;Preferences p;if(p.begin("meshmesh-ui",false)){p.putBool("kb_ru",keyboardRussian);p.end();}notice(keyboardRussian?t("Input: Russian (phonetic)","Ввод: русский (фонетический)"):t("Input: English","Ввод: английский"));}
// Two spaces within 300 ms switch the input language, as on WadaMesh; the first space is removed.
bool spaceSwitch(String& text,int key){
 if(key!=' '){lastSpace=0;return false;}
 if(lastSpace&&millis()-lastSpace<300&&text.endsWith(" ")){text.remove(text.length()-1);lastSpace=0;toggleLanguage();return true;}
 lastSpace=millis();return false;
}
void drawLayoutHelp(){
 uint16_t* px=g().getBuffer();for(int i=0;i<320*240;i++)px[i]=(px[i]>>1)&0x7bef;
 panel(6,24,308,192,cardHi,10);g().drawRoundRect(6,24,308,192,10,line);text(16,42,t("Russian input: phonetic","Русский ввод: фонетический"),ink,bold);
 const char* rows[]={"qwertyuiop","asdfghjkl","zxcvbnm","1234567"};
 for(int r=0;r<4;r++){int n=strlen(rows[r]),x0=160-n*29/2,y=50+r*36;for(int i=0;i<n;i++){char k=rows[r][i];int x=x0+i*29;panel(x,y,27,33,r==3?rgb(0x14524b):card,4);textCenter(x+13,y+11,String(k),dim,small);textCenter(x+13,y+28,k>='a'?ruLower[k-'a']:ruExtra[k-'1'],ink,bold);}}
 text(16,208,t("2x space: RU/EN   Shift or Right after a letter: capital","2×пробел: RU/EN   Shift или -> после буквы: заглавная"),dim,small);
}

void alter(int dir){if(page==Radio){switch(selected){case 0:draft.frequency=constrain(roundf((draft.frequency+dir*.001f)*1000)/1000,863.f,870.f);break;case 1:{float bw[]={62.5,125,250,500};int i=0;while(i<3&&draft.bandwidth!=bw[i])i++;draft.bandwidth=bw[(i+dir+4)%4];break;}case 2:draft.sf=constrain(int(draft.sf)+dir,7,12);break;case 3:draft.cr=constrain(int(draft.cr)+dir,5,8);break;case 4:draft.power=constrain(int(draft.power)+dir,0,22);break;case 5:draft.hops=constrain(int(draft.hops)+dir,0,7);break;case 6:draft.relay=!draft.relay;break;}}else switch(selected){case 1:draft.russian=!draft.russian;break;case 2:draft.brightness=constrain(int(draft.brightness)+dir*15,10,255);break;case 3:draft.sound=!draft.sound;break;case 4:draft.autoLock=constrain(int(draft.autoLock)+dir*30,0,600);break;case 5:draft.dimAfter=constrain(int(draft.dimAfter)+dir*10,0,600);break;case 6:draft.gps=!draft.gps;break;case 7:draft.utcOffset=constrain(int(draft.utcOffset)+dir*15,-720,840);break;case 8:draft.batteryVolts=!draft.batteryVolts;break;}dirty=true;}
void saveDraft(){StaticJsonDocument<768>d;deserializeJson(d,configJson());if(page==Radio){d["frequency"]=draft.frequency;d["bandwidth"]=draft.bandwidth;d["sf"]=int(draft.sf);d["cr"]=int(draft.cr);d["power"]=int(draft.power);d["hops"]=int(draft.hops);d["relay"]=draft.relay;}else{d["name"]=draft.name;d["russian"]=draft.russian;d["brightness"]=int(draft.brightness);d["sound"]=draft.sound;d["gps"]=draft.gps;d["auto_lock"]=int(draft.autoLock);d["dim_after"]=int(draft.dimAfter);d["utc_offset"]=int(draft.utcOffset);d["battery_volts"]=draft.batteryVolts;}String reply=applySettings(d.as<JsonObjectConst>());bool saved=reply.startsWith("OK");notice(saved?t("Settings saved","Настройки сохранены"):reply,saved?ok:bad);if(saved)change(Settings);}
void runNodeAction(){
 Peer* p=focusedPeer();if(!p)return;NodeAction acts[4];unsigned n=nodeActions(*p,acts);NodeAction a=acts[constrain(action,0,int(n)-1)];
 if(a!=ActForget)deleteArmed=false;
 switch(a){
 case ActWrite:recipient=p->id;chatReturn=Node;composer="";change(Chat);break;
 case ActMap:maps.follow=false;maps.center(p->latitude,p->longitude);change(Map);break;
 case ActResetPath:{if(meshRadio.busy()){notice(t("Radio busy, try again","Радио занято, повторите"),warn);break;}bool reset=meshRadio.resetPath(p->id);notice(reset?t("Path reset: next message floods","Путь сброшен: следующее сообщение пойдёт flood"):t("Path reset failed","Не удалось сбросить путь"),reset?ok:bad);break;}
 case ActForget:
  if(!deleteArmed){deleteArmed=true;notice(t("OK again removes the contact","Нажмите OK ещё раз для удаления"),warn);break;}
  deleteArmed=false;if(meshRadio.removeContact(p->id)){notice(t("Contact removed; its next advert adds it again","Контакт удалён; новое объявление вернёт его"),ok);change(Nodes);}else notice(t("Cannot remove while sending","Нельзя удалить во время отправки"),bad);break;
 }
}
}
bool uiRadarPage(){return page==Scope||page==Homing||page==Motion;}
void uiBegin(){Preferences p;keyboardRussian=config.russian;if(p.begin("meshmesh-ui",true)){keyboardRussian=p.getBool("kb_ru",config.russian);p.end();}lastInput=millis();draw();}
String uiStatus(){StaticJsonDocument<1024>d;d["page"]=pageNames[page];d["locked"]=locked;d["selected"]=selected;d["recipient"]=recipient==meshmesh::Broadcast?"ALL":meshRadio.idText(recipient);d["composer"]=composer;d["composer_bytes"]=composer.length();d["keyboard_language"]=keyboardRussian?"RU":"EN";d["editing"]=editing;d["chat_offset"]=chatOffset;d["idle_seconds"]=(millis()-lastInput)/1000;d["layout_help"]=layoutHelp;if(page==Game)gameStatus(d);if((page==Nodes||page==Node)&&focusNode)d["selected_node"]=meshRadio.idText(focusNode);if(page==Node)d["action"]=action;if(page==Scope||page==Homing||page==Motion){d["csi_role"]=radar.csi;d["radar_targets"]=radar.count;d["radar_selected"]=scopeSelected();d["radar_sound"]=radarSound;}String s;serializeJson(d,s);return s;}
void uiKey(int key){bool asleep=wakeOnly;lastInput=millis();hardware.brightness(config.brightness);wakeOnly=false;dirty=true;if(locked){if(key==KeyHold)locked=false;return;}if(asleep)return;
 if(key==KeyMic){locked=true;return;}
 if(layoutHelp){layoutHelp=false;return;}
 if(key==KeyAt&&(page==Chat||(page==Display&&editing))){toggleLanguage();return;}
 if(editing){if(key==KeyBack){editing=false;return;}if(key==Erase){edit.remove(meshmesh::previousCharacter(edit.c_str(),edit.length()));return;}if(key==KeyRight&&page!=Network){toggleLastCase(edit);return;}if(key==Enter){if(page==Network){StaticJsonDocument<128>d;d["key"]=edit;String r=applySettings(d.as<JsonObjectConst>());if(r.startsWith("OK"))editing=false;notice(r);}else if(edit.length()&&edit.length()<=24&&meshmesh::validUtf8((const uint8_t*)edit.c_str(),edit.length())){strlcpy(draft.name,edit.c_str(),sizeof draft.name);editing=false;}else notice(t("Name: 1-24 UTF-8 bytes","Имя: 1-24 байта UTF-8"),bad);return;}if(key>=32&&key<127){if(page!=Network&&spaceSwitch(edit,key))return;String ch=page==Network?String(char(key)):keyboard(key);if(edit.length()+ch.length()<=(page==Network?64u:24u))edit+=ch;}return;}
 if(page==Node&&key!=Enter&&key!=KeyHold)deleteArmed=false;
 if(key==KeyHome){change(Home);return;}if(key==KeyMsg){change(Threads);return;}if(key==KeyMap){change(Map);return;}if(key==KeySet){change(Settings);return;}if(key==KeyAdv){bool sent=meshRadio.sendHello();notice(sent?t("Node announced","Узел объявлен"):t("Announcement failed","Не удалось объявить узел"),sent?ok:bad);return;}
 if(key==KeyGps){config.gps=!config.gps;config.save();hardware.setGps(config.gps);notice("GPS: "+flag(config.gps),config.gps?ok:dim);return;}
 if(page==Game&&gameKey(key))return; // the game handles BACK, arrows, OK and letters itself
 if(key==KeyBack){change(page==Library?Map:page==Homing?Scope:page==Chat?chatReturn:page==Node?Nodes:(page==Radio||page==Display||page==Sensors||page==Diagnostics||page==Help)?Settings:Home);return;}
 if(key==KeyAt){change(Diagnostics);return;}
 if(page==Chat){if(key==Enter){if(meshRadio.sendMessage(composer,recipient)){composer="";chatOffset=0;notice(t("Message queued","Сообщение в очереди"));}else notice(t("Cannot send: empty, full queue or radio","Не отправлено: текст, очередь или радио"),bad);return;}if(key==Erase){composer.remove(meshmesh::previousCharacter(composer.c_str(),composer.length()));return;}if(key==KeyUp){unsigned total=0;for(unsigned i=0;i<meshRadio.historyCount;i++)if(belongs(meshRadio.history[i],recipient))total++;chatOffset=min(chatOffset+1,max(0,int(total)-1));return;}if(key==KeyDown){chatOffset=max(0,chatOffset-1);return;}if(key==KeyHold){layoutHelp=true;return;}if(key==KeyRight){toggleLastCase(composer);return;}if(key>=32&&key<127){if(spaceSwitch(composer,key))return;String ch=keyboard(key);if(composer.length()+ch.length()<=meshRadio.messageLimit(recipient))composer+=ch;else notice(t("UTF-8 byte limit: ","Лимит байт UTF-8: ")+String(meshRadio.messageLimit(recipient)),warn);}return;}
 if(page==Map){if(key=='l'||key=='L'){change(Library);return;}if(key==KeyLeft)maps.pan(-70,0);if(key==KeyRight)maps.pan(70,0);if(key==KeyUp)maps.pan(0,-70);if(key==KeyDown)maps.pan(0,70);if(key=='+'||key=='=')maps.changeZoom(1);if(key=='-'||key=='_')maps.changeZoom(-1);if(key==Enter){maps.follow=true;if(hardware.gpsFix())maps.center(hardware.gps.location.lat(),hardware.gps.location.lng());else notice(t("Waiting for GPS fix","Ожидание GPS-позиции"),warn);}return;}
 if((page==Nodes||page==Node)&&(key=='p'||key=='P')){if(Peer* p=focusedPeer()){if(p->position){maps.follow=false;maps.center(p->latitude,p->longitude);change(Map);}else notice(t("This node has not shared GPS","Узел пока не передал GPS"),warn);}return;}
 if(page==Home&&(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown)){int col=selected%tileColumns,row=selected/tileColumns,n=min(tileColumns,tileCount-row*tileColumns);if(key==KeyLeft)col=(col+n-1)%n;if(key==KeyRight)col=(col+1)%n;if(key==KeyUp)row=(row+tileRows-1)%tileRows;if(key==KeyDown)row=(row+1)%tileRows;selected=min(row*tileColumns+col,tileCount-1);return;}
 if(page==Node&&(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown)){Peer* p=focusedPeer();if(!p)return;NodeAction acts[4];int n=nodeActions(*p,acts);action=(action+(key==KeyLeft||key==KeyUp?-1:1)+n)%n;return;}
 if(page==Sensors&&(key==KeyLeft||key==KeyRight)){selected^=1;return;}
 if(page==Scope&&(key==KeyUp||key==KeyDown)){int i=scopeSelected();scopeManual=true;if(radar.count)scopeSelect((i+(key==KeyUp?-1:1)+radar.count)%radar.count);return;}
 if(page==Scope&&(key==KeyLeft||key==KeyRight)){change(Motion);return;}
 if(page==Motion&&(key==KeyLeft||key==KeyRight)){change(Scope);return;}
 if(page==Motion&&(key=='b'||key=='B')){csiBeaconRole=!csiBeaconRole;radar.setCsi(csiBeaconRole?Radar::CsiBeacon:Radar::CsiSensor);notice(csiBeaconRole?t("This board is now the beacon","Эта плата — маяк"):t("This board is now the sensor","Эта плата — приёмник"));return;}
 if(page==Homing&&(key==KeyLeft||key==KeyRight)){radar.resetPeak();notice(t("Peak reset","Пик сброшен"),dim);return;}
 if(key==KeyUp||key==KeyDown){if(page==Threads)threads();if(page==Nodes)sortNodes();int total=page==Threads?conversationCount:page==Nodes?nodeTotal:page==Settings?settingsCount:page==Radio||page==Display?settingRows():page==Network?3:page==Sensors?2:page==Library?int(library.size()):1;selected=total?(selected+(key==KeyUp?-1:1)+total)%total:0;if(page==Nodes&&nodeTotal)focusNode=meshRadio.peers[nodeOrder[selected]].id;return;}
 if((page==Radio||page==Display)&&(key==KeyLeft||key==KeyRight)){alter(key==KeyLeft?-1:1);return;}
 if(key!=Enter&&key!=KeyHold)return;
 if(page==Home){Page pages[]={Threads,Map,Nodes,Sensors,Network,Scope,Diagnostics,Settings,Game};change(pages[selected]);}
 else if(page==Library&&library.size()){if(maps.selectArea(library[selected]["id"].as<String>()))change(Map);else notice(t("Map unavailable","Карта недоступна"),bad);}
 else if(page==Threads){threads();recipient=conversations[selected];chatReturn=Threads;composer="";change(Chat);}
 else if(page==Nodes&&nodeTotal){focusNode=meshRadio.peers[nodeOrder[selected]].id;change(Node);}
 else if(page==Node)runNodeAction();
 else if(page==Settings){Page pages[]={Radio,Display,Sensors,Network,Diagnostics,Help,Library};change(pages[selected]);}
 else if(page==Radio||page==Display){if(selected==settingRows()-1){if(draftChanged())saveDraft();else notice(t("Nothing to save","Изменений нет"),dim);}else if(page==Display&&selected==0){editing=true;edit=draft.name;}else alter(1);}
 else if(page==Network){if(selected==0)portalToggle();if(selected==1)bleToggle();if(selected==2)notice(t("Public key: ","Открытый ключ: ")+meshRadio.publicKeyText().substring(0,16));}
 else if(page==Sensors){if(selected==0){bool sent=meshRadio.sendPosition();notice(sent?t("Position shared","Позиция передана"):t("Waiting for GPS fix","Ожидание GPS-позиции"),sent?ok:warn);}else if(navigation.calibrating){bool saved=navigation.finish();notice(saved?t("Calibration saved","Калибровка сохранена"):t("Rotate wider, at least 20 seconds","Вращайте шире, не менее 20 секунд"),saved?ok:warn);}else{navigation.start();notice(t("Rotate in all directions","Вращайте устройство во все стороны"));}}
 else if(page==Help)layoutHelp=true;
 else if(page==Scope){int i=scopeSelected();if(i>=0&&radar.track(i)){scopeManual=true;pingedSamples=radar.samples;change(Homing);}else notice(t("No signal selected","Сигнал не выбран"),warn);}
 else if(page==Motion){if(radar.csi==Radar::CsiSensor&&radar.beaconHeard()){radar.calibrate();notice(t("Calibrating: keep the area still for 10 s","Калибровка: 10 с без движения в зоне"),info);}else notice(t("Needs a heard beacon","Нужен услышанный маяк"),warn);}
 else if(page==Homing){radarSound=!radarSound;notice(radarSound?(config.sound?t("Ping on","Звук пеленга включён"):t("Device sound is off in settings","Звук устройства выключен в настройках")):t("Ping off","Звук пеленга выключен"),radarSound&&!config.sound?warn:dim);}
 else if(page==Diagnostics){hardware.beep();bool passed=meshRadio.selfTest();notice(passed?t("Encryption test passed","Проверка шифрования пройдена"):t("Encryption test failed","Ошибка шифрования"),passed?ok:bad);}}
String eventLabel(const String& value){if(!config.russian)return value;if(value.startsWith("New message from "))return "Сообщение от "+value.substring(17);if(value.startsWith("Delivered to "))return "Доставлено: "+value.substring(13);if(value=="Queued: waiting for delivery")return "Ожидание подтверждения";if(value=="Queued: broadcast")return "Сообщение в общем чате отправляется";if(value=="No delivery ACK")return "Получатель не подтвердил доставку";if(value.startsWith("Radio TX error")||value.startsWith("TX failed"))return "Ошибка передачи по радио";return value;}
void uiTick(){uint32_t now=millis();
 if(page==Game)gameTick(now);
 // Homing is used while walking without pressing keys: the screen stays on and unlocked.
 if((page==Homing||page==Motion)&&!locked)lastInput=now;
 // Motion start beeps once (at most every 5 s).
 if(page==Motion&&radar.moving&&!wasMoving&&now-motionBeepAt>5000){motionBeepAt=now;hardware.ping(1800,120);}
 wasMoving=page==Motion&&radar.moving;
 if(config.autoLock&&now-lastInput>=config.autoLock*1000UL&&!locked){locked=true;dirty=true;}if(config.dimAfter&&now-lastInput>=config.dimAfter*1000UL){hardware.brightness(0);wakeOnly=true;}
 // Ping: faster and higher as the signal strengthens (-85..-30 dBm, steeper when close); LoRa pings
 // once per new packet. A lost target gets a quiet low tick every 2 s instead of a stale ping.
 if(page==Homing&&!locked&&radarSound&&radar.tracking){float x=constrain((radar.fast+85)/55.f,0.f,1.f);
  if(!radarFresh()){if(radar.samples&&now-pingAt>=2000){pingAt=now;hardware.ping(300,30);}}
  else if(radar.focus.kind==RadarTarget::Lora){if(radar.samples!=pingedSamples){pingedSamples=radar.samples;hardware.ping(600+1000*x,60);}}
  else if(now-pingAt>=uint32_t(1200-1140*x*x)){pingAt=now;hardware.ping(600+1000*x,40);}}
 if(meshRadio.event!=eventSeen){eventSeen=meshRadio.event;bool incoming=eventSeen.startsWith("New message from ");if(page==Chat){markRead();notice(eventLabel(eventSeen),eventSeen.startsWith("Delivered")?ok:eventSeen.startsWith("No delivery")||eventSeen.startsWith("Radio TX")?bad:accent);}else if(incoming&&!locked&&page!=Threads)notice(eventLabel(eventSeen),accent);}
 if(toast.length()&&now-toastAt>=3500){toast="";dirty=true;}
 // The HUD clock, signal and battery change on every page.
 bool animate=!locked&&((page==Game&&gameAnimating())||page==Scope||page==Homing||page==Motion); // the sweep beam moves every frame
 bool update=dirty||meshRadio.dirty||maps.dirty||animate||now-lastDraw>=1000;if(update&&now-lastDraw>=150){draw();lastDraw=now;dirty=false;meshRadio.dirty=false;maps.dirty=false;radar.dirty=false;}}
