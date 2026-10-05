// Included by UiHeltec.cpp: chess with contacts on the 128x64 screen.
// Joystick (GAT562): the list shows the games (up/down, OK opens). On the board the joystick moves a
// cursor; OK picks a piece and OK on a highlighted square plays the move (a pawn promotes to a queen);
// Back drops the piece or returns to the list; holding OK opens the game menu.
// One button (Heltec and the other compact boards): the list opens a game from its menu. On your move
// a click steps through your pieces that can move, then "Game menu"; hold picks the piece. Then a click
// steps through the squares it reaches, then "Cancel"; hold plays the move. Off your move a click goes
// to the next screen and hold opens the game menu.
// Games start from the node card ("Chess: invite") or by accepting a challenge. Network, rules and
// storage: ChessNet.
namespace {
// chessCursor -1: on one-button boards, the choice after the squares ("Game menu" or "Cancel").
ChessMatch* chessOpen=nullptr;int chessSel=0,chessCursor=12,chessHeld=-1,chessPly=-1;bool chessMenu=false;int chessMenuIndex=0;
// 5x6 silhouettes, top row first, bit 4 = left column. Black pieces are solid; white pieces are
// the one-pixel ring around a dark silhouette, so the two sides differ even on an 8-pixel square.
const uint8_t glyphs[7][6]={{0},
 {0x00,0x04,0x0e,0x04,0x0e,0x1f},  // pawn
 {0x06,0x0f,0x1b,0x07,0x0e,0x1f},  // knight
 {0x04,0x0c,0x0a,0x0e,0x04,0x1f},  // bishop
 {0x15,0x1f,0x0e,0x0e,0x0e,0x1f},  // rook
 {0x15,0x0a,0x1f,0x0e,0x0e,0x1f},  // queen
 {0x04,0x0e,0x04,0x1f,0x0e,0x1f}}; // king
// Up to `rows` lines of `cols` characters from x, wrapped at spaces.
void textLinesAt(const String& value,int x,int y,unsigned rows,unsigned cols){
 unsigned at=0;for(unsigned r=0;r<rows&&at<value.length();r++){unsigned n=0,end=at,space=0;
  while(end<value.length()&&n<cols){uint8_t c=value[end];end+=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;n++;if(c==' ')space=end;}
  if(end<value.length()&&space>at)end=space;say(x,y+r*9,value.substring(at,end),small);at=end;while(at<value.length()&&value[at]==' ')at++;}
}
bool glyphAt(int piece,int x,int y){return x>=0&&x<5&&y>=0&&y<6&&(glyphs[piece][y]>>(4-x)&1);}
bool ringAt(int piece,int x,int y){if(glyphAt(piece,x,y))return false;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(glyphAt(piece,x+dx,y+dy))return true;return false;}
unsigned chessGames(ChessMatch** out){unsigned n=0;for(auto& m:chessNet.matches)if(m.state!=ChessMatch::Free)out[n++]=&m;
 for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&out[j]->changedAt>out[j-1]->changedAt;j--)std::swap(out[j],out[j-1]);return n;}
