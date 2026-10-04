// Host check of rated results (src/ChessRating.cpp): tools/chess/rating_check.sh
// Signatures with the MeshCore Ed25519 code, ratings, the daily limit, folding of old records and the saved file.
#include "ChessRating.h"
#include "ed_25519.h"
#include <cstdio>
#include <vector>
static std::vector<uint8_t> file;
bool rating::storeWrite(const uint8_t* d,size_t n){file.assign(d,d+n);return true;}
size_t rating::storeRead(uint8_t* d,size_t cap){if(file.size()>cap)return 0;memcpy(d,file.data(),file.size());return file.size();}
static int failures=0;
#define CHECK(c,...) do{if(!(c)){printf("FAIL %s:%d ",__FILE__,__LINE__);printf(__VA_ARGS__);printf("\n");failures++;}}while(0)
struct Key{uint8_t pub[32],prv[64];explicit Key(uint8_t s){uint8_t seed[32];memset(seed,s,32);ed25519_create_keypair(pub,prv,seed);}};
static rating::Record game(const Key& w,const Key& b,uint16_t id,uint8_t result,uint32_t tw,uint32_t tb){
  rating::Record r{};uint16_t moves[4]={0x0c1c,0x3424,0x0605,0x3e2d};
  rating::makeCore(r.core,w.pub,b.pub,id,result,1,moves,4);r.timeW=tw;r.timeB=tb;uint8_t m[rating::CoreSize+4];
  rating::signedBytes(m,r.core,tw);ed25519_sign(r.sigW,m,sizeof m,w.pub,w.prv);rating::signedBytes(m,r.core,tb);ed25519_sign(r.sigB,m,sizeof m,b.pub,b.prv);return r;
}
int main(){
  Key a(1),b(2),c(3);
  // Each player signs the core with their own clock; a changed result or clock breaks the signature.
  rating::Record r=game(a,b,0x3F2A,rating::WhiteWon,1791130000,1791130007);uint8_t m[rating::CoreSize+4];
  rating::signedBytes(m,r.core,r.timeB);CHECK(ed25519_verify(r.sigB,m,sizeof m,b.pub),"Black's signature");
  rating::signedBytes(m,r.core,r.timeW);CHECK(!ed25519_verify(r.sigB,m,sizeof m,b.pub),"Black's signature with White's clock");
  m[70]=rating::Drawn;rating::signedBytes(m,r.core,r.timeW);m[70]=rating::Drawn;CHECK(!ed25519_verify(r.sigW,m,sizeof m,a.pub),"changed result");
  CHECK(rating::when(r)==1791130000,"earlier clock");rating::Record u=r;u.timeW=0;CHECK(rating::when(u)==1791130007,"unset clock ignored");
  char text[rating::SigTextSize+1];rating::sigText(r.sigW,text);uint8_t back[64];
  CHECK(strlen(text)==88&&rating::sigParse(text,back)&&!memcmp(back,r.sigW,64),"base64 round trip");text[5]='*';CHECK(!rating::sigParse(text,back),"bad base64 refused");
  // Ratings: 1500 against 1500, a win gives 16 with K=32.
  rating::book.begin(a.pub,"A");CHECK(rating::book.add(r,"B"),"first record");CHECK(!rating::book.add(r,"B"),"duplicate refused");
  CHECK(rating::book.myElo()==1516&&rating::book.elo(rating::idOf(b.pub))==1484,"after a win: %d %d",rating::book.myElo(),rating::book.elo(rating::idOf(b.pub)));
  const rating::Delta* d=rating::book.delta(rating::idOf(b.pub),0x3F2A);CHECK(d&&d->before==1500&&d->after==1516&&d->counted,"my change");
  int win,draw,loss;rating::book.expected(rating::idOf(b.pub),win,draw,loss);CHECK(win>0&&loss<0&&win<16&&loss<-16,"expected change against a weaker player: %d %d",win,loss);
  // Games between two other players change their ratings, not mine; four in one day count three.
  for(int i=0;i<4;i++)rating::book.add(game(b,c,0x100+i,rating::WhiteWon,1791140000+i*60,1791140000+i*60),"");
  CHECK(rating::book.games(rating::idOf(c.pub))==3,"daily limit: %u",rating::book.games(rating::idOf(c.pub)));CHECK(rating::book.myElo()==1516,"others' games");
  // The saved file gives the same ratings.
  std::vector<uint8_t> saved=file;String before=rating::book.json();rating::Book again;again.begin(a.pub,"A");CHECK(again.json()==before,"reloaded: %s",again.json().c_str());
  // Past the limit, old records fold into the players' base: the counts keep growing, the file does not.
  for(unsigned i=0;i<rating::MaxRecords+10;i++)rating::book.add(game(a,c,0x2000+i,i%3?rating::WhiteWon:rating::Drawn,1791200000+i*86400,1791200000+i*86400),"C");
  CHECK(rating::book.records==rating::MaxRecords,"records kept: %u",rating::book.records);
  CHECK(rating::book.games(rating::idOf(a.pub))==1+rating::MaxRecords+10,"my games after folding: %u",rating::book.games(rating::idOf(a.pub)));
  String folded=rating::book.json();rating::Book third;third.begin(a.pub,"A");CHECK(third.json()==folded,"reloaded after folding");
  CHECK(rating::book.historyCount==rating::History,"history length");
  printf("%s\n",failures?"rating check FAILED":"rating check passed");return failures!=0;
}
