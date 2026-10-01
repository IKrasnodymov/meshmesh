#include "ChessNet.h"
#include "Config.h"
#include "MeshRadio.h"
#include "Hardware.h"
#include <ArduinoJson.h>
#include <time.h>
ChessNet chessNet;
using namespace chess;
namespace {
const char Tag[]="\xe2\x99\x9f"; // ♟
constexpr uint8_t Queued=ChatMessage::Queued,Delivered=ChatMessage::Delivered,Failed=ChatMessage::Failed;
String tr(const char* en,const char* ru){return config.russian?ru:en;}
uint32_t unixNow(){time_t t=time(nullptr);return t>1700000000?uint32_t(t):0;}
// Saved form: "MMC1", version, count, then per game a fixed header and its moves; CRC-32 at the end.
constexpr uint8_t SaveVersion=2;          // 2: the open-move flag; version 1 files are still read
constexpr size_t RecordHeader=8+2+25+7+8+1+80+2;
// Automatic resending: after 2, 5 and 10 minutes, then every 15; at once when the other player is
// heard again (at most every 2 min, so a one-way link does not fill the air); stops after 24 h without confirmation (R still resends).
constexpr uint32_t RetryDelays[]={120000,300000,600000,900000},RetryWindow=86400000,HeardGap=120000;
uint32_t retryDelay(uint8_t n){return RetryDelays[n<3?n:3];}
bool inFlight(uint8_t s){return s==ChatMessage::Queued||s==ChatMessage::Sent;}
uint32_t crc32(const uint8_t* p,size_t n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(int k=0;k<8;k++)c=c>>1^(0xedb88320&-(c&1));}return ~c;}
void put(uint8_t*& w,const void* v,size_t n){memcpy(w,v,n);w+=n;}
void get(const uint8_t*& r,void* v,size_t n){memcpy(v,r,n);r+=n;}
}
bool ChessMatch::pending() const{return moveOpen||(out[0]&&outStatus!=Delivered);}
bool ChessMatch::sending() const{return (moveOpen&&inFlight(moveStatus))||(out[0]&&inFlight(outStatus));}
uint8_t ChessMatch::link() const{
  if((moveOpen&&moveStatus==Failed)||(out[0]&&outStatus==Failed))return Failed;
  if(moveOpen)return moveStatus;return out[0]?outStatus:0;
}

