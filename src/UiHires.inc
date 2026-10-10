// Included by UiHeltec.cpp on the T114 (MM_HIRES): the one-button interface drawn at the TFT's own
// 240x135 in the style of the M9 (Ui.cpp): its colours (Palette.h) and icons (UiIcons.h), dark cards,
// the accent bar on the chosen row, avatars, a status bar with the clock and battery and key chips
// below. Pages, menus, keys and texts are those of the compact interface above; only the drawing differs.
// Chess fills the screen: the 128x128 board (drawHiresBoard) and a narrow panel with its own key chips.
namespace hi {
GFXcanvas8& g(){return hardware.canvas->screen;}
// Text roles as hiFont() draws them: tiny 7x13, small 9x15, body 10x20, bold, big inr24. y is the baseline.
const uint8_t* const tiny=u8g2_font_4x6_t_cyrillic;
const uint8_t* const big=u8g2_font_10x20_t_cyrillic;
constexpr int Foot=117; // footer line; the status bar ends at y 20
int width(const String& s,const uint8_t* f=body){String v=visualText(s);int w=0;for(unsigned i=0;i<v.length();)w+=hiGlyph(0,0,utf8Next(v,i),f,0,false);return w;}
int text(int x,int y,const String& s,uint16_t c=ColInk,const uint8_t* f=body){String v=visualText(s);for(unsigned i=0;i<v.length();)x+=hiGlyph(x,y,utf8Next(v,i),f,c,true);return x;}
void right(int x,int y,const String& s,uint16_t c=ColInk,const uint8_t* f=body){text(x-width(s,f),y,s,c,f);}
void center(int x,int y,const String& s,uint16_t c=ColInk,const uint8_t* f=body){text(x-width(s,f)/2,y,s,c,f);}
String fit(const String& s,int px,const uint8_t* f=body){
 if(width(s,f)<=px)return s;int room=px-width("..",f),w=0;unsigned i=0;
 while(i<s.length()){unsigned n=i;uint32_t cp=utf8Next(s,n);w+=hiGlyph(0,0,cp,f,0,false);if(w>room)break;i=n;}
 return s.substring(0,i)+"..";
}
// Rows of at most px pixels, broken at spaces; longer text pages every four seconds (as textLines).
void lines(const String& s,int x,int y,int px,int step,unsigned most,uint16_t c,const uint8_t* f=body){
 String rows[12];unsigned at=0,count=0;
 while(at<s.length()&&count<12){unsigned end=at,space=0;int w=0;
  while(end<s.length()&&s[end]!='\n'){unsigned next=end;uint32_t cp=utf8Next(s,next);w+=hiGlyph(0,0,cp,f,0,false);if(w>px&&end>at)break;end=next;if(cp==' ')space=end;}
  if(end<s.length()&&s[end]!='\n'&&space>at)end=space;rows[count++]=s.substring(at,end);at=end;while(at<s.length()&&(s[at]=='\n'||s[at]==' '))at++;}
 unsigned first=count>most?(millis()/4000)%(count-most+1):0;
 for(unsigned i=first;i<count&&i<first+most;i++)text(x,y+(i-first)*step,rows[i],c,f);
}
void panel(int x,int y,int w,int h,uint16_t c,int r=7){g().fillRoundRect(x,y,w,h,r,c);}
void row(int y,int h,bool focus){if(focus){panel(4,y,232,h,ColCardHi,6);g().fillRoundRect(4,y+5,3,h-10,1,ColAccent);}}
void icon(Icon id,int cx,int cy,int s,uint16_t c,uint16_t hole=ColBack){drawIcon(g(),id,cx,cy,s,c,hole,ColFaint);}
void tri(int x,int y,bool up,int s,uint16_t c){if(up)g().fillTriangle(x-s,y+s/2,x+s,y+s/2,x,y-s/2-1,c);else g().fillTriangle(x-s,y-s/2,x+s,y-s/2,x,y+s/2+1,c);}
void bars(int x,int y,unsigned level,uint16_t c){for(unsigned k=0;k<4;k++){int h=3+k*2;g().fillRect(x+k*4,y-h,3,h,k<level?c:ColLine);}}
unsigned snrLevel(float snr){return snr>=5?4:snr>=0?3:snr>=-5?2:snr>=-10?1:0;}
// Avatars as on the M9: the initial on a colour from the ID; channels, repeaters, rooms and sensors by icon.
String initial(const String& name){
 if(!name.length())return "?";unsigned i=0;uint32_t cp=utf8Next(name,i);
 if(cp>=0x430&&cp<=0x44f)cp-=0x20;else if(cp==0x451)cp=0x401;else if(cp<128)cp=toupper(cp);else if(cp>=0x800)return "?";
 char b[4]={};if(cp<0x80)b[0]=cp;else{b[0]=0xc0|cp>>6;b[1]=0x80|(cp&0x3f);}return b;
}
void avatar(int cx,int cy,int r,uint64_t id,const String& name,int type=-1){
 auto& d=g();
 if(channels::isChannel(id)){const channels::Channel* ch=meshRadio.channel(id);bool priv=ch&&!channels::isPublic(ch->secret)&&!channels::isHashtag(*ch);
  d.fillCircle(cx,cy,r,priv?ColLockBack:ColHashBack);icon(priv?IcLock:IcHash,cx,cy,r/2,priv?ColViolet:ColAccent,priv?ColLockBack:ColHashBack);return;}
 if(type>1){uint16_t c=type==2?ColViolet:type==3?ColWarn:ColInfo;d.fillCircle(cx,cy,r,ColCard);d.drawCircle(cx,cy,r,c);icon(type==2?IcTower:type==3?IcRoom:IcSensor,cx,cy,r/2+1,c,ColCard);return;}
 d.fillCircle(cx,cy,r,ColHue0+(id^id>>17^id>>41)%8);center(cx,cy+(r>=12?6:5),initial(name),ColInk,r>=12?bold:small);
}
// A compact-interface hint "key: action" as a key chip and the action; action replaces the hint's own.
// Hints without a colon are plain text. Returns the x after it.
int chip(int x,int y,const String& hint,int limit=236){
 int colon=hint.indexOf(':');
 if(colon<=0)return text(x,y,fit(hint,limit-x,small),ColDim,small)+10;
 String key=hint.substring(0,colon),act=hint.substring(colon+1);key.trim();act.trim();
 int kw=width(key,tiny)+8;g().fillRoundRect(x,y-11,kw,14,3,ColLine);text(x+4,y,key,ColInk,tiny);
 return text(x+kw+4,y+1,fit(act,limit-x-kw-4,small),ColDim,small)+10;
}
void footer(std::initializer_list<String> hints){
 auto& d=g();d.fillRect(0,Foot+1,240,134-Foot,ColBar);d.drawFastHLine(0,Foot,240,ColLine);int x=5;
 for(auto& h:hints)if(h.length()&&x<200)x=chip(x,131,h);
}
// Status bar as on the M9: title, then unread, Bluetooth, GPS, signal, clock and battery from the right.
// A long title first drops the battery percentage, then the Bluetooth icon.
void statusBar(const String& title,bool home=false){
 auto& d=g();d.fillRect(0,0,240,20,ColBar);d.drawFastHLine(0,20,240,ColLine);int x=236;
 unsigned mv=hardware.batteryMv,pct=mv<=3300?0:mv>=4200?100:(mv-3300)/9;bool external=mv>4250;
 uint16_t level=!mv?ColFaint:external?ColAccent:pct<=15?ColBad:pct<=30?ColWarn:ColGood;
 String p=mv&&(config.batteryVolts||!external)?(config.batteryVolts?String(mv/1000.f,2)+"V":String(pct)+"%"):String(),clock=clockText(time(nullptr)),n=unreadCount&&page!=Messages?String(unreadCount):String();
 bool ble=bleActive();int used=21+(p.length()?width(p,tiny)+5:0)+width(clock,small)+6+20+(config.gps?13:0)+(portalActive()?15:0)+(ble?11:0)+(n.length()?width(n,tiny)+18:0),need=width(title)+12;
 if(236-used<need&&p.length()){used-=width(p,tiny)+5;p="";}
 if(236-used<need&&ble)ble=false;
 d.drawRoundRect(x-17,5,17,10,2,ColDim);d.fillRect(x,8,2,4,ColDim);
 if(external){d.fillTriangle(x-7,6,x-12,11,x-8,11,ColAccent);d.fillTriangle(x-9,9,x-5,9,x-10,14,ColAccent);}else if(mv)d.fillRect(x-15,7,max(1u,13*pct/100),6,level);
 x-=21;if(p.length()){right(x,15,p,ColDim,tiny);x-=width(p,tiny)+5;}
 right(x,16,clock,ColInk,small);x-=width(clock,small)+6;
 bool fresh=meshRadio.lastRxAt&&millis()-meshRadio.lastRxAt<600000;bars(x-15,15,meshRadio.ready&&fresh?max(1u,snrLevel(meshRadio.lastSnr)):0,ColGood);
 if(!meshRadio.ready){d.drawLine(x-15,5,x-4,15,ColBad);d.drawLine(x-4,5,x-15,15,ColBad);}x-=20;
 if(config.gps){icon(IcPin,x-5,10,5,hardware.gpsFix()?ColGood:hardware.clockConflict?ColBad:ColWarn,ColBar);x-=13;}
 if(portalActive()){icon(IcWifi,x-5,11,6,ColAccent,ColBar);x-=15;}
 if(ble){icon(IcBle,x-3,10,5,ColInfo,ColBar);x-=11;}
 if(n.length()){right(x,15,n,ColWarn,tiny);x-=width(n,tiny)+2;icon(IcMail,x-6,10,6,ColWarn,ColBar);x-=16;}
 text(5,16,fit(title,x-10),home?ColAccent:ColInk); // regular: the doubled bold blurs Ш, Ж, М at this size
}
void toast(){
 if(!action.length()||millis()-actionAt>=3500)return;String s=fit(action,196,small);int w=width(s,small)+24,x=120-w/2;
 panel(x,86,w,25,ColCardHi,6);g().drawRoundRect(x,86,w,25,6,ColLine);g().fillRoundRect(x+5,91,3,15,1,ColAccent);text(x+14,104,s,ColInk,small);
}
// A menu over the page: three rows, the chosen one lit with the accent bar.
template<class F> void menuBox(unsigned n,int index,F name){
 int first=max(0,min(index-1,int(n)-3));panel(6,24,228,90,ColCardHi,8);g().drawRoundRect(6,24,228,90,8,ColLine);
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){int y=28+(i-first)*28;bool focus=int(i)==index;
  if(focus){panel(10,y,218,26,ColCard,6);g().fillRoundRect(10,y+6,3,14,1,ColAccent);}text(20,y+19,fit(name(i),200),focus?ColInk:ColDim);}
 if(n>3){int th=max(10,80*3/int(n)),ty=29+(80-th)*first/max(1,int(n)-3);g().fillRoundRect(230,29,2,80,1,ColCard);g().fillRoundRect(230,ty,2,th,1,ColFaint);}
}
void empty(Icon ic,const String& a,const String& b){icon(ic,120,46,12,ColFaint);center(120,84,fit(a,228),ColInk);center(120,103,fit(b,228,small),ColDim,small);}
String pageTitle(int p){
 switch(p){case Home:return "MeshMesh";
#if MM_PET
 case PetPage:return petTitle();
#endif
#if MM_DICE
 case DicePage:return diceTitle();
#endif
 case Messages:return t("Messages","Сообщения");case Nodes:return t("Nodes","Узлы");case Chess:return t("Chess","Шахматы");
 case PeoplePage:return t("People counter","Счётчик людей");case QuickPage:return t("Quick send","Быстрая отправка");case NotifyPage:return t("Notifications","Уведомления");case Signals:return radar.tracking?t("Homing","Пеленг"):t("Radar","Радар");case Gps:return "GPS";case Wifi:return "Wi-Fi";case Ble:return "Bluetooth";case Settings:return t("Settings","Настройки");}
 return t("Modules","Модули");
}
// The click hint names the next screen: "click: Nodes".
String clickHint(const String& action){String k=t("click: next","клик: далее");int c=k.indexOf(':');return (c>0?k.substring(0,c):k)+": "+action;}

