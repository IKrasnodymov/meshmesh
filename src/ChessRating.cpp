#include "ChessRating.h"
#include <SHA256.h>
#include <ArduinoJson.h>
#include <math.h>
namespace rating {
Book book;
namespace {
const char Magic[]="MMR1";
constexpr uint8_t FileVersion=1;
constexpr size_t OffWhite=4,OffBlack=36,OffGame=68,OffResult=70,OffReason=71,OffPlies=72,OffDigest=74,OffFlags=90;
constexpr size_t PlayerBytes=32+25+4+2*4,RecordBytes=CoreSize+8+2*SigSize,Head=8;
constexpr size_t FileCap=Head+MaxPlayers*PlayerBytes+MaxRecords*RecordBytes+4;
const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void put16(uint8_t* p,uint16_t v){p[0]=v;p[1]=v>>8;}
void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;i++)p[i]=v>>(8*i);}
uint16_t get16(const uint8_t* p){return p[0]|p[1]<<8;}
uint32_t get32(const uint8_t* p){return p[0]|p[1]<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc32(const uint8_t* p,size_t n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(int k=0;k<8;k++)c=c>>1^(0xedb88320&-(c&1));}return ~c;}
// Records in replay order: by time, then by the moves digest and the rest of the core.
bool before(const Record& a,const Record& b){uint32_t x=when(a),y=when(b);if(x!=y)return x<y;return memcmp(a.core+OffDigest,b.core+OffDigest,16)<0||(!memcmp(a.core+OffDigest,b.core+OffDigest,16)&&memcmp(a.core,b.core,CoreSize)<0);}
float expectedScore(float a,float b){return 1/(1+powf(10,(b-a)/400));}
unsigned kOf(const Player& p){return p.games<NewGames?NewK:K;}
}

void makeCore(uint8_t out[CoreSize],const uint8_t white[32],const uint8_t black[32],uint16_t game,uint8_t result,uint8_t reason,const uint16_t* moves,uint16_t plies){
  memcpy(out,Magic,4);memcpy(out+OffWhite,white,32);memcpy(out+OffBlack,black,32);put16(out+OffGame,game);out[OffResult]=result;out[OffReason]=reason;put16(out+OffPlies,plies);
  SHA256 h;h.reset();for(unsigned i=0;i<plies;i++){uint8_t b[2]={uint8_t(moves[i]),uint8_t(moves[i]>>8)};h.update(b,2);}
  uint8_t d[32];h.finalize(d,32);memcpy(out+OffDigest,d,16);out[OffFlags]=1;
}
void signedBytes(uint8_t out[CoreSize+4],const uint8_t core[CoreSize],uint32_t time){memcpy(out,core,CoreSize);put32(out+CoreSize,time);}
uint32_t when(const Record& r){return r.timeW&&r.timeB?min(r.timeW,r.timeB):max(r.timeW,r.timeB);}
uint64_t idOf(const uint8_t key[32]){uint64_t n=0;for(unsigned i=0;i<8;i++)n=n<<8|key[i];return n;}
void sigText(const uint8_t sig[SigSize],char out[SigTextSize+1]){
  char* w=out;for(size_t i=0;i<SigSize;i+=3){uint32_t v=uint32_t(sig[i])<<16|(i+1<SigSize?sig[i+1]<<8:0)|(i+2<SigSize?sig[i+2]:0);
    *w++=B64[v>>18&63];*w++=B64[v>>12&63];*w++=i+1<SigSize?B64[v>>6&63]:'=';*w++=i+2<SigSize?B64[v&63]:'=';}
  *w=0;
}
bool sigParse(const char* text,uint8_t sig[SigSize]){
  if(strlen(text)<SigTextSize||(text[SigTextSize]&&text[SigTextSize]!=' '))return false;
  size_t n=0;uint32_t v=0;int bits=0;
  for(size_t i=0;i<SigTextSize;i++){char c=text[i];if(c=='='){if(i<SigTextSize-2)return false;continue;}const char* p=strchr(B64,c);if(!c||!p)return false;
    v=v<<6|uint32_t(p-B64);bits+=6;if(bits>=8){bits-=8;if(n<SigSize)sig[n++]=v>>bits;}}
  return n==SigSize;
}

