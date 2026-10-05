#include "ChessTour.h"
#include "ChessNet.h"
#include "ChessRating.h"
#include "MeshRadio.h"
#include "Config.h"
#include <ArduinoJson.h>
namespace tour {
Net net;
namespace {
const char Tag[]="\xe2\x99\x9e"; // ♞
constexpr uint8_t SaveVersion=1,DueStart=1,DueRound=2;
constexpr uint32_t RetryDelays[]={120000,300000,600000,900000},RetryWindow=86400000,BusyRetry=15000;
const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
uint64_t me(){return meshRadio.nodeId;}
// Players are sent as the first 6 hex digits of their ID: such a short ID matches by its prefix, full IDs exactly.
bool same(uint64_t a,uint64_t b){return (a<<24&&b<<24)?a==b:a>>40==b>>40;}
String hex6(uint64_t id){char b[8];snprintf(b,sizeof b,"%06lX",(unsigned long)(id>>40));return b;}
String hex4(uint16_t v){char b[6];snprintf(b,sizeof b,"%04X",v);return b;}
String keyText(const uint8_t k[32]){String s;for(int i=0;i<32;i+=3){uint32_t v=uint32_t(k[i])<<16|(i+1<32?k[i+1]<<8:0)|(i+2<32?k[i+2]:0);s+=B64[v>>18&63];s+=B64[v>>12&63];s+=i+1<32?B64[v>>6&63]:'=';s+=i+2<32?B64[v&63]:'=';}return s;}
bool keyParse(const char* t,uint8_t k[32]){size_t n=0;uint32_t v=0;int bits=0;for(int i=0;i<44;i++){char c=t[i];if(c=='=')continue;const char* p=c?strchr(B64,c):nullptr;if(!p)return false;v=v<<6|uint32_t(p-B64);bits+=6;if(bits>=8){bits-=8;if(n<32)k[n++]=v>>bits;}}return n==32;}
uint64_t idOfKey(const uint8_t k[32]){uint64_t n=0;for(int i=0;i<8;i++)n=n<<8|k[i];return n;}
uint32_t crc32(const uint8_t* p,size_t n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(int k=0;k<8;k++)c=c>>1^(0xedb88320&-(c&1));}return ~c;}
// A copy cut at a whole UTF-8 character.
void copyText(char* out,const char* in,size_t cap){size_t n=strnlen(in,cap);if(n>=cap){n=cap-1;while(n&&(uint8_t(in[n])&0xC0)==0x80)n--;}memcpy(out,in,n);out[n]=0;}
uint32_t retryDelay(uint8_t n){return RetryDelays[n<3?n:3];}
uint8_t resultOf(char c){return c=='w'?WhiteWon:c=='b'?BlackWon:c=='d'?Drawn:Pending;}
char resultChar(uint8_t r){return r==WhiteWon?'w':r==BlackWon?'b':'d';}
bool decided(uint8_t r){return r==WhiteWon||r==BlackWon||r==Drawn||r==Bye;}
Peer* peerOf(uint64_t id){for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==id)return &meshRadio.peers[i];return nullptr;}
// Points of everyone after the rounds that are finished or under way, in half points.
void score(const Tour& t,uint8_t* half,uint8_t* buch){
  memset(half,0,MaxPlayers);if(buch)memset(buch,0,MaxPlayers);
  for(unsigned r=0;r<t.round&&r<MaxRounds;r++)for(unsigned k=0;k<t.pairCount[r];k++){const Pair& p=t.pairs[r][k];
    if(p.result==Bye&&p.white<MaxPlayers)half[p.white]+=2;
    if(p.white>=MaxPlayers||p.black>=MaxPlayers)continue;
    if(p.result==WhiteWon)half[p.white]+=2;else if(p.result==BlackWon)half[p.black]+=2;else if(p.result==Drawn){half[p.white]++;half[p.black]++;}}
  if(buch)for(unsigned r=0;r<t.round&&r<MaxRounds;r++)for(unsigned k=0;k<t.pairCount[r];k++){const Pair& p=t.pairs[r][k];
    if(p.white<MaxPlayers&&p.black<MaxPlayers){buch[p.white]+=half[p.black];buch[p.black]+=half[p.white];}}
}
}
bool Tour::organising() const{return organizer&&organizer==meshRadio.nodeId;}
int Tour::index(uint64_t id) const{for(unsigned i=0;i<playerCount;i++)if(same(players[i].id,id))return i;return -1;}
const char* Tour::nameOf(uint64_t id) const{
  int i=index(id);if(i>=0&&players[i].name[0])return players[i].name;
  Peer* p=peerOf(id);if(p)return p->name;static char hex[8];snprintf(hex,sizeof hex,"%06lX",(unsigned long)(id>>40));return hex;
}

