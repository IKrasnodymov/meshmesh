#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: the game messages are not speed-critical
#endif
#include "Duel.h"
#include "Config.h"
#include "MeshRadio.h"
#include "Hardware.h"
#include "BlobStore.h"
#include <ArduinoJson.h>
#include <time.h>
#if MM_DUEL
DuelNet duels;
namespace {
const char TagCheckers[]="\xf0\x9f\x8f\x81",TagSea[]="\xf0\x9f\x9a\xa2"; // 🏁 🚢
const char* tagOf(const Duel& g){return g.kind==Duel::Sea?TagSea:TagCheckers;}
constexpr uint8_t Queued=ChatMessage::Queued,Delivered=ChatMessage::Delivered,Failed=ChatMessage::Failed;
uint32_t unixNow(){time_t t=time(nullptr);return t>1700000000?uint32_t(t):0;}
// Resending as in chess: after 2, 5 and 10 minutes, then every 15; at once when the other player is heard again
// (at most every 2 min); it stops after 24 h without confirmation (resend starts it again).
constexpr uint32_t RetryDelays[]={120000,300000,600000,900000},RetryWindow=86400000,HeardGap=120000;
bool inFlight(uint8_t s){return s==ChatMessage::Queued||s==ChatMessage::Sent;}
constexpr uint32_t Magic=0x31444d4d; // "MMD1"
struct Saved{Duel games[DuelNet::Max];};
uint32_t seed(){return uint32_t(random(0x7fffffff))^(millis()*2654435761u)^uint32_t(meshRadio.nodeId);}
String cellText(int c){char b[4];sea::cellName(c,b);return b;}
String gameName(Duel::Kind k){return k==Duel::Sea?tr("Sea battle","Морской бой"):tr("Checkers","Шашки");}
}
bool Duel::myTurn() const{if(state!=Playing)return false;return kind==Checkers?game.pos.side==mine:seaTurn&&aim<0;}
bool Duel::pending() const{return moveOpen||(out[0]&&outStatus!=Delivered);}
bool Duel::sending() const{return (moveOpen&&inFlight(moveStatus))||(out[0]&&inFlight(outStatus));}
uint8_t Duel::link() const{
  if((moveOpen&&moveStatus==Failed)||(out[0]&&outStatus==Failed))return Failed;
  if(moveOpen)return moveStatus;return out[0]?outStatus:0;
}
bool Duel::canShuffle() const{if(kind!=Sea||!active())return false;for(uint8_t b:home.shot)if(b)return false;return true;}