const Player* Book::find(uint64_t id) const{for(unsigned i=0;i<playerCount;i++)if(idOf(players[i].key)==id)return &players[i];return nullptr;}
int Book::elo(uint64_t id) const{const Player* p=find(id);return p?int(lroundf(p->elo)):Start;}
unsigned Book::games(uint64_t id) const{const Player* p=find(id);return p?p->games:0;}
const Delta* Book::delta(uint64_t peer,uint16_t game) const{for(unsigned i=0;i<deltaCount;i++)if(deltas[i].peer==peer&&deltas[i].game==game)return &deltas[i];return nullptr;}
void Book::rename(uint64_t id,const char* name){for(unsigned i=1;i<playerCount;i++)if(idOf(players[i].key)==id&&name&&name[0])strlcpy(players[i].name,name,sizeof players[i].name);}
void Book::expected(uint64_t id,int& win,int& draw,int& loss) const{
  const Player* o=find(id);float me=ready?players[0].elo:Start,them=o?o->elo:Start,e=expectedScore(me,them),k=ready?kOf(players[0]):NewK;
  win=lroundf(k*(1-e));draw=lroundf(k*(.5f-e));loss=lroundf(-k*e);
}
int Book::index(const uint8_t key[32]){
  for(unsigned i=0;i<playerCount;i++)if(!memcmp(players[i].key,key,32))return i;
  if(playerCount>=MaxPlayers)return -1;
  Player& p=players[playerCount];p=Player();memcpy(p.key,key,32);p.elo=p.baseElo=Start;return playerCount++;
}
// From the players' base, every record in order: ratings, my results against each player, my recent changes.
void Book::replay(Record* all,unsigned n){
  for(unsigned i=0;i<playerCount;i++){Player& p=players[i];p.elo=p.baseElo;p.games=p.baseGames;p.wins=p.baseWins;p.draws=p.baseDraws;p.losses=p.baseLosses;}
  for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&before(all[j],all[j-1]);j--)std::swap(all[j],all[j-1]);
  struct Pair{uint8_t a,b;uint32_t day;uint8_t n;};Pair* pairs=(Pair*)malloc(sizeof(Pair)*(n?n:1));unsigned pairCount=0;
  historyCount=deltaCount=0;
  for(unsigned i=0;i<n;i++){
    const Record& r=all[i];int w=index(r.core+OffWhite),b=index(r.core+OffBlack);if(w<0||b<0||w==b)continue;
    uint8_t lo=min(w,b),hi=max(w,b);uint32_t day=when(r)/86400;Pair* pair=nullptr;
    for(unsigned k=0;k<pairCount;k++)if(pairs&&pairs[k].a==lo&&pairs[k].b==hi&&pairs[k].day==day)pair=&pairs[k];
    if(!pair&&pairs){pair=&pairs[pairCount++];*pair={lo,hi,day,0};}
    bool counted=pair&&pair->n<PerDay;if(pair)pair->n++;
    float score=r.core[OffResult]==WhiteWon?1:r.core[OffResult]==BlackWon?0:.5f;
    Player& pw=players[w];Player& pb=players[b];float mine=w==0?pw.elo:b==0?pb.elo:0;
    if(counted){float e=expectedScore(pw.elo,pb.elo);unsigned kw=kOf(pw),kb=kOf(pb);pw.elo+=kw*(score-e);pb.elo+=kb*((1-score)-(1-e));pw.games++;pb.games++;}
    if(w!=0&&b!=0)continue;
    Player& them=w==0?pb:pw;float my=w==0?score:1-score;
    if(my==1){players[0].wins++;them.wins++;}else if(my==0){players[0].losses++;them.losses++;}else{players[0].draws++;them.draws++;}
    Delta d={idOf(them.key),get16(r.core+OffGame),int16_t(lroundf(mine)),int16_t(lroundf(players[0].elo)),counted};
    if(deltaCount==MaxDeltas){memmove(deltas,deltas+1,sizeof(Delta)*(MaxDeltas-1));deltaCount--;}deltas[deltaCount++]=d;
    if(counted){if(historyCount==History){memmove(history,history+1,sizeof(int16_t)*(History-1));historyCount--;}history[historyCount++]=d.after;}
  }
  free(pairs);records=n;
}
unsigned Book::load(Record* all){
  uint8_t* buf=(uint8_t*)malloc(FileCap);if(!buf)return 0;size_t size=storeRead(buf,FileCap);unsigned n=0;
  if(size>=Head+4&&!memcmp(buf,Magic,4)&&buf[4]==FileVersion){
    unsigned pc=buf[5],rc=get16(buf+6);const uint8_t* r=buf+Head;
    if(pc<=MaxPlayers&&rc<=MaxRecords&&Head+pc*PlayerBytes+rc*RecordBytes+4==size){
      for(unsigned i=0;i<pc;i++,r+=PlayerBytes){int k=index(r);if(k<0)continue;Player& p=players[k];
        if(k>0||!p.name[0]){memcpy(p.name,r+32,25);p.name[24]=0;}float f;memcpy(&f,r+57,4);p.baseElo=isfinite(f)?f:Start;p.baseGames=get16(r+61);p.baseWins=get16(r+63);p.baseDraws=get16(r+65);p.baseLosses=get16(r+67);}
      for(;n<rc;n++,r+=RecordBytes){Record& x=all[n];memcpy(x.core,r,CoreSize);x.timeW=get32(r+CoreSize);x.timeB=get32(r+CoreSize+4);memcpy(x.sigW,r+CoreSize+8,SigSize);memcpy(x.sigB,r+CoreSize+8+SigSize,SigSize);}
    }
  }
  free(buf);return n;
}
bool Book::save(const Record* all,unsigned n){
  size_t size=Head+playerCount*PlayerBytes+n*RecordBytes+4;uint8_t* buf=(uint8_t*)malloc(size);if(!buf)return false;
  memcpy(buf,Magic,4);buf[4]=FileVersion;buf[5]=playerCount;put16(buf+6,n);uint8_t* w=buf+Head;
  for(unsigned i=0;i<playerCount;i++,w+=PlayerBytes){const Player& p=players[i];memcpy(w,p.key,32);memcpy(w+32,p.name,25);memcpy(w+57,&p.baseElo,4);put16(w+61,p.baseGames);put16(w+63,p.baseWins);put16(w+65,p.baseDraws);put16(w+67,p.baseLosses);}
  for(unsigned i=0;i<n;i++,w+=RecordBytes){const Record& x=all[i];memcpy(w,x.core,CoreSize);put32(w+CoreSize,x.timeW);put32(w+CoreSize+4,x.timeB);memcpy(w+CoreSize+8,x.sigW,SigSize);memcpy(w+CoreSize+8+SigSize,x.sigB,SigSize);}
  put32(w,crc32(buf,w-buf));bool ok=storeWrite(buf,size);free(buf);return ok;
}
void Book::begin(const uint8_t selfKey[32],const char* selfName){
  playerCount=0;index(selfKey);strlcpy(players[0].name,selfName,sizeof players[0].name);
  Record* all=(Record*)malloc(sizeof(Record)*MaxRecords);unsigned n=all?load(all):0;replay(all,n);free(all);ready=true;
}
bool Book::add(const Record& rec,const char* opponentName){
  if(!ready)return false;Record* all=(Record*)malloc(sizeof(Record)*(MaxRecords+1));if(!all)return false;
  unsigned n=load(all);
  for(unsigned i=0;i<n;i++)if(!memcmp(all[i].core,rec.core,CoreSize)){free(all);return false;}
  all[n++]=rec;
  if(n>MaxRecords){ // the oldest goes into the players' base
    unsigned oldest=0;for(unsigned i=1;i<n;i++)if(before(all[i],all[oldest]))oldest=i;
    Record first=all[oldest];replay(&first,1);
    for(unsigned i=0;i<playerCount;i++){Player& p=players[i];p.baseElo=p.elo;p.baseGames=p.games;p.baseWins=p.wins;p.baseDraws=p.draws;p.baseLosses=p.losses;}
    all[oldest]=all[--n];
  }
  replay(all,n);
  for(const uint8_t* key:{rec.core+OffWhite,rec.core+OffBlack}){int k=index(key);if(k>0&&opponentName&&opponentName[0])strlcpy(players[k].name,opponentName,sizeof players[k].name);}
  bool ok=save(all,n);free(all);return ok;
}
String Book::json() const{
  DynamicJsonDocument d(6144);const Player& me=players[0];
  d["elo"]=myElo();d["games"]=ready?me.games:0;d["wins"]=ready?me.wins:0;d["draws"]=ready?me.draws:0;d["losses"]=ready?me.losses:0;d["records"]=records;d["max_records"]=MaxRecords;
  JsonArray h=d.createNestedArray("history");for(unsigned i=0;i<historyCount;i++)h.add(history[i]);
  // Other players, the highest rating first.
  unsigned order[MaxPlayers],n=0;for(unsigned i=1;i<playerCount;i++)if(players[i].games||players[i].wins+players[i].draws+players[i].losses)order[n++]=i;
  for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&players[order[j]].elo>players[order[j-1]].elo;j--)std::swap(order[j],order[j-1]);
  JsonArray list=d.createNestedArray("players");
  for(unsigned i=0;i<n;i++){const Player& p=players[order[i]];JsonObject o=list.createNestedObject();char id[17];snprintf(id,sizeof id,"%08lX%08lX",(unsigned long)(idOf(p.key)>>32),(unsigned long)idOf(p.key));
    o["id"]=id;o["name"]=p.name;o["elo"]=int(lroundf(p.elo));o["games"]=p.games;o["wins"]=p.wins;o["draws"]=p.draws;o["losses"]=p.losses;}
  String s;serializeJson(d,s);return s;
}
}