void Net::begin(){load();}
// In place: a tournament is large for the stack of the radio callback.
void Net::reset(Tour& t){memset(&t,0,sizeof t);t.rounds=5;t.hours=24;}
Tour* Net::find(uint16_t id){for(auto& t:tours)if(t.state!=Free&&t.id==id)return &t;return nullptr;}
Tour* Net::slot(){Tour* old=nullptr;for(auto& t:tours){if(t.state==Free)return &t;if(!t.active()&&(!old||int32_t(t.changedAt-old->changedAt)<0))old=&t;}return old;}
unsigned Net::waiting() const{unsigned n=0;for(auto& t:tours)n+=t.state==Invited||t.unseen;return n;}
void Net::news(Tour& t,const String& text){event=text;events++;eventTour=&t;t.unseen=true;dirty=true;}
void Net::changed(Tour& t,bool now){t.changedAt=millis();dirty=true;if(now)save();else if(!saveDue){saveDue=true;saveAt=millis()+3000;}}
void Net::viewed(Tour& t){if(t.unseen){t.unseen=false;changed(t,false);}}

// Outgoing commands wait for their ACK and are resent after 2, 5, 10 and then every 15 minutes, for a day.
void Net::send(uint64_t peer,const String& text){
  if(!peer||peer==me())return;Out* o=nullptr;
  for(auto& x:outbox)if(!x.used){o=&x;break;}
  if(!o){o=&outbox[0];for(auto& x:outbox)if(int32_t(x.since-o->since)<0)o=&x;event=tr("Tournament: message queue full","Турнир: очередь сообщений переполнена");events++;} // the oldest gives way
  *o=Out();o->used=true;o->peer=peer;strlcpy(o->text,text.c_str(),sizeof o->text);o->since=millis();transmit(*o);save();
}
void Net::transmit(Out& o){
  o.id=meshRadio.sendGame(o.text,o.peer);o.status=o.id?ChatMessage::Queued:ChatMessage::Failed;
  o.retryAt=millis()+(o.id?retryDelay(o.retries):BusyRetry);dirty=true;
}
bool Net::delivery(uint32_t id,uint8_t status){
  for(auto& o:outbox){if(!o.used||o.id!=id)continue;o.status=status;
    if(status==ChatMessage::Delivered){o.used=false;save();}else if(status==ChatMessage::Failed)o.retryAt=millis()+retryDelay(o.retries);
    return true;}
  return false;
}
void Net::tick(){
  uint32_t now=millis();
  for(auto& o:outbox){if(!o.used||o.status!=ChatMessage::Failed||int32_t(now-o.retryAt)<0)continue;
    if(now-o.since>RetryWindow){o.used=false;dirty=true;continue;}
    o.retries=o.retries<255?o.retries+1:255;transmit(o);}
  for(auto& t:tours){if(!t.due)continue;uint8_t due=t.due;t.due=0;if(due&DueStart)start(t);if(due&DueRound&&t.state==Running)roundDone(t);}
  // A pairing that found no free chess board: tried again every 30 s.
  if(now-boardAt>=30000){boardAt=now;
    for(auto& t:tours){if(t.state!=Running||!t.round)continue;const Pair* p=myPair(t,t.round);if(!p||p->black>=MaxPlayers||p->result!=Pending)continue;
      bool here=false;for(auto& m:chessNet.matches)here|=m.state!=ChessMatch::Free&&m.tour==t.id&&m.id==p->game;if(here)continue;
      const Player& op=t.players[myColor(t,*p)?p->white:p->black];if(op.hasKey)chessNet.tourGame(op.id,op.key,op.name,p->game,myColor(t,*p),t.id,t.round);}}
  if(saveDue&&int32_t(now-saveAt)>=0)save();
}