// Home: the identity card and three tiles, as the M9's strip and grid.
void server();
void home(){
 if(config.role!=RoleNormal){server();return;}
 auto& d=g();panel(4,25,232,40,ColCard,8);icon(IcRadio,20,45,8,meshRadio.ready?ColAccent:ColBad,ColCard);
 text(36,41,fit(String(config.name),150,bold),ColInk,bold);
 text(36,58,fit(meshRadio.ready?String(config.frequency,3)+" SF"+String(config.sf)+" BW"+String(config.bandwidth,1):t("Radio error ","Ошибка радио ")+String(meshRadio.radioError),150,tiny),meshRadio.ready?ColDim:ColBad,tiny);
 String tx=String(meshRadio.txCount),rx=String(meshRadio.rxCount);
 right(230,41,tx,ColInk,small);tri(230-width(tx,small)-7,37,true,3,ColAccent);right(230,58,rx,ColInk,small);tri(230-width(rx,small)-7,54,false,3,ColGood);
 unsigned near=0,games=0,mine=chessNet.waiting();for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].heard&&millis()-meshRadio.peers[i].seen<1800000)near++;
 for(auto& m:chessNet.matches)if(m.state!=ChessMatch::Free)games++;
 struct {Icon ic;uint16_t hue;String name;unsigned n;bool news;} tiles[3]={
  {IcChat,ColAccent,t("Chats","Чаты"),unreadCount?unreadCount:meshRadio.historyCount,unreadCount>0},
  {IcMesh,ColViolet,t("Nodes","Узлы"),near,false},
#if MM_CHESS
  {IcChess,ColInk,t("Chess","Шахматы"),mine?mine:games,mine>0}};