void ChessNet::begin(){load();}
void ChessNet::tick(){
  uint32_t now=millis();
  for(auto& m:matches){
    if(m.state==ChessMatch::Free||!m.pending()||m.sending()||m.autoStopped)continue;
    if(!m.openSince)m.openSince=now;
    if(now-m.openSince>RetryWindow){m.autoStopped=true;m.retryAt=0;event=String(m.name)+tr(": no confirmation for 24 h, R resends",": нет подтверждения 24 ч, R — повторить");events++;eventMatch=&m;dirty=true;continue;}
    Peer* p=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==m.peer)p=&meshRadio.peers[i];
    bool heard=p&&p->heard&&int32_t(p->seen-m.triedAt)>0&&now-m.triedAt>=HeardGap; // they are on air again
    if(!m.retryAt&&!heard){m.retryAt=now+retryDelay(m.retries);dirty=true;continue;}
    if(heard||int32_t(now-m.retryAt)>=0)retry(m);
  }
  if(saveDue&&int32_t(now-saveAt)>=0)save();
}
int32_t ChessNet::retryIn(const ChessMatch& m) const{if(!m.pending()||m.sending()||m.autoStopped||!m.retryAt)return -1;int32_t left=int32_t(m.retryAt-millis());return left>0?(left+999)/1000:0;}
// One command per attempt: the open move first, then the last other command.
void ChessNet::retry(ChessMatch& m){
  m.retries=m.retries<255?m.retries+1:255;m.retryAt=0;
  if(m.moveOpen&&!inFlight(m.moveStatus)&&m.moveStatus!=Delivered)sendMove(m);
  else if(m.out[0]&&m.outStatus==Failed){String t=m.out;send(m,t);}
  dirty=true;
}
// Everything sent from here is confirmed: the next failure starts a new series of attempts.
void ChessNet::confirmed(ChessMatch& m){if(m.pending())return;m.retries=0;m.retryAt=0;m.openSince=0;m.autoStopped=false;}
ChessMatch* ChessNet::find(uint64_t peer,uint16_t id){for(auto& m:matches)if(m.state!=ChessMatch::Free&&m.peer==peer&&m.id==id)return &m;return nullptr;}
ChessMatch* ChessNet::find(uint16_t id){for(auto& m:matches)if(m.state!=ChessMatch::Free&&m.id==id)return &m;return nullptr;}
unsigned ChessNet::count() const{unsigned n=0;for(auto& m:matches)n+=m.state!=ChessMatch::Free;return n;}
unsigned ChessNet::waiting() const{unsigned n=0;for(auto& m:matches)n+=m.state==ChessMatch::Invited||m.myTurn()||m.unseen;return n;}
// A free slot, or the finished game that changed longest ago.
ChessMatch* ChessNet::slot(){
  ChessMatch* oldest=nullptr;
  for(auto& m:matches){if(m.state==ChessMatch::Free)return &m;if(m.state==ChessMatch::Over&&!m.unseen&&(!oldest||int32_t(m.changedAt-oldest->changedAt)<0))oldest=&m;}
  return oldest;
}
void ChessNet::changed(ChessMatch& m,bool now){m.changedAt=millis();m.updated=unixNow();dirty=true;if(now)save();else if(!saveDue){saveDue=true;saveAt=millis()+3000;}}
void ChessNet::news(ChessMatch& m,const String& text){event=text;events++;eventMatch=&m;m.unseen=true;dirty=true;}
void ChessNet::viewed(ChessMatch& m){if(m.unseen){m.unseen=false;changed(m,false);}}
bool ChessNet::send(ChessMatch& m,const String& text){
  strlcpy(m.out,text.c_str(),sizeof m.out);m.outId=meshRadio.sendGame(text,m.peer);m.outStatus=m.outId?Queued:Failed;m.triedAt=millis();dirty=true;return m.outId;
}
// "♟3F2A 5 g1f3 3. Nf3": ply and UCI for the program, the usual notation for a human reader.
String ChessNet::moveText(const ChessMatch& m) const{
  auto& g=m.game;Position before=g.at(g.plies-1);Move mv=g.moves[g.plies-1];char san[12],u[6],t[48];before.san(mv,san,sizeof san);uci(mv,u);
  snprintf(t,sizeof t,"%s%04X %u %s %u.%s %s",Tag,m.id,g.plies,u,before.fullmove,before.side==White?"":"..",san);return t;
}
// The move is also the last command when it was sent last; a resend does not overwrite a later
// command (a draw offer or resignation) that is still waiting for its own confirmation.
bool ChessNet::sendMove(ChessMatch& m){
  bool last=!m.out[0]||m.outId==m.moveId||m.outStatus==Delivered;String text=moveText(m);
  if(last)return send(m,text),m.moveId=m.outId,m.moveStatus=m.outStatus,m.moveId!=0;
  m.moveId=meshRadio.sendGame(text,m.peer);m.moveStatus=m.moveId?Queued:Failed;m.triedAt=millis();dirty=true;return m.moveId;
}
void ChessNet::delivery(uint32_t id,uint8_t status){
  for(auto& m:matches){
    if(m.state==ChessMatch::Free)continue;bool hit=false;
    if(m.moveOpen&&m.moveId==id){m.moveStatus=status;m.moveOpen=status!=Delivered;hit=true;}
    if(m.outId==id&&m.outStatus!=Delivered){m.outStatus=status;hit=true;}
    if(!hit)continue;
    if(status==Delivered){confirmed(m);if(m.pending())m.retryAt=millis();} // the next open command goes at once
    if(status==Failed&&!m.retries){event=String(m.name)+tr(": not confirmed, will resend",": не подтвердил, повторю автоматически");events++;eventMatch=&m;}
    changed(m,status==Delivered||status==Failed);return;
  }
}
// Their move answers my last move, and "yes" answers my challenge: those arrived even when the ACK was lost.
// Their move also answers my acceptance; their agreement answers my draw offer.
void ChessNet::received(ChessMatch& m,bool move){
  if(move){m.moveOpen=false;m.moveStatus=Delivered;}
  const char* verb=strchr(m.out,' ');
  if(verb&&m.outStatus!=Delivered){verb++;if(move?((*verb>='1'&&*verb<='9')||!strncmp(verb,"yes",3)):(!strncmp(verb,"new ",4)||!strncmp(verb,"draw?",5)))m.outStatus=Delivered;}
  confirmed(m);
}
void ChessNet::finish(ChessMatch& m,ChessMatch::Result result,ChessMatch::Reason reason){m.state=ChessMatch::Over;m.result=result;m.reason=reason;m.drawOffer=ChessMatch::NoOffer;}
void ChessNet::judge(ChessMatch& m){
  auto& g=m.game;if(g.outcome==Ongoing)return;
  static const ChessMatch::Reason reasons[]={ChessMatch::NoReason,ChessMatch::Mate,ChessMatch::Stalemate,ChessMatch::FiftyMoves,ChessMatch::Repetition,ChessMatch::DeadPosition,ChessMatch::TooLong};
  finish(m,g.outcome==Checkmate?(g.pos.side==White?ChessMatch::BlackWon:ChessMatch::WhiteWon):ChessMatch::Drawn,reasons[g.outcome]);
}