// The organiser asks the chosen chat contacts; it plays as well.
Tour* Net::create(const String& name,uint8_t rounds,uint8_t hours,const uint64_t* ids,unsigned n){
  if(!name.length()||n<1||n>MaxPlayers-1||rounds<1||rounds>MaxRounds){event=tr("Tournament: 2-10 players, 1-9 rounds","Турнир: 2-10 игроков, 1-9 туров");events++;dirty=true;return nullptr;}
  const uint8_t* self=meshRadio.nodeKey();Tour* t=slot();if(!self||!t){event=tr("Tournament: no free place","Турнир: нет свободного места");events++;dirty=true;return nullptr;}
  uint16_t id;do id=1+random(0xffff);while(find(id));
  reset(*t);t->id=id;t->state=Inviting;copyText(t->name,name.c_str(),sizeof t->name);t->organizer=me();t->rounds=rounds;t->hours=hours;
  Player& o=t->players[t->playerCount++];o.id=me();memcpy(o.key,self,32);o.hasKey=true;copyText(o.name,config.name,sizeof o.name);o.state=Joined;
  for(unsigned i=0;i<n;i++){Peer* p=peerOf(ids[i]);if(!p||p->type!=1||t->index(p->id)>=0)continue;
    Player& x=t->players[t->playerCount++];x.id=p->id;memcpy(x.key,p->publicKey,32);x.hasKey=true;copyText(x.name,p->name,sizeof x.name);x.state=Asked;}
  if(t->playerCount<2){reset(*t);event=tr("Tournament: choose chat contacts","Турнир: выберите чат-контакты");events++;dirty=true;return nullptr;}
  String inv=String(Tag)+hex4(id)+" inv "+String(rounds)+" "+String(hours)+" "+String(t->playerCount)+" "+t->name;
  for(unsigned i=1;i<t->playerCount;i++)send(t->players[i].id,inv);
  changed(*t);return t;
}
bool Net::accept(Tour& t){if(t.state!=Invited)return false;t.state=Running;t.round=0;t.unseen=false;send(t.organizer,String(Tag)+hex4(t.id)+" yes");changed(t);return true;}
bool Net::decline(Tour& t){if(t.state!=Invited)return false;t.state=Declined;t.unseen=false;send(t.organizer,String(Tag)+hex4(t.id)+" no");changed(t);return true;}
bool Net::cancel(Tour& t){
  if(!t.organising()||!t.active())return false;
  for(unsigned i=1;i<t.playerCount;i++)if(t.players[i].state!=Refused)send(t.players[i].id,String(Tag)+hex4(t.id)+" cancel");
  t.state=Cancelled;changed(t);return true;
}
bool Net::remove(Tour& t){if(t.active())return false;reset(t);dirty=true;save();return true;}
// The start: only those who agreed play; the first round is paired and sent.
bool Net::start(Tour& t){
  if(!t.organising()||t.state!=Inviting)return false;
  unsigned n=0;for(unsigned i=0;i<t.playerCount;i++)if(t.players[i].state==Joined)t.players[n++]=t.players[i];
  if(n<2){event=tr("Tournament: nobody has agreed yet","Турнир: пока никто не согласился");events++;dirty=true;return false;}
  t.playerCount=n;t.state=Running;t.round=1;pairRound(t);sendPlayers(t);announceRound(t);changed(t);return true;
}
// "pl ID6:name,ID6:name": the players, in messages that fit the MeshCore limit.
void Net::sendPlayers(Tour& t){
  String head=String(Tag)+hex4(t.id)+" pl ",line;
  auto flush=[&]{if(!line.length())return;for(unsigned i=1;i<t.playerCount;i++)send(t.players[i].id,head+line);line="";};
  for(unsigned i=0;i<t.playerCount;i++){String name=t.players[i].name;name.replace(",",".");name.replace(":",".");if(name.length()>12)name=name.substring(0,12);
    while(name.length()&&(uint8_t(name[name.length()-1])&0xC0)==0x80)name.remove(name.length()-1); // no cut UTF-8 character
    if(name.length()&&(uint8_t(name[name.length()-1])&0xC0)==0xC0)name.remove(name.length()-1);
    String entry=hex6(t.players[i].id)+":"+name;if(head.length()+line.length()+entry.length()+1>148)flush();line+=(line.length()?",":"")+entry;}
  flush();
}
// Pairing: round 1 by lot; later by score (then rating), top half against bottom half of a score group,
// nobody twice against the same opponent while it can be avoided, one bye at most per player.
void Net::pairRound(Tour& t){
  unsigned r=t.round-1,n=t.playerCount;uint8_t half[MaxPlayers],order[MaxPlayers];bool met[MaxPlayers][MaxPlayers]={};int balance[MaxPlayers]={};int8_t last[MaxPlayers];bool bye[MaxPlayers]={};
  memset(last,-1,sizeof last);score(t,half,nullptr);
  for(unsigned q=0;q<r;q++)for(unsigned k=0;k<t.pairCount[q];k++){const Pair& p=t.pairs[q][k];
    if(p.black>=MaxPlayers){if(p.white<MaxPlayers)bye[p.white]=true;continue;}met[p.white][p.black]=met[p.black][p.white]=true;balance[p.white]++;balance[p.black]--;last[p.white]=0;last[p.black]=1;}
  for(unsigned i=0;i<n;i++)order[i]=i;
  if(!r){uint32_t s=t.id*2654435761u|1;for(unsigned i=n-1;i>0;i--){s^=s<<13;s^=s>>17;s^=s<<5;unsigned j=s%(i+1);std::swap(order[i],order[j]);}}
  else{int elo[MaxPlayers];for(unsigned i=0;i<n;i++)elo[i]=rating::book.elo(t.players[i].id);
    for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0;j--){uint8_t a=order[j],b=order[j-1];if(half[a]>half[b]||(half[a]==half[b]&&elo[a]>elo[b]))std::swap(order[j],order[j-1]);else break;}}
  Pair out[MaxPairs];unsigned count=0,m=n;
  if(n&1){int pick=-1;for(int i=n-1;i>=0;i--)if(!bye[order[i]]){pick=i;break;}if(pick<0)pick=n-1;
    out[count++]={order[pick],NoPlayer,0,Bye,Bye,Bye};for(unsigned i=pick;i+1<n;i++)order[i]=order[i+1];m--;}
  uint8_t list[MaxPlayers];memcpy(list,order,m);
  if(!r){for(unsigned i=0;i<m/2;i++)out[count++]={list[i],list[i+m/2],0,Pending,Pending,Pending};}
  else{
    // Depth-first: the highest player left takes the best allowed opponent; repeats only if unavoidable.
    struct Search{const uint8_t* half;bool (*met)[MaxPlayers];bool strict;
      bool run(uint8_t* l,unsigned k,Pair* o,unsigned& c){
        if(!k)return true;uint8_t p=l[0];unsigned group=0;while(group<k&&half[l[group]]==half[p])group++;
        uint8_t cand[MaxPlayers];unsigned cn=0;for(unsigned i=1;i<k;i++)if(!strict||!met[p][l[i]])cand[cn++]=i;
        auto cost=[&](unsigned i){int d=abs(int(half[p])-int(half[l[i]]));int pos=i<group?abs(int(i)-int(group/2)):0;return d*100+pos;};
        for(unsigned a=1;a<cn;a++)for(unsigned b=a;b>0&&cost(cand[b])<cost(cand[b-1]);b--)std::swap(cand[b],cand[b-1]);
        for(unsigned a=0;a<cn;a++){unsigned i=cand[a];uint8_t rest[MaxPlayers];unsigned rn=0;for(unsigned j=1;j<k;j++)if(j!=i)rest[rn++]=l[j];
          o[c++]={p,l[i],0,Pending,Pending,Pending};if(run(rest,rn,o,c))return true;c--;}
        return false;}};
    Search s{half,met,true};unsigned base=count;if(!s.run(list,m,out,count)){count=base;s.strict=false;s.run(list,m,out,count);}
  }
  // Colours: fewer Whites so far gets White; then the one who had Black last; then by the lot.
  for(unsigned k=0;k<count;k++){Pair& p=out[k];if(p.black>=MaxPlayers)continue;uint8_t a=p.white,b=p.black;bool swap=false;
    if(balance[a]!=balance[b])swap=balance[a]>balance[b];else if(last[a]!=last[b])swap=last[a]==0||(last[a]<0&&last[b]==1);else swap=(t.id>>(k&15)&1)!=(r&1);
    if(swap)std::swap(p.white,p.black);
    uint16_t g;do g=1+random(0xffff);while(chessNet.find(g));p.game=g;}
  memcpy(t.pairs[r],out,sizeof(Pair)*count);t.pairCount[r]=count;
}
// "rN w GAME KEY NAME" to each player of a pair (the organiser starts its own game here), "rN bye" for a bye.
void Net::announceRound(Tour& t){
  unsigned r=t.round-1;String head=String(Tag)+hex4(t.id)+" r"+String(t.round)+" ";
  for(unsigned k=0;k<t.pairCount[r];k++){const Pair& p=t.pairs[r][k];
    if(p.black>=MaxPlayers){if(t.players[p.white].id==me())news(t,tr("Round ","Тур ")+String(t.round)+tr(": a bye for you (+1)",": вы свободны от игры (+1)"));else send(t.players[p.white].id,head+"bye");continue;}
    bool local=false;
    for(int side=0;side<2;side++){const Player& pl=t.players[side?p.black:p.white];const Player& op=t.players[side?p.white:p.black];
      if(pl.id==me()){local=true;continue;}
      send(pl.id,head+(side?"b ":"w ")+hex4(p.game)+" "+keyText(op.key)+" "+op.name);}
    if(local)localGame(t,p);
  }
}
void Net::localGame(Tour& t,const Pair& p){
  bool white=t.players[p.white].id==me();const Player& op=t.players[white?p.black:p.white];
  ChessMatch* m=chessNet.tourGame(op.id,op.key,op.name,p.game,white?0:1,t.id,t.round);
  news(t,String(t.name)+" · "+tr("round ","тур ")+String(t.round)+": "+op.name+(white?tr(", you play White",", вы белыми"):tr(", you play Black",", вы чёрными")));
  if(!m){event=tr("Tournament: no free chess board","Турнир: нет свободной доски");events++;}
}
// "st ROUND ID6:POINTS*2:BUCHHOLZ*2,..." after a round; "end" carries the final standings the same way,
// so the last place is right whichever message comes first.
void Net::sendStandings(Tour& t,const char* verb){
  Row rows[MaxPlayers];unsigned n=standings(t,rows);String line=String(Tag)+hex4(t.id)+" "+verb+" "+String(t.round)+" ";
  for(unsigned i=0;i<n;i++)line+=(i?",":"")+hex6(rows[i].id)+":"+String(rows[i].half)+":"+String(rows[i].buch);
  for(unsigned i=1;i<t.playerCount;i++)send(t.players[i].id,line);
}
// A report from one side of a game (the organiser's own game reports here directly).
void Net::record(Tour& t,uint16_t game,uint64_t from,uint8_t result){
  if(!t.organising()||t.state!=Running||!t.round||!result)return;unsigned r=t.round-1;
  for(unsigned k=0;k<t.pairCount[r];k++){Pair& p=t.pairs[r][k];if(p.game!=game||p.black>=MaxPlayers)continue;
    if(same(t.players[p.white].id,from))p.reportW=result;else if(same(t.players[p.black].id,from))p.reportB=result;else return;
    if(p.reportW&&p.reportB&&!decided(p.result)){
      if(p.reportW==p.reportB)p.result=p.reportW;
      else if(p.result!=Disputed){p.result=Disputed;news(t,String(t.name)+": "+tr("results differ in game ","результаты расходятся в партии ")+hex4(game));}}
    t.due|=DueRound;changed(t);return;}
}
bool Net::setResult(Tour& t,uint16_t game,uint8_t result){
  if(!t.organising()||t.state!=Running||!t.round||!(result==WhiteWon||result==BlackWon||result==Drawn))return false;unsigned r=t.round-1;
  for(unsigned k=0;k<t.pairCount[r];k++){Pair& p=t.pairs[r][k];if(p.game!=game||p.black>=MaxPlayers)continue;p.result=p.reportW=p.reportB=result;t.due|=DueRound;changed(t);return true;}
  return false;
}
// Every game of the round decided: standings to everyone, then the next round or the end.
void Net::roundDone(Tour& t){
  unsigned r=t.round-1;for(unsigned k=0;k<t.pairCount[r];k++)if(!decided(t.pairs[r][k].result))return;
  if(t.round<t.rounds){sendStandings(t);t.round++;pairRound(t);announceRound(t);}
  else{t.state=Over;sendStandings(t,"end");
    Row rows[MaxPlayers];unsigned n=standings(t,rows),place=0;for(unsigned i=0;i<n;i++)if(rows[i].me)place=i+1;
    news(t,String(t.name)+tr(": finished, your place ",": турнир окончен, ваше место ")+String(place)+tr(" of "," из ")+String(n));}
  changed(t);
}
// A tournament game ended on this device: the result goes to the organiser (or is recorded here).
void Net::gameOver(ChessMatch& m){
  Tour* t=find(m.tour);if(!t||m.result==ChessMatch::Undecided)return;uint8_t res=m.result==ChessMatch::WhiteWon?WhiteWon:m.result==ChessMatch::BlackWon?BlackWon:Drawn;
  if(m.round&&m.round<=MaxRounds)for(unsigned k=0;k<t->pairCount[m.round-1];k++){Pair& p=t->pairs[m.round-1][k];if(p.game==m.id&&!t->organising())p.result=res;}
  if(t->organising())record(*t,m.id,me(),res);else send(t->organizer,String(Tag)+hex4(t->id)+" res "+hex4(m.id)+" "+resultChar(res));
  changed(*t);
}

