// Included by UiHeltec.cpp: checkers and sea battle with contacts (Duel.h) on the 128x64 screen; the T114 draws
// the same states at 240x135 (UiHires.inc). A page per game lists its games, the latest first; a game fills the screen.
// One button, checkers: on your move a click steps through your pieces that can move, then "Game menu"; hold picks
// the piece. Then a click steps through its moves (the path on the right, the end marked, the pieces taken crossed),
// then "Cancel"; hold plays the move. Sea battle: a click steps through the cells next to a ship hit but not sunk
// ("finish it"), then the columns with cells left, then "Game menu"; hold on a cell fires, hold on a column fixes it,
// then a click steps through its cells ("Cancel" last). Off your move a click goes to the next screen and hold opens
// the game menu (accept, decline, draw, shuffle the fleet, my fleet, resign, send again, delete, list).
// Joystick (GAT562): the list shows the games (up/down, OK opens). On a game the joystick moves a cursor: checkers -
// OK picks a piece and OK on a square plays the move (several paths to it: up/down choose, OK plays); sea battle -
// OK fires at the cell. Back drops the piece or returns to the list; holding OK opens the game menu.
// Games start from the node card ("Checkers: invite", "Sea battle: invite") or by accepting a challenge.
namespace {
Duel* duelOpen=nullptr;int duelSel=0,duelMenuIndex=0;bool duelMenu=false,duelFleet=false,duelPaths=false;
// duelCursor - one button: the choice (checkers: the square of a piece, -1 "Game menu"; sea: a cell, 100+column, -1
// "Game menu" or "Cancel"); joystick: the screen square, row*8+column (checkers), or the cell (sea).
// duelHeld: the picked piece; duelChoice: its move (one button: the n-th of its moves, -1 "Cancel"; joystick: the
// n-th path to the cursor while duelPaths); duelColumn: the sea column fixed by a hold; duelPly: the position seen.
int duelCursor=0,duelHeld=-1,duelChoice=0,duelColumn=-1,duelPly=-1;
Duel::Kind duelKind(){return page==SeaPage?Duel::Sea:Duel::Checkers;}
int duelPageOf(const Duel& g){return g.kind==Duel::Sea?SeaPage:CheckersPage;}
// The game open on this page: one of its kind that is still kept (else the list).
Duel* shownDuel(){if(duelOpen&&(duelOpen->state==Duel::Free||duelPageOf(*duelOpen)!=page)){duelOpen=nullptr;duelMenu=false;}return duelOpen;}
unsigned duelGames(Duel::Kind k,Duel** out){unsigned n=0;for(auto& g:duels.games)if(g.state!=Duel::Free&&g.kind==k)out[n++]=&g;
 for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&int32_t(out[j]->changedAt-out[j-1]->changedAt)>0;j--)std::swap(out[j],out[j-1]);return n;}
String duelName(Duel::Kind k){return k==Duel::Sea?t("Sea battle","Морской бой"):t("Checkers","Шашки");}
String duelState(const Duel& g){
 switch(g.state){
 case Duel::Inviting:return t("invited, waiting","вызов отправлен");
 case Duel::Invited:return t("invites you","зовёт играть");
 case Duel::Playing:
  if(g.kind==Duel::Sea)return g.aim>=0?t("waiting for the answer","ждём ответа"):g.myTurn()?t("your shot","ваш выстрел"):t("their shot","стреляет соперник");
  if(g.drawOffer==Duel::OfferedToMe)return t("offers a draw","предлагает ничью");return g.myTurn()?t("your move","ваш ход"):t("their move","ход соперника");
 case Duel::Over:return g.result==Duel::Won?t("you won","вы победили"):g.result==Duel::Lost?t("you lost","вы проиграли"):g.result==Duel::Drawn?t("draw","ничья"):g.reason==Duel::Declined?t("declined","отклонена"):t("cancelled","отменена");
 default:return "";}
}
void openDuel(Duel* g){duelOpen=g;duelHeld=-1;duelMenu=duelPaths=false;duelColumn=duelPly=-1;dirty=true;if(!g)return;duels.viewed(*g);
 duelFleet=g->kind==Duel::Sea&&(g->state==Duel::Invited||g->state==Duel::Inviting); // before the first shot: the own fleet, to shuffle it
#if defined(MM_JOYSTICK)
 duelCursor=g->kind==Duel::Sea?44:7*8+2; // the field's middle; the own bottom row
#else
 duelCursor=-2;
#endif
}
enum DuelAct {DaAccept,DaDecline,DaDraw,DaAcceptDraw,DaShuffle,DaFleet,DaResign,DaResend,DaDelete,DaList,DaClose};
unsigned duelActions(const Duel& g,DuelAct* out){unsigned n=0;
 if(g.state==Duel::Invited){out[n++]=DaAccept;out[n++]=DaDecline;}
 if(g.state==Duel::Inviting)out[n++]=DaDecline;
 if(g.canShuffle())out[n++]=DaShuffle;
 if(g.kind==Duel::Sea)out[n++]=DaFleet;
 if(g.state==Duel::Playing){if(g.kind==Duel::Checkers&&g.drawOffer==Duel::OfferedToMe)out[n++]=DaAcceptDraw;else if(g.kind==Duel::Checkers&&g.drawOffer==Duel::NoOffer)out[n++]=DaDraw;out[n++]=DaResign;}
 if(g.active()&&g.link()==ChatMessage::Failed)out[n++]=DaResend;
 if(!g.active())out[n++]=DaDelete;
#if !defined(MM_JOYSTICK)
 out[n++]=DaList; // the joystick has Back
#endif
 out[n++]=DaClose;return n;}