void DuelNet::begin(){load();}
void DuelNet::tick(){
  uint32_t now=millis();
  for(auto& g:games){
    if(g.state==Duel::Free||!g.pending()||g.sending()||g.autoStopped)continue;
    if(!g.openSince)g.openSince=now;
    if(now-g.openSince>RetryWindow){g.autoStopped=true;g.retryAt=0;event=String(g.name)+tr(": no confirmation for 24 h",": нет подтверждения 24 ч");events++;eventGame=&g;dirty=true;continue;}
    const Peer* p=meshRadio.findContact(g.peer);
    bool heard=p&&p->heard&&int32_t(p->seen-g.triedAt)>0&&now-g.triedAt>=HeardGap;
    if(!g.retryAt&&!heard){g.retryAt=now+RetryDelays[g.retries<3?g.retries:3];dirty=true;continue;}
    if(heard||int32_t(now-g.retryAt)>=0)retry(g);
  }
  if(saveDue&&int32_t(now-saveAt)>=0)save();
}
int32_t DuelNet::retryIn(const Duel& g) const{if(!g.pending()||g.sending()||g.autoStopped||!g.retryAt)return -1;int32_t left=int32_t(g.retryAt-millis());return left>0?(left+999)/1000:0;}
void DuelNet::retry(Duel& g){
  g.retries=g.retries<255?g.retries+1:255;g.retryAt=0;
  if(g.moveOpen&&!inFlight(g.moveStatus))sendMove(g);
  else if(g.out[0]&&g.outStatus==Failed){g.outId=meshRadio.sendGame(g.out,g.peer);g.outStatus=g.outId?Queued:Failed;g.triedAt=millis();}
  dirty=true;
}
void DuelNet::confirmed(Duel& g){if(g.pending())return;g.retries=0;g.retryAt=0;g.openSince=0;g.autoStopped=false;}
Duel* DuelNet::find(uint64_t peer,uint16_t id,Duel::Kind kind){for(auto& g:games)if(g.state!=Duel::Free&&g.peer==peer&&g.id==id&&g.kind==kind)return &g;return nullptr;}
Duel* DuelNet::find(uint16_t id){for(auto& g:games)if(g.state!=Duel::Free&&g.id==id)return &g;return nullptr;}
unsigned DuelNet::count(int kind) const{unsigned n=0;for(auto& g:games)n+=g.state!=Duel::Free&&(kind<0||g.kind==kind);return n;}
unsigned DuelNet::waiting(int kind) const{unsigned n=0;for(auto& g:games)n+=(kind<0||g.kind==kind)&&(g.state==Duel::Invited||g.myTurn()||g.unseen);return n;}
bool DuelNet::playsWith(uint64_t peer) const{for(auto& g:games)if(g.state!=Duel::Free&&g.peer==peer)return true;return false;}
// A free place, or the finished game that changed longest ago (one already opened first).
Duel* DuelNet::slot(){
  Duel* seen=nullptr;Duel* any=nullptr;
  for(auto& g:games){if(g.state==Duel::Free)return &g;if(g.state!=Duel::Over||g.pending())continue;
    if(!g.unseen&&(!seen||int32_t(g.changedAt-seen->changedAt)<0))seen=&g;if(!any||int32_t(g.changedAt-any->changedAt)<0)any=&g;}
  return seen?seen:any;
}
bool DuelNet::room(){return slot()!=nullptr;}
void DuelNet::changed(Duel& g,bool now){g.changedAt=millis();g.updated=unixNow();dirty=true;if(now)save();else if(!saveDue){saveDue=true;saveAt=millis()+3000;}}
void DuelNet::news(Duel& g,const String& text){event=text;events++;eventGame=&g;g.unseen=true;dirty=true;}
void DuelNet::viewed(Duel& g){if(g.unseen){g.unseen=false;changed(g,false);}}
bool DuelNet::send(Duel& g,const String& text){strlcpy(g.out,text.c_str(),sizeof g.out);g.outId=meshRadio.sendGame(text,g.peer);g.outStatus=g.outId?Queued:Failed;g.triedAt=millis();dirty=true;return g.outId;}
bool DuelNet::sendMove(Duel& g){g.moveId=meshRadio.sendGame(g.move,g.peer);g.moveStatus=g.moveId?Queued:Failed;g.triedAt=millis();dirty=true;return g.moveId;}
bool DuelNet::delivery(uint32_t id,uint8_t status){
  if(!id)return false;
  for(auto& g:games){
    if(g.state==Duel::Free)continue;bool hit=false;
    if(g.moveOpen&&g.moveId==id){g.moveStatus=status;g.moveOpen=status!=Delivered;hit=true;}
    if(g.outId==id&&g.outStatus!=Delivered){g.outStatus=status;hit=true;}
    if(!hit)continue;
    if(status==Delivered){confirmed(g);if(g.pending())g.retryAt=millis();}
    if(status==Failed&&!g.retries){event=String(g.name)+tr(": not confirmed, will resend",": не подтвердил, повторю автоматически");events++;eventGame=&g;}
    changed(g,status==Delivered||status==Failed);return true;
  }
  return false;
}
// Their move, shot or answer confirms my last one; it also answers my challenge or my acceptance. Their
// agreement or draw offer answers my challenge or my draw offer.
void DuelNet::received(Duel& g,bool play){
  if(play){g.moveOpen=false;g.moveStatus=Delivered;}
  const char* verb=strchr(g.out,' ');
  if(verb&&g.outStatus!=Delivered){verb++;if(!strncmp(verb,"new",3)||(play?!strncmp(verb,"yes",3):!strncmp(verb,"draw?",5)))g.outStatus=Delivered;}
  confirmed(g);
}
// The winner of a sea battle shows the fleet: the loser sees the ships it did not find and checks the answers.
void DuelNet::finish(Duel& g,Duel::Result r,Duel::Reason why){
  g.state=Duel::Over;g.result=r;g.reason=why;g.drawOffer=Duel::NoOffer;
  if(g.kind==Duel::Sea&&r==Duel::Won){char f[64];g.home.fleet.text(f,sizeof f);char t[16];snprintf(t,sizeof t,"%s%04X fleet ",TagSea,g.id);send(g,String(t)+f);}
}
void DuelNet::judge(Duel& g){
  auto& m=g.game;if(m.outcome==checkers::Ongoing)return;
  if(m.outcome==checkers::QuietDraw)finish(g,Duel::Drawn,Duel::Quiet);else finish(g,m.pos.side==g.mine?Duel::Lost:Duel::Won,Duel::Rules);
}
// Their shot at my field: the answer goes out at once and stays the open command until their next one.
void DuelNet::seaAnswer(Duel& g,int cell){
  uint8_t a=g.home.answer(cell);g.shots++;g.lastIn=int8_t(cell);g.lastAnswer=a;received(g,true);g.seaTurn=a==sea::AnswerMiss;
  snprintf(g.move,sizeof g.move,"%s%04X %u = %s",TagSea,g.id,g.shots,sea::answerWord(a));g.moveOpen=true;g.retries=0;g.retryAt=0;g.openSince=0;g.autoStopped=false;sendMove(g);
  String what=String(g.name)+": "+cellText(cell)+" - ";
  if(a==sea::AnswerWin){finish(g,Duel::Lost,Duel::Rules);what+=tr("the fleet is sunk, you lost","флот потоплен, вы проиграли");}
  else what+=a==sea::AnswerMiss?tr("miss, your shot","мимо, ваш выстрел"):a==sea::AnswerHit?tr("hit","попадание"):tr("ship sunk","корабль потоплен");
  news(g,what);hardware.ping(a==sea::AnswerMiss?1100:700,a==sea::AnswerMiss?60:150);changed(g);
}

