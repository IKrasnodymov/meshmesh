#include "Solitaire.h"
#include <string.h>
namespace solitaire {
namespace {
constexpr uint8_t SavedVersion=1;
constexpr uint8_t capacity(uint8_t p){return p<=Waste?24:isFoundation(p)?13:19;} // 6 face-down + K..A
bool fits(uint8_t lower,uint8_t upper){return red(lower)!=red(upper)&&rank(lower)==rank(upper)+1;}
}

void Game::deal(uint32_t seed,uint8_t cardsPerDraw){
 uint8_t deck[52];for(uint8_t i=0;i<52;i++)deck[i]=i;
 uint32_t x=seed?seed:0x9e3779b9;auto next=[&]{x^=x<<13;x^=x>>17;x^=x<<5;return x;}; // xorshift32
 for(int i=51;i>0;i--){int j=next()%(i+1);uint8_t t=deck[i];deck[i]=deck[j];deck[j]=t;}
 for(auto& p:pile)p.n=0;unsigned k=0;
 for(uint8_t col=0;col<7;col++)for(uint8_t row=0;row<=col;row++){Pile& p=pile[Tableau+col];p.c[p.n++]=deck[k++]|(row==col?FaceUp:0);}
 while(k<52)pile[Stock].c[pile[Stock].n++]=deck[k++];
 draw=cardsPerDraw==3?3:1;score=0;moves=0;seconds=0;counted=false;undoSize=0;
}
void Game::addScore(int delta){score=delta<-score?0:score+delta;}
void Game::flipTops(){for(uint8_t p=Tableau;p<PileCount;p++){Pile& t=pile[p];if(t.n&&!up(t.top())){t.c[t.n-1]|=FaceUp;addScore(5);}}}
void Game::remember(){save(history[undoHead]);undoHead=(undoHead+1)%UndoDepth;if(undoSize<UndoDepth)undoSize++;}

bool Game::turnStock(){
 Pile& s=pile[Stock];Pile& w=pile[Waste];
 if(!s.n&&!w.n)return false;remember();
 if(s.n){for(uint8_t i=0;i<draw&&s.n;i++)w.c[w.n++]=s.c[--s.n]|FaceUp;}
 else{while(w.n)s.c[s.n++]=w.c[--w.n]&~FaceUp;addScore(draw==1?-100:-20);}
 moves++;counted=true;return true;
}
uint8_t Game::movable(uint8_t from) const{
 const Pile& p=pile[from];if(from==Stock||!p.n)return 0;if(!isTableau(from))return 1;
 uint8_t k=0;while(k<p.n&&up(p.c[p.n-1-k]))k++;return k; // face-up tableau cards always form a run
}
bool Game::canMove(uint8_t from,uint8_t count,uint8_t to) const{
 if(from>=PileCount||to>=PileCount||from==to||!count||count>movable(from)||to<=Waste)return false;
 const Pile& src=pile[from];const Pile& dst=pile[to];uint8_t first=src.c[src.n-count];
 if(isFoundation(to)){if(count!=1)return false;return dst.n?suit(first)==suit(dst.top())&&rank(first)==rank(dst.top())+1:rank(first)==0;}
 if(dst.n+count>capacity(to))return false;
 return dst.n?up(dst.top())&&fits(dst.top(),first):rank(first)==12;
}
bool Game::move(uint8_t from,uint8_t count,uint8_t to){
 if(!canMove(from,count,to))return false;remember();
 Pile& src=pile[from];Pile& dst=pile[to];
 memcpy(dst.c+dst.n,src.c+src.n-count,count);dst.n+=count;src.n-=count;
 if(from==Waste)addScore(isFoundation(to)?10:5);else if(isTableau(from)&&isFoundation(to))addScore(10);else if(isFoundation(from)&&isTableau(to))addScore(-15);
 flipTops();moves++;counted=true;return true;
}
int Game::target(uint8_t from,uint8_t count) const{
 if(count==1&&!isFoundation(from))for(uint8_t f=Foundation;f<Tableau;f++)if(canMove(from,1,f))return f;
 // Tableau: the nearest column to the right; an empty one only if it changes something.
 bool wholeColumn=isTableau(from)&&count==pile[from].n;int empty=-1;
 for(uint8_t i=1;i<=7;i++){uint8_t to=Tableau+((isTableau(from)?from-Tableau:6)+i)%7;if(!canMove(from,count,to))continue;if(pile[to].n)return to;if(empty<0&&!wholeColumn)empty=to;}
 return empty;
}
bool Game::hint(Move& m) const{
 auto found=[&](uint8_t from,uint8_t count,uint8_t to){m={from,count,to};return true;};
 // 1. Anything that goes to a foundation.
 for(uint8_t from=Waste;from<PileCount;from++)if(!isFoundation(from)&&movable(from))for(uint8_t f=Foundation;f<Tableau;f++)if(canMove(from,1,f))return found(from,1,f);
 // 2. Whole runs that uncover a face-down card.
 for(uint8_t from=Tableau;from<PileCount;from++){uint8_t k=movable(from);if(!k||k==pile[from].n)continue;for(uint8_t to=Tableau;to<PileCount;to++)if(canMove(from,k,to))return found(from,k,to);}
 // 3. The waste card onto the tableau.
 if(pile[Waste].n)for(uint8_t to=Tableau;to<PileCount;to++)if(canMove(Waste,1,to))return found(Waste,1,to);
 // 4. Runs that empty a column, if there is a king to fill it.
 bool king=false;for(uint8_t p=Stock;p<PileCount;p++)if(!isFoundation(p))for(uint8_t i=0;i<pile[p].n;i++){uint8_t c=pile[p].c[i];if(rank(c)==12&&!(isTableau(p)&&i==0))king=true;}
 if(king)for(uint8_t from=Tableau;from<PileCount;from++){uint8_t k=movable(from);if(!k||k!=pile[from].n||rank(pile[from].c[0])==12)continue;for(uint8_t to=Tableau;to<PileCount;to++)if(pile[to].n&&canMove(from,k,to))return found(from,k,to);}
 // 5. The stock, if one of its or the waste cards can be played somewhere.
 for(uint8_t p=Stock;p<=Waste;p++)for(uint8_t i=0;i<pile[p].n;i++){
  uint8_t c=pile[p].c[i];if(p==Waste&&i==pile[p].n-1)continue; // the top waste card was checked above
  bool playable=false;
  for(uint8_t q=Foundation;q<Tableau;q++){const Pile& fq=pile[q];if(fq.n?suit(fq.top())==suit(c)&&rank(fq.top())+1==rank(c):rank(c)==0)playable=true;}
  for(uint8_t to=Tableau;to<PileCount;to++){const Pile& t=pile[to];if(t.n?fits(t.top(),c):rank(c)==12)playable=true;}
  if(playable)return found(Stock,1,Waste);
 }
 return false;
}
bool Game::collect(){
 int best=-1;uint8_t bestRank=13,bestTo=0;
 for(uint8_t from=Waste;from<PileCount;from++){if(isFoundation(from)||!pile[from].n)continue;for(uint8_t f=Foundation;f<Tableau;f++)if(canMove(from,1,f)&&rank(pile[from].top())<bestRank){best=from;bestRank=rank(pile[from].top());bestTo=f;}}
 return best>=0&&move(best,1,bestTo);
}
bool Game::finishable() const{
 if(pile[Stock].n||pile[Waste].n||won())return false;
 for(uint8_t p=Tableau;p<PileCount;p++)for(uint8_t i=0;i<pile[p].n;i++)if(!up(pile[p].c[i]))return false;return true;
}
unsigned Game::onFoundations() const{unsigned n=0;for(uint8_t f=Foundation;f<Tableau;f++)n+=pile[f].n;return n;}
bool Game::won() const{return onFoundations()==52;}
bool Game::undo(){
 if(!undoSize)return false;undoHead=(undoHead+UndoDepth-1)%UndoDepth;undoSize--;
 uint32_t played=seconds;unsigned head=undoHead,size=undoSize;
 bool restored=load(history[undoHead],SavedSize);undoHead=head;undoSize=size; // load() clears the history
 seconds=played;counted=true;return restored;
}

// Saved form: version, draw, score, moves, seconds, flags, 13 pile sizes, 52 cards.
size_t Game::save(uint8_t* out) const{
 out[0]=SavedVersion;out[1]=draw;out[2]=score&0xff;out[3]=score>>8;out[4]=moves&0xff;out[5]=moves>>8;
 for(int i=0;i<4;i++)out[6+i]=seconds>>(8*i);out[10]=counted;
 uint8_t* cards=out+24;for(uint8_t p=0;p<PileCount;p++){out[11+p]=pile[p].n;memcpy(cards,pile[p].c,pile[p].n);cards+=pile[p].n;}
 return SavedSize;
}
bool Game::load(const uint8_t* in,size_t size){
 if(size!=SavedSize||in[0]!=SavedVersion||(in[1]!=1&&in[1]!=3)||in[10]>1)return false;
 Pile next[PileCount];unsigned total=0;uint64_t seen=0;const uint8_t* cards=in+24;
 for(uint8_t p=0;p<PileCount;p++){
  uint8_t n=in[11+p];if(n>capacity(p)||total+n>52)return false;next[p].n=n;memcpy(next[p].c,cards+total,n);total+=n;
  bool faceUpSeen=false;
  for(uint8_t i=0;i<n;i++){
   uint8_t c=next[p].c[i];if((c&0x7f)>=52||(c&0x40)||(seen>>(c&0x3f)&1))return false;seen|=1ULL<<(c&0x3f);
   if(p==Stock&&up(c))return false;if(p!=Stock&&!isTableau(p)&&!up(c))return false;
   if(isFoundation(p)&&(rank(c)!=i||suit(c)!=suit(next[p].c[0])))return false;
   if(isTableau(p)){if(faceUpSeen&&!up(c))return false;if(faceUpSeen&&!fits(next[p].c[i-1],c))return false;faceUpSeen|=up(c);}
  }
  if(isTableau(p)&&n&&!up(next[p].c[n-1]))return false;
 }
 if(total!=52)return false;
 memcpy(pile,next,sizeof pile);draw=in[1];score=int16_t(in[2]|in[3]<<8);if(score<0)score=0;moves=in[4]|in[5]<<8;
 seconds=0;for(int i=0;i<4;i++)seconds|=uint32_t(in[6+i])<<(8*i);counted=in[10];undoSize=0;return true;
}
}