String duelActName(DuelAct a){switch(a){
 case DaAccept:return t("Accept challenge","Принять вызов");case DaDecline:return t("Decline / cancel","Отклонить / отменить");
 case DaDraw:return t("Offer a draw","Предложить ничью");case DaAcceptDraw:return t("Accept the draw","Согласиться на ничью");
 case DaShuffle:return t("Shuffle fleet","Перемешать флот");case DaFleet:return duelFleet?t("Their field","Поле соперника"):t("My fleet","Мой флот");
 case DaResign:return t("Resign","Сдаться");case DaResend:return t("Send again","Отправить снова");case DaDelete:return t("Delete game","Удалить игру");case DaList:return t("Game list","Список игр");
 default:return t("< Back to the game","< К игре");}}
void duelRun(Duel& g,DuelAct a){bool ok=true;switch(a){
 case DaAccept:ok=duels.accept(g);break;case DaDecline:ok=duels.decline(g);break;case DaDraw:case DaAcceptDraw:ok=duels.draw(g);break;
 case DaShuffle:ok=duels.shuffle(g);if(ok){duelFleet=true;notice(t("Fleet placed again","Флот расставлен заново"));}break;case DaFleet:duelFleet=!duelFleet;break;
 case DaResign:ok=duels.resign(g);break;case DaResend:ok=duels.resend(g);break;
 case DaDelete:ok=duels.remove(g);if(ok)duelOpen=nullptr;break;case DaList:duelOpen=nullptr;break;default:break;}
 if(!ok)notice(t("Not now: radio busy?","Сейчас нельзя"));}
void duelSent(Duel& g,bool sent){if(!sent){notice(t("Not sent","Не отправлено"));return;}hardware.ping(1400,15);if(g.link()==ChatMessage::Failed)notice(t("Not sent: will resend","Не отправлено: повторю"));}

#if MM_CHECKERS
// Board squares on the screen, the player's side at the bottom: the screen column and row of a square, and back
// (-1 on a light square).
void ckXY(const Duel& g,int s,int& x,int& y){int c=checkers::colOf(s),r=checkers::rowOf(s);if(g.mine==checkers::White){x=c;y=7-r;}else{x=7-c;y=r;}}
int ckSquare(const Duel& g,int x,int y){return g.mine==checkers::White?checkers::squareAt(x,7-y):checkers::squareAt(7-x,y);}
bool ckPicking(const Duel& g){return g.kind==Duel::Checkers&&g.state==Duel::Playing&&g.myTurn();}
// The own pieces that can move, as the player reads the board: the bottom row first, left to right.
unsigned ckPieces(const Duel& g,int* out){checkers::Move l[checkers::MaxMoves];unsigned n=g.game.pos.legal(l),k=0;bool from[32]={};for(unsigned i=0;i<n;i++)from[l[i].from()]=true;
 for(int y=7;y>=0;y--)for(int x=0;x<8;x++){int s=ckSquare(g,x,y);if(s>=0&&from[s])out[k++]=s;}return k;}