bool Net::receive(uint64_t from,const char* name,const char* text){
  if(strncmp(text,Tag,3))return false;
  const char* c=text+3;uint16_t id=0;
  for(int i=0;i<4;i++){char h=c[i];int v=h>='0'&&h<='9'?h-'0':(h|32)>='a'&&(h|32)<='f'?(h|32)-'a'+10:-1;if(v<0)return false;id=id<<4|v;}
  if(c[4]!=' '||!id)return false;
  const char* rest=c+5;char verb[8]={};int used=0;if(sscanf(rest,"%7s%n",verb,&used)<1)return false;const char* args=rest+used;while(*args==' ')args++;
  Tour* t=find(id);
  if(!strcmp(verb,"inv")){
    if(t)return true;unsigned rounds=0,hours=0,count=0;int at=0;if(sscanf(args,"%u %u %u %n",&rounds,&hours,&count,&at)<3||!at||!rounds||rounds>MaxRounds)return true;
    t=slot();if(!t){event=String(name)+tr(": tournament invitation, no free place",": приглашение в турнир, нет места");events++;dirty=true;return true;}
    reset(*t);t->id=id;t->state=Invited;t->organizer=from;t->rounds=rounds;t->hours=hours;copyText(t->name,args+at,sizeof t->name);
    news(*t,String(name)+tr(" invites you to the tournament \""," приглашает в турнир «")+t->name+tr("\"","»"));changed(*t);return true;
  }
  if(!t)return true;
  if(t->organising()){
    int i=t->index(from);if(i<0)return true;
    if(!strcmp(verb,"yes")||!strcmp(verb,"no")){
      if(t->state!=Inviting||t->players[i].state!=Asked)return true;t->players[i].state=strcmp(verb,"yes")?Refused:Joined;
      news(*t,String(t->players[i].name)+(t->players[i].state==Joined?tr(" joins \""," участвует в «"):tr(" declines \""," отказался от «"))+t->name+tr("\"","»"));
      bool all=true;unsigned joined=0;for(unsigned k=0;k<t->playerCount;k++){all&=t->players[k].state!=Asked;joined+=t->players[k].state==Joined;}
      if(all&&joined>=2)t->due|=DueStart; // started from tick(): pairing and sending need more stack than the radio callback has
      changed(*t);return true;
    }
    if(!strcmp(verb,"res")){unsigned g=0;char r[4]={};if(sscanf(args,"%x %3s",&g,r)==2)record(*t,uint16_t(g),from,resultOf(r[0]));return true;}
    return true;
  }
  if(from!=t->organizer)return true; // a player only takes the organiser's commands
  if(!strcmp(verb,"pl")){
    String list=args;int at=0;
    while(at<int(list.length())){int end=list.indexOf(',',at);if(end<0)end=list.length();String e=list.substring(at,end);at=end+1;int colon=e.indexOf(':');if(colon!=6)continue;
      uint64_t pid=strtoull(e.substring(0,6).c_str(),nullptr,16)<<40;if(t->index(pid)>=0||t->playerCount>=MaxPlayers)continue;
      Player& p=t->players[t->playerCount++];p=Player();p.id=pid;copyText(p.name,e.substring(colon+1).c_str(),sizeof p.name);p.state=Joined;}
    if(t->state==Invited)t->state=Running;changed(*t,false);return true;
  }
  if(verb[0]=='r'&&verb[1]>='1'&&verb[1]<='9'){
    unsigned round=atoi(verb+1);if(!round||round>MaxRounds)return true;
    if(t->state==Invited||t->state==Running)t->state=Running;else return true;
    if(round<t->round)return true;t->round=round;
    if(t->index(me())<0&&t->playerCount<MaxPlayers){Player& p=t->players[t->playerCount++];p=Player();p.id=me();copyText(p.name,config.name,sizeof p.name);p.state=Joined;}
    uint8_t mine=t->index(me());
    if(!strncmp(args,"bye",3)){if(!t->pairCount[round-1]){t->pairs[round-1][0]={mine,NoPlayer,0,Bye,Bye,Bye};t->pairCount[round-1]=1;
      news(*t,String(t->name)+": "+tr("round ","тур ")+String(round)+tr(": a bye for you (+1)",": вы свободны от игры (+1)"));}changed(*t);return true;}
    char color=0;unsigned game=0;char key[48]={};int at=0;if(sscanf(args,"%c %x %47s %n",&color,&game,key,&at)<3||(color!='w'&&color!='b')||!game)return true;
    uint8_t k32[32];if(!keyParse(key,k32))return true;const char* oname=at?args+at:"";
    if(t->pairCount[round-1]&&t->pairs[round-1][0].game==game)return true; // repeat
    uint64_t oid=idOfKey(k32);int oi=t->index(oid);
    if(oi<0&&t->playerCount<MaxPlayers){oi=t->playerCount++;t->players[oi]=Player();t->players[oi].state=Joined;}
    if(oi<0)return true;Player& op=t->players[oi];op.id=oid;memcpy(op.key,k32,32);op.hasKey=true;if(oname[0])copyText(op.name,oname,sizeof op.name);
    t->pairs[round-1][0]={color=='w'?mine:uint8_t(oi),color=='w'?uint8_t(oi):mine,uint16_t(game),Pending,Pending,Pending};t->pairCount[round-1]=1;
    ChessMatch* m=chessNet.tourGame(oid,k32,op.name,uint16_t(game),color=='w'?0:1,t->id,round);
    news(*t,String(t->name)+" · "+tr("round ","тур ")+String(round)+": "+op.name+(color=='w'?tr(", you play White",", вы белыми"):tr(", you play Black",", вы чёрными")));
    if(!m){event=tr("Tournament: no free chess board","Турнир: нет свободной доски");events++;}
    changed(*t);return true;
  }
  if(!strcmp(verb,"st")){readStandings(*t,args);changed(*t,false);return true;}
  if(!strcmp(verb,"end")){bool fresh=readStandings(*t,args);if(t->state==Over){if(fresh)changed(*t,false);return true;}t->state=Over;Row rows[MaxPlayers];unsigned n=standings(*t,rows),place=0;for(unsigned i=0;i<n;i++)if(rows[i].me)place=i+1;
    news(*t,String(t->name)+tr(": finished, your place ",": турнир окончен, ваше место ")+String(place)+tr(" of "," из ")+String(n));changed(*t);return true;}
  if(!strcmp(verb,"cancel")){if(!t->active())return true;t->state=Cancelled;news(*t,String(t->name)+tr(": cancelled by the organiser",": отменён организатором"));changed(*t);return true;}
  return true;
}