String chessStateText(const ChessMatch& m){
 switch(m.state){
 case ChessMatch::Inviting:return t("invited, waiting","вызов отправлен");
 case ChessMatch::Invited:return t("challenges you","вызывает вас");
 case ChessMatch::Playing:if(m.drawOffer==ChessMatch::OfferedToMe)return t("offers a draw","предлагает ничью");
  return m.myTurn()?(m.game.pos.inCheck()?t("your move, check!","ваш ход, шах!"):t("your move","ваш ход")):t("their move","ход соперника");
 case ChessMatch::Over:return m.won()?t("you won","вы победили"):m.lost()?t("you lost","вы проиграли"):m.result==ChessMatch::Drawn?t("draw","ничья"):t("ended","завершена");
 default:return "";
 }
}
String chessLink(const ChessMatch& m){
 switch(m.link()){case ChatMessage::Queued:return t("sending","отправка");case ChatMessage::Sent:return t("no ACK yet","ждём ACK");
 case ChatMessage::Delivered:return t("delivered","доставлено");case ChatMessage::Failed:return m.autoStopped?t("not delivered","не доставлено"):t("will resend","повторю");}
 return "";
}
// Board squares on the screen: the player's side at the bottom.
int squareAt(const ChessMatch& m,int col,int row){return m.mine==chess::White?(7-row)*8+col:row*8+(7-col);}
void squareXY(const ChessMatch& m,int sq,int& x,int& y){int f=chess::fileOf(sq),r=chess::rankOf(sq);if(m.mine==chess::White){x=f*8;y=(7-r)*8;}else{x=(7-f)*8;y=r*8;}}
bool targetOf(const ChessMatch& m,int from,int to,chess::Move* found=nullptr){
 chess::Move list[chess::MaxMoves];unsigned n=m.game.pos.legal(list);chess::Move best=0;
 for(unsigned i=0;i<n;i++)if(chess::moveFrom(list[i])==from&&chess::moveTo(list[i])==to&&(!best||chess::movePromotion(list[i])==chess::Queen))best=list[i];
 if(found)*found=best;return best!=0;
}
void openChess(ChessMatch* m){chessOpen=m;chessHeld=-1;chessPly=-1;chessMenu=false;if(!m)return;chessNet.viewed(*m);int k=m->game.pos.king(m->mine);chessCursor=k>=0?k:12;dirty=true;}
enum ChessAct {CaAccept,CaDecline,CaDraw,CaAcceptDraw,CaResign,CaResend,CaDelete,CaList,CaClose};
unsigned chessActions(const ChessMatch& m,ChessAct* out){unsigned n=0;
 if(m.state==ChessMatch::Invited){out[n++]=CaAccept;out[n++]=CaDecline;}
 if(m.state==ChessMatch::Inviting)out[n++]=CaDecline;
 if(m.state==ChessMatch::Playing){if(m.drawOffer==ChessMatch::OfferedToMe)out[n++]=CaAcceptDraw;else if(m.drawOffer==ChessMatch::NoOffer)out[n++]=CaDraw;out[n++]=CaResign;}
 if(m.active()&&m.link()==ChatMessage::Failed)out[n++]=CaResend;
 if(!m.active())out[n++]=CaDelete;
#if !defined(MM_JOYSTICK)
 out[n++]=CaList; // the joystick has Back
#endif
 out[n++]=CaClose;return n;}
String chessActName(ChessAct a){switch(a){
 case CaAccept:return t("Accept challenge","Принять вызов");case CaDecline:return t("Decline / cancel","Отклонить / отменить");
 case CaDraw:return t("Offer a draw","Предложить ничью");case CaAcceptDraw:return t("Accept the draw","Согласиться на ничью");
 case CaResign:return t("Resign","Сдаться");case CaResend:return t("Send again","Отправить снова");case CaDelete:return t("Delete game","Удалить партию");case CaList:return t("Game list","Список партий");
 default:return t("< Back to board","< К доске");}}
void chessRun(ChessMatch& m,ChessAct a){bool ok=true;switch(a){
 case CaAccept:ok=chessNet.accept(m);break;case CaDecline:ok=chessNet.decline(m);break;case CaDraw:ok=chessNet.offerDraw(m);break;
 case CaAcceptDraw:ok=chessNet.acceptDraw(m);break;case CaResign:ok=chessNet.resign(m);break;case CaResend:ok=chessNet.resend(m);break;
 case CaDelete:ok=chessNet.remove(m);if(ok)chessOpen=nullptr;break;case CaList:chessOpen=nullptr;break;default:break;}
 if(!ok)notice(t("Not now: radio busy?","Сейчас нельзя"));}