// The moves of the picked piece (to one square when to>=0): indices into l, which legal() fills.
unsigned ckMoves(const Duel& g,checkers::Move* l,int* out,int to=-1){unsigned n=g.game.pos.legal(l),k=0;for(unsigned i=0;i<n;i++)if(l[i].from()==duelHeld&&(to<0||l[i].to()==to))out[k++]=i;return k;}
// A new position drops the picked piece; the cursor that is no longer a choice moves to the first piece.
void ckSettle(const Duel& g){
 if(!ckPicking(g)){duelHeld=-1;duelPaths=false;return;}
 if(duelPly!=int(g.game.plies)){duelPly=g.game.plies;duelHeld=-1;duelPaths=false;duelChoice=0;
#if !defined(MM_JOYSTICK)
  duelCursor=-2;
#endif
 }
#if !defined(MM_JOYSTICK)
 if(duelHeld>=0||duelCursor==-1)return;int p[32];unsigned k=ckPieces(g,p);for(unsigned i=0;i<k;i++)if(p[i]==duelCursor)return;duelCursor=k?p[0]:-1;
#endif
}
// The move offered now: the chosen move of the picked piece (one button) or the chosen path to the cursor (joystick).
bool ckShown(const Duel& g,checkers::Move& m){
 if(duelHeld<0||duelChoice<0||!ckPicking(g))return false;checkers::Move l[checkers::MaxMoves];int idx[checkers::MaxMoves];
#if defined(MM_JOYSTICK)
 int to=ckSquare(g,duelCursor%8,duelCursor/8);if(!duelPaths||to<0)return false;unsigned k=ckMoves(g,l,idx,to);
#else
 unsigned k=ckMoves(g,l,idx);
#endif
 if(!k)return false;m=l[idx[duelChoice%k]];return true;}
// The squares a move takes pieces from (the Turkish strike: each once).
unsigned ckTaken(const Duel& g,const checkers::Move& m,int* out){unsigned k=0;if(!m.capture)return 0;
 for(unsigned i=1;i<m.n;i++){int a=m.path[i-1],b=m.path[i],dc=checkers::colOf(b)>checkers::colOf(a)?1:-1,dr=checkers::rowOf(b)>checkers::rowOf(a)?1:-1;
  for(int x=checkers::colOf(a)+dc,y=checkers::rowOf(a)+dr;x!=checkers::colOf(b);x+=dc,y+=dr){int s=checkers::squareAt(x,y);if(g.game.pos.board[s])out[k++]=s;}}
 return k;}
String ckMoveText(const checkers::Move& m){char b[40];checkers::moveText(m,b,sizeof b);return b;}
// The choice in words: the piece ("c3"), the move ("c3:e5:g7"), "Game menu" or "Cancel".
String ckChoiceText(const Duel& g){
 checkers::Move m;if(ckShown(g,m))return ckMoveText(m);
 if(duelHeld>=0)return t("Cancel","Отмена");if(duelCursor<0)return t("Game menu","Меню партии");char b[3];checkers::squareName(duelCursor,b);return b;}