// Standings from the organiser; an older round's list does not replace a newer one.
bool Net::readStandings(Tour& t,const char* args){
  unsigned round=0;int at=0;if(sscanf(args,"%u %n",&round,&at)<1||!at||round<t.standRound)return false;
  String list=args+at;int pos=0;unsigned n=0;
  while(pos<int(list.length())&&n<MaxPlayers){int end=list.indexOf(',',pos);if(end<0)end=list.length();String e=list.substring(pos,end);pos=end+1;unsigned h=0,b=0;char idh[8]={};
    if(sscanf(e.c_str(),"%6[0-9A-Fa-f]:%u:%u",idh,&h,&b)<2)continue;t.standId[n]=strtoull(idh,nullptr,16)<<40;t.standHalf[n]=h;t.standBuch[n]=b;n++;}
  if(!n)return false;t.standCount=n;t.standRound=round;return true;
}
unsigned Net::standings(const Tour& t,Row* out) const{
  unsigned n=0;
  if(t.organising()){uint8_t half[MaxPlayers],buch[MaxPlayers];score(t,half,buch);
    for(unsigned i=0;i<t.playerCount;i++)if(t.players[i].state==Joined)out[n++]={t.players[i].id,t.players[i].name,half[i],buch[i],t.players[i].id==me()};
    for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0;j--){Row& a=out[j];Row& b=out[j-1];
      bool up=a.half>b.half||(a.half==b.half&&(a.buch>b.buch||(a.buch==b.buch&&rating::book.elo(a.id)>rating::book.elo(b.id))));if(up)std::swap(a,b);else break;}
    return n;}
  if(t.standCount){for(unsigned i=0;i<t.standCount;i++)out[n++]={t.standId[i],t.nameOf(t.standId[i]),t.standHalf[i],t.standBuch[i],same(t.standId[i],me())};return n;}
  for(unsigned i=0;i<t.playerCount;i++)out[n++]={t.players[i].id,t.players[i].name,0,0,same(t.players[i].id,me())};
  return n;
}
const Pair* Net::myPair(const Tour& t,uint8_t round) const{
  if(!round||round>MaxRounds)return nullptr;int mine=t.index(me());if(mine<0)return nullptr;
  for(unsigned k=0;k<t.pairCount[round-1];k++){const Pair& p=t.pairs[round-1][k];if(p.white==mine||p.black==mine)return &p;}
  return nullptr;
}
uint8_t Net::myColor(const Tour& t,const Pair& p) const{if(p.black>=MaxPlayers)return 2;return int(p.white)==t.index(me())?0:1;}
uint64_t Net::opponent(const Tour& t,const Pair& p) const{if(p.black>=MaxPlayers)return 0;const Player& o=t.players[myColor(t,p)?p.white:p.black];return o.id;}