// Commands from the other player: only this contact's own games; anything out of turn or repeated is ignored.
bool DuelNet::receive(uint64_t from,const char* name,const char* text){
  Duel::Kind kind;
  if(!strncmp(text,TagCheckers,4))kind=Duel::Checkers;else if(!strncmp(text,TagSea,4))kind=Duel::Sea;else return false;
  if(!built(kind))return false; // shown as a chat message: its text explains the game
  const char* c=text+4;uint16_t id=0;
  for(int i=0;i<4;i++){char h=c[i];int v=h>='0'&&h<='9'?h-'0':(h|32)>='a'&&(h|32)<='f'?(h|32)-'a'+10:-1;if(v<0)return false;id=uint16_t(id<<4|v);}
  if(c[4]!=' '||!id)return false;
  char a[12]={},b[44]={},f[8]={};if(sscanf(c+5,"%11s %43s %7s",a,b,f)<1)return false;
  Duel* g=find(from,id,kind);
  if(!strcmp(a,"new")){
    if(kind==Duel::Checkers&&b[0]!='w'&&b[0]!='b')return false;if(g)return true;
    g=slot();
    if(!g){char t[16];snprintf(t,sizeof t,"%s%04X no",kind==Duel::Sea?TagSea:TagCheckers,id);meshRadio.sendGame(t,from);event=String(name)+tr(": game refused, six games in progress",": игра отклонена, идут шесть игр");events++;dirty=true;return true;}
    *g=Duel();g->kind=kind;g->peer=from;g->id=id;strlcpy(g->name,name,sizeof g->name);g->state=Duel::Invited;g->started=unixNow();
    if(kind==Duel::Checkers){g->mine=b[0]=='w'?checkers::Black:checkers::White;g->game.reset();}else{g->mine=1;g->home.fleet.random(seed());}
    news(*g,String(name)+tr(" invites you: "," зовёт играть: ")+gameName(kind));hardware.ping(1320,120);changed(*g);return true;
  }
  if(!g){event=gameName(kind)+tr(": a move of an unknown game",": ход в неизвестной игре");events++;dirty=true;return true;}
  strlcpy(g->name,name,sizeof g->name);
  if(a[0]>='1'&&a[0]<='9'){
    unsigned n=strtoul(a,nullptr,10);
    if(kind==Duel::Checkers){
      auto& m=g->game;uint8_t theirs=g->mine^1;
      if(g->state==Duel::Inviting&&g->mine==checkers::Black&&n==1)g->state=Duel::Playing; // they play White: their move accepts
      if(g->state!=Duel::Playing)return true;
      if(n&&n==m.plies&&((n-1)&1)==theirs){char was[40];checkers::moveText(m.last,was,sizeof was);if(!strcmp(was,b))return true;} // a repeat
      checkers::Move mv;
      if(n!=m.plies+1u||m.pos.side!=theirs||!m.pos.parse(b,mv)){event=String(g->name)+tr(": move not accepted (",": ход не принят (")+b+")";events++;dirty=true;return true;}
      m.play(mv);received(*g,true);if(g->drawOffer==Duel::OfferedByMe)g->drawOffer=Duel::NoOffer;judge(*g);
      char t[40];checkers::moveText(mv,t,sizeof t);String what=String(g->name)+": "+t;
      if(g->state==Duel::Over)what+=g->result==Duel::Won?tr(", you won",", вы победили"):g->result==Duel::Lost?tr(", you lost",", вы проиграли"):tr(", draw",", ничья");
      else what+=tr(", your move",", ваш ход");
      news(*g,what);hardware.ping(1100,60);changed(*g);return true;
    }
    if(g->state!=Duel::Playing)return true;
    if(!strcmp(b,"=")){ // the answer to my shot
      int ans=sea::answerOf(f);if(!ans||g->aim<0||n!=g->shots+1u)return true; // a repeat
      int cell=g->aim;g->target.record(cell,uint8_t(ans));g->lastOut=g->aim;g->aim=-1;g->shots++;received(*g,true);g->seaTurn=ans!=sea::AnswerMiss;
      String what=String(g->name)+": "+cellText(cell)+" - ";
      if(ans==sea::AnswerWin){finish(*g,Duel::Won,Duel::Rules);what+=tr("the whole fleet sunk, you won","весь флот потоплен, вы победили");}
      else what+=ans==sea::AnswerMiss?tr("miss, their shot","мимо, стреляет соперник"):ans==sea::AnswerHit?tr("hit! shoot again","попадание! стреляйте ещё"):tr("ship sunk! shoot again","корабль потоплен! стреляйте ещё");
      news(*g,what);hardware.ping(ans==sea::AnswerMiss?900:1500,120);changed(*g);return true;
    }
    int cell=sea::parseCell(b);if(cell<0)return true;
    if(n==g->shots&&cell==g->lastIn){ // they did not get my answer: once more (untracked when my next shot replaced it)
      char t[40];snprintf(t,sizeof t,"%s%04X %u = %s",TagSea,g->id,g->shots,sea::answerWord(g->lastAnswer));
      if(strcmp(g->move,t))meshRadio.sendGame(t,from);else{g->moveOpen=true;if(!inFlight(g->moveStatus))sendMove(*g);}
      return true;}
    if(g->aim>=0&&n==g->shots+2u){g->target.record(g->aim,sea::AnswerMiss);g->lastOut=g->aim;g->aim=-1;g->shots++;g->seaTurn=false;} // they shoot: mine missed (its answer was lost)
    if(g->seaTurn||n!=g->shots+1u){event=String(g->name)+tr(": shot not accepted (",": выстрел не принят (")+b+")";events++;dirty=true;return true;}
    seaAnswer(*g,cell);return true;
  }
  if(!strcmp(a,"fleet")){
    if(kind!=Duel::Sea||g->state!=Duel::Over||g->revealed)return true;const char* t=strstr(c+5,"fleet ");
    if(!t||!g->theirs.parse(t+6))return true;g->revealed=true;g->fair=sea::honest(g->theirs,g->target);
    if(!g->fair)news(*g,String(g->name)+tr(": the fleet does not match the answers",": расстановка не сходится с ответами"));changed(*g);return true;
  }
  if(!strcmp(a,"yes")){if(g->state!=Duel::Inviting)return true;g->state=Duel::Playing;received(*g,false);g->seaTurn=kind==Duel::Sea;
    news(*g,String(g->name)+(kind==Duel::Sea?tr(" accepted: your shot"," принял вызов: ваш выстрел"):tr(" accepted: game on"," принял вызов: играем")));hardware.ping(1320,120);changed(*g);return true;}
  if(!strcmp(a,"no")){
    bool fresh=g->state==Duel::Playing&&(kind==Duel::Checkers?!g->game.plies:!g->shots);
    if(g->state==Duel::Inviting){finish(*g,Duel::Undecided,Duel::Declined);news(*g,String(g->name)+tr(" declined the game"," отказался от игры"));}
    else if(g->state==Duel::Invited||fresh){finish(*g,Duel::Undecided,Duel::Cancelled);news(*g,String(g->name)+tr(" cancelled the game"," отменил игру"));}
    else return true;
    changed(*g);return true;
  }
  if(kind==Duel::Checkers&&!strcmp(a,"draw?")){if(g->state!=Duel::Playing||g->drawOffer==Duel::OfferedToMe)return true;
    if(g->drawOffer==Duel::OfferedByMe){finish(*g,Duel::Drawn,Duel::Agreed);news(*g,tr("Draw agreed","Ничья по соглашению"));}else{g->drawOffer=Duel::OfferedToMe;news(*g,String(g->name)+tr(" offers a draw"," предлагает ничью"));}changed(*g);return true;}
  if(kind==Duel::Checkers&&!strcmp(a,"draw")){if(g->state!=Duel::Playing||g->drawOffer!=Duel::OfferedByMe)return true;received(*g,false);finish(*g,Duel::Drawn,Duel::Agreed);news(*g,String(g->name)+tr(" accepted the draw"," согласился на ничью"));changed(*g);return true;}
  if(!strcmp(a,"resign")){if(g->state!=Duel::Playing)return true;finish(*g,Duel::Won,Duel::Resigned);news(*g,String(g->name)+tr(" resigned: you won"," сдался: вы победили"));hardware.ping(1760,200);changed(*g);return true;}
  return true; // a newer command this version does not know
}