// Commands from the other player. Only this contact's own games are touched; anything out of turn,
// illegal or repeated is ignored (a repeat arrives when an ACK was lost and the sender tried again).
bool ChessNet::receive(uint64_t from,const char* name,const char* text){
  if(strncmp(text,Tag,3))return false;
  const char* c=text+3;uint16_t id=0;
  for(int i=0;i<4;i++){char h=c[i];int v=h>='0'&&h<='9'?h-'0':(h|32)>='a'&&(h|32)<='f'?(h|32)-'a'+10:-1;if(v<0)return false;id=id<<4|v;}
  if(c[4]!=' '||!id)return false;
  char a[12]={},b[8]={};if(sscanf(c+5,"%11s %7s",a,b)<1)return false;
  ChessMatch* m=find(from,id);
  if(!strcmp(a,"new")){
    if(b[0]!='w'&&b[0]!='b')return false;if(m)return true;
    m=slot();
    if(!m){meshRadio.sendGame(String(Tag)+String(c).substring(0,4)+" no",from);event=String(name)+tr(": challenge refused, no free board",": вызов отклонён, нет свободной доски");events++;dirty=true;return true;}
    *m=ChessMatch();m->peer=from;m->id=id;strlcpy(m->name,name,sizeof m->name);m->mine=b[0]=='w'?Black:White;m->state=ChessMatch::Invited;m->started=unixNow();m->game.reset();
    news(*m,String(name)+tr(" challenges you to chess"," вызывает на партию в шахматы"));hardware.ping(1320,120);changed(*m);return true;
  }
  if(!m){event=tr("Chess: move for an unknown game","Шахматы: ход в неизвестной партии");events++;dirty=true;return true;}
  strlcpy(m->name,name,sizeof m->name);auto& g=m->game;uint8_t theirs=m->mine^1;
  if(a[0]>='1'&&a[0]<='9'){
    unsigned ply=strtoul(a,nullptr,10);
    // A move for an invitation where they play White means they accepted it.
    if(m->state==ChessMatch::Inviting&&m->mine==Black&&ply==1)m->state=ChessMatch::Playing;
    if(m->state!=ChessMatch::Playing)return true;
    Move mv;
    if(ply&&ply==g.plies&&((ply-1)&1)==theirs){char was[6];uci(g.moves[ply-1],was);if(!strcmp(was,b)||(strlen(b)==4&&!strncmp(was,b,4)))return true;} // repeat
    if(ply!=g.plies+1u||g.pos.side!=theirs||!g.pos.parseUci(b,mv)){event=String(m->name)+tr(": move not accepted (",": ход не принят (")+b+")";events++;dirty=true;return true;}
    char san[12];g.pos.san(mv,san,sizeof san);g.play(mv);received(*m,true);if(m->drawOffer==ChessMatch::OfferedByMe)m->drawOffer=ChessMatch::NoOffer;
    judge(*m);
    String what=String(m->name)+": "+chessLocalSan(san);
    if(m->state==ChessMatch::Over)what+=m->won()?tr(" · you won",", вы победили"):m->lost()?tr(" · you lost",", вы проиграли"):tr(" · draw",", ничья");
    else what+=g.pos.inCheck()?tr(" · check, your move",", шах, ваш ход"):tr(" · your move",", ваш ход");
    news(*m,what);hardware.ping(1100,60);changed(*m);return true;
  }
  if(!strcmp(a,"yes")){if(m->state!=ChessMatch::Inviting)return true;m->state=ChessMatch::Playing;received(*m,false);news(*m,String(m->name)+tr(" accepted: game on"," принял вызов: играем"));hardware.ping(1320,120);changed(*m);return true;}
  if(!strcmp(a,"no")){
    if(m->state==ChessMatch::Inviting){finish(*m,ChessMatch::Undecided,ChessMatch::Declined);news(*m,String(m->name)+tr(" declined the game"," отказался от партии"));}
    else if(m->state==ChessMatch::Invited||(m->state==ChessMatch::Playing&&!g.plies)){finish(*m,ChessMatch::Undecided,ChessMatch::Cancelled);news(*m,String(m->name)+tr(" cancelled the game"," отменил партию"));}
    else return true;
    changed(*m);return true;
  }
  if(!strcmp(a,"draw?")){if(m->state!=ChessMatch::Playing||m->drawOffer==ChessMatch::OfferedToMe)return true;if(m->drawOffer==ChessMatch::OfferedByMe){finish(*m,ChessMatch::Drawn,ChessMatch::Agreed);news(*m,tr("Draw agreed","Ничья по соглашению"));}else{m->drawOffer=ChessMatch::OfferedToMe;news(*m,String(m->name)+tr(" offers a draw"," предлагает ничью"));}changed(*m);return true;}
  if(!strcmp(a,"draw")){if(m->state!=ChessMatch::Playing||m->drawOffer!=ChessMatch::OfferedByMe)return true;received(*m,false);finish(*m,ChessMatch::Drawn,ChessMatch::Agreed);news(*m,String(m->name)+tr(" accepted the draw"," согласился на ничью"));changed(*m);return true;}
  if(!strcmp(a,"resign")){if(m->state!=ChessMatch::Playing)return true;finish(*m,m->mine==White?ChessMatch::WhiteWon:ChessMatch::BlackWon,ChessMatch::Resigned);news(*m,String(m->name)+tr(" resigned: you won"," сдался: вы победили"));hardware.ping(1760,200);changed(*m);return true;}
  return true; // a newer command this version does not know
}

