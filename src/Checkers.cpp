#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: as Chess.cpp
#endif
#include "Checkers.h"
#include <string.h>
namespace checkers {
namespace {
constexpr int8_t Taken=100; // a piece captured during the move: it still blocks and is never taken twice
const int8_t Dirs[4][2]={{1,1},{-1,1},{1,-1},{-1,-1}};
}
bool sameMove(const Move& a,const Move& b){return a.n==b.n&&!memcmp(a.path,b.path,a.n);}
void squareName(int s,char out[3]){out[0]=char('a'+colOf(s));out[1]=char('1'+rowOf(s));out[2]=0;}
int parseSquare(const char* t){if(t[0]<'a'||t[0]>'h'||t[1]<'1'||t[1]>'8')return -1;return squareAt(t[0]-'a',t[1]-'1');}
size_t moveText(const Move& m,char* out,size_t cap){
  size_t w=0;if(!cap)return 0;out[0]=0;
  for(unsigned i=0;i<m.n&&w+3<cap;i++){if(i)out[w++]=m.capture?':':'-';char s[3];squareName(m.path[i],s);out[w++]=s[0];out[w++]=s[1];}
  out[w]=0;return w;
}
void Position::start(){
  memset(board,0,sizeof board);side=White;
  for(int s=0;s<12;s++)board[s]=Man;for(int s=20;s<32;s++)board[s]=-Man;
}
unsigned Position::count(uint8_t color) const{unsigned n=0;for(int8_t p:board)n+=color==White?p>0:p<0;return n;}
// Captures from s with the board b (the moving piece lifted, the taken ones marked). A king lands on any empty
// square behind the piece, but only on those from which it can take again when there are such (Russian rules).
void Position::captures(int s,bool king,int8_t* b,Move& m,Move* out,unsigned& n) const{
  bool more=false;int c=colOf(s),r=rowOf(s);
  for(auto& d:Dirs){
    int x=c+d[0],y=r+d[1];
    if(king)while(squareAt(x,y)>=0&&b[squareAt(x,y)]==Empty){x+=d[0];y+=d[1];}
    int over=squareAt(x,y);if(over<0)continue;int8_t p=b[over];
    if(p==Empty||p==Taken||(side==White?p>0:p<0))continue;
    int lands[8],k=0;
    for(int lx=x+d[0],ly=y+d[1];squareAt(lx,ly)>=0&&b[squareAt(lx,ly)]==Empty&&k<8;lx+=d[0],ly+=d[1]){lands[k++]=squareAt(lx,ly);if(!king)break;}
    if(!k)continue;
    more=true;b[over]=Taken;
    bool cont[8],any=false;
    for(int i=0;i<k;i++){
      bool crowned=king||rowOf(lands[i])==(side==White?7:0);
      // Can it take again from there? (a quick look: one piece with an empty square behind it)
      cont[i]=false;int lc=colOf(lands[i]),lr=rowOf(lands[i]);
      for(auto& e:Dirs){int ex=lc+e[0],ey=lr+e[1];if(crowned)while(squareAt(ex,ey)>=0&&b[squareAt(ex,ey)]==Empty){ex+=e[0];ey+=e[1];}
        int o=squareAt(ex,ey);if(o<0)continue;int8_t q=b[o];if(q==Empty||q==Taken||(side==White?q>0:q<0))continue;
        int t=squareAt(ex+e[0],ey+e[1]);if(t>=0&&b[t]==Empty){cont[i]=true;break;}}
      any|=cont[i];
    }
    for(int i=0;i<k;i++){
      if(any&&!cont[i])continue;if(m.n>=MaxPath)continue;
      m.path[m.n++]=lands[i];captures(lands[i],king||rowOf(lands[i])==(side==White?7:0),b,m,out,n);m.n--;
    }
    b[over]=p;
  }
  if(!more&&m.n>1&&n<MaxMoves){out[n]=m;out[n].capture=true;n++;}
}
unsigned Position::legal(Move* out) const{
  unsigned n=0;
  for(int s=0;s<32;s++){if(own(s)<=0)continue;int8_t b[32];memcpy(b,board,32);b[s]=Empty;Move m;m.n=1;m.path[0]=uint8_t(s);captures(s,own(s)==King,b,m,out,n);}
  if(n)return n;
  for(int s=0;s<32&&n<MaxMoves;s++){
    int8_t p=own(s);if(p<=0)continue;int c=colOf(s),r=rowOf(s);
    for(auto& d:Dirs){
      if(p==Man&&d[1]!=(side==White?1:-1))continue;
      for(int x=c+d[0],y=r+d[1];squareAt(x,y)>=0&&board[squareAt(x,y)]==Empty&&n<MaxMoves;x+=d[0],y+=d[1]){
        Move& m=out[n++];m=Move();m.n=2;m.path[0]=uint8_t(s);m.path[1]=uint8_t(squareAt(x,y));if(p==Man)break;}
    }
  }
  return n;
}
void Position::apply(const Move& m){
  int8_t p=board[m.from()];board[m.from()]=Empty;bool crown=false;
  for(unsigned i=0;i<m.n;i++){
    if(rowOf(m.path[i])==(side==White?7:0))crown=true;
    if(!i||!m.capture)continue;
    int a=m.path[i-1],b=m.path[i],dc=colOf(b)>colOf(a)?1:-1,dr=rowOf(b)>rowOf(a)?1:-1;
    for(int x=colOf(a)+dc,y=rowOf(a)+dr;x!=colOf(b);x+=dc,y+=dr)board[squareAt(x,y)]=Empty;
  }
  if(crown&&(p==Man||p==-Man))p=p>0?King:-King;
  board[m.to()]=p;side^=1;
}
bool Position::parse(const char* text,Move& m) const{
  Move want;const char* t=text;
  while(*t&&want.n<MaxPath){int s=parseSquare(t);if(s<0)return false;want.path[want.n++]=uint8_t(s);t+=2;if(*t=='-'||*t==':'||*t=='x')t++;else break;}
  if(want.n<2||(*t&&*t!=' '))return false;
  Move list[MaxMoves];unsigned n=legal(list);
  for(unsigned i=0;i<n;i++)if(sameMove(list[i],want)){m=list[i];return true;}
  // "c3:g7": the start and the end, when the path between them is the only one
  if(want.n==2){int found=-1;for(unsigned i=0;i<n;i++)if(list[i].from()==want.path[0]&&list[i].to()==want.path[1]){if(found>=0)return false;found=int(i);}
    if(found>=0){m=list[found];return true;}}
  return false;
}
bool Game::play(const Move& m){
  if(outcome!=Ongoing)return false;Move list[MaxMoves];unsigned n=pos.legal(list);bool ok=false;
  for(unsigned i=0;i<n&&!ok;i++)ok=sameMove(list[i],m);if(!ok)return false;
  int8_t p=pos.board[m.from()];bool man=p==Man||p==-Man;
  pos.apply(m);plies++;quiet=m.capture||man?0:uint8_t(quiet<255?quiet+1:255);last=m;judge();return true;
}
void Game::judge(){
  if(quiet>=QuietPlies){outcome=QuietDraw;return;}
  Move list[MaxMoves];outcome=pos.legal(list)?Ongoing:NoMoves;
}
}
