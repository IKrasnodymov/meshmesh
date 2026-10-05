#include "ChessNet.h"
#include "ChessRating.h"
#include "ChessTour.h"
#include "ChessSync.h"
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
uint32_t unixNow(){time_t t=time(nullptr);return t>1700000000?uint32_t(t):0;}
// Saved form: "MMC1", version, count, then per game a fixed header and its moves; CRC-32 at the end.
constexpr uint8_t SaveVersion=4;          // 2: the open-move flag; 3: the rated result; 4: the tournament; older files are still read
constexpr size_t RecordHeader=8+2+25+7+8+1+80+2,RatingBlock=4+32+8+64+64,TourBlock=4;
// Automatic resending: after 2, 5 and 10 minutes, then every 15; at once when the other player is
// heard again (at most every 2 min, so a one-way link does not fill the air); stops after 24 h without confirmation (R still resends).
constexpr uint32_t RetryDelays[]={120000,300000,600000,900000},RetryWindow=86400000,HeardGap=120000;
uint32_t retryDelay(uint8_t n){return RetryDelays[n<3?n:3];}
bool inFlight(uint8_t s){return s==ChatMessage::Queued||s==ChatMessage::Sent;}
uint32_t crc32(const uint8_t* p,size_t n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(int k=0;k<8;k++)c=c>>1^(0xedb88320&-(c&1));}return ~c;}
void put(uint8_t*& w,const void* v,size_t n){memcpy(w,v,n);w+=n;}
void get(const uint8_t*& r,void* v,size_t n){memcpy(v,r,n);r+=n;}
}
bool ChessMatch::pending() const{return moveOpen||sigOpen||(out[0]&&outStatus!=Delivered);}
bool ChessMatch::sending() const{return (moveOpen&&inFlight(moveStatus))||(sigOpen&&inFlight(sigStatus))||(out[0]&&inFlight(outStatus));}
uint8_t ChessMatch::link() const{
  if((moveOpen&&moveStatus==Failed)||(sigOpen&&sigStatus==Failed)||(out[0]&&outStatus==Failed))return Failed;
  if(moveOpen)return moveStatus;if(sigOpen)return sigStatus;return out[0]?outStatus:0;
}