// The player's actions. Each one changes the game here first and then sends one command.
ChessMatch* ChessNet::invite(uint64_t peer,int color){
  Peer* p=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==peer)p=&meshRadio.peers[i];
  if(!p||p->type!=1){event=tr("Chess: choose a chat contact","Шахматы: выберите чат-контакт");events++;dirty=true;return nullptr;}
  ChessMatch* m=slot();if(!m){event=tr("Chess: all boards busy, finish a game","Шахматы: все доски заняты, завершите партию");events++;dirty=true;return nullptr;}
  uint16_t id;do id=1+random(0xffff);while(find(id));
  *m=ChessMatch();m->peer=peer;m->id=id;strlcpy(m->name,p->name,sizeof m->name);m->mine=color==2?random(2):color&1;m->state=ChessMatch::Inviting;m->started=unixNow();m->game.reset();
  char head[16];snprintf(head,sizeof head,"%s%04X new %c",Tag,id,m->mine==White?'w':'b');
  send(*m,String(head)+(m->mine==White?tr(" · MeshMesh chess: you play Black"," · шахматы MeshMesh: вы играете чёрными"):tr(" · MeshMesh chess: you play White"," · шахматы MeshMesh: вы играете белыми")));
  changed(*m);return m;
}
bool ChessNet::accept(ChessMatch& m){
  if(m.state!=ChessMatch::Invited)return false;m.state=ChessMatch::Playing;m.unseen=false;
  char t[16];snprintf(t,sizeof t,"%s%04X yes",Tag,m.id);send(m,t);changed(m);return true;
}
bool ChessNet::decline(ChessMatch& m){
  if(m.state!=ChessMatch::Invited&&m.state!=ChessMatch::Inviting&&!(m.state==ChessMatch::Playing&&!m.game.plies))return false;
  finish(m,ChessMatch::Undecided,m.state==ChessMatch::Invited?ChessMatch::Declined:ChessMatch::Cancelled);m.unseen=false;
  char t[16];snprintf(t,sizeof t,"%s%04X no",Tag,m.id);send(m,t);changed(m);return true;
}
bool ChessNet::move(ChessMatch& m,Move mv){
  if(!m.myTurn()||!m.game.play(mv))return false;
  if(m.drawOffer==ChessMatch::OfferedToMe)m.drawOffer=ChessMatch::NoOffer;judge(m);
  m.moveOpen=true;m.retries=0;m.retryAt=0;m.openSince=0;m.autoStopped=false;m.moveId=m.outId;sendMove(m);changed(m);return true; // a new move is the last command
}
bool ChessNet::offerDraw(ChessMatch& m){
  if(m.state!=ChessMatch::Playing||m.drawOffer!=ChessMatch::NoOffer)return false;m.drawOffer=ChessMatch::OfferedByMe;
  char t[16];snprintf(t,sizeof t,"%s%04X draw?",Tag,m.id);send(m,t);changed(m);return true;
}
bool ChessNet::acceptDraw(ChessMatch& m){
  if(m.state!=ChessMatch::Playing||m.drawOffer!=ChessMatch::OfferedToMe)return false;finish(m,ChessMatch::Drawn,ChessMatch::Agreed);
  char t[16];snprintf(t,sizeof t,"%s%04X draw",Tag,m.id);send(m,t);changed(m);return true;
}
bool ChessNet::resign(ChessMatch& m){
  if(m.state!=ChessMatch::Playing)return false;finish(m,m.mine==White?ChessMatch::BlackWon:ChessMatch::WhiteWon,ChessMatch::Resigned);
  char t[18];snprintf(t,sizeof t,"%s%04X resign",Tag,m.id);send(m,t);changed(m);return true;
}
// R: at once, and automatic resending starts again if it had stopped.
bool ChessNet::resend(ChessMatch& m){if(m.link()!=Failed||m.sending())return false;m.autoStopped=false;m.openSince=millis();retry(m);changed(m,false);return m.link()!=Failed;}
bool ChessNet::remove(ChessMatch& m){if(m.active())return false;m=ChessMatch();dirty=true;save();return true;}