String Net::json() const{
  DynamicJsonDocument d(12288);JsonArray list=d.to<JsonArray>();
  static const char* states[]={"free","inviting","invited","declined","running","over","cancelled"};static const char* results[]={"","white","black","draw","bye","disputed"};
  for(auto& t:tours){if(t.state==Free)continue;JsonObject o=list.createNestedObject();
    o["id"]=hex4(t.id);o["name"]=t.name;o["state"]=states[t.state];o["organizer"]=meshRadio.idText(t.organizer);o["organizer_name"]=t.organising()?config.name:t.nameOf(t.organizer);
    o["mine"]=t.organising();o["rounds"]=t.rounds;o["hours"]=t.hours;o["round"]=t.round;o["unseen"]=t.unseen;
    if(t.organising()){JsonArray ps=o.createNestedArray("players");static const char* ps_[]={"asked","joined","refused"};
      for(unsigned i=0;i<t.playerCount;i++){JsonObject p=ps.createNestedObject();p["id"]=meshRadio.idText(t.players[i].id);p["name"]=t.players[i].name;p["state"]=ps_[t.players[i].state<3?t.players[i].state:0];}}
    Row rows[MaxPlayers];unsigned n=standings(t,rows);JsonArray st=o.createNestedArray("standings");
    for(unsigned i=0;i<n;i++){JsonObject r=st.createNestedObject();r["id"]=hex6(rows[i].id);r["name"]=rows[i].name;r["points"]=rows[i].half/2.0;r["buchholz"]=rows[i].buch/2.0;r["me"]=rows[i].me;}
    JsonArray games=o.createNestedArray("my_games");
    for(unsigned r=1;r<=t.round&&r<=MaxRounds;r++){const Pair* p=myPair(t,r);if(!p)continue;JsonObject g=games.createNestedObject();g["round"]=r;uint8_t c=myColor(t,*p);
      g["color"]=c==2?"bye":c?"black":"white";if(c!=2){g["game"]=hex4(p->game);uint64_t op=opponent(t,*p);g["opponent"]=t.nameOf(op);}g["result"]=results[p->result<6?p->result:0];}
    if(t.organising()&&t.round){JsonArray pr=o.createNestedArray("pairs");unsigned r=t.round-1;
      for(unsigned k=0;k<t.pairCount[r];k++){const Pair& p=t.pairs[r][k];JsonObject x=pr.createNestedObject();x["white"]=t.players[p.white].name;
        if(p.black<MaxPlayers){x["black"]=t.players[p.black].name;x["game"]=hex4(p.game);}x["result"]=results[p.result<6?p.result:0];}}
  }
  String s;serializeJson(d,s);return s;
}
String Net::command(const String& line){
  if(line=="tour")return json();
  String rest=line.substring(5);rest.trim();int sp=rest.indexOf(' ');String verb=sp<0?rest:rest.substring(0,sp),arg=sp<0?String():rest.substring(sp+1);arg.trim();
  if(verb=="create"){ // tour create ROUNDS HOURS ID,ID,... NAME
    unsigned rounds=0,hours=0;char ids[200]={};int at=0;if(sscanf(arg.c_str(),"%u %u %199s %n",&rounds,&hours,ids,&at)<3||!at)return "ERR tour create ROUNDS HOURS NODE_ID,NODE_ID,... NAME";
    uint64_t list[MaxPlayers];unsigned n=0;for(char* p=strtok(ids,",");p&&n<MaxPlayers;p=strtok(nullptr,","))list[n++]=strtoull(p,nullptr,16);
    Tour* t=create(arg.substring(at),rounds,hours,list,n);return t?"OK tour "+hex4(t->id):"ERR "+event;
  }
  int at=arg.indexOf(' ');String tid=at<0?arg:arg.substring(0,at),extra=at<0?String():arg.substring(at+1);
  char* e=nullptr;unsigned long id=strtoul(tid.c_str(),&e,16);Tour* t=id&&e&&!*e&&tid.length()==4?find(uint16_t(id)):nullptr;
  if(!t)return "ERR tour create|accept|decline|start|cancel|remove|result|seen TOUR_ID ...";
  bool done=false;
  if(verb=="accept")done=accept(*t);else if(verb=="decline")done=decline(*t);else if(verb=="start")done=start(*t);else if(verb=="cancel")done=cancel(*t);
  else if(verb=="remove")done=remove(*t);else if(verb=="seen"){viewed(*t);done=true;}
  else if(verb=="result"){unsigned g=0;char r[4]={};done=sscanf(extra.c_str(),"%x %3s",&g,r)==2&&setResult(*t,uint16_t(g),resultOf(r[0]));}
  else return "ERR unknown tour command";
  return done?"OK "+verb:"ERR "+verb+" not possible now";
}