// The player's actions: the game changes here first, then one command goes out.
Duel* DuelNet::invite(uint64_t peer,Duel::Kind kind,int color){
  const Peer* p=meshRadio.findContact(peer);
  if(!built(kind)){event=tr("This game is not in this build","Этой игры нет в прошивке");events++;dirty=true;return nullptr;}
  if(!p||p->type!=1){event=gameName(kind)+tr(": choose a chat contact",": выберите чат-контакт");events++;dirty=true;return nullptr;}
  Duel* g=slot();if(!g){event=tr("Six games in progress, finish one","Идут шесть игр, завершите одну");events++;dirty=true;return nullptr;}
  uint16_t id;do id=uint16_t(1+random(0xffff));while(find(id));
  *g=Duel();g->kind=kind;g->peer=peer;g->id=id;strlcpy(g->name,p->name,sizeof g->name);g->state=Duel::Inviting;g->started=unixNow();
  char head[24];
  if(kind==Duel::Checkers){g->mine=uint8_t(color==2?random(2):color&1);g->game.reset();snprintf(head,sizeof head,"%s%04X new %c",TagCheckers,id,g->mine==checkers::White?'w':'b');
    send(*g,String(head)+(g->mine==checkers::White?tr(" · MeshMesh checkers: you play Black"," · шашки MeshMesh: вы играете чёрными"):tr(" · MeshMesh checkers: you play White"," · шашки MeshMesh: вы играете белыми")));}
  else{g->mine=0;g->home.fleet.random(seed());snprintf(head,sizeof head,"%s%04X new",TagSea,id);send(*g,String(head)+tr(" · MeshMesh sea battle"," · морской бой MeshMesh"));}
  changed(*g);return g;
}
bool DuelNet::accept(Duel& g){
  if(g.state!=Duel::Invited)return false;g.state=Duel::Playing;g.unseen=false;g.seaTurn=false;
  char t[16];snprintf(t,sizeof t,"%s%04X yes",tagOf(g),g.id);send(g,t);changed(g);return true;
}
bool DuelNet::decline(Duel& g){
  bool fresh=g.state==Duel::Playing&&(g.kind==Duel::Checkers?!g.game.plies:!g.shots&&g.aim<0);
  if(g.state!=Duel::Invited&&g.state!=Duel::Inviting&&!fresh)return false;
  finish(g,Duel::Undecided,g.state==Duel::Invited?Duel::Declined:Duel::Cancelled);g.unseen=false;
  char t[16];snprintf(t,sizeof t,"%s%04X no",tagOf(g),g.id);send(g,t);changed(g);return true;
}
bool DuelNet::resign(Duel& g){
  if(g.state!=Duel::Playing)return false;finish(g,Duel::Lost,Duel::Resigned);
  char t[20];snprintf(t,sizeof t,"%s%04X resign",tagOf(g),g.id);send(g,t);changed(g);return true;
}
bool DuelNet::draw(Duel& g){
  if(g.kind!=Duel::Checkers||g.state!=Duel::Playing||g.drawOffer==Duel::OfferedByMe)return false;
  char t[20];bool accept=g.drawOffer==Duel::OfferedToMe;snprintf(t,sizeof t,"%s%04X %s",TagCheckers,g.id,accept?"draw":"draw?");
  if(accept)finish(g,Duel::Drawn,Duel::Agreed);else g.drawOffer=Duel::OfferedByMe;send(g,t);changed(g);return true;
}
bool DuelNet::move(Duel& g,const checkers::Move& mv){
  if(g.kind!=Duel::Checkers||!g.myTurn()||!g.game.play(mv))return false;
  if(g.drawOffer==Duel::OfferedToMe)g.drawOffer=Duel::NoOffer;judge(g);
  char t[40];checkers::moveText(mv,t,sizeof t);snprintf(g.move,sizeof g.move,"%s%04X %u %s",TagCheckers,g.id,g.game.plies,t);
  g.moveOpen=true;g.retries=0;g.retryAt=0;g.openSince=0;g.autoStopped=false;sendMove(g);changed(g);return true;
}
bool DuelNet::shoot(Duel& g,int cell){
  if(g.kind!=Duel::Sea||!g.myTurn()||!g.target.open(cell))return false;
  g.aim=int8_t(cell);snprintf(g.move,sizeof g.move,"%s%04X %u %s",TagSea,g.id,g.shots+1u,cellText(cell).c_str());
  g.moveOpen=true;g.retries=0;g.retryAt=0;g.openSince=0;g.autoStopped=false;sendMove(g);changed(g);return true;
}
bool DuelNet::shuffle(Duel& g){if(!g.canShuffle())return false;g.home.fleet.random(seed());changed(g,false);return true;}
bool DuelNet::resend(Duel& g){if(g.link()!=Failed||g.sending())return false;g.autoStopped=false;g.openSince=millis();retry(g);changed(g,false);return g.link()!=Failed;}
bool DuelNet::remove(Duel& g){if(g.active())return false;if(eventGame==&g)eventGame=nullptr;g=Duel();dirty=true;save();return true;}