#else
  {IcDice,ColWarn,t("Dice","Кости"),dicer.rolls,false}}; // chess left out of this image (Modules.h)
#endif
 for(int i=0;i<(MM_CHESS||MM_DICE?3:2);i++){int x=4+i*79,y=69;panel(x,y,74,45,ColCard,8);d.fillRoundRect(x+6,y+6,22,22,6,ColBack);icon(tiles[i].ic,x+17,y+17,6,tiles[i].hue);
  text(x+35,y+23,String(tiles[i].n),tiles[i].news?ColWarn:ColInk,bold);text(x+6,y+41,fit(tiles[i].name,64,tiny),ColDim,tiny);}
}
// Repeater and room server: role, radio, traffic and the passwords an owner needs for the MeshCore app.
void server(){
 ServerView v=meshServer.view();bool room=config.role==RoleRoom;
 panel(4,25,232,40,ColCard,8);icon(room?IcRoom:IcTower,20,45,8,room?ColWarn:ColViolet,ColCard);
 text(36,41,roleShort(config.role),ColInk,bold);
 text(36,58,meshRadio.ready?String(config.frequency,3)+t(" MHz"," МГц"):t("Radio error","Ошибка радио"),meshRadio.ready?ColDim:ColBad,tiny);
 text(6,82,fit(room?t("posts ","постов ")+String(v.posts)+t("  in ","  вошли ")+String(v.clients):"RX "+String(meshRadio.rxCount)+" TX "+String(meshRadio.txCount)+t(" fwd "," перес. ")+String(meshRadio.relayed),228,small),ColInk,small);
 text(6,97,fit(t("admin ","админ ")+v.password,228,small),ColDim,small);
 text(6,112,fit((room?t("room ","комната "):t("guest ","гость "))+(v.guest.length()?v.guest:t("none","нет"))+(v.forwarding?"":t("  no fwd","  без перес.")),228,small),ColDim,small);
}
// Messages: the sender with an avatar, then the text in a bubble (own ones in the dark accent).
void messages(){
 const ChatMessage* m=shownMessage();
 if(!m){empty(IcChat,t("No messages yet","Сообщений ещё нет"),t("They appear here","Здесь появятся входящие"));return;}
 bool channel=channels::isChannel(m->destination);String who=m->outgoing?t("You","Вы"):String(m->name);if(channel)who+=channelTag(m->destination);
 const char* states[]={"",tr("queued","очередь"),tr("sent","отправл."),tr("delivered","доставл."),tr("no ACK","нет ACK")};
 String route=meshRadio.routeText(*m,true),st=m->outgoing?String(states[m->status])+(route.length()?" "+route:String()):(route.length()?route+" ":String())+clockText(m->timestamp);
 avatar(17,38,12,channel?m->destination:m->outgoing?meshRadio.nodeId:m->source,m->outgoing?String(config.name):String(m->name));
 text(35,37,fit(who,196-width(st,tiny),bold),ColInk,bold);right(236,35,st,m->outgoing&&m->status==ChatMessage::Delivered?ColGood:ColDim,tiny);
 right(236,50,String(meshRadio.historyCount-messageOffset)+"/"+String(meshRadio.historyCount),ColFaint,tiny);
 panel(4,55,232,59,m->outgoing?ColOutBubble:ColInBubble,8);lines(messageText(*m),11,72,218,17,3,ColInk);
}
// Nodes: the card of one node, most recently heard first.
void nodes(){
 unsigned order[24];unsigned n=sortedNodes(order);Peer* p=shownNode();
 if(!p){empty(IcMesh,t("No nodes heard","Узлы пока не найдены"),t("hold: announce","держите: объявить"));return;}
 avatar(20,42,15,p->id,p->name,p->type);bars(218,37,p->heard?max(1u,snrLevel(p->snr)):0,ColGood);
 text(42,39,fit(p->name,168,bold),ColInk,bold);
 uint16_t kind=p->type==2?ColViolet:p->type==3?ColWarn:p->type==4?ColInfo:ColAccent;
 text(42,55,fit(typeText(p->type)+", "+pathText(*p),156,tiny),kind,tiny);right(236,55,String(nodeIndex%n+1)+"/"+String(n),ColFaint,tiny);
 panel(4,63,232,51,ColCard,8);
 text(12,82,p->heard?String(int(p->rssi))+" dBm  SNR "+String(p->snr,1):t("saved, not heard","сохранён, не слышен"),p->heard?ColInk:ColDim,small);
 if(p->heard)right(228,82,ago(millis()-p->seen),ColDim,small);
 float metres,bearing;String where;
 if(distanceTo(*p,metres,bearing)){const char* dirs[]={tr("N","С"),tr("NE","СВ"),tr("E","В"),tr("SE","ЮВ"),tr("S","Ю"),tr("SW","ЮЗ"),tr("W","З"),tr("NW","СЗ")};int k=int((bearing+22.5f)/45)%8;
  where=(metres<1000?String(int(metres))+t(" m "," м "):String(metres/1000,1)+t(" km "," км "))+dirs[k];}
 else if(p->position)where=t("has GPS position","есть GPS-позиция");
 String login=remoteLogin(*p),result=remoteResult(*p); // a server's or a trace's answer takes the second row
 if(login.length()||result.length()){text(12,103,fit(login.length()&&result.length()?login+" · "+result:login+result,212,small),ColAccent,small);return;}
 if(where.length()){icon(IcPin,16,97,5,ColGood,ColCard);text(28,103,where,ColDim,small);}
}
// Radar: signals with a colour per kind, the chosen one lit; homing with the level, trend and peak.
uint16_t signalColor(const RadarTarget& r){
 if(r.kind==RadarTarget::Lora)return ColViolet;if(r.kind==RadarTarget::Wifi)return ColInfo;
 return r.device==RadarTarget::Phone||r.device==RadarTarget::Watch||r.device==RadarTarget::Personal?ColPink:r.device==RadarTarget::Audio?ColWarn:ColDim;
}
void signals(String& title){
 auto& d=g();
 if(radar.csi==Radar::CsiSensor){title=t("Motion CSI","Движение CSI");bool heard=radar.beaconHeard();
  String state=radar.wifi!=Radar::WifiReady?t("no Wi-Fi: AP on?","нет Wi-Fi: точка?"):!heard?t("no beacon","нет маяка"):radar.csiStale?t("CSI frozen","CSI стоит"):radar.calibrateUntil?t("calibrating","калибровка"):radar.moving?t("MOTION","ДВИЖЕНИЕ"):t("still","тихо");
  text(6,58,state,radar.moving?ColWarn:ColInk,big);float threshold=radar.motionThreshold(),scale=max(threshold*2.5f,radar.activity*1.1f);
  panel(6,70,228,10,ColCard,4);if(heard)panel(6,70,max(6,int(228*min(1.f,radar.activity/scale))),10,radar.moving?ColWarn:ColAccent,4);d.fillRect(6+int(226*threshold/scale),66,2,18,ColDim);
  text(6,102,String(radar.activity*1000,1)+t(" / thr "," / порог ")+String(threshold*1000,1)+"  "+String(radar.csiRate)+t("/s","/с"),ColDim,small);return;}
 if(radar.tracking){const RadarTarget& f=radar.focus;bool fresh=homingFresh();int trend=radar.trend();title=t("Homing","Пеленг");
  d.fillCircle(10,35,4,signalColor(f));text(20,41,fit(signalName(f),214,bold),ColInk,bold);
  int shown=lroundf(radar.fast);int x=text(6,82,radar.samples?String(shown):String("--"),ColInk,big);text(x+4,82,"dBm",ColDim,small);
  bool lost=(f.kind==RadarTarget::Wifi&&radar.wifi!=Radar::WifiReady)||(f.kind==RadarTarget::Ble&&radar.ble!=Radar::BleReady);
  uint16_t tc=lost||!fresh?ColDim:trend>0?ColGood:trend<0?ColWarn:ColInk;
  right(236,64,lost?(f.kind==RadarTarget::Ble?bleState():wifiState()):!fresh?(radar.samples>1?t("lost","потерян"):t("no signal","нет сигнала")):trend>0?t("stronger","теплее"):trend<0?t("weaker","холоднее"):t("steady","ровно"),tc,bold);
  if(fresh&&!lost&&trend)tri(150,59,trend>0,5,tc);
  right(236,82,t("peak ","пик ")+(radar.samples?String(int(lroundf(radar.peak))):String("-")),ColDim,small);
  panel(6,91,228,9,ColCard,4);if(radar.samples){panel(6,91,max(6,int(228*signalLevel(shown))),9,ColAccent,4);d.fillRect(6+int(226*signalLevel(lroundf(radar.peak))),88,2,15,ColWarn);}
  text(6,113,f.kind==RadarTarget::Lora?String(radar.samples)+t(" pkts"," пак."):String(radar.rate)+t("/s","/с"),ColDim,tiny);return;}
 bool beacon=radar.csi==Radar::CsiBeacon;int sel=shownSignal();
 title=beacon?t("CSI beacon ","Маяк CSI ")+String(radar.csiRate)+t("/s","/с"):t("Radar","Радар")+
#if !defined(MM_NO_WIFI)
  " W"+String(radar.counted(RadarTarget::Wifi))+
#endif
  " B"+String(radar.counted(RadarTarget::Ble))+" L"+String(radar.counted(RadarTarget::Lora));
 String st=beacon?String():wifiState();if(!st.length()&&!beacon)st=bleState();
 if(sel<0){empty(IcRadar,beacon?t("Beacon on","Маяк включён"):t("No signals yet","Сигналов пока нет"),st.length()?st:beacon?t("hold: switch off","держите: выключить"):
#if defined(MM_NO_WIFI)
  t("BLE and LoRa","BLE и LoRa")
#else
  t("Wi-Fi and LoRa","Wi-Fi и LoRa")
#endif
  );return;}
 unsigned rows=st.length()?3:4;int first=max(0,min(sel-1,int(radar.count)-int(rows)));
 for(int i=first;i<int(radar.count)&&i<first+int(rows);i++){const RadarTarget& r=radar.targets[i];int y=23+(i-first)*24;bool focus=i==sel;row(y,22,focus);
  d.fillCircle(17,y+11,3,signalColor(r));text(27,y+16,fit(signalName(r),166,small),focus?ColInk:ColDim,small);right(231,y+17,String(int(r.rssi)),focus?ColInk:ColDim,bold);}
 if(st.length())text(8,112,fit(st,224,tiny),ColWarn,tiny);
}
#if MM_PET
// The pet: the creature on its card in colour, its words in a bubble, the food, joy and health bars.
uint8_t petInk(uint8_t ink,pet::Mood m){
 bool dead=m==pet::Gone;
 switch(ink){case pet::InkOutline:return ColBar;case pet::InkBody:return dead?ColPetStone:m==pet::Sick?ColPetSick:ColPetBody0+creature.s.species%pet::Species;
 case pet::InkLight:return dead?ColPetStoneLight:ColPetAccent0+creature.s.species%pet::Species;case pet::InkShell:return ColPetShell;case pet::InkEye:return ColPetEye;case pet::InkCheek:return ColPink;case pet::InkTear:return ColInfo;case pet::InkGlow:return ColTarget;}return ColCard;
}
void petArt(int x,int y,int scale,uint32_t now){
 uint8_t grid[pet::Size][pet::Size];creature.sprite(grid,now);pet::Mood m=creature.mood();
 y-=creature.bob(now)*2;x+=creature.sway(now)*2;
 for(unsigned r=0;r<pet::Size;r++)for(unsigned k=0;k<pet::Size;k++)if(grid[r][k])g().fillRect(x+k*scale,y+r*scale,scale,scale,petInk(grid[r][k],m));
}
void petBar(int y,Icon ic,uint16_t ink,uint16_t v){
 icon(ic,112,y+3,4,ink,ColBack);uint16_t c=v>=500?ColGood:v>=250?ColWarn:ColBad;
 panel(122,y,82,7,ColLine,3);if(v)panel(122,y,max(6,82*v/pet::Full),7,c,3);right(236,y+8,String((v+5)/10)+"%",v<250?ColBad:ColDim,tiny);
}
void petPage(){
 auto& d=g();uint32_t now=millis();const auto& s=creature.s;
 if(!creature.has()){empty(IcPaw,t("No pet","Нет питомца"),t("Optional","Заводить по желанию"));return;}
 panel(4,24,96,91,ColCard,8);d.fillRoundRect(16,104,72,5,2,ColBack);petArt(12,26,5,now);
 if(creature.mood()==pet::Sleepy)text(80,46-((now/700)%2)*4,"z",ColDim,small);
 uint32_t e=now-creature.effectAt;
 if(e<2500&&creature.effect==pet::Hearts)for(int i=0;i<2;i++){int hx=36+i*30,hy=max(30,52-int(e/150)-i*6);d.fillCircle(hx-2,hy,3,ColPink);d.fillCircle(hx+2,hy,3,ColPink);d.fillTriangle(hx-5,hy+1,hx+5,hy+1,hx,hy+6,ColPink);}
 if(e<2500&&creature.effect==pet::Sparkle)for(int i=0;i<3;i++){int sx=22+i*28,sy=40+(i%2)*12;d.drawFastHLine(sx-3,sy,7,ColTarget);d.drawFastVLine(sx,sy-3,7,ColTarget);}
 if(e<2500&&creature.effect==pet::Medicine){int cy=44-int(e/120);d.fillRect(46,cy-2,13,5,ColGood);d.fillRect(50,cy-6,5,13,ColGood);}
 if(creature.talking(now)){panel(106,24,130,48,ColInk,8);d.fillTriangle(106,52,106,62,98,58,ColInk);lines(creature.speech,112,42,118,16,2,ColBack,small);}
 else{text(106,41,fit(s.name,128,bold),ColInk,bold);
  String sub=!creature.alive()?creature.causeText(s.cause):s.stage==pet::Egg?creature.stageName(pet::Egg):String(creature.kind().name)+" · "+creature.stageName(s.stage); // the level is in the title
  text(106,57,fit(sub,130,tiny),creature.alive()?ColDim:ColBad,tiny);
  pet::Mood m=creature.mood();if(creature.alive()&&s.stage!=pet::Egg)text(106,71,fit(creature.moodName(),130,small),m==pet::Happy?ColGood:m==pet::Hungry||m==pet::Lonely?ColWarn:m==pet::Sick?ColBad:ColDim,small);}
 if(s.stage==pet::Egg){text(106,80,fit(t("hatches in","до вылупления"),130,small),ColDim,small);text(106,98,creature.ageText(creature.hatchLeft()),ColInk,bold);
  panel(106,104,128,7,ColLine,3);panel(106,104,max(6,int(128*(900-creature.hatchLeft())/900)),7,ColAccent,3);return;}
 if(!creature.alive()){text(106,90,fit(t("lived ","прожил ")+creature.ageText(s.age),130,small),ColInk,small);
  text(106,108,fit(s.mortal?t("death: on","смерть: вкл."):t("death: off","смерть: выкл."),130,tiny),ColDim,tiny);return;}
 petBar(78,IcMail,ColWarn,s.food);petBar(91,IcPaw,ColPink,s.joy);petBar(104,IcPulse,ColGood,s.health);
}
#endif
#if MM_DICE
// Dice: the RPG card with the dice in the M9's shapes and the pool, the Warhammer grid, three counters a page.
void hiDie(int cx,int cy,uint8_t shape,unsigned v,bool negative,int s){
 unsigned top=shape<dice::D66?dice::faces(shape):0;bool high=top&&v==top,one=top>=4&&v==1;
 uint8_t fill=negative?ColDim:shape==dice::D66||shape==dice::DPercent?ColAccent:high?ColGood:one?ColBad:ColInk,face=one?ColInk:ColBack;
 drawDieShape(g(),shape,cx,cy,s,fill,ColDim,face,shape==dice::D6?v:0);
 if(shape==dice::D6&&v>=1&&v<=6)return;
 String l=shape==dice::DPercent?dice::valueText(dice::DPercent,v):String(v);center(cx,cy+(shape==dice::D4?s/2+4:5),l,face,l.length()>2?tiny:small);
}
void dicePage(){
 auto& d=g();const auto& s=dicer.s;
 if(s.mode==dice::Grid){
  for(unsigned i=0;i<s.gridSize;i++){int x=5+(i%10)*23,y=24+(i/10)*18;bool chosen=i<s.gridChosen,rolled=chosen&&dicer.gridRolled,pass=!s.thresholdOn||dicer.grid[i]>=s.threshold;
   d.fillRoundRect(x,y,21,16,4,rolled?(pass?ColDicePass:ColDiceFail):chosen?ColDiceChosen:ColCard);
   if(chosen)center(x+11,y+13,dice::valueText(s.gridType,dicer.grid[i]),ColInk,small);}
  if(s.gridSize<=30&&!s.gridChosen)center(120,104,fit(t("Choose dice: Dice +1","Выберите: Кости +1"),228,tiny),ColDim,tiny);
  return;}
 if(s.mode==dice::Tally){
  const auto& c=dicer.hero();if(!diceShownCounter()){empty(IcDice,t("No counters","Счётчиков нет"),t("Add one in the menu","Добавьте в меню"));return;}
  int first=diceCounterSel/3*3;
  for(int i=first;i<int(c.counters)&&i<first+3;i++){int x=4+(i-first)*79;const dice::Counter& k=c.counter[i];bool focus=i==diceCounterSel;uint8_t hue=ColDice0+k.color%dice::Colors;
   panel(x,24,74,90,focus?ColCardHi:ColCard,8);d.fillRoundRect(x,24,74,5,2,hue);if(focus){d.drawRoundRect(x,24,74,90,8,hue);d.drawRoundRect(x+1,25,72,88,7,hue);}
   text(x+6,44,fit(k.name,64,tiny),focus?ColInk:ColDim,tiny);String v=String(k.value);center(x+37,88,v,ColInk,width(v,big)<=68?big:bold);}
  return;}
 const dice::Result* r=dicer.last();panel(4,24,150,90,ColCard,8);
 if(!r){icon(IcDice,79,54,12,ColFaint,ColCard);center(79,96,fit(t("No rolls yet","Бросков ещё нет"),146,small),ColDim,small);}
 else{
  text(10,40,fit(r->label[0]?String(r->label)+" · "+r->formula:String(r->formula),138,tiny),r->label[0]?ColViolet:ColDim,tiny);
  auto label=[&](unsigned i){unsigned v=r->value[i];return r->kind[i]==dice::PartNumber?String(r->negative[i]?"-":"+")+String(v):r->kind[i]==dice::PartPercent?dice::valueText(dice::DPercent,v):String(v);};
  auto wide=[&](unsigned i){return r->kind[i]==dice::PartNumber?width(label(i),small)+8:r->shape[i]>=dice::D66?30:26;};
  int x=10;bool many=r->more;for(unsigned i=0;i<r->parts&&!many;i++){x+=wide(i)+2;many=x>150;}
  if(many)lines(dicer.resultText(*r,false),10,60,138,15,2,ColInk,small);
  else{x=10;for(unsigned i=0;i<r->parts;i++){int w=wide(i);
    if(r->kind[i]==dice::PartNumber){panel(x,49,w,18,ColDiceNumber,5);center(x+w/2,63,label(i),ColInfo,small);}
    else hiDie(x+w/2,58,r->shape[i],r->value[i],r->negative[i],11);
    x+=w+2;}}
  if(!r->special){String total=String(r->total);int tw=width(total,big);right(148,108,total,ColWarn,big);text(148-tw-16,103,"=",ColDim);
   if(r->parts>1&&!many)text(10,108,fit(dicer.resultText(*r,false),max(30,148-tw-32),tiny),ColDim,tiny);}
 }
 panel(158,24,78,34,ColCardHi,8);String pool=dicer.poolText();center(197,47,pool,ColInk,width(pool,bold)<=70?bold:small);
 if(dicer.resultCount>1)text(160,74,t("Before","Ранее"),ColFaint,tiny);
 int y=92;for(int i=int(dicer.resultCount)-2;i>=0&&y<=112;i--,y+=18){const dice::Result& p=dicer.results[i];
  String v=p.special?String():String(p.total);text(160,y,fit(p.label[0]?String(p.label):String(p.formula),74-width(v,small)-4,tiny),ColDim,tiny);right(236,y,v,ColInk,small);}
}
#endif
void gps(){
 bool fix=hardware.gpsFix();uint16_t c=!config.gps?ColFaint:fix?ColGood:hardware.clockConflict?ColBad:ColWarn;
 panel(4,25,232,40,ColCard,8);icon(IcPin,20,45,8,c,ColCard);
 text(36,42,fit(!config.gps?t("GPS off","GPS выключен"):fix?t("Position fix","Позиция есть"):hardware.clockConflict?t("Old GPS date","Старая дата GPS"):hardware.gps.passedChecksum()?t("Searching sky","Поиск спутников"):t("No data from GPS","Нет данных GPS"),196,bold),ColInk,bold);
 text(36,58,t("Satellites ","Спутники ")+String(hardware.gps.satellites.value())+"  NMEA "+String(hardware.gps.passedChecksum()),ColDim,tiny);
 if(fix)text(6,86,String(hardware.gps.location.lat(),5)+", "+String(hardware.gps.location.lng(),5),ColInk,small);
 text(6,106,fit(t("Clock ","Часы ")+clockText(time(nullptr))+" "+(hardware.clockSource=="unset"?t("not set","не задано"):hardware.clockSource),228,small),ColDim,small);
}
void wifi(){
 bool on=portalActive();panel(4,25,232,40,ColCard,8);icon(IcWifi,20,47,8,on?ColAccent:ColFaint,ColCard);
 if(on){text(36,42,"MM-"+meshRadio.idText(meshRadio.nodeId).substring(6),ColInk,bold);text(36,58,"192.168.4.1",ColDim,tiny);text(6,96,portalPassword(),ColAccent,big);}
 else{text(36,42,fit(t("Access point off","Точка доступа выкл."),196,bold),ColInk,bold);text(36,58,fit(t("Web chat and settings","Веб-чат и настройки"),196,tiny),ColDim,tiny);}
}
void ble(){
 bool on=bleActive();panel(4,25,232,40,ColCard,8);icon(IcBle,20,45,8,on?ColInfo:ColFaint,ColCard);
 {String n=on?bleName():String(t("Bluetooth off","Bluetooth выкл."));const uint8_t* f=width(n,bold)<=196?bold:small;text(36,42,fit(n,196,f),ColInk,f);} // the whole name, key digits too
 text(36,58,fit(t("Secure pairing","Защищённое сопряжение"),196,tiny),ColDim,tiny);
 if(on){text(8,100,"PIN",ColDim,small);text(48,104,String(blePin()),ColAccent,big);}
}
void settings(){
 struct {Icon ic;uint16_t c;String s;} rows[5]={
  {IcChat,ColAccent,t("Language: ","Язык: ")+langNames[config.lang<LangCount?config.lang:0]},
  {IcPulse,ColGood,t("Battery: ","Батарея: ")+(config.batteryVolts?t("volts","вольты"):t("percent","проценты"))},
  {IcScreen,ColInfo,t("Screen off: ","Гасить экран: ")+(config.dimAfter?String(config.dimAfter)+t(" s"," с"):t("never","никогда"))},
  {IcGear,ColDim,t("Contrast: ","Контраст: ")+String(config.brightness)},
  {IcRadio,ColViolet,String(config.frequency,3)+" SF"+String(config.sf)+" CR4/"+String(config.cr)+" "+String(config.power)+"dBm"}};
 for(int i=0;i<5;i++){int y=23+i*18;icon(rows[i].ic,14,y+8,5,rows[i].c);text(28,y+14,fit(rows[i].s,206,small),ColInk,small);}
}
void modules(){
 struct {const char* name;uint16_t c;} chips[3]={{"LoRa",meshRadio.ready?ColGood:ColBad},{"FS",hardware.fsOk?ColGood:ColBad},{"GPS",!config.gps?ColFaint:hardware.gps.passedChecksum()?ColGood:ColWarn}};
 for(int i=0;i<3;i++){int x=4+i*79;panel(x,25,74,24,ColCard,7);g().fillCircle(x+13,37,4,chips[i].c);text(x+24,42,chips[i].name,ColInk,small);}
 text(6,70,String(ESP.getFreeHeap()/1024)+"K RAM"+(ESP.getPsramSize()?"  "+String(ESP.getFreePsram()/1024)+"K PSRAM":String()),ColInk,small);
 text(6,88,fit(t("relayed ","переслано ")+String(meshRadio.relayed)+t("  rejected ","  откл. ")+String(meshRadio.rejected),228,small),ColDim,small);
 text(6,106,fit(t("up ","работа ")+ago(millis())+t("  boot ","  загр. ")+String(config.bootCounter),228,small),ColDim,small);
}
// New message and chess news over the page.
void popup(const ChatMessage& m){
 bool channel=channels::isChannel(m.destination);panel(4,25,232,89,ColCard,8);
 avatar(21,43,12,channel?m.destination:m.source,m.name);
 text(39,49,fit(String(m.name)+(channel?channelTag(m.destination):String()),190,bold),ColInk,bold);
 lines(messageText(m),10,72,220,17,3,ColInk);
}
void chessPopup(){
 panel(4,25,232,89,ColCard,8);g().drawRoundRect(4,25,232,89,8,ColLine);
 g().fillCircle(21,43,12,ColCardHi);icon(IcChess,21,43,7,ColBoardLight,ColCardHi);
 text(39,49,t("Chess","Шахматы"),ColInk,bold);lines(chessPopupText,10,72,220,17,3,ColInk);
}
void rolePage(){
 g().fillScreen(ColBack);statusBar(t("Device mode","Режим работы"));const Icon icons[]={IcChat,IcTower,IcRoom};
 for(int r=0;r<RoleCount&&r<3;r++){int y=25+r*29;bool focus=r==roleSel;row(y,27,focus);
  icon(icons[r],22,y+13,7,focus?ColAccent:ColDim,focus?ColCardHi:ColBack);text(40,y+19,roleShort(r),focus?ColInk:ColDim);
  if(r==config.role)right(228,y+18,t("now","сейчас"),ColGood,tiny);}
 uint32_t gone=millis()-rolePickAt;unsigned left=gone>=5000?0:(5000-gone+999)/1000;
 footer({rolePickBoot?t("as now in ","как сейчас через ")+String(left)+t("s","с"):t("click-next hold-choose","клик-далее держ-выбор")});
 toast();
}
#if MM_CHESS
// Chess: the game list with avatars; a game fills the screen.
void chessList(){
 ChessMatch* games[ChessNet::MaxMatches];unsigned n=chessGames(games);
 if(!n){icon(IcChess,120,42,10,ColFaint);center(120,76,t("No games yet","Партий пока нет"));
  center(120,94,fit(t("Node menu: Chess invite","Меню узла: вызвать"),228,small),ColDim,small);center(120,110,fit(t("or accept a challenge","или примите вызов"),228,small),ColDim,small);return;}
 chessSel%=n;int first=max(0,min(chessSel-1,int(n)-3));
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){const ChessMatch& m=*games[i];int y=23+(i-first)*31;bool focus=int(i)==chessSel;row(y,29,focus);
  avatar(22,y+14,10,m.peer,m.name);text(40,y+15,fit(m.name,150,bold),focus?ColInk:ColDim,bold);
  if(m.game.plies)right(230,y+14,String((m.game.plies+1)/2)+t(" mv"," ход"),ColDim,tiny);if(m.unseen)g().fillCircle(30,y+5,3,ColWarn); // news not opened yet
  uint16_t sc=m.state==ChessMatch::Playing&&m.myTurn()?ColAccent:m.state==ChessMatch::Invited||m.drawOffer==ChessMatch::OfferedToMe?ColWarn:ColDim;
  text(40,y+28,fit(chessStateText(m),190,tiny),sc,tiny);}
}
void chessBoard(const ChessMatch& m){
 auto& d=g();d.fillScreen(ColBack);
 chessSettle(m);bool picking=chessPicking(m),cursor=picking&&chessCursor>=0&&(chessHeld>=0||(millis()/400)%3);
 chess::Move list[chess::MaxMoves];unsigned n=chessHeld>=0&&m.myTurn()?m.game.pos.legal(list):0;
 drawHiresBoard(m,cursor,list,n);
 constexpr int X=134,W=104;
 text(X,16,fit(m.name,W,bold),ColInk,bold);
 bool white=m.mine==chess::White;d.fillCircle(X+4,28,4,white?ColInk:ColPieceDark);d.drawCircle(X+4,28,4,ColDim);
 text(X+12,33,fit(t("you: ","вы: ")+(white?t("white","белые"):t("black","чёрные")),W-12,tiny),ColDim,tiny);
 uint16_t sc=m.state==ChessMatch::Playing&&m.myTurn()?(m.game.pos.inCheck()?ColBad:ColAccent):m.state==ChessMatch::Invited||m.drawOffer==ChessMatch::OfferedToMe?ColWarn:ColDim;
 lines(chessStateText(m),X,51,W,15,picking?1:2,sc,small);
 if(picking){panel(X-3,58,W+5,22,ColCardHi,5);if(chessCursor<0)d.drawRoundRect(X-3,58,W+5,22,5,ColAccent);String choice=chessChoiceText(m);const uint8_t* f=width(choice,small)<=W-6?small:tiny;text(X+2,74,fit(choice,W-6,f),chessCursor<0?ColAccent:ColTarget,f);}
 else text(X,82,fit(chessLink(m),W,tiny),ColDim,tiny);
 if(m.game.plies){chess::Position before=m.game.at(m.game.plies-1);char san[16];before.san(m.game.moves[m.game.plies-1],san,sizeof san);
  text(X,97,fit(String((m.game.plies+1)/2)+(m.game.plies%2?". ":"... ")+chessLocalSan(san),W,small),ColDim,small);}
 if(picking)chip(X-2,113,t("click: next","клик: далее"),238);
 chip(X-2,131,!picking||chessCursor<0?(chessHeld<0?t("hold: menu","держ: меню"):t("hold: cancel","держ: отмена")):chessHeld<0?t("hold: pick","держ: взять"):t("hold: move","держ: сходить"),238);
 if(chessMenu){ChessAct acts[8];unsigned k=chessActions(m,acts);chessMenuIndex%=k;menuBox(k,chessMenuIndex,[&](unsigned i){return chessActName(acts[i]);});}
 toast();
}
#endif
// Radio settings: four rows, changed values in amber, the value being set in the accent colour.
void radioPage(){
 g().fillScreen(ColBack);statusBar(t("Radio","Радио")+(radioChanged()?" *":""));int first=max(0,min(radioRow-1,RadioRows-4));
 for(int r=first;r<first+4;r++){int y=23+(r-first)*23;bool focus=r==radioRow;row(y,22,focus);
  text(14,y+16,radioName(r),focus?ColInk:ColDim,small);if(r>=RowSave)continue;
  String v=radioValue(radioDraft,r);bool editing=focus&&radioEditing;uint16_t vc=editing?ColAccent:radioValue(config,r)!=v?ColWarn:ColInk;int x=228-width(v,small);right(228,y+16,v,vc,small);
  if(editing){if(r==RowFreq){unsigned a,b;radioDigitSpan(v,a,b);g().fillRect(x+width(v.substring(0,a),small),y+18,width(v.substring(a,b),small),2,ColAccent);}
   else g().drawRoundRect(x-4,y+2,width(v,small)+8,18,4,ColAccent);}}
 // "click-next hold-change" as two key chips
 String h=radioHint(),parts[2];int space=h.indexOf(' ');parts[0]=h.substring(0,space);parts[1]=h.substring(space+1);
 for(auto& s:parts){int dash=s.indexOf('-');if(dash>0)s=s.substring(0,dash)+": "+s.substring(dash+1);}footer({parts[0],parts[1]});
 toast();
}
void offPage(){g().fillScreen(ColBack);icon(IcPower,120,42,12,ColWarn);center(120,82,farewell?t("Device is off","Устройство выключено"):t("Turning off...","Выключение..."),ColInk,bold);center(120,106,t("To turn on:","Включить:")+" "+powerOnHint(),ColDim,small);}
void draw(){
 auto& d=g();
 if(powerOffPending()){offPage();hardware.flush();return;}
 if(rolePick){rolePage();hardware.flush();return;}
 if(radioEdit){radioPage();hardware.flush();return;}
 d.fillScreen(ColBack);
 const ChatMessage* last=&incomingPopup;
 if(popupAt&&millis()-popupAt<8000&&last&&!last->outgoing){statusBar(t("Messages","Сообщения"));popup(*last);footer({t("click: close","клик: закрыть")});hardware.flush();return;}
 popupAt=0;
 if(chessPopupAt&&millis()-chessPopupAt<8000){statusBar(t("Chess","Шахматы"));chessPopup();footer({config.role==RoleNormal?t("hold: open the game","держ: открыть партию"):String()});hardware.flush();return;}
 chessPopupAt=0;
#if MM_CHESS
 if(page==Chess&&chessOpen&&chessOpen->state!=ChessMatch::Free){chessBoard(*chessOpen);hardware.flush();return;}
 if(page==Chess)chessOpen=nullptr;
#endif
 String title=pageTitle(page);
 switch(page){
 case PeoplePage:{text(12,42,t("Manual: ","Вручную: ")+String(people::settings.manual),ColInk,bold);text(12,64,"BLE: "+String(people::count(false))+(people::saturated(false)?"+":""),ColAccent);text(12,84,"Wi-Fi: "+String(people::count(true))+(people::saturated(true)?"+":""),ColInfo);text(12,104,String(people::settings.window)+t("s window","с окно"),ColDim,small);break;}
 case QuickPage:{if(quickTargets){text(8,38,fit(t("Choose recipient","Выберите получателя"),224,bold),ColAccent,bold);unsigned n=quickTargetCount();int first=max(0,min(int(quickTargetIndex)-1,int(n)-3));for(unsigned i=first;i<n&&i<unsigned(first+3);i++){int y=44+(i-first)*23;row(y,22,i==quickTargetIndex);text(12,y+16,fit(quickName(quickTarget(i)),216,small),ColInk,small);}}
  else{bool focus=!quickIndex;panel(4,25,232,38,focus?ColCardHi:ColCard,6);g().drawRoundRect(4,25,232,38,6,focus?ColAccent:ColLine);text(12,40,fit(t("Choose recipient","Выберите получателя"),200,tiny),ColDim,tiny);text(12,57,fit(quickName(quickReply?quickReplyTo:quickSend::state.to),200,bold),ColAccent,bold);g().fillTriangle(221,40,221,48,227,44,ColAccent);
   int first=max(1,min(int(quickIndex)-1,int(quickSend::Count)));for(unsigned i=first;i<=quickSend::Count+1&&i<unsigned(first+2);i++){int y=67+(i-first)*23;row(y,22,i==quickIndex);text(12,y+16,fit(quickLabel(i),216,small),ColInk,small);}}break;}
 case NotifyPage:statusBar(t("Notifications","Уведомления"));lines(t("LED / screen / popup","LED / экран / окно"),10,60,220,18,2,ColInk);break;case Home:home();break;case Messages:messages();break;case Nodes:nodes();break;
#if MM_CHESS
 case Chess:chessList();break;
#endif
#if MM_PET
 case PetPage:petPage();break;
#endif
#if MM_DICE
 case DicePage:dicePage();break;
#endif
 case Signals:signals(title);break;
 case Gps:gps();break;case Wifi:wifi();break;case Ble:ble();break;case Settings:settings();break;default:modules();}
 statusBar(title,page==Home);
 if(menuOpen){Act acts[MenuMax];unsigned n=actions(acts);if(n){menuIndex%=n;menuBox(n,menuIndex,[&](unsigned i){return actName(acts[i]);});}footer({t("click-next hold-run","клик-далее держ-ОК")});}
#if MM_DICE
 else if(diceTapOn())footer({t("click: +1","клик: +1"),t("hold: -1","держ: -1"),t("5 s: exit","5 с: выход")});
 else if(page==DicePage&&dicer.s.mode!=dice::Tally)footer({t("click: roll","клик: бросок"),t("hold: menu","держ: меню")});
 #endif
 else if(page==PeoplePage)footer({t("click: +1","клик: +1"),t("hold: menu","держ: меню")});
 else if(page==QuickPage)footer({t("click: choose","клик: выбор"),quickTargets||!quickIndex?t("hold: ","держ: ")+t("select","выбрать"):quickIndex>quickSend::Count?t("hold: ","держ: ")+t("exit","выход"):t("hold: send","держ: слать")});
 else footer({clickHint(pageTitle(nextPage(page))),hint()});
 toast();hardware.flush();
}
}