// Saved form: "MMT1", version, the size of a tournament, the tournaments and the outbox, CRC-32.
void Net::save(){
  saveDue=false;size_t size=8+sizeof tours+sizeof outbox+4;uint8_t* buf=(uint8_t*)malloc(size);if(!buf)return;
  memcpy(buf,"MMT1",4);buf[4]=SaveVersion;buf[5]=MaxTours;uint16_t ts=sizeof(Tour);memcpy(buf+6,&ts,2);memcpy(buf+8,tours,sizeof tours);memcpy(buf+8+sizeof tours,outbox,sizeof outbox);
  uint32_t crc=crc32(buf,size-4);memcpy(buf+size-4,&crc,4);if(!storeWrite(buf,size)){event=tr("Tournament: cannot save","Турнир: не удалось сохранить");events++;}free(buf);
}
void Net::load(){
  size_t size=8+sizeof tours+sizeof outbox+4;uint8_t* buf=(uint8_t*)malloc(size);if(!buf)return;uint16_t ts=0;uint32_t crc=0;
  if(storeRead(buf,size)==size&&!memcmp(buf,"MMT1",4)&&buf[4]==SaveVersion&&buf[5]==MaxTours&&(memcpy(&ts,buf+6,2),ts==sizeof(Tour))&&(memcpy(&crc,buf+size-4,4),crc==crc32(buf,size-4))){
    memcpy(tours,buf+8,sizeof tours);memcpy(outbox,buf+8+sizeof tours,sizeof outbox);
    for(auto& o:outbox)if(o.used&&o.status!=ChatMessage::Delivered){o.status=ChatMessage::Failed;o.retryAt=millis()+60000;o.since=millis();} // resent a minute after the start
    for(auto& t:tours)t.changedAt=millis();
  }
  free(buf);dirty=true;
}
}

#if !defined(MM_UI_PREVIEW)
#include <LittleFS.h>
#include "Hardware.h"
namespace tour {
bool storeWrite(const uint8_t* data,size_t size){
  if(!hardware.fsOk)return false;LittleFS.mkdir("/meshmesh");
  File f=LittleFS.open("/meshmesh/tour.new","w");if(!f)return false;bool ok=f.write(data,size)==size;f.close();if(!ok)return false;
  LittleFS.remove("/meshmesh/tour.old");LittleFS.rename("/meshmesh/tour.bin","/meshmesh/tour.old");
  return LittleFS.rename("/meshmesh/tour.new","/meshmesh/tour.bin");
}
size_t storeRead(uint8_t* data,size_t cap){
  if(!hardware.fsOk)return 0;
  for(const char* path:{"/meshmesh/tour.bin","/meshmesh/tour.old"}){
    File f=LittleFS.open(path,"r");if(!f)continue;size_t n=f.size()==cap?f.read(data,cap):0;f.close();uint32_t crc;
    if(n==cap&&!memcmp(data,"MMT1",4)&&(memcpy(&crc,data+n-4,4),crc==crc32(data,n-4)))return n;
  }
  return 0;
}
}
#endif