void ckPlay(Duel& g,const checkers::Move& m){bool sent=duels.move(g,m);if(sent)duelHeld=-1;duelPaths=false;duelSent(g,sent);}
#if defined(MM_JOYSTICK)
bool ckJoy(Duel& g,bool up,bool down,bool left,bool right,bool ok,bool back){
 ckSettle(g);int x=duelCursor%8,y=duelCursor/8,s=ckSquare(g,x,y);checkers::Move l[checkers::MaxMoves];int idx[checkers::MaxMoves];
 if(duelPaths){unsigned k=s>=0?ckMoves(g,l,idx,s):0;if(!k){duelPaths=false;return true;}
  if(up||down){duelChoice=(duelChoice+(up?k-1:1))%k;return true;}
  if(ok)ckPlay(g,l[idx[duelChoice%k]]);else duelPaths=false;return true;}
 if(back){if(duelHeld>=0)duelHeld=-1;else duelOpen=nullptr;return true;}
 if(up)y=(y+7)%8;if(down)y=(y+1)%8;if(left)x=(x+7)%8;if(right)x=(x+1)%8;
 if(up||down||left||right){duelCursor=y*8+x;return true;}
 if(!ok)return true;
 if(g.state!=Duel::Playing){duelMenu=true;duelMenuIndex=0;return true;} // accept, decline or delete
 if(!g.myTurn()){notice(t("Their move","Ход соперника"));return true;}
 if(s>=0&&g.game.pos.own(s)>0){int p[32];unsigned k=ckPieces(g,p);for(unsigned i=0;i<k;i++)if(p[i]==s){duelHeld=s;return true;}
  unsigned n=g.game.pos.legal(l);notice(n&&l[0].capture?t("You must capture","Бить обязательно"):t("This piece cannot move","Этой шашке хода нет"));return true;}
 if(duelHeld<0){notice(t("Pick your piece","Выберите свою шашку"));return true;}
 unsigned k=s>=0?ckMoves(g,l,idx,s):0;
 if(!k){notice(t("Not a legal move","Так ходить нельзя"));return true;}
 if(k==1)ckPlay(g,l[idx[0]]);else{duelPaths=true;duelChoice=0;} // several paths to the square: up/down choose
 return true;
}
#else
bool ckButton(Duel& g,bool click,bool hold){
 ckSettle(g);
 if(duelHeld<0){int p[32];unsigned k=ckPieces(g,p);
  if(click){unsigned i=0;while(i<k&&p[i]!=duelCursor)i++;duelCursor=duelCursor<0?(k?p[0]:-1):i+1<k?p[i+1]:-1;return true;}
  if(hold){if(duelCursor<0){duelMenu=true;duelMenuIndex=0;}else{duelHeld=duelCursor;duelChoice=0;}}return true;}
 checkers::Move l[checkers::MaxMoves];int idx[checkers::MaxMoves];unsigned k=ckMoves(g,l,idx);
 if(click){duelChoice=duelChoice<0?0:duelChoice+1<int(k)?duelChoice+1:-1;return true;}
 if(!hold)return true;
 if(duelChoice<0||!k){duelHeld=-1;return true;} // Cancel: the cursor stays on the piece
 ckPlay(g,l[idx[duelChoice%k]]);return true;
}
#endif
// 8x8 squares: empty dark ones dotted; white men a ring, black men a disc, kings a bullseye (a dot in the ring, a
// ring cut into the disc). The last move: corner ticks; the picked piece blinks; its other moves end in a dot, the
// offered one in a frame, with its landings dotted and the pieces it takes crossed.
void ckBoard(const Duel& g){
 auto& c=*hardware.canvas;c.fillScreen(0);ckSettle(g);bool picking=ckPicking(g),blink=(millis()/300)%2;
 checkers::Move shown;bool offer=ckShown(g,shown);int taken[12];unsigned nt=offer?ckTaken(g,shown,taken):0;
 checkers::Move l[checkers::MaxMoves];unsigned n=duelHeld>=0&&picking?g.game.pos.legal(l):0;
 for(int s=0;s<32;s++){int x,y;ckXY(g,s,x,y);x*=8;y*=8;
  int8_t p=g.game.pos.board[s];bool cross=false;for(unsigned i=0;i<nt;i++)cross|=taken[i]==s;
  if(!p)for(int yy=0;yy<8;yy+=2)for(int xx=0;xx<8;xx+=2)c.drawPixel(x+xx,y+yy,1);
  else{bool white=p>0,king=p==checkers::King||p==-checkers::King;
   if(white){c.drawCircle(x+4,y+4,3,1);if(king)c.fillRect(x+3,y+3,3,3,1);}else{c.fillCircle(x+4,y+4,3,1);if(king)c.drawCircle(x+4,y+4,1,0);}
   if(cross){c.drawLine(x+1,y+1,x+7,y+7,!white);c.drawLine(x+7,y+1,x+1,y+7,!white);}}
  for(unsigned i=0;i<n;i++)if(l[i].from()==duelHeld&&l[i].to()==s)c.fillRect(x+3,y+3,2,2,1);
 }
 if(g.game.plies)for(int s:{int(g.game.last.from()),int(g.game.last.to())}){int x,y;ckXY(g,s,x,y);x*=8;y*=8;c.drawPixel(x,y,1);c.drawPixel(x+7,y,1);c.drawPixel(x,y+7,1);c.drawPixel(x+7,y+7,1);}
 if(offer){for(unsigned i=1;i+1<shown.n;i++){int x,y;ckXY(g,shown.path[i],x,y);c.fillRect(x*8+3,y*8+3,2,2,1);c.drawRect(x*8+2,y*8+2,4,4,1);}
  int x,y;ckXY(g,shown.to(),x,y);c.drawRect(x*8,y*8,8,8,1);c.drawRect(x*8+1,y*8+1,6,6,1);c.fillRect(x*8+3,y*8+3,2,2,1);}
 if(duelHeld>=0&&blink){int x,y;ckXY(g,duelHeld,x,y);c.drawRect(x*8,y*8,8,8,1);}
#if defined(MM_JOYSTICK)
 {int x=duelCursor%8,y=duelCursor/8;c.drawRect(x*8,y*8,8,8,1);c.drawRect(x*8+1,y*8+1,6,6,1);}
#else
 if(picking&&duelHeld<0&&duelCursor>=0&&(millis()/400)%3){int x,y;ckXY(g,duelCursor,x,y);c.drawRect(x*8,y*8,8,8,1);c.drawRect(x*8+1,y*8+1,6,6,0);}
#endif
 c.drawFastVLine(65,0,64,1);bool white=g.mine==checkers::White;
 say(68,8,clipped(g.name,11),small);say(68,17,t("you: ","вы: ")+(white?t("white","белые"):t("black","чёрные")),small);
 String st=duelState(g);if(picking){unsigned k=g.game.pos.legal(l);if(k&&l[0].capture)st=t("must take","надо бить");}
 textLinesAt(st,68,27,picking?1:2,11);
#if defined(MM_JOYSTICK)
 if(offer){checkers::Move l2[checkers::MaxMoves];int idx[checkers::MaxMoves];unsigned k=ckMoves(g,l2,idx,shown.to());textLinesAt(String(duelChoice%k+1)+"/"+String(k)+" "+ckMoveText(shown),68,36,2,11);}
 else if(g.game.plies)say(68,46,clipped(String((g.game.plies+1)/2)+(g.game.plies%2?". ":"... ")+ckMoveText(g.game.last),11),small);
 say(68,55,clipped(offer?t("^v path, OK","^v путь, OK"):linkText(g.link(),g.autoStopped),11),small);
 say(68,63,t("hold OK: menu","держ.OK: меню"),u8g2_font_4x6_t_cyrillic);
#else
 if(picking){
  // The current choice, lit when it is "Game menu" or "Cancel"; a long path takes two rows.
  bool word=duelHeld>=0?!offer:duelCursor<0;if(word)c.fillRect(67,29,61,9,1);
  String ch=ckChoiceText(g);if(word)say(68,36,clipped(ch,11),small,0);else textLinesAt(ch,68,36,2,12);
  say(68,55,t("click: next","клик: далее"),u8g2_font_4x6_t_cyrillic);
  say(68,63,duelHeld<0?(duelCursor<0?t("hold: menu","держ: меню"):t("hold: pick","держ: взять")):offer?t("hold: move","держ: сходить"):t("hold: cancel","держ: отмена"),u8g2_font_4x6_t_cyrillic);}
 else{if(g.game.plies)say(68,46,clipped(String((g.game.plies+1)/2)+(g.game.plies%2?". ":"... ")+ckMoveText(g.game.last),11),small);
  say(68,55,clipped(linkText(g.link(),g.autoStopped),11),small);say(68,63,t("hold: menu","держ: меню"),u8g2_font_4x6_t_cyrillic);}
#endif
}
#endif

