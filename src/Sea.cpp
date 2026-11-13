#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: as Chess.cpp
#endif
#include "Sea.h"
#include <string.h>
#include <stdio.h>
namespace sea {
void cellName(int c,char out[4]){snprintf(out,4,"%c%d",'a'+c%Size,c/Size+1);}
int parseCell(const char* t,const char** end){
  if(t[0]<'a'||t[0]>'j'||t[1]<'1'||t[1]>'9')return -1;int row=t[1]-'0';const char* e=t+2;
  if(row==1&&*e=='0'){row=10;e++;}if(*e>='0'&&*e<='9')return -1;if(end)*end=e;return cellAt(t[0]-'a',row-1);
}
int Fleet::shipAt(int c) const{for(int s=0;s<Ships;s++)for(int k=0;k<Lengths[s];k++)if(cell(s,k)==c)return s;return -1;}
bool Fleet::valid() const{
  for(int s=0;s<Ships;s++){int c=at[s];if(c>=Cells)return false;bool d=down>>s&1;
    if((d?c/Size:c%Size)+Lengths[s]>Size)return false;}
  for(int s=0;s<Ships;s++)for(int t=s+1;t<Ships;t++)for(int i=0;i<Lengths[s];i++)for(int j=0;j<Lengths[t];j++){
    int a=cell(s,i),b=cell(t,j),dc=a%Size-b%Size,dr=a/Size-b/Size;if(dc>=-1&&dc<=1&&dr>=-1&&dr<=1)return false;}
  return true;
}
// The largest ships first; a placement that gets stuck starts over.
void Fleet::random(uint32_t seed){
  uint32_t x=seed?seed:0x9e3779b9;auto next=[&]{x^=x<<13;x^=x>>17;x^=x<<5;return x;};
  for(;;){
    bool ok=true;
    for(int s=0;s<Ships&&ok;s++){
      ok=false;
      for(int tries=0;tries<300&&!ok;tries++){
        int len=Lengths[s];bool d=len>1&&(next()&1);int col=next()%(d?Size:Size-len+1),row=next()%(d?Size-len+1:Size);
        at[s]=uint8_t(row*Size+col);down=uint16_t(d?down|1<<s:down&~(1<<s));
        ok=true;for(int t=0;t<s&&ok;t++)for(int i=0;i<Lengths[s]&&ok;i++)for(int j=0;j<Lengths[t]&&ok;j++){
          int a=cell(s,i),b=cell(t,j),dc=a%Size-b%Size,dr=a/Size-b/Size;if(dc>=-1&&dc<=1&&dr>=-1&&dr<=1)ok=false;}
      }
    }
    if(ok&&valid())return;
  }
}
size_t Fleet::text(char* out,size_t cap) const{
  size_t w=0;if(!cap)return 0;out[0]=0;
  for(int s=0;s<Ships;s++){char c[4];cellName(at[s],c);int n=snprintf(out+w,cap-w,"%s%s%s",s?" ":"",c,Lengths[s]>1?(down>>s&1?"v":"h"):"");if(n<0||size_t(n)>=cap-w)break;w+=n;}
  return w;
}
bool Fleet::parse(const char* t){
  Fleet f;
  for(int s=0;s<Ships;s++){
    while(*t==' ')t++;const char* e;int c=parseCell(t,&e);if(c<0)return false;f.at[s]=uint8_t(c);t=e;
    if(Lengths[s]>1){if(*t!='h'&&*t!='v')return false;if(*t=='v')f.down|=1<<s;t++;}
    if(*t&&*t!=' ')return false;
  }
  if(!f.valid())return false;*this=f;return true;
}
const char* answerWord(uint8_t a){static const char* w[]={"","miss","hit","sunk","win"};return a<=AnswerWin?w[a]:"";}
int answerOf(const char* word){for(int a=AnswerMiss;a<=AnswerWin;a++)if(!strncmp(word,answerWord(a),strlen(answerWord(a))))return a;return NoAnswer;}
bool Home::sunk(int s) const{for(int k=0;k<Lengths[s];k++)if(!isShot(fleet.cell(s,k)))return false;return true;}
unsigned Home::shipsLeft() const{unsigned n=0;for(int s=0;s<Ships;s++)n+=!sunk(s);return n;}
uint8_t Home::answer(int c){
  shot[c>>3]|=uint8_t(1<<(c&7));int s=fleet.shipAt(c);if(s<0)return AnswerMiss;
  if(!sunk(s))return AnswerHit;return shipsLeft()?AnswerSunk:AnswerWin;
}
void Target::record(int c,uint8_t answer){
  if(answer==AnswerMiss){mark[c]=Miss;return;}
  mark[c]=Hit;if(answer==AnswerHit)return;
  // Sunk: the hit cells joined to this one along a row or a column are the ship.
  uint8_t stack[ShipCells];unsigned n=0;stack[n++]=uint8_t(c);mark[c]=Sunk;
  while(n){int x=stack[--n];const int next[4]={x%Size?x-1:-1,x%Size<Size-1?x+1:-1,x>=Size?x-Size:-1,x+Size<Cells?x+Size:-1};
    for(int y:next)if(y>=0&&mark[y]==Hit&&n<ShipCells){mark[y]=Sunk;stack[n++]=uint8_t(y);}}
}
bool Target::around(int c) const{
  if(mark[c]!=Unknown)return false;
  for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){int y=cellAt(c%Size+dc,c/Size+dr);if(y>=0&&mark[y]==Sunk)return true;}
  return false;
}
unsigned Target::sunkShips() const{
  // Each sunk ship is a straight line: count the cells that start one (no sunk cell to the left or above).
  unsigned n=0;for(int c=0;c<Cells;c++)if(mark[c]==Sunk&&!(c%Size&&mark[c-1]==Sunk)&&!(c>=Size&&mark[c-Size]==Sunk))n++;return n;
}
bool honest(const Fleet& f,const Target& t){
  if(!f.valid())return false;
  for(int c=0;c<Cells;c++){int s=f.shipAt(c);uint8_t m=t.mark[c];
    if(m==Miss&&s>=0)return false;if((m==Hit||m==Sunk)&&s<0)return false;
    if(m==Sunk)for(int k=0;k<Lengths[s];k++)if(t.mark[f.cell(s,k)]!=Sunk)return false;}
  return true;
}
}