void ChessNet::begin(){load();}
void ChessNet::tick(){
  uint32_t now=millis();
  // Rated results: the book needs the node key; signing and checking run here, not in the radio callback.
  if(!rating::book.ready){const uint8_t* key=meshRadio.nodeKey();if(key){rating::book.begin(key,config.name);dirty=true;}}
  for(auto& m:matches){
    if(m.state!=ChessMatch::Over||!rating::book.ready)continue;
    if(m.sign==ChessMatch::SignDue)signResult(m);
    if(m.sign==ChessMatch::SignSent&&m.theirSigned)storeResult(m);
  }
  for(auto& m:matches)if(m.state==ChessMatch::Over&&m.tour&&!m.tourReported){m.tourReported=true;tour::net.gameOver(m);changed(m);}
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
  else if(m.sigOpen&&!inFlight(m.sigStatus))sendSig(m);
  else if(m.out[0]&&m.outStatus==Failed){String t=m.out;send(m,t);}
  dirty=true;
}
// Everything sent from here is confirmed: the next failure starts a new series of attempts.
void ChessNet::confirmed(ChessMatch& m){if(m.pending())return;m.retries=0;m.retryAt=0;m.openSince=0;m.autoStopped=false;}
ChessMatch* ChessNet::find(uint64_t peer,uint16_t id){for(auto& m:matches)if(m.state!=ChessMatch::Free&&m.peer==peer&&m.id==id)return &m;return nullptr;}
ChessMatch* ChessNet::find(uint16_t id){for(auto& m:matches)if(m.state!=ChessMatch::Free&&m.id==id)return &m;return nullptr;}
unsigned ChessNet::count() const{unsigned n=0;for(auto& m:matches)n+=m.state!=ChessMatch::Free;return n;}
unsigned ChessNet::waiting() const{unsigned n=0;for(auto& m:matches)n+=m.state==ChessMatch::Invited||m.myTurn()||m.unseen;return n;}
// A free slot, or the finished game that changed longest ago. A tournament pairing (force) may also take a
// finished game nobody opened (a board without a chess screen never opens them) once its rating and report are done.
ChessMatch* ChessNet::slot(bool force){
  ChessMatch* oldest=nullptr;
  for(auto& m:matches){if(m.state==ChessMatch::Free)return &m;if(m.state==ChessMatch::Over&&!m.unseen&&(!oldest||int32_t(m.changedAt-oldest->changedAt)<0))oldest=&m;}
  if(oldest||!force)return oldest;
  for(auto& m:matches){bool done=m.state==ChessMatch::Over&&!m.sigOpen&&m.sign!=ChessMatch::SignDue&&!(m.sign==ChessMatch::SignSent&&!m.theirSigned)&&(!m.tour||m.tourReported);
    if(done&&(!oldest||int32_t(m.changedAt-oldest->changedAt)<0))oldest=&m;}
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
    if(m.sigOpen&&m.sigId==id){m.sigStatus=status;m.sigOpen=status!=Delivered;hit=true;}
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
// A rated game with a result and a move from each side gets signed (in tick()).
void ChessNet::finish(ChessMatch& m,ChessMatch::Result result,ChessMatch::Reason reason){m.state=ChessMatch::Over;m.result=result;m.reason=reason;m.drawOffer=ChessMatch::NoOffer;
  if(m.rated&&result!=ChessMatch::Undecided&&m.game.plies>=2&&m.sign==ChessMatch::SignNone)m.sign=ChessMatch::SignDue;}
// The signed record: White's and Black's keys, the game, its result and moves.
bool ChessNet::core(const ChessMatch& m,uint8_t* out) const{
  const uint8_t* self=meshRadio.nodeKey();bool known=false;for(uint8_t b:m.peerKey)known|=b!=0;if(!self||!known)return false;
  rating::makeCore(out,m.mine==White?self:m.peerKey,m.mine==White?m.peerKey:self,m.id,m.result,m.reason,m.game.moves,m.game.plies);return true;
}
// "♟3F2A sig 1791140000 <base64>": my clock and my signature of the result.
bool ChessNet::sendSig(ChessMatch& m){
  char sig[rating::SigTextSize+1],t[128];rating::sigText(m.mySig,sig);snprintf(t,sizeof t,"%s%04X sig %lu %s",Tag,m.id,(unsigned long)m.myTime,sig);
  m.sigId=meshRadio.sendGame(t,m.peer);m.sigStatus=m.sigId?Queued:Failed;m.triedAt=millis();dirty=true;return m.sigId;
}
void ChessNet::signResult(ChessMatch& m){
  uint8_t c[rating::CoreSize],bytes[rating::CoreSize+4];if(!core(m,c)){m.sign=ChessMatch::SignBad;changed(m);return;}
  m.myTime=unixNow();rating::signedBytes(bytes,c,m.myTime);if(!meshRadio.nodeSign(m.mySig,bytes,sizeof bytes))return; // tried again on the next tick
  m.sign=ChessMatch::SignSent;m.sigOpen=true;m.retries=0;m.retryAt=0;m.openSince=0;m.autoStopped=false;sendSig(m);changed(m);
}
// Their signature must be over the same result: then the record goes into the rating book.
void ChessNet::storeResult(ChessMatch& m){
  uint8_t c[rating::CoreSize],bytes[rating::CoreSize+4];if(!core(m,c)){m.sign=ChessMatch::SignBad;changed(m);return;}
  rating::signedBytes(bytes,c,m.theirTime);
  if(!MeshRadio::nodeVerify(m.peerKey,m.theirSig,bytes,sizeof bytes)){m.sign=ChessMatch::SignBad;news(m,String(m.name)+tr(": signatures differ, the game is not rated",": подписи не сошлись, партия без рейтинга"));changed(m);return;}
  rating::Record r;memcpy(r.core,c,sizeof c);bool white=m.mine==White;
  r.timeW=white?m.myTime:m.theirTime;r.timeB=white?m.theirTime:m.myTime;memcpy(r.sigW,white?m.mySig:m.theirSig,64);memcpy(r.sigB,white?m.theirSig:m.mySig,64);
  rating::book.add(r,m.name);m.sign=ChessMatch::SignStored; // a record already there (a second copy of the game) counts once
  if(const rating::Delta* d=rating::book.delta(m.peer,m.id)){int diff=d->after-d->before;
    news(m,tr("Rating: ","Рейтинг: ")+String(d->before)+" > "+String(d->after)+" ("+(diff>=0?"+":"")+String(diff)+")"+(d->counted?String():String(tr(", over the daily limit",", сверх дневного предела"))));}
  changed(m);
}
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
  char a[12]={},b[8]={},f[4]={};if(sscanf(c+5,"%11s %7s %3s",a,b,f)<1)return false;
  ChessMatch* m=find(from,id);
  if(!strcmp(a,"new")){
    if(b[0]!='w'&&b[0]!='b')return false;if(m)return true;
    m=slot();
    if(!m){meshRadio.sendGame(String(Tag)+String(c).substring(0,4)+" no",from);event=String(name)+tr(": challenge refused, no free board",": вызов отклонён, нет свободной доски");events++;dirty=true;return true;}
    *m=ChessMatch();m->peer=from;m->id=id;strlcpy(m->name,name,sizeof m->name);m->mine=b[0]=='w'?Black:White;m->state=ChessMatch::Invited;m->started=unixNow();m->game.reset();
    m->rated=!strcmp(f,"r");for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==from)memcpy(m->peerKey,meshRadio.peers[i].publicKey,32);
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
  if(!strcmp(a,"sig")){ // their signature of the result; checked in tick() once mine is made
    if(!m->rated||m->sign==ChessMatch::SignStored||m->sign==ChessMatch::SignBad)return true;
    const char* t=strstr(c+5,"sig ");char* e=nullptr;unsigned long time=strtoul(t+4,&e,10);if(!e||*e!=' '||!rating::sigParse(e+1,m->theirSig))return true;
    m->theirTime=time;m->theirSigned=true;changed(*m,false);return true;
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
ChessMatch* ChessNet::invite(uint64_t peer,int color,bool rated){
  Peer* p=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==peer)p=&meshRadio.peers[i];
  if(!p||p->type!=1){event=tr("Chess: choose a chat contact","Шахматы: выберите чат-контакт");events++;dirty=true;return nullptr;}
  ChessMatch* m=slot();if(!m){event=tr("Chess: all boards busy, finish a game","Шахматы: все доски заняты, завершите партию");events++;dirty=true;return nullptr;}
  uint16_t id;do id=1+random(0xffff);while(find(id));
  *m=ChessMatch();m->peer=peer;m->id=id;strlcpy(m->name,p->name,sizeof m->name);m->mine=color==2?random(2):color&1;m->state=ChessMatch::Inviting;m->started=unixNow();m->game.reset();
  m->rated=rated;memcpy(m->peerKey,p->publicKey,32);
  char head[20];snprintf(head,sizeof head,"%s%04X new %c%s",Tag,id,m->mine==White?'w':'b',rated?" r":"");
  send(*m,String(head)+(m->mine==White?tr(" · MeshMesh chess: you play Black"," · шахматы MeshMesh: вы играете чёрными"):tr(" · MeshMesh chess: you play White"," · шахматы MeshMesh: вы играете белыми")));
  changed(*m);return m;
}
ChessMatch* ChessNet::tourGame(uint64_t peer,const uint8_t key[32],const char* name,uint16_t id,int color,uint16_t tourId,uint8_t round){
  if(ChessMatch* old=find(peer,id))return old;
  meshRadio.learnContact(key,name);ChessMatch* m=slot(true);if(!m)return nullptr;
  *m=ChessMatch();m->peer=peer;m->id=id;strlcpy(m->name,name,sizeof m->name);m->mine=color&1;m->state=ChessMatch::Playing;m->started=unixNow();m->game.reset();
  m->rated=true;memcpy(m->peerKey,key,32);m->tour=tourId;m->round=round;m->unseen=m->mine==White;
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
  saveDue=false;size_t cap=8+MaxMatches*(RecordHeader+2*MaxPlies+RatingBlock+TourBlock)+4;uint8_t* buf=(uint8_t*)malloc(cap);if(!buf)return;
  uint8_t* w=buf;put(w,"MMC1",4);*w++=SaveVersion;uint8_t* countAt=w++;*w++=0;*w++=0;uint8_t n=0;
  for(auto& m:matches){
    if(m.state==ChessMatch::Free)continue;n++;
    put(w,&m.peer,8);put(w,&m.id,2);put(w,m.name,25);uint8_t f[7]={m.mine,m.state,m.result,m.reason,m.drawOffer,m.unseen,m.moveOpen};put(w,f,7);
    put(w,&m.started,4);put(w,&m.updated,4);put(w,&m.outStatus,1);put(w,m.out,80);put(w,&m.game.plies,2);put(w,m.game.moves,2*m.game.plies);
    uint8_t r[4]={m.rated,m.sign,m.theirSigned,m.sigOpen};put(w,r,4);put(w,m.peerKey,32);put(w,&m.myTime,4);put(w,&m.theirTime,4);put(w,m.mySig,64);put(w,m.theirSig,64);
    put(w,&m.tour,2);put(w,&m.round,1);put(w,&m.tourReported,1);
  }
  *countAt=n;uint32_t crc=crc32(buf,w-buf);put(w,&crc,4);
  if(!chessStoreWrite(buf,w-buf)){event=tr("Chess: cannot save games","Шахматы: не удалось сохранить партии");events++;}
  free(buf);
}
void ChessNet::load(){
  size_t cap=8+MaxMatches*(RecordHeader+2*MaxPlies+RatingBlock+TourBlock)+4;uint8_t* buf=(uint8_t*)malloc(cap);if(!buf)return;
  size_t size=chessStoreRead(buf,cap);uint32_t crc;
  if(size<12||memcmp(buf,"MMC1",4)||buf[4]<1||buf[4]>SaveVersion||(memcpy(&crc,buf+size-4,4),crc!=crc32(buf,size-4))){free(buf);return;}
  const uint8_t* r=buf+8;const uint8_t* end=buf+size-4;unsigned count=buf[5],slot=0,dropped=0,flags=buf[4]==1?6:7;
  for(unsigned i=0;i<count&&slot<MaxMatches&&r+RecordHeader-7+flags<=end;i++){
    ChessMatch& m=matches[slot];m=ChessMatch();uint8_t f[7]={};uint16_t plies;
    get(r,&m.peer,8);get(r,&m.id,2);get(r,m.name,25);m.name[24]=0;get(r,f,flags);get(r,&m.started,4);get(r,&m.updated,4);get(r,&m.outStatus,1);get(r,m.out,80);m.out[79]=0;get(r,&plies,2);
    if(plies>MaxPlies||r+2*plies>end)break;Move moves[MaxPlies];get(r,moves,2*plies);
    if(buf[4]>=3){if(r+RatingBlock>end)break;uint8_t q[4];get(r,q,4);m.rated=q[0];m.sign=q[1]<=ChessMatch::SignBad?q[1]:ChessMatch::SignNone;m.theirSigned=q[2];m.sigOpen=q[3];
      get(r,m.peerKey,32);get(r,&m.myTime,4);get(r,&m.theirTime,4);get(r,m.mySig,64);get(r,m.theirSig,64);}
    if(buf[4]>=4){if(r+TourBlock>end)break;get(r,&m.tour,2);get(r,&m.round,1);get(r,&m.tourReported,1);}
    m.mine=f[0]&1;m.state=ChessMatch::State(f[1]);m.result=ChessMatch::Result(f[2]);m.reason=ChessMatch::Reason(f[3]);m.drawOffer=ChessMatch::Offer(f[4]);m.unseen=f[5];
    // Replaying checks every move; a damaged game is dropped rather than shown wrong.
    if(m.state==ChessMatch::Free||m.state>ChessMatch::Over||!m.game.load(moves,plies)){m=ChessMatch();dropped++;continue;}
    if(m.outStatus!=Delivered&&m.out[0])m.outStatus=Failed; // unconfirmed before the restart: not delivered
    // Version 1 kept no flag: the last move is open when it is mine and the last command was it, unconfirmed.
    const char* verb=strchr(m.out,' ');
    m.moveOpen=flags==7?f[6]!=0:m.game.plies&&((m.game.plies-1)&1)==m.mine&&m.outStatus==Failed&&verb&&verb[1]>='1'&&verb[1]<='9';
    if(m.moveOpen&&!m.game.plies)m.moveOpen=false;
    if(m.moveOpen)m.moveStatus=Failed;
    if(m.sigOpen)m.sigStatus=Failed;
    if(m.pending()){m.openSince=millis();m.retryAt=millis()+60000;} // resend a minute after the start
    m.changedAt=millis()-(count-i);slot++;
  }
  free(buf);if(dropped){event=tr("Chess: a damaged game was dropped","Шахматы: повреждённая партия удалена");events++;}dirty=true;
}

String chessLocalSan(const char* san){
  String out;
  for(const char* c=san;*c;c++){
    bool piece=strchr("KQRBN",*c)&&(c==san||c[-1]=='=');
    if(piece&&config.lang==LangRu){switch(*c){case 'K':out+="Кр";break;case 'Q':out+="Ф";break;case 'R':out+="Л";break;case 'B':out+="С";break;default:out+="К";}}
    else out+=*c;
  }
  return out;
}
String ChessNet::web() const{StaticJsonDocument<256> d;d["events"]=events;d["event"]=event;d["waiting"]=waiting();d["elo"]=rating::book.myElo();d["tours"]=tour::net.waiting(); // the page shows the Tournaments tab
String s;serializeJson(d,s);s.remove(s.length()-1);return s+",\"games\":"+json()+"}";}
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
    if(m.tour){char tid[5];snprintf(tid,sizeof tid,"%04X",m.tour);o["tour"]=tid;o["round"]=m.round;if(const tour::Tour* t=tour::net.find(m.tour))o["tour_name"]=t->name;}
    static const char* signs[]={"none","due","sent","stored","bad"};o["rated"]=m.rated;if(m.rated){o["sign"]=signs[m.sign];o["their_signed"]=m.theirSigned;}
    if(const rating::Delta* r=m.sign==ChessMatch::SignStored?rating::book.delta(m.peer,m.id):nullptr){o["elo_before"]=r->before;o["elo_after"]=r->after;o["elo_counted"]=r->counted;}
  }
  String s;serializeJson(d,s);return s;
}
String ChessNet::command(const String& line){
  if(line=="chess")return json();
  if(line=="chess web")return web();
  if(line=="chess rating"){String s=rating::book.json();s.remove(s.length()-1);return s+",\"ledger\":"+ledger::exchange.json()+"}";}
  // chess sync [NODE_ID]: compare ledgers now with one node, or with every rated player heard in a day.
  if(line=="chess sync"){unsigned n=ledger::exchange.startAll();return "OK sync with "+String(n)+" nodes";}
  if(line.startsWith("chess sync ")){char* e=nullptr;uint64_t id=strtoull(line.c_str()+11,&e,16);return id&&e&&!*e&&ledger::exchange.start(id)?String("OK sync started"):String("ERR chess sync [NODE_ID]");}
  if(line=="chess rating clear")return rating::book.clear()?"OK rating cleared":"ERR rating not ready";
  String rest=line.substring(6);rest.trim();int sp=rest.indexOf(' ');String verb=sp<0?rest:rest.substring(0,sp),arg=sp<0?String():rest.substring(sp+1);arg.trim();
  if(verb=="invite"){
    // chess invite NODE_ID [w|b|r] [friendly]: rated unless "friendly"
    bool friendly=arg.endsWith(" friendly");if(friendly)arg=arg.substring(0,arg.length()-9);
    int at=arg.indexOf(' ');String node=at<0?arg:arg.substring(0,at),color=at<0?String("r"):arg.substring(at+1);
    char* e=nullptr;uint64_t peer=strtoull(node.c_str(),&e,16);if(!peer||!e||*e)return "ERR chess invite NODE_ID w|b|r [friendly]";
    ChessMatch* m=invite(peer,color=="w"?White:color=="b"?Black:2,!friendly);if(!m)return "ERR "+event;char id[5];snprintf(id,sizeof id,"%04X",m->id);return String("OK game ")+id+(friendly?" friendly":" rated")+(m->mine==White?" white":" black");
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