#if MM_SEA
// Cells that can still hold a ship: not shot, not by a sunk ship, not diagonal to a hit (ships never touch).
bool seaPossible(const Duel& g,int c){if(!g.target.open(c))return false;for(int dy=-1;dy<=1;dy+=2)for(int dx=-1;dx<=1;dx+=2){int d=sea::cellAt(c%10+dx,c/10+dy);if(d>=0&&g.target.mark[d]==sea::Hit)return false;}return true;}
bool seaPicking(const Duel& g){return g.kind==Duel::Sea&&g.state==Duel::Playing&&g.myTurn();}
// One button: the cells next to a ship hit but not sunk (along its line once two cells are hit), the columns with
// cells left (100+column), then "Game menu" (-1); with a column fixed: its cells, then "Cancel" (-1). 112 at most.
unsigned seaChoices(const Duel& g,int* out){
 unsigned k=0;
 if(duelColumn>=0){for(int r=0;r<10;r++)if(seaPossible(g,r*10+duelColumn))out[k++]=r*10+duelColumn;out[k++]=-1;return k;}
 for(int c=0;c<sea::Cells;c++){if(g.target.mark[c]!=sea::Hit)continue;int x=c%10,y=c/10;auto hit=[&](int d){return d>=0&&g.target.mark[d]==sea::Hit;};
  bool across=hit(sea::cellAt(x-1,y))||hit(sea::cellAt(x+1,y)),down=hit(sea::cellAt(x,y-1))||hit(sea::cellAt(x,y+1));
  const int8_t d[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
  for(auto& v:d){if((across&&v[1])||(down&&v[0]))continue;int n=sea::cellAt(x+v[0],y+v[1]);bool have=false;for(unsigned i=0;i<k;i++)have|=out[i]==n;if(n>=0&&!have&&seaPossible(g,n))out[k++]=n;}}
 for(int x=0;x<10;x++)for(int r=0;r<10;r++)if(seaPossible(g,r*10+x)){out[k++]=100+x;break;}
 out[k++]=-1;return k;}
bool seaHunting(const Duel& g,int c){if(duelColumn>=0||c<0||c>=100)return false;int l[112];unsigned k=seaChoices(g,l);for(unsigned i=0;i<k&&l[i]<100;i++)if(l[i]==c)return true;return false;}
void seaSettle(const Duel& g){
 if(!seaPicking(g)){duelColumn=-1;return;}
 if(duelPly!=int(g.shots)){duelPly=g.shots;duelColumn=-1;
#if defined(MM_JOYSTICK)
  int l[112];if(seaChoices(g,l)&&l[0]>=0&&l[0]<100)duelCursor=l[0]; // finish the ship: the cursor next to it
#else
  duelCursor=-2;
#endif
 }
#if !defined(MM_JOYSTICK)
 int l[112];unsigned k=seaChoices(g,l);for(unsigned i=0;i<k;i++)if(l[i]==duelCursor)return;duelCursor=l[0];
#endif
}
String seaCell(int c){char b[4];sea::cellName(c,b);return b;}
String seaChoiceText(const Duel& g){
 if(duelCursor<0)return duelColumn>=0?t("Cancel","Отмена"):t("Game menu","Меню партии");
 if(duelCursor>=100)return t("column ","столбец ")+String(char('a'+duelCursor-100));
 return seaCell(duelCursor)+(seaHunting(g,duelCursor)?t(": finish",": добить"):String());}
String seaMark(uint8_t m){return m==sea::Miss?t("miss","мимо"):m==sea::Hit?t("hit","попал"):m==sea::Sunk?t("sunk","потоплен"):String("?");}
void seaFire(Duel& g,int cell){bool sent=duels.shoot(g,cell);if(sent)duelColumn=-1;duelSent(g,sent);}
#if defined(MM_JOYSTICK)
bool seaJoy(Duel& g,bool up,bool down,bool left,bool right,bool ok,bool back){
 seaSettle(g);if(back){duelOpen=nullptr;return true;}
 int x=duelCursor%10,y=duelCursor/10;if(up)y=(y+9)%10;if(down)y=(y+1)%10;if(left)x=(x+9)%10;if(right)x=(x+1)%10;
 if(up||down||left||right){duelCursor=y*10+x;return true;}
 if(!ok)return true;
 if(g.state!=Duel::Playing){duelMenu=true;duelMenuIndex=0;return true;}
 if(!g.myTurn())notice(g.aim>=0?t("Waiting for the answer","Ждём ответа"):t("Their shot","Стреляет соперник"));
 else if(!g.target.open(duelCursor))notice(t("Already known","Уже известно"));
 else seaFire(g,duelCursor);
 return true;
}
#else
bool seaButton(Duel& g,bool click,bool hold){
 seaSettle(g);int l[112];unsigned k=seaChoices(g,l);
 if(click){unsigned i=0;while(i<k&&l[i]!=duelCursor)i++;duelCursor=l[(i+1)%k];return true;}
 if(!hold)return true;
 if(duelCursor<0){if(duelColumn>=0){duelCursor=100+duelColumn;duelColumn=-1;}else{duelMenu=true;duelMenuIndex=0;}return true;}
 if(duelCursor>=100){duelColumn=duelCursor-100;duelCursor=seaChoices(g,l)?l[0]:-1;return true;}
 seaFire(g,duelCursor);return true;
}
#endif
// 6 px cells (5x5 inside, ticked on the frame): their field - a miss or water by a sunk ship a dot, a hit a cross,
// a sunk ship filled, their fleet shown at the end framed, my shot in flight blinking; my fleet - ships framed, hits
// crossed, sunk ships filled, their misses dotted. The last shot at the field: its cell framed.
void seaField(const Duel& g,bool own,int x0,int y0){
 auto& c=*hardware.canvas;c.drawRect(x0,y0,61,61,1);for(int i=1;i<10;i++){c.drawPixel(x0+i*6,y0+1,1);c.drawPixel(x0+i*6,y0+59,1);c.drawPixel(x0+1,y0+i*6,1);c.drawPixel(x0+59,y0+i*6,1);} // the cells ticked on the frame
 bool blink=(millis()/300)%2;int last=own?g.lastIn:g.lastOut;
 for(int k=0;k<sea::Cells;k++){int x=x0+1+k%10*6,y=y0+1+k/10*6;
  if(own){int s=g.home.fleet.shipAt(k);bool shot=g.home.isShot(k);
   if(s>=0&&shot&&g.home.sunk(s))c.fillRect(x,y,5,5,1);else if(s>=0){c.drawRect(x,y,5,5,1);if(shot){c.drawLine(x,y,x+4,y+4,1);c.drawLine(x+4,y,x,y+4,1);}}else if(shot)c.drawPixel(x+2,y+2,1);}
  else{uint8_t m=g.target.mark[k];
   if(k==g.aim){if(blink)c.fillRect(x+1,y+1,3,3,1);c.drawRect(x,y,5,5,1);}
   else if(m==sea::Miss||g.target.around(k))c.drawPixel(x+2,y+2,1);
   else if(m==sea::Hit){c.drawLine(x,y,x+4,y+4,1);c.drawLine(x+4,y,x,y+4,1);}
   else if(m==sea::Sunk)c.fillRect(x,y,5,5,1);
   else if(g.revealed&&g.theirs.shipAt(k)>=0){c.drawRect(x,y,5,5,1);c.drawPixel(x+2,y+2,1);}}
  if(k==last)c.drawRect(x-1,y-1,7,7,1);}
}
void seaBoard(const Duel& g){
 auto& c=*hardware.canvas;c.fillScreen(0);seaSettle(g);bool picking=seaPicking(g)&&!duelFleet,blink=(millis()/400)%3;
 seaField(g,duelFleet,0,0);
 // The choice: a cell or a column framed (blinking), the column also marked under the field.
#if defined(MM_JOYSTICK)
 if(!duelFleet&&g.state==Duel::Playing){int x=duelCursor%10*6,y=duelCursor/10*6;c.drawRect(x,y,7,7,1);if(blink)c.drawRect(x+1,y+1,5,5,0);c.fillRect(x+1,62,5,2,1);}
#else
 if(picking){int col=duelColumn>=0?duelColumn:duelCursor>=100?duelCursor-100:-1;
  if(col>=0){c.fillRect(col*6+1,62,5,2,1);if(duelColumn<0&&blink)c.drawRect(col*6,0,7,61,1);}
  if(duelCursor>=0&&duelCursor<100&&blink){int x=duelCursor%10*6,y=duelCursor/10*6;c.drawRect(x,y,7,7,1);c.drawRect(x+1,y+1,5,5,0);}}
#endif
 const uint8_t* tiny=u8g2_font_4x6_t_cyrillic;
 say(64,8,clipped(g.name,12),small);
 if(duelFleet){say(64,17,t("my fleet","мой флот"),small);say(64,26,clipped(g.state==Duel::Playing?t("afloat ","целы ")+String(g.home.shipsLeft())+"/10":duelState(g),12),small);
  if(g.canShuffle())textLinesAt(t("menu: shuffle","меню: перемешать"),64,36,2,12);}
 else{bool one=picking||g.state==Duel::Over;textLinesAt(duelState(g),64,17,one?1:2,12);
  int y=one?25:34;say(64,y,t("sunk ","потоплено ")+String(g.target.sunkShips())+"/10",tiny);say(64,y+7,t("mine ","моих ")+String(g.home.shipsLeft())+"/10",tiny);}
 if(picking){bool word=duelCursor<0;if(word)c.fillRect(63,35,65,10,1);say(64,43,clipped(seaChoiceText(g),12),small,!word);
#if defined(MM_JOYSTICK)
  say(64,55,t("OK: fire","OK: огонь"),tiny);say(64,63,t("hold OK: menu","держ.OK: меню"),tiny);return;}
#else
  say(64,55,t("click: next","клик: далее"),tiny);
  say(64,63,duelCursor<0?(duelColumn>=0?t("hold: cancel","держ: отмена"):t("hold: menu","держ: меню")):duelCursor>=100?t("hold: fix","держ: выбрать"):t("hold: fire","держ: огонь"),tiny);return;}
#endif
 bool unfair=g.state==Duel::Over&&g.revealed&&!g.fair; // their fleet shown at the end: some answer was false
 if(unfair)textLinesAt(t("answers do not match!","ответы не сходятся!"),64,44,2,12);
 else if(!duelFleet&&g.lastOut>=0)say(64,48,t("you ","вы ")+seaCell(g.lastOut)+" "+seaMark(g.target.mark[g.lastOut]),tiny);
 if(!unfair)say(64,55,g.lastIn>=0&&g.state==Duel::Playing?t("they ","они ")+seaCell(g.lastIn)+" "+seaMark(g.lastAnswer==sea::AnswerWin?sea::Sunk:g.lastAnswer):clipped(linkText(g.link(),g.autoStopped),15),tiny);
#if defined(MM_JOYSTICK)
 say(64,63,t("hold OK: menu","держ.OK: меню"),tiny);
#else
 say(64,63,duelFleet&&g.state==Duel::Playing?t("click: back","клик: назад"):t("hold: menu","держ: меню"),tiny);
#endif
}
#endif