#if !defined(MM_JOYSTICK)
// One button: the choices in the order the player sees them, bottom row first, left to right: the own
// pieces that can move, or the squares the picked piece reaches.
unsigned chessChoices(const ChessMatch& m,int* out){
 bool to[64]={};chess::Move list[chess::MaxMoves];unsigned n=m.game.pos.legal(list);
 for(unsigned i=0;i<n;i++){if(chessHeld<0)to[chess::moveFrom(list[i])]=true;else if(chess::moveFrom(list[i])==chessHeld)to[chess::moveTo(list[i])]=true;}
 unsigned k=0;for(int o=0;o<64;o++){int sq=squareAt(m,o%8,7-o/8);if(to[sq])out[k++]=sq;}return k;
}
bool chessPicking(const ChessMatch& m){return m.state==ChessMatch::Playing&&m.myTurn();}
// A new position starts from the first piece; a cursor that is no longer a choice moves to the first one.
void chessSettle(const ChessMatch& m){
 if(!chessPicking(m)){chessHeld=-1;return;}
 if(chessPly!=int(m.game.plies)){chessPly=m.game.plies;chessHeld=-1;chessCursor=-2;}
 if(chessCursor==-1)return;
 int c[64];unsigned k=chessChoices(m,c);for(unsigned i=0;i<k;i++)if(c[i]==chessCursor)return;chessCursor=k?c[0]:-1;
}
// The choice in words: the piece ("Ng1") or the move ("Ng1-f3", "e7-e8=Q"), in the screen language.
String chessChoiceText(const ChessMatch& m){
 if(chessCursor<0)return chessHeld<0?t("Game menu","Меню партии"):t("Cancel","Отмена");
 auto name=[](int sq){char b[3]={char('a'+chess::fileOf(sq)),char('1'+chess::rankOf(sq)),0};return String(b);};
 int from=chessHeld<0?chessCursor:chessHeld;int8_t p=m.game.pos.board[from];int type=p>0?p:-p;
 String san=(type==chess::Pawn?String():String(" PNBRQK"[type]))+name(from);
 if(chessHeld>=0){chess::Move mv;san+="-"+name(chessCursor);if(targetOf(m,chessHeld,chessCursor,&mv)&&chess::movePromotion(mv))san+="=Q";}
 return chessLocalSan(san.c_str());
}
#endif
void drawChessBoard(const ChessMatch& m){
 auto& c=*hardware.canvas;c.fillScreen(0);
#if defined(MM_JOYSTICK)
 bool picking=false,cursor=true;
#else
 // No cursor off the own move; on a piece it blinks so the piece shows.
 chessSettle(m);bool picking=chessPicking(m),cursor=picking&&chessCursor>=0&&(chessHeld>=0||(millis()/400)%3);
#endif
 chess::Move list[chess::MaxMoves];unsigned n=chessHeld>=0&&m.myTurn()?m.game.pos.legal(list):0;
 for(int sq=0;sq<64;sq++){int x,y;squareXY(m,sq,x,y);bool dark=(chess::fileOf(sq)+chess::rankOf(sq))%2==0;
  if(dark)for(int yy=0;yy<8;yy+=2)for(int xx=0;xx<8;xx+=2)c.drawPixel(x+xx,y+yy,1);
  int8_t p=m.game.pos.board[sq];
  if(p){int type=p>0?p:-p;
   // Glyph at (x+2,y+1); its ring spans the square without touching the neighbours.
   for(int yy=-1;yy<=6;yy++)for(int xx=-1;xx<=5;xx++){bool in=glyphAt(type,xx,yy),ring=ringAt(type,xx,yy);if(!in&&!ring)continue;
    c.drawPixel(x+2+xx,y+1+yy,p>0?ring:in);}}
  for(unsigned i=0;i<n;i++)if(chess::moveFrom(list[i])==chessHeld&&chess::moveTo(list[i])==sq){c.fillRect(x+3,y+3,2,2,!p);if(p)c.drawRect(x,y,8,8,1);}
 }
 // Last move: corner ticks on both squares.
 if(m.game.plies){chess::Move last=m.game.moves[m.game.plies-1];for(int sq:{chess::moveFrom(last),chess::moveTo(last)}){int x,y;squareXY(m,sq,x,y);c.drawPixel(x,y,1);c.drawPixel(x+7,y,1);c.drawPixel(x,y+7,1);c.drawPixel(x+7,y+7,1);}}
 if(chessHeld>=0&&(millis()/300)%2){int x,y;squareXY(m,chessHeld,x,y);c.drawRect(x,y,8,8,1);c.drawRect(x+1,y+1,6,6,0);}
 if(cursor){int x,y;squareXY(m,chessCursor,x,y);c.drawRect(x,y,8,8,1);c.drawRect(x+1,y+1,6,6,1);}
 c.drawFastVLine(65,0,64,1);
 say(68,8,clipped(m.name,11),small);say(68,17,t("you: ","вы: ")+(m.mine==chess::White?t("white","белые"):t("black","чёрные")),small);
 textLinesAt(chessStateText(m),68,27,picking?1:2,11);
 if(m.game.plies){chess::Position before=m.game.at(m.game.plies-1);char san[16];before.san(m.game.moves[m.game.plies-1],san,sizeof san);
  say(68,46,String((m.game.plies+1)/2)+(m.game.plies%2?". ":"... ")+chessLocalSan(san),small);}
#if defined(MM_JOYSTICK)
 say(68,55,clipped(chessLink(m),11),small);
 say(68,63,t("hold OK: menu","держ.OK: меню"),u8g2_font_4x6_t_cyrillic);
#else
 if(picking){
  // The current choice, lit when it is "Game menu" or "Cancel"; the move sent last is acknowledged by the reply.
  if(chessCursor<0)c.fillRect(67,29,61,9,1);say(68,36,clipped(chessChoiceText(m),11),small,chessCursor>=0);
  say(68,55,t("click: next","клик: далее"),u8g2_font_4x6_t_cyrillic);
  say(68,63,chessCursor<0?(chessHeld<0?t("hold: menu","держ: меню"):t("hold: cancel","держ: отмена")):chessHeld<0?t("hold: pick","держ: взять"):t("hold: move","держ: сходить"),u8g2_font_4x6_t_cyrillic);}
 else{say(68,55,clipped(chessLink(m),11),small);say(68,63,t("hold: menu","держ: меню"),u8g2_font_4x6_t_cyrillic);}
#endif
 if(chessMenu){ChessAct acts[8];unsigned k=chessActions(m,acts);chessMenuIndex%=k;int first=max(0,min(chessMenuIndex-1,int(k)-3));
  c.fillRect(0,12,128,45,0);c.drawRect(0,12,128,45,1);
  for(unsigned i=first;i<k&&i<unsigned(first+3);i++){int y=14+(i-first)*14;bool focus=int(i)==chessMenuIndex;if(focus)c.fillRect(2,y,124,13,1);say(5,y+10,clipped(chessActName(acts[i]),20),body,focus?0:1);}}
 if(action.length()&&millis()-actionAt<3000){c.fillRect(0,22,128,20,0);c.drawRect(0,22,128,20,1);say(3,36,clipped(action,20));}
 hardware.flush();
}
void drawChessList(String& title){
 auto& c=*hardware.canvas;ChessMatch* games[ChessNet::MaxMatches];unsigned n=chessGames(games);title=t("Chess","Шахматы")+(n?" "+String(chessSel%n+1)+"/"+String(n):"");
 if(!n){say(0,28,t("No games yet","Партий пока нет"));
#if defined(MM_JOYSTICK)
  say(0,40,t("Nodes: OK - Chess invite","Узлы: OK - вызвать"),small);
#else
  say(0,40,t("Node menu: Chess invite","Меню узла: вызвать"),small);
#endif
  say(0,49,t("or accept a challenge","или примите вызов"),small);return;}
 chessSel%=n;int first=max(0,min(chessSel-1,int(n)-3));
 for(unsigned i=first;i<n&&i<unsigned(first+3);i++){const ChessMatch& m=*games[i];int y=13+(i-first)*15;bool focus=int(i)==chessSel;
  if(focus)c.fillRect(0,y,128,15,1);say(2,y+7,clipped(String(m.unseen?"* ":"")+m.name,14),small,!focus);sayRight(126,y+7,m.game.plies?String((m.game.plies+1)/2)+t(" mv"," ход"):String(""),small,!focus);
  say(2,y+14,clipped(chessStateText(m),24),small,!focus);}
}
#if defined(MM_JOYSTICK)
// Keys while the Chess page is shown; false lets the page switch handle them (list view only).
bool chessKey(int key){
 bool up=key==0xb5,down=key==0xb6,left=key==0xb4,right=key==0xb7,ok=key==13,hold=key==0xa3,back=key==0x86||key==0x82;
 if(!chessOpen){ChessMatch* games[ChessNet::MaxMatches];unsigned n=chessGames(games);
  if(up||down){if(n)chessSel=(chessSel+(up?n-1:1))%n;return true;}
  if((ok||hold)&&n){openChess(games[chessSel%n]);return true;}
  return false;}
 ChessMatch& m=*chessOpen;
 if(m.state==ChessMatch::Free){chessOpen=nullptr;return true;}
 if(chessMenu){ChessAct acts[8];unsigned k=chessActions(m,acts);
  if(up||down){chessMenuIndex=(chessMenuIndex+(up?k-1:1))%k;return true;}
  if(ok||hold){chessMenu=false;chessRun(m,acts[chessMenuIndex%k]);return true;}
  chessMenu=false;return true;}
 if(hold){chessMenu=true;chessMenuIndex=0;return true;}
 if(back){if(chessHeld>=0)chessHeld=-1;else chessOpen=nullptr;return true;}
 int col,row;{int x,y;squareXY(m,chessCursor,x,y);col=x/8;row=y/8;}
 if(up)row=(row+7)%8;if(down)row=(row+1)%8;if(left)col=(col+7)%8;if(right)col=(col+1)%8;
 if(up||down||left||right){chessCursor=squareAt(m,col,row);return true;}
 if(ok){
  if(m.state!=ChessMatch::Playing){chessMenu=true;chessMenuIndex=0;return true;} // accept, decline or delete
  if(!m.myTurn()){notice(t("Their move","Ход соперника"));return true;}
  int8_t p=m.game.pos.board[chessCursor];bool mine=p&&((p>0)==(m.mine==chess::White));
  if(chessHeld<0||mine){if(mine)chessHeld=chessCursor;else notice(t("Pick your piece","Выберите свою фигуру"));return true;}
  chess::Move mv;if(!targetOf(m,chessHeld,chessCursor,&mv)){notice(t("Not a legal move","Так ходить нельзя"));return true;}
  if(chessNet.move(m,mv)){chessHeld=-1;hardware.ping(1400,15);if(m.link()==ChatMessage::Failed)notice(t("Not sent: will resend","Не отправлено: повторю"));}
  else notice(t("Move not sent","Ход не отправлен"));
  return true;}
 return true;
}
#else
// The button while the Chess page is shown; false lets the page switch handle it (the next screen).
bool chessButton(int key){
 bool click=key==13||key==0x82,hold=key==0xa3;
 if(!chessOpen)return false; // the list: the page menu opens the game
 ChessMatch& m=*chessOpen;
 if(m.state==ChessMatch::Free){chessOpen=nullptr;return true;}
 if(chessMenu){ChessAct acts[8];unsigned k=chessActions(m,acts);
  if(click){chessMenuIndex=(chessMenuIndex+1)%k;return true;}
  if(hold){chessMenu=false;chessRun(m,acts[chessMenuIndex%k]);}return true;}
 if(!chessPicking(m)){if(hold){chessMenu=true;chessMenuIndex=0;return true;}return false;} // the board stays open
 chessSettle(m);int c[64];unsigned k=chessChoices(m,c);
 if(click){unsigned i=0;while(i<k&&c[i]!=chessCursor)i++;chessCursor=chessCursor<0?(k?c[0]:-1):i+1<k?c[i+1]:-1;return true;}
 if(!hold)return true;
 if(chessCursor<0){if(chessHeld>=0){chessCursor=chessHeld;chessHeld=-1;}else{chessMenu=true;chessMenuIndex=0;}return true;}
 if(chessHeld<0){chessHeld=chessCursor;chessCursor=chessChoices(m,c)?c[0]:-1;return true;}
 chess::Move mv;if(!targetOf(m,chessHeld,chessCursor,&mv))return true;
 if(chessNet.move(m,mv)){chessHeld=-1;hardware.ping(1400,15);if(m.link()==ChatMessage::Failed)notice(t("Not sent: will resend","Не отправлено: повторю"));}
 else notice(t("Move not sent","Ход не отправлен"));
 return true;
}
#endif
}