void DuelNet::save(){
  saveDue=false;Saved* s=new(std::nothrow) Saved();if(!s)return;memcpy(s->games,games,sizeof games);
  if(!blob::save("/meshmesh/duels",Magic,*s)){event=tr("Games: cannot save","Игры: не удалось сохранить");events++;}
  delete s;
}
void DuelNet::load(){
  Saved* s=new(std::nothrow) Saved();if(!s)return;
  if(blob::load("/meshmesh/duels",Magic,*s)){
    unsigned n=0;for(auto& g:s->games){
      if(g.state==Duel::Free||g.state>Duel::Over||g.kind>Duel::Sea||!built(g.kind))continue;
      Duel& d=games[n];d=g;d.name[sizeof d.name-1]=0;d.move[sizeof d.move-1]=0;d.out[sizeof d.out-1]=0;
      if(d.out[0]&&d.outStatus!=Delivered)d.outStatus=Failed; // unconfirmed before the restart: not delivered
      if(d.moveOpen)d.moveStatus=Failed;
      d.retries=0;d.autoStopped=false;d.retryAt=0;d.openSince=0;
      if(d.pending()){d.openSince=millis();d.retryAt=millis()+60000;} // resend a minute after the start
      d.changedAt=millis()-(Max-n);n++;
    }
  }
  delete s;dirty=true;
}