void drawDuel(const Duel& g){
#if MM_SEA
 if(g.kind==Duel::Sea)seaBoard(g);
#endif
#if MM_CHECKERS
 if(g.kind==Duel::Checkers)ckBoard(g);
#endif
 auto& c=*hardware.canvas;
 if(duelMenu){DuelAct acts[12];unsigned k=duelActions(g,acts);duelMenuIndex%=k;int first=max(0,min(duelMenuIndex-1,int(k)-3));
  c.fillRect(0,12,128,45,0);c.drawRect(0,12,128,45,1);
  for(unsigned i=first;i<k&&i<unsigned(first+3);i++){int y=14+(i-first)*14;bool focus=int(i)==duelMenuIndex;if(focus)c.fillRect(2,y,124,13,1);say(5,y+10,clipped(duelActName(acts[i]),20),body,focus?0:1);}}
 if(action.length()&&millis()-actionAt<3000){c.fillRect(0,22,128,20,0);c.drawRect(0,22,128,20,1);say(3,36,clipped(action,20));}
 hardware.flush();
}
String duelListSide(const Duel& g){return g.kind==Duel::Sea?String(g.target.sunkShips())+":"+String(10-g.home.shipsLeft()):g.game.plies?String((g.game.plies+1)/2)+t(" mv"," ход"):String();}
void drawDuelList(String& title){
 auto& c=*hardware.canvas;Duel::Kind k=duelKind();Duel* games[DuelNet::Max];unsigned n=duelGames(k,games);title=duelName(k)+(n?" "+String(duelSel%n+1)+"/"+String(n):"");
 if(!n){say(0,28,t("No games yet","Игр пока нет"));
#if defined(MM_JOYSTICK)
  say(0,40,t("Nodes: OK - invite","Узлы: OK - вызвать"),small);
#else
  say(0,40,t("Node menu: invite","Меню узла: вызвать"),small);
#endif
  say(0,49,t("or accept a challenge","или примите вызов"),small);return;}
 duelSel%=n;int first=max(0,min(duelSel-1,int(n)-3));
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){const Duel& g=*games[i];int y=13+(i-first)*15;bool focus=int(i)==duelSel;
  if(focus)c.fillRect(0,y,128,15,1);say(2,y+7,clipped(String(g.unseen?"* ":"")+g.name,14),small,!focus);sayRight(126,y+7,duelListSide(g),small,!focus);
  say(2,y+14,clipped(duelState(g),24),small,!focus);}
}
// Double click back (one button): the menu closes, a fixed column or a picked piece is dropped, the fleet view
// returns to their field, a game returns to the list. False: nothing of a game to step back from.
bool duelBack(){
 if((page!=CheckersPage&&page!=SeaPage)||!shownDuel())return false;
 if(duelMenu)duelMenu=false;else if(duelColumn>=0){duelCursor=100+duelColumn;duelColumn=-1;}else if(duelFleet)duelFleet=false;else if(duelHeld>=0)duelHeld=-1;else duelOpen=nullptr;
 return true;
}
bool duelMenuKey(Duel& g,bool next,bool prev,bool run){
 DuelAct acts[12];unsigned k=duelActions(g,acts);
 if(next||prev){duelMenuIndex=(duelMenuIndex+(prev?k-1:1))%k;return true;}
 duelMenu=false;if(run)duelRun(g,acts[duelMenuIndex%k]);return true;
}
#if defined(MM_JOYSTICK)
// Keys while a game page is shown; false lets the page switch handle them (list view only).
bool duelKey(int key){
 bool up=key==0xb5,down=key==0xb6,left=key==0xb4,right=key==0xb7,ok=key==13,hold=key==0xa3,back=key==0x86||key==0x82;
 if(!shownDuel()){Duel* games[DuelNet::Max];unsigned n=duelGames(duelKind(),games);
  if(up||down){if(n)duelSel=(duelSel+(up?n-1:1))%n;return true;}
  if((ok||hold)&&n){openDuel(games[duelSel%n]);return true;}
  return false;}
 Duel& g=*duelOpen;
 if(duelMenu)return duelMenuKey(g,down,up,ok||hold);
 if(hold){duelMenu=true;duelMenuIndex=0;return true;}
 if(duelFleet){if(ok||back)duelFleet=false;return true;}
#if MM_SEA
 if(g.kind==Duel::Sea)return seaJoy(g,up,down,left,right,ok,back);
#endif
#if MM_CHECKERS
 if(g.kind==Duel::Checkers)return ckJoy(g,up,down,left,right,ok,back);
#endif
 return true;
}
#else
// The button while a game page is shown; false lets the page switch handle it (the next screen).
bool duelButton(int key){
 bool click=key==13||key==0x82,hold=key==0xa3;
 if(!shownDuel()||key==0x86)return false; // the list: the page menu opens the game; Back (USB): Home
 Duel& g=*duelOpen;
 if(duelMenu)return duelMenuKey(g,click,false,hold);
 if(duelFleet&&click){duelFleet=false;return true;}
 bool picking=false;
#if MM_SEA
 picking|=seaPicking(g)&&!duelFleet;
#endif
#if MM_CHECKERS
 picking|=ckPicking(g);
#endif
 if(!picking){if(hold){duelMenu=true;duelMenuIndex=0;return true;}return false;} // the board stays open
#if MM_SEA
 if(g.kind==Duel::Sea)return seaButton(g,click,hold);
#endif
#if MM_CHECKERS
 if(g.kind==Duel::Checkers)return ckButton(g,click,hold);
#endif
 return true;
}
#endif
void duelStatus(JsonObject d){
 char id[5];snprintf(id,sizeof id,"%04X",duelOpen->id);d["duel_game"]=id;d["duel_cursor"]=duelCursor;d["duel_held"]=duelHeld;d["duel_choice"]=duelChoice;
 d["duel_column"]=duelColumn;d["duel_fleet"]=duelFleet;d["duel_paths"]=duelPaths;d["duel_menu"]=duelMenu;
 if(duelMenu){DuelAct acts[12];unsigned k=duelActions(*duelOpen,acts);d["duel_act"]=int(acts[duelMenuIndex%k]);}
}
}