#if !defined(MM_UI_PREVIEW) && !defined(MM_HOST_CHECK)
#include <LittleFS.h>
#include "Hardware.h"
namespace rating {
// Written to a new file and renamed; the previous copy is read when the main file is damaged.
bool storeWrite(const uint8_t* data,size_t size){
  if(!hardware.fsOk)return false;LittleFS.mkdir("/meshmesh");
  File f=LittleFS.open("/meshmesh/rating.new","w");if(!f)return false;bool ok=f.write(data,size)==size;f.close();if(!ok)return false;
  LittleFS.remove("/meshmesh/rating.old");LittleFS.rename("/meshmesh/rating.bin","/meshmesh/rating.old");
  return LittleFS.rename("/meshmesh/rating.new","/meshmesh/rating.bin");
}
size_t storeRead(uint8_t* data,size_t cap){
  if(!hardware.fsOk)return 0;
  for(const char* path:{"/meshmesh/rating.bin","/meshmesh/rating.old"}){
    File f=LittleFS.open(path,"r");if(!f)continue;size_t n=f.size()<=cap?f.read(data,f.size()):0;f.close();
    if(n>=Head+4&&!memcmp(data,Magic,4)&&get32(data+n-4)==crc32(data,n-4))return n;
  }
  return 0;
}
}
#endif