void ChessNet::save(){
  saveDue=false;size_t cap=8+MaxMatches*(RecordHeader+2*MaxPlies)+4;uint8_t* buf=(uint8_t*)malloc(cap);if(!buf)return;
  uint8_t* w=buf;put(w,"MMC1",4);*w++=SaveVersion;uint8_t* countAt=w++;*w++=0;*w++=0;uint8_t n=0;
  for(auto& m:matches){
    if(m.state==ChessMatch::Free)continue;n++;
    put(w,&m.peer,8);put(w,&m.id,2);put(w,m.name,25);uint8_t f[7]={m.mine,m.state,m.result,m.reason,m.drawOffer,m.unseen,m.moveOpen};put(w,f,7);
    put(w,&m.started,4);put(w,&m.updated,4);put(w,&m.outStatus,1);put(w,m.out,80);put(w,&m.game.plies,2);put(w,m.game.moves,2*m.game.plies);
  }
  *countAt=n;uint32_t crc=crc32(buf,w-buf);put(w,&crc,4);
  if(!chessStoreWrite(buf,w-buf)){event=tr("Chess: cannot save games","Шахматы: не удалось сохранить партии");events++;}
  free(buf);
}
void ChessNet::load(){
  size_t cap=8+MaxMatches*(RecordHeader+2*MaxPlies)+4;uint8_t* buf=(uint8_t*)malloc(cap);if(!buf)return;
  size_t size=chessStoreRead(buf,cap);uint32_t crc;
  if(size<12||memcmp(buf,"MMC1",4)||buf[4]<1||buf[4]>SaveVersion||(memcpy(&crc,buf+size-4,4),crc!=crc32(buf,size-4))){free(buf);return;}
  const uint8_t* r=buf+8;const uint8_t* end=buf+size-4;unsigned count=buf[5],slot=0,dropped=0,flags=buf[4]==1?6:7;
  for(unsigned i=0;i<count&&slot<MaxMatches&&r+RecordHeader-7+flags<=end;i++){
    ChessMatch& m=matches[slot];m=ChessMatch();uint8_t f[7]={};uint16_t plies;
    get(r,&m.peer,8);get(r,&m.id,2);get(r,m.name,25);m.name[24]=0;get(r,f,flags);get(r,&m.started,4);get(r,&m.updated,4);get(r,&m.outStatus,1);get(r,m.out,80);m.out[79]=0;get(r,&plies,2);
    if(plies>MaxPlies||r+2*plies>end)break;Move moves[MaxPlies];get(r,moves,2*plies);
    m.mine=f[0]&1;m.state=ChessMatch::State(f[1]);m.result=ChessMatch::Result(f[2]);m.reason=ChessMatch::Reason(f[3]);m.drawOffer=ChessMatch::Offer(f[4]);m.unseen=f[5];
    // Replaying checks every move; a damaged game is dropped rather than shown wrong.
    if(m.state==ChessMatch::Free||m.state>ChessMatch::Over||!m.game.load(moves,plies)){m=ChessMatch();dropped++;continue;}
    if(m.outStatus!=Delivered&&m.out[0])m.outStatus=Failed; // unconfirmed before the restart: not delivered
    // Version 1 kept no flag: the last move is open when it is mine and the last command was it, unconfirmed.
    const char* verb=strchr(m.out,' ');
    m.moveOpen=flags==7?f[6]!=0:m.game.plies&&((m.game.plies-1)&1)==m.mine&&m.outStatus==Failed&&verb&&verb[1]>='1'&&verb[1]<='9';
    if(m.moveOpen&&!m.game.plies)m.moveOpen=false;
    if(m.moveOpen)m.moveStatus=Failed;
    if(m.pending()){m.openSince=millis();m.retryAt=millis()+60000;} // resend a minute after the start
    m.changedAt=millis()-(count-i);slot++;
  }
  free(buf);if(dropped){event=tr("Chess: a damaged game was dropped","Шахматы: повреждённая партия удалена");events++;}dirty=true;
}