// JSON for the web page and the app. Fields: checkers - 32 dark squares from a1 ('.', w W b B); sea - 100 cells
// from a1 along the rows: mine ('.' water, 'o' a miss, 's' a ship, 'x' hit, 'X' sunk), theirs ('.' unknown,
// 'o' miss, '~' water by a sunk ship, 'x' hit, 'X' sunk, '*' my shot in flight, 's' a ship shown at the end).
namespace {
void common(JsonObject o,const Duel& g,int32_t retry){
  static const char* states[]={"free","inviting","invited","playing","over"};static const char* results[]={"","won","lost","draw"};
  static const char* reasons[]={"","rules","resigned","agreed","declined","cancelled","quiet"};
  char id[5];snprintf(id,sizeof id,"%04X",g.id);o["id"]=id;o["kind"]=g.kind==Duel::Sea?"sea":"checkers";o["peer"]=meshRadio.idText(g.peer);o["name"]=g.name;
  o["state"]=states[g.state];o["result"]=results[g.result];o["reason"]=reasons[g.reason];o["my_turn"]=g.myTurn();o["unseen"]=g.unseen;
  o["out_status"]=g.link();o["retry_in"]=retry;o["auto_stopped"]=g.autoStopped;o["updated"]=g.updated;
  if(g.kind==Duel::Checkers){char t[40];o["color"]=g.mine==checkers::White?"white":"black";o["plies"]=g.game.plies;
    if(g.game.plies){checkers::moveText(g.game.last,t,sizeof t);o["last"]=t;}
    o["draw_offer"]=g.drawOffer==Duel::OfferedByMe?"mine":g.drawOffer==Duel::OfferedToMe?"theirs":"";
    o["mine_left"]=g.game.pos.count(g.mine);o["their_left"]=g.game.pos.count(g.mine^1);}
  else{o["shots"]=g.shots;o["first"]=g.mine==0;o["aim"]=g.aim>=0?cellText(g.aim):String();o["last_out"]=g.lastOut>=0?cellText(g.lastOut):String();o["last_in"]=g.lastIn>=0?cellText(g.lastIn):String();
    o["ships_left"]=g.home.shipsLeft();o["sunk"]=g.target.sunkShips();o["can_shuffle"]=g.canShuffle();if(g.state==Duel::Over&&g.revealed)o["fair"]=g.fair;}
}
}
String DuelNet::json() const{
  DynamicJsonDocument d(6144);JsonArray list=d.createNestedArray("games");
  for(auto& g:games)if(g.state!=Duel::Free)common(list.createNestedObject(),g,retryIn(g));
  d["events"]=events;d["event"]=event;d["waiting"]=waiting();
  JsonArray k=d.createNestedArray("kinds");if(MM_CHECKERS)k.add("checkers");if(MM_SEA)k.add("sea");
  String s;serializeJson(d,s);return s;
}
String DuelNet::detail(const Duel& g) const{
  DynamicJsonDocument d(4096);JsonObject o=d.to<JsonObject>();common(o,g,retryIn(g));
  if(g.kind==Duel::Checkers){
    char b[33];for(int s=0;s<32;s++){int8_t p=g.game.pos.board[s];b[s]=p==checkers::Man?'w':p==checkers::King?'W':p==-checkers::Man?'b':p==-checkers::King?'B':'.';}b[32]=0;o["board"]=b;
    JsonArray legal=o.createNestedArray("legal");
    if(g.myTurn()){checkers::Move list[checkers::MaxMoves];unsigned n=g.game.pos.legal(list);char t[40];for(unsigned i=0;i<n;i++){checkers::moveText(list[i],t,sizeof t);legal.add(t);}}
    if(g.game.plies){JsonArray path=o.createNestedArray("last_path");char s[3];for(unsigned i=0;i<g.game.last.n;i++){checkers::squareName(g.game.last.path[i],s);path.add(s);}}
  }else{
    char mine[sea::Cells+1],theirs[sea::Cells+1];
    for(int c=0;c<sea::Cells;c++){
      int s=g.home.fleet.shipAt(c);bool shot=g.home.isShot(c);mine[c]=s<0?(shot?'o':'.'):!shot?'s':g.home.sunk(s)?'X':'x';
      uint8_t m=g.target.mark[c];theirs[c]=c==g.aim?'*':m==sea::Miss?'o':m==sea::Hit?'x':m==sea::Sunk?'X':g.target.around(c)?'~':g.revealed&&g.theirs.shipAt(c)>=0?'s':'.';}
    mine[sea::Cells]=theirs[sea::Cells]=0;o["mine"]=mine;o["theirs"]=theirs;
  }
  String s;serializeJson(d,s);return s;
}
// duel; duel invite NODE_ID checkers|sea [w|b|r]; duel accept|decline|resign|draw|resend|remove|seen|shuffle|show ID;
// duel move ID c3-d4; duel shoot ID e5
String DuelNet::command(const String& line){
  if(line=="duel")return json();
  String rest=line.substring(5);rest.trim();int sp=rest.indexOf(' ');String verb=sp<0?rest:rest.substring(0,sp),arg=sp<0?String():rest.substring(sp+1);arg.trim();
  if(verb=="invite"){
    int at=arg.indexOf(' ');String node=at<0?arg:arg.substring(0,at),more=at<0?String():arg.substring(at+1);more.trim();
    int at2=more.indexOf(' ');String game=at2<0?more:more.substring(0,at2),color=at2<0?String("r"):more.substring(at2+1);color.trim();
    char* e=nullptr;uint64_t peer=strtoull(node.c_str(),&e,16);
    if(!peer||!e||*e||(game!="checkers"&&game!="sea"))return "ERR duel invite NODE_ID checkers|sea [w|b|r]";
    Duel* g=invite(peer,game=="sea"?Duel::Sea:Duel::Checkers,color=="w"?checkers::White:color=="b"?checkers::Black:2);if(!g)return "ERR "+event;
    char id[5];snprintf(id,sizeof id,"%04X",g->id);return String("OK game ")+id+(g->kind==Duel::Sea?" sea":g->mine==checkers::White?" white":" black");
  }
  int at=arg.indexOf(' ');String gid=at<0?arg:arg.substring(0,at),extra=at<0?String():arg.substring(at+1);extra.trim();
  char* e=nullptr;unsigned long id=strtoul(gid.c_str(),&e,16);Duel* g=id&&e&&!*e&&gid.length()==4?find(uint16_t(id)):nullptr;
  if(!g)return "ERR duel invite|accept|decline|move|shoot|shuffle|draw|resign|resend|remove|show|seen GAME_ID ...";
  bool done=false;
  if(verb=="accept")done=accept(*g);
  else if(verb=="decline")done=decline(*g);
  else if(verb=="move"){checkers::Move mv;done=g->kind==Duel::Checkers&&g->myTurn()&&g->game.pos.parse(extra.c_str(),mv)&&move(*g,mv);}
  else if(verb=="shoot")done=shoot(*g,sea::parseCell(extra.c_str()));
  else if(verb=="shuffle")done=shuffle(*g);
  else if(verb=="draw")done=draw(*g);
  else if(verb=="resign")done=resign(*g);
  else if(verb=="resend")done=resend(*g);
  else if(verb=="remove")done=remove(*g);
  else if(verb=="show")return detail(*g);
  else if(verb=="seen"){viewed(*g);return "OK seen";}
  else return "ERR unknown duel command";
  return done?"OK "+verb:"ERR "+verb+" not possible now";
}
#else
DuelNet duels; // Duel.h: the stand-in
#endif
