#include "Chess.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
namespace chess {
namespace {
const int8_t knightSteps[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
const int8_t kingSteps[8][2]={{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
const int8_t diagonals[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
const int8_t lines[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
inline bool onBoard(int f,int r){return f>=0&&f<8&&r>=0&&r<8;}
inline bool owns(int8_t p,uint8_t color){return color==White?p>0:p<0;}
inline int8_t sign(uint8_t color){return color==White?1:-1;}
const char pieceLetters[]=" PNBRQK";
}

void Position::start(){fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");}
bool Position::fromFen(const char* fen){
  Position p;const char* c=fen;int r=7,f=0;
  for(;*c&&*c!=' ';c++){
    if(*c=='/'){if(f!=8||r==0)return false;r--;f=0;continue;}
    if(*c>='1'&&*c<='8'){f+=*c-'0';if(f>8)return false;continue;}
    const char* at=strchr(pieceLetters+1,*c>='a'?*c-32:*c);if(!at||f>7)return false;
    p.board[r*8+f++]=int8_t((at-pieceLetters)*(*c>='a'?-1:1));
  }
  if(r||f!=8||*c++!=' ')return false;
  if(*c!='w'&&*c!='b')return false;p.side=*c++=='w'?White:Black;if(*c++!=' ')return false;
  if(*c=='-')c++;else for(;*c&&*c!=' ';c++){const char* k=strchr("KQkq",*c);if(!k)return false;p.castling|=1<<(k-"KQkq");}
  if(*c++!=' ')return false;
  if(*c=='-')c++;else{if(c[0]<'a'||c[0]>'h'||(c[1]!='3'&&c[1]!='6'))return false;p.enPassant=int8_t((c[1]-'1')*8+c[0]-'a');c+=2;}
  if(*c==' '){char* end;long h=strtol(c+1,&end,10),m=*end==' '?strtol(end+1,nullptr,10):1;if(h<0||h>999||m<1||m>9999)return false;p.halfmove=h;p.fullmove=m;}
  if(p.king(White)<0||p.king(Black)<0)return false;
  *this=p;return true;
}
size_t Position::fen(char* out,size_t cap) const{
  char b[96];size_t n=0;
  for(int r=7;r>=0;r--){int gap=0;for(int f=0;f<8;f++){int8_t p=board[r*8+f];if(!p){gap++;continue;}if(gap)b[n++]='0'+gap;gap=0;char l=pieceLetters[abs(p)];b[n++]=p>0?l:l+32;}if(gap)b[n++]='0'+gap;if(r)b[n++]='/';}
  b[n++]=' ';b[n++]=side==White?'w':'b';b[n++]=' ';
  if(!castling)b[n++]='-';for(int i=0;i<4;i++)if(castling>>i&1)b[n++]="KQkq"[i];
  b[n++]=' ';if(enPassant<0)b[n++]='-';else{b[n++]='a'+fileOf(enPassant);b[n++]='1'+rankOf(enPassant);}
  n+=snprintf(b+n,sizeof b-n," %u %u",halfmove,fullmove);
  if(!cap)return n;size_t k=n<cap?n:cap-1;memcpy(out,b,k);out[k]=0;return k;
}
int Position::king(uint8_t color) const{int8_t k=King*sign(color);for(int s=0;s<64;s++)if(board[s]==k)return s;return -1;}
bool Position::attacked(int square,uint8_t by) const{
  if(square<0)return false;int f=fileOf(square),r=rankOf(square);int8_t s=sign(by);
  for(int df:{-1,1})if(onBoard(f+df,r-s)&&board[(r-s)*8+f+df]==s*Pawn)return true;
  for(auto& d:knightSteps)if(onBoard(f+d[0],r+d[1])&&board[(r+d[1])*8+f+d[0]]==s*Knight)return true;
  for(auto& d:kingSteps)if(onBoard(f+d[0],r+d[1])&&board[(r+d[1])*8+f+d[0]]==s*King)return true;
  for(int k=0;k<2;k++)for(auto& d:k?lines:diagonals){
    int x=f+d[0],y=r+d[1];
    while(onBoard(x,y)){int8_t p=board[y*8+x];if(p){if(p==s*Queen||p==s*(k?Rook:Bishop))return true;break;}x+=d[0];y+=d[1];}
  }
  return false;
}
unsigned Position::pseudo(Move* out) const{
  unsigned n=0;int8_t s=sign(side);
  auto pawnTo=[&](int from,int to){if(rankOf(to)==0||rankOf(to)==7)for(int p:{Queen,Rook,Bishop,Knight})out[n++]=makeMove(from,to,p);else out[n++]=makeMove(from,to);};
  for(int sq=0;sq<64;sq++){
    int8_t p=board[sq];if(!owns(p,side))continue;int type=p*s,f=fileOf(sq),r=rankOf(sq);
    if(type==Pawn){
      int ahead=r+s;
      if(onBoard(f,ahead)&&!board[ahead*8+f]){pawnTo(sq,ahead*8+f);if(r==(side==White?1:6)&&!board[(r+2*s)*8+f])out[n++]=makeMove(sq,(r+2*s)*8+f);}
      for(int df:{-1,1}){if(!onBoard(f+df,ahead))continue;int t=ahead*8+f+df;if((board[t]&&!owns(board[t],side))||t==enPassant)pawnTo(sq,t);}
      continue;
    }
    if(type==Knight||type==King){
      for(auto& d:type==Knight?knightSteps:kingSteps){int x=f+d[0],y=r+d[1];if(onBoard(x,y)&&!owns(board[y*8+x],side))out[n++]=makeMove(sq,y*8+x);}
      if(type==King&&sq==(side==White?4:60)&&!attacked(sq,side^1)){
        int base=sq-4;uint8_t shortRight=side==White?WhiteShort:BlackShort,longRight=side==White?WhiteLong:BlackLong;
        if((castling&shortRight)&&board[base+7]==s*Rook&&!board[base+5]&&!board[base+6]&&!attacked(base+5,side^1)&&!attacked(base+6,side^1))out[n++]=makeMove(sq,base+6);
        if((castling&longRight)&&board[base]==s*Rook&&!board[base+1]&&!board[base+2]&&!board[base+3]&&!attacked(base+3,side^1)&&!attacked(base+2,side^1))out[n++]=makeMove(sq,base+2);
      }
      continue;
    }
    for(int k=0;k<2;k++){
      if((k==0&&type==Rook)||(k==1&&type==Bishop))continue;
      for(auto& d:k?lines:diagonals){int x=f+d[0],y=r+d[1];while(onBoard(x,y)){int8_t t=board[y*8+x];if(owns(t,side))break;out[n++]=makeMove(sq,y*8+x);if(t)break;x+=d[0];y+=d[1];}}
    }
  }
  return n;
}
void Position::apply(Move m){
  int from=moveFrom(m),to=moveTo(m),promotion=movePromotion(m);int8_t p=board[from];int type=abs(p);bool capture=board[to]!=0;
  if(type==Pawn&&to==enPassant&&!capture){board[to-8*sign(side)]=Empty;capture=true;}
  if(type==King&&abs(to-from)==2){int rookFrom=to>from?to+1:to-2,rookTo=to>from?to-1:to+1;board[rookTo]=board[rookFrom];board[rookFrom]=Empty;}
  board[to]=promotion?int8_t(promotion*sign(side)):p;board[from]=Empty;
  enPassant=type==Pawn&&abs(to-from)==16?int8_t((from+to)/2):-1;
  for(int sq:{from,to}){if(sq==4)castling&=~(WhiteShort|WhiteLong);if(sq==60)castling&=~(BlackShort|BlackLong);if(sq==0)castling&=~WhiteLong;if(sq==7)castling&=~WhiteShort;if(sq==56)castling&=~BlackLong;if(sq==63)castling&=~BlackShort;}
  halfmove=type==Pawn||capture?0:halfmove+1;if(side==Black)fullmove++;side^=1;
}
unsigned Position::legal(Move* out) const{
  Move all[MaxMoves];unsigned total=pseudo(all),n=0;
  for(unsigned i=0;i<total;i++){Position next=*this;next.apply(all[i]);if(!next.attacked(next.king(side),next.side))out[n++]=all[i];}
  return n;
}
bool Position::isLegal(Move m) const{Move list[MaxMoves];unsigned n=legal(list);for(unsigned i=0;i<n;i++)if(list[i]==m)return true;return false;}
int Position::enPassantTarget() const{
  if(enPassant<0)return -1;int f=fileOf(enPassant),r=rankOf(enPassant)-sign(side);
  for(int df:{-1,1})if(onBoard(f+df,r)&&board[r*8+f+df]==Pawn*sign(side))return enPassant;
  return -1;
}
bool Position::samePlacement(const Position& o) const{return side==o.side&&castling==o.castling&&enPassantTarget()==o.enPassantTarget()&&!memcmp(board,o.board,64);}
bool Position::insufficientMaterial() const{
  int minors=0,bishops=0,colors=0;
  for(int s=0;s<64;s++){int t=abs(board[s]);if(t==Pawn||t==Rook||t==Queen)return false;if(t==Knight)minors++;if(t==Bishop){minors++;bishops++;colors|=1<<((fileOf(s)+rankOf(s))&1);}}
  return minors<=1||(bishops==minors&&colors!=3); // lone minor piece, or bishops on one colour only
}
bool Position::parseUci(const char* text,Move& m) const{
  size_t n=strlen(text);if(n<4||n>5)return false;
  if(text[0]<'a'||text[0]>'h'||text[1]<'1'||text[1]>'8'||text[2]<'a'||text[2]>'h'||text[3]<'1'||text[3]>'8')return false;
  int from=(text[1]-'1')*8+text[0]-'a',to=(text[3]-'1')*8+text[2]-'a',promotion=Empty;
  if(n==5){const char* at=strchr("nbrq",text[4]|32);if(!at)return false;promotion=Knight+int(at-"nbrq");}
  else if(abs(board[from])==Pawn&&(rankOf(to)==0||rankOf(to)==7))promotion=Queen;
  m=makeMove(from,to,promotion);return isLegal(m);
}
void uci(Move m,char out[6]){int f=moveFrom(m),t=moveTo(m);out[0]='a'+fileOf(f);out[1]='1'+rankOf(f);out[2]='a'+fileOf(t);out[3]='1'+rankOf(t);out[4]=movePromotion(m)?"  nbrq"[movePromotion(m)]:0;out[5]=0;}
size_t Position::san(Move m,char* out,size_t cap) const{
  char b[12];size_t n=0;int from=moveFrom(m),to=moveTo(m),type=abs(board[from]);
  bool capture=board[to]||(type==Pawn&&fileOf(from)!=fileOf(to));
  if(type==King&&abs(to-from)==2){strcpy(b,to>from?"O-O":"O-O-O");n=strlen(b);}
  else{
    if(type!=Pawn){
      b[n++]=pieceLetters[type];
      Move list[MaxMoves];unsigned count=legal(list);bool other=false,sameFile=false,sameRank=false;
      for(unsigned i=0;i<count;i++){int f=moveFrom(list[i]);if(f==from||moveTo(list[i])!=to||abs(board[f])!=type)continue;other=true;sameFile|=fileOf(f)==fileOf(from);sameRank|=rankOf(f)==rankOf(from);}
      if(other){if(!sameFile)b[n++]='a'+fileOf(from);else if(!sameRank)b[n++]='1'+rankOf(from);else{b[n++]='a'+fileOf(from);b[n++]='1'+rankOf(from);}}
    }else if(capture)b[n++]='a'+fileOf(from);
    if(capture)b[n++]='x';
    b[n++]='a'+fileOf(to);b[n++]='1'+rankOf(to);
    if(movePromotion(m)){b[n++]='=';b[n++]=pieceLetters[movePromotion(m)];}
  }
  Position next=*this;next.apply(m);
  if(next.inCheck()){Move list[MaxMoves];b[n++]=next.legal(list)?'+':'#';}
  b[n]=0;if(!cap)return n;size_t k=n<cap?n:cap-1;memcpy(out,b,k);out[k]=0;return k;
}

bool Game::play(Move m){
  if(outcome!=Ongoing||plies>=MaxPlies||!pos.play(m))return false;
  moves[plies++]=m;outcome=judge();return true;
}
bool Game::load(const Move* list,unsigned count){
  reset();if(count>MaxPlies)return false;
  for(unsigned i=0;i<count;i++)if(!play(list[i])){reset();return false;}
  return true;
}
Position Game::at(unsigned ply) const{Position p;p.start();for(unsigned i=0;i<ply&&i<plies;i++)p.apply(moves[i]);return p;}
int Game::material(uint8_t color) const{static const int value[]={0,1,3,3,5,9,0};int sum=0;for(int8_t p:pos.board)if(owns(p,color))sum+=value[abs(p)];return sum;}
Outcome Game::judge() const{
  Move list[MaxMoves];
  if(!pos.legal(list))return pos.inCheck()?Checkmate:Stalemate;  // mate stands even on the 50th move
  if(pos.insufficientMaterial())return DeadPosition;
  if(pos.halfmove>=100)return FiftyMoves;
  if(pos.halfmove>=8){ // only positions since the last capture or pawn move can repeat
    Position p;p.start();unsigned seen=1,first=pos.halfmove>plies?0:plies-pos.halfmove;
    for(unsigned k=0;k<plies;k++){if(k>=first&&p.samePlacement(pos)&&++seen>=3)return Repetition;p.apply(moves[k]);}
  }
  return plies>=MaxPlies?TooLong:Ongoing;
}
}