String chessLocalSan(const char* san){
  String out;
  for(const char* c=san;*c;c++){
    bool piece=strchr("KQRBN",*c)&&(c==san||c[-1]=='=');
    if(piece&&config.russian){switch(*c){case 'K':out+="Кр";break;case 'Q':out+="Ф";break;case 'R':out+="Л";break;case 'B':out+="С";break;default:out+="К";}}
    else out+=*c;
  }
  return out;
}
String ChessNet::web() const{StaticJsonDocument<256> d;d["events"]=events;d["event"]=event;d["waiting"]=waiting();String s;serializeJson(d,s);s.remove(s.length()-1);return s+",\"games\":"+json()+"}";}
String ChessNet::detail(const ChessMatch& m) const{
  DynamicJsonDocument d(12288);char id[5];snprintf(id,sizeof id,"%04X",m.id);d["id"]=id;
  // Standard notation of every move, replayed from the start (the saved form is the move list).
  JsonArray san=d.createNestedArray("san");Position p;p.start();char b[12];
  for(unsigned i=0;i<m.game.plies;i++){p.san(m.game.moves[i],b,sizeof b);san.add(b);p.apply(m.game.moves[i]);}
  JsonArray legal=d.createNestedArray("legal");
  if(m.myTurn()){Move list[MaxMoves];unsigned n=m.game.pos.legal(list);for(unsigned i=0;i<n;i++){uci(list[i],b);legal.add(b);}}
  d["check"]=m.game.pos.inCheck();
  String s;serializeJson(d,s);return s;
}
String ChessNet::json() const{
  DynamicJsonDocument d(6144);JsonArray list=d.to<JsonArray>();
  static const char* states[]={"free","inviting","invited","playing","over"};static const char* results[]={"","white","black","draw"};
  static const char* reasons[]={"","mate","resigned","stalemate","repetition","fifty","material","too_long","agreed","declined","cancelled"};
  for(auto& m:matches){
    if(m.state==ChessMatch::Free)continue;JsonObject o=list.createNestedObject();char id[5];snprintf(id,sizeof id,"%04X",m.id);char fen[96];m.game.pos.fen(fen,sizeof fen);
    o["id"]=id;o["peer"]=meshRadio.idText(m.peer);o["name"]=m.name;o["state"]=states[m.state];o["color"]=m.mine==White?"white":"black";o["plies"]=m.game.plies;o["fen"]=fen;
    if(m.game.plies){char u[6],san[12];uci(m.game.moves[m.game.plies-1],u);o["last"]=u;m.game.at(m.game.plies-1).san(m.game.moves[m.game.plies-1],san,sizeof san);o["last_san"]=san;}
    o["my_turn"]=m.myTurn();o["result"]=results[m.result];o["reason"]=reasons[m.reason];o["draw_offer"]=m.drawOffer==ChessMatch::OfferedByMe?"mine":m.drawOffer==ChessMatch::OfferedToMe?"theirs":"";
    o["unseen"]=m.unseen;o["out_status"]=m.link();o["updated"]=m.updated;o["check"]=m.game.pos.inCheck();
    o["move_open"]=m.moveOpen;o["retry_in"]=retryIn(m);o["retries"]=m.retries;o["auto_stopped"]=m.autoStopped;
  }
  String s;serializeJson(d,s);return s;
}
String ChessNet::command(const String& line){
  if(line=="chess")return json();
  if(line=="chess web")return web();
  String rest=line.substring(6);rest.trim();int sp=rest.indexOf(' ');String verb=sp<0?rest:rest.substring(0,sp),arg=sp<0?String():rest.substring(sp+1);arg.trim();
  if(verb=="invite"){
    int at=arg.indexOf(' ');String node=at<0?arg:arg.substring(0,at),color=at<0?String("r"):arg.substring(at+1);
    char* e=nullptr;uint64_t peer=strtoull(node.c_str(),&e,16);if(!peer||!e||*e)return "ERR chess invite NODE_ID w|b|r";
    ChessMatch* m=invite(peer,color=="w"?White:color=="b"?Black:2);if(!m)return "ERR "+event;char id[5];snprintf(id,sizeof id,"%04X",m->id);return String("OK game ")+id+(m->mine==White?" white":" black");
  }
  int at=arg.indexOf(' ');String gid=at<0?arg:arg.substring(0,at),extra=at<0?String():arg.substring(at+1);
  char* e=nullptr;unsigned long id=strtoul(gid.c_str(),&e,16);ChessMatch* m=id&&e&&!*e&&gid.length()==4?find(uint16_t(id)):nullptr;
  if(!m)return "ERR chess invite|accept|decline|move|draw|resign|resend|remove|show|seen GAME_ID ...";
  bool done=false;
  if(verb=="accept")done=accept(*m);
  else if(verb=="decline")done=decline(*m);
  else if(verb=="move"){Move mv;done=m->myTurn()&&m->game.pos.parseUci(extra.c_str(),mv)&&move(*m,mv);}
  else if(verb=="draw")done=m->drawOffer==ChessMatch::OfferedToMe?acceptDraw(*m):offerDraw(*m);
  else if(verb=="resign")done=resign(*m);
  else if(verb=="resend")done=resend(*m);
  else if(verb=="remove")done=remove(*m);
  else if(verb=="show")return detail(*m);
  else if(verb=="seen"){viewed(*m);return "OK seen";}
  else return "ERR unknown chess command";
  return done?"OK "+verb:"ERR "+verb+" not possible now";
}

#if !defined(MM_UI_PREVIEW)
#include <LittleFS.h>
// Written to a new file and renamed; the previous copy is read if the main file is damaged.
bool chessStoreWrite(const uint8_t* data,size_t size){
  if(!hardware.fsOk)return false;LittleFS.mkdir("/meshmesh");
  File f=LittleFS.open("/meshmesh/chess.new","w");if(!f)return false;bool ok=f.write(data,size)==size;f.close();if(!ok)return false;
  LittleFS.remove("/meshmesh/chess.old");LittleFS.rename("/meshmesh/chess.bin","/meshmesh/chess.old");
  return LittleFS.rename("/meshmesh/chess.new","/meshmesh/chess.bin");
}
size_t chessStoreRead(uint8_t* data,size_t cap){
  if(!hardware.fsOk)return 0;
  for(const char* path:{"/meshmesh/chess.bin","/meshmesh/chess.old"}){
    File f=LittleFS.open(path,"r");if(!f)continue;size_t n=f.size()<=cap?f.read(data,f.size()):0;f.close();uint32_t crc;
    if(n>=12&&!memcmp(data,"MMC1",4)&&(memcpy(&crc,data+n-4,4),crc==crc32(data,n-4)))return n;
  }
  return 0;
}
#endif
