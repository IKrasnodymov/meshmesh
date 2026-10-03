// Chess with contacts on the M9 screen. Included by Ui.cpp inside its anonymous namespace.
// Games, their network commands and saving live in ChessNet; this file draws the list of games, the
// choice of an opponent and the board. Arrows move the cursor, OK takes a piece and puts it.
namespace ch=chess;
ChessMatch* chessOpen=nullptr;          // the game on the board page
int chessCursor=12,chessHeld=-1,chessPromo=-1,chessPickColor=2;
bool chessHelp=false,chessResignArmed=false,chessRemoveArmed=false;
uint32_t chessEventsSeen=0;
unsigned chessPeers[24],chessPeerCount=0;
ChessMatch* chessRows[ChessNet::MaxMatches];unsigned chessRowCount=0;

const uint16_t boardLight=rgb(0xf0d9b5),boardDark=rgb(0xb58863),lastLight=rgb(0xcdd26a),lastDark=rgb(0xaaa23a),heldLight=rgb(0x9fbf6e),heldDark=rgb(0x7d9a4c);
const uint16_t moveDot=rgb(0x5c6b3a),checkFill=rgb(0xe06c5a),pieceWhite=rgb(0xf8f4ea),pieceWhiteInk=rgb(0x1c2026),pieceBlack=rgb(0x2b3038),pieceBlackInk=rgb(0xc5ccd4),pieceBlackEdge=rgb(0x0b0d10);
constexpr int BoardX=4,BoardY=25,Cell=24;
// Pieces, 18x16: '#' body, '+' a line inside the body. The outline is added around the shape.
const char* const pieceArt[6][16]={
 {"..................","..................","..................","........##........",".......####.......","......######......","......######......",".......####.......",
  "......######......",".......####.......",".......####.......","......######......",".....########.....","....##########....","...############...","...############..."},
 {"........#.#.......",".......#####......","......########....",".....##+#######...","....############..","...#############..","..###############.",".#######..#######.",
  ".#####....#######.","..###....#######..","........#######...",".......#######....","......########....","....#++++++++#....","...############...","..##############.."},
 {"........##........","........##........",".......####.......","......###+##......",".....###+####.....",".....##+#####.....",".....########.....","......######......",
  ".......####.......","......#++++#......",".....########.....","......######......",".....########.....","....##########....","..##############..","..##############.."},
 {"..................","..##...####...##..","..##...####...##..","..##############..","..##############..","...#++++++++++#...","....##########....","....##########....",
  "....##########....","....##########....","....##########....","...#++++++++++#...","..##############..","..##############..",".################.",".################."},
 {".#......##......#.",".##....####....##.",".###..######..###.",".####.######.####.",".################.","..##############..","...############...","....##########....",
  ".....########.....",".....########.....","....#++++++++#....","....##########....",".....########.....","...############...","..##############..","..##############.."},
 {"........##........","......######......","........##........","....###.##.###....","..######++######..",".#######++#######.",".################.",".################.",
  "..##############..","...############...","....#++++++++#....","....##########....",".....########.....","...############...","..##############..","..##############.."}};
struct Sprite{uint32_t body[18],line[18],edge[18];} sprites[6];bool spritesReady=false;
// Rows and columns are shifted by one so the outline has room on every side.
void buildSprites(){
 for(int p=0;p<6;p++){
  Sprite& s=sprites[p];memset(&s,0,sizeof s);uint32_t all[18]={};
  for(int r=0;r<16;r++)for(int c=0;c<18;c++){char v=pieceArt[p][r][c];if(v=='#')s.body[r+1]|=1u<<(c+1);if(v=='+')s.line[r+1]|=1u<<(c+1);}
  for(int r=0;r<18;r++)all[r]=s.body[r]|s.line[r];
  for(int r=0;r<18;r++){uint32_t near=0;for(int d=-1;d<=1;d++){int k=r+d;if(k<0||k>17)continue;near|=all[k]|all[k]<<1|all[k]>>1;}s.edge[r]=near&~all[r];}
 }
 spritesReady=true;
}
void drawPiece(int8_t piece,int x,int y,int scale=1){
 if(!piece)return;if(!spritesReady)buildSprites();const Sprite& s=sprites[abs(piece)-1];bool white=piece>0;
 uint16_t fill=white?pieceWhite:pieceBlack,ink=white?pieceWhiteInk:pieceBlackInk,edge=white?pieceWhiteInk:pieceBlackEdge;
 for(int r=0;r<18;r++)for(int c=0;c<20;c++){uint32_t bit=1u<<c;uint16_t color;if(s.body[r]&bit)color=fill;else if(s.line[r]&bit)color=ink;else if(s.edge[r]&bit)color=edge;else continue;
  if(scale==1)g().drawPixel(x+c,y+r,color);else g().fillRect(x+c*scale,y+r*scale,scale,scale,color);}
}
String localSan(const char* san){return chessLocalSan(san);}
String lastSan(const ChessMatch& m){if(!m.game.plies)return "";char s[12];m.game.at(m.game.plies-1).san(m.game.moves[m.game.plies-1],s,sizeof s);return localSan(s);}
String moveCount(const ChessMatch& m){return count((m.game.plies+1)/2,"move","moves","ход","хода","ходов");}
String colorName(uint8_t c,bool of=false){return c==ch::White?(of?t("White","белыми"):t("White","белые")):(of?t("Black","чёрными"):t("Black","чёрные"));}
// One line about the state of a game, for the list and for the board.
String chessState(const ChessMatch& m,bool board=false){
 switch(m.state){
 case ChessMatch::Inviting:return t("Challenge sent","Вызов отправлен");
 case ChessMatch::Invited:return t("Challenges you: you play ","Вызывает вас: вы играете ")+colorName(m.mine,true);
 case ChessMatch::Playing:if(m.drawOffer==ChessMatch::OfferedToMe&&!board)return t("Offers a draw","Предлагает ничью"); // the board shows it on its own strip
  return m.myTurn()?(m.game.pos.inCheck()?t("Your move: check!","Ваш ход: шах!"):t("Your move","Ваш ход")):t("Their move","Ход соперника");
 case ChessMatch::Over:{
  if(m.reason==ChessMatch::Declined)return t("Challenge declined","Вызов отклонён");
  if(m.reason==ChessMatch::Cancelled)return t("Game cancelled","Партия отменена");
  String r=m.won()?t("You won","Победа"):m.lost()?t("You lost","Поражение"):t("Draw","Ничья");
  switch(m.reason){case ChessMatch::Mate:return r+t(": checkmate",": мат");case ChessMatch::Resigned:return r+(m.won()?t(": they resigned",": соперник сдался"):t(": you resigned",": вы сдались"));
   case ChessMatch::Stalemate:return r+t(": stalemate",": пат");case ChessMatch::Repetition:return r+t(": repetition",": повторение");case ChessMatch::FiftyMoves:return r+t(": 50 moves",": 50 ходов");
   case ChessMatch::DeadPosition:return r+t(": no mating material",": мало фигур");case ChessMatch::TooLong:return r+t(": move limit",": предел ходов");case ChessMatch::Agreed:return r+t(" by agreement"," по соглашению");default:return r;}
 }
 default:return "";
 }
}
uint16_t chessStateColor(const ChessMatch& m){if(m.state==ChessMatch::Invited||m.myTurn()||m.drawOffer==ChessMatch::OfferedToMe)return accent;if(m.state==ChessMatch::Over)return m.won()?ok:m.lost()?bad:dim;return dim;}
String deliveryText(const ChessMatch& m){
 switch(m.link()){case ChatMessage::Queued:return t("sending","отправляется");case ChatMessage::Sent:return t("sent, no ACK yet","отправлено, ждём ACK");case ChatMessage::Delivered:return t("delivered","доставлено");
 case ChatMessage::Failed:{if(m.autoStopped)return t("not delivered (R)","не доставлено (R)");int32_t s=chessNet.retryIn(m);
  return s<0?t("not delivered, resending","не доставлено, повторяю"):s<60?t("not delivered, retry now","не доставлено, повтор сейчас"):t("not delivered, retry in ","не доставлено, повтор через ")+String((s+59)/60)+t(" min"," мин");}}
 return "";
}
String chessTileDetail(){unsigned w=chessNet.waiting(),n=chessNet.count();if(w)return count(w,"game waits","games wait","ждёт хода","ждут хода","ждут хода");if(n)return count(n,"game","games","партия","партии","партий");return t("With contacts","С контактами");}
bool chessPage(){return page==ChessList||page==ChessPick||page==ChessBoard;}

// The list: games that need the player first, then the newest.
void chessSortRows(){
 chessRowCount=0;for(auto& m:chessNet.matches)if(m.state!=ChessMatch::Free)chessRows[chessRowCount++]=&m;
 auto rank=[](const ChessMatch* m){return m->state==ChessMatch::Invited||m->myTurn()||m->unseen?0:m->active()?1:2;};
 for(unsigned i=1;i<chessRowCount;i++)for(unsigned j=i;j>0;j--){ChessMatch* a=chessRows[j];ChessMatch* b=chessRows[j-1];if(rank(a)<rank(b)||(rank(a)==rank(b)&&int32_t(a->changedAt-b->changedAt)>0))std::swap(chessRows[j],chessRows[j-1]);else break;}
}
void chessSortPeers(){
 chessPeerCount=0;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].type==1)chessPeers[chessPeerCount++]=i;
 uint32_t now=millis();auto before=[&](const Peer& a,const Peer& b){if(a.heard!=b.heard)return a.heard;return a.heard&&now-a.seen<now-b.seen;};
 for(unsigned i=1;i<chessPeerCount;i++)for(unsigned j=i;j>0&&before(meshRadio.peers[chessPeers[j]],meshRadio.peers[chessPeers[j-1]]);j--)std::swap(chessPeers[j],chessPeers[j-1]);
}
void colorDisc(int cx,int cy,uint8_t c){g().fillCircle(cx,cy,5,c==ch::White?pieceWhite:pieceBlack);g().drawCircle(cx,cy,5,c==ch::White?faint:dim);}
void drawChessList(){
 chessSortRows();int total=1+chessRowCount;if(selected>=total)selected=total-1;int first=max(0,selected-3);
 for(int i=first;i<total&&i<first+4;i++){
  int y=24+(i-first)*48;bool focus=selected==i;listRow(y,45,focus);target(8,y,304,45,i);
  if(i==0){g().fillCircle(32,y+22,15,rgb(0x14524b));g().fillRect(31,y+14,3,17,accent);g().fillRect(24,y+21,17,3,accent);text(56,y+19,t("New game","Новая партия"),ink,bold);text(56,y+35,fit(chessNet.count()>=ChessNet::MaxMatches?t("All boards busy: finish a game","Все доски заняты: завершите партию"):t("Challenge a chat contact","Вызвать чат-контакт"),240,small),dim,small);continue;}
  const ChessMatch& m=*chessRows[i-1];Peer* p=peerOf(m.peer);avatar(32,y+22,15,m.peer,m.name,p?p->type:-1);colorDisc(44,y+34,m.mine);
  String right=m.game.plies?moveCount(m):String();if(m.updated)right=timeText(m.updated);int rw=measure(right,small);
  text(56,y+19,fit(m.name,232-rw,bold),ink,bold);textRight(304,y+18,right,faint,small);
  String detail=chessState(m);if(m.game.plies&&m.state!=ChessMatch::Invited)detail+=" · "+lastSan(m);if(m.state==ChessMatch::Playing&&m.link()==ChatMessage::Failed)detail=deliveryText(m);
  text(56,y+35,fit(detail,236,small),m.link()==ChatMessage::Failed&&m.active()?bad:chessStateColor(m),small);
  if(m.unseen||m.state==ChessMatch::Invited||m.myTurn())g().fillCircle(300,y+32,4,accent);
 }
 scrollbar(first,4,total,24,190);
 if(!chessRowCount){String rows[3];unsigned n=wrap(t("Moves travel as direct MeshCore messages with delivery confirmation. Both players need a MeshMesh M9 with chess.","Ходы идут личными сообщениями MeshCore с подтверждением доставки. Сопернику нужен MeshMesh M9 с шахматами."),rows,3,300);for(unsigned k=0;k<n;k++)textCenter(160,100+k*16,rows[k],faint);}
 bool over=selected>0&&chessRows[selected-1]->state==ChessMatch::Over;
 if(over)footer({{"OK",t("Open","Открыть")},{"DEL",chessRemoveArmed?t("Sure?","Удалить?"):t("Remove","Удалить")},{"BACK",t("Menu","Меню")}});
 else footer({{"OK",selected?t("Open","Открыть"):t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"BACK",t("Menu","Меню")}});
}
void drawChessPick(){
 chessSortPeers();panel(8,24,304,30,card,7);text(18,43,t("You play","Вы играете"),dim);
 String colors[]={t("White","Белыми"),t("Black","Чёрными"),t("Random","Случайно")};int cx=222;tri(cx-58,39,3,4,accent);tri(cx+58,39,1,4,accent);
 if(chessPickColor<2)colorDisc(cx-42,39,chessPickColor);textCenter(cx+(chessPickColor<2?6:0),44,colors[chessPickColor],ink,bold);
 target(8,24,304,30,-KeyRight); // the colour
 if(!chessPeerCount){textCenter(160,100,t("No chat contacts yet","Чат-контактов пока нет"),ink,bold);textCenter(160,122,t("Press ADV to announce this node and wait","Нажмите ADV, чтобы объявить узел, и"),dim);textCenter(160,138,t("for the other player's announcement","дождитесь объявления соперника"),dim);footer({{"ADV",t("Announce","Объявить")},{"BACK",t("Games","Партии")}});return;}
 if(selected>=int(chessPeerCount))selected=chessPeerCount-1;int first=max(0,selected-2);
 for(int i=first;i<int(chessPeerCount)&&i<first+3;i++){
  const Peer& p=meshRadio.peers[chessPeers[i]];int y=60+(i-first)*48;bool focus=selected==i;listRow(y,45,focus);target(8,y,304,45,i);avatar(32,y+22,15,p.id,p.name,p.type);
  text(56,y+19,fit(p.name,240,bold),ink,bold);
  text(56,y+35,fit((p.heard?(millis()-p.seen<60000?ago(0):ago(millis()-p.seen)+t(" ago"," назад"))+" · ":String())+pathText(p),240,small),p.heard&&millis()-p.seen<1800000?ok:dim,small);
 }
 scrollbar(first,3,chessPeerCount,60,142);
 footer({{"OK",t("Challenge","Вызвать")},{"<>",t("Colour","Цвет")},{"^v",t("Contact","Контакт")},{"BACK",t("Back","Назад")}});
}

// Board: from the player's side; a1 is bottom left for White and top right for Black.
bool flipped(){return chessOpen&&chessOpen->mine==ch::Black;}
int squareX(int s){return BoardX+Cell*(flipped()?7-ch::fileOf(s):ch::fileOf(s));}
int squareY(int s){return BoardY+Cell*(flipped()?ch::rankOf(s):7-ch::rankOf(s));}
bool heldTarget(int s,ch::Move* found=nullptr){
 if(chessHeld<0||!chessOpen)return false;ch::Move list[ch::MaxMoves];unsigned n=chessOpen->game.pos.legal(list);
 for(unsigned i=0;i<n;i++)if(ch::moveFrom(list[i])==chessHeld&&ch::moveTo(list[i])==s){if(found)*found=list[i];return true;}return false;
}
bool canTake(int s){if(!chessOpen||!chessOpen->myTurn())return false;int8_t p=chessOpen->game.pos.board[s];if(!p||(p>0)!=(chessOpen->mine==ch::White))return false;ch::Move list[ch::MaxMoves];unsigned n=chessOpen->game.pos.legal(list);for(unsigned i=0;i<n;i++)if(ch::moveFrom(list[i])==s)return true;return false;}
void drawBoard(){
 auto& d=g();const ChessMatch& m=*chessOpen;const ch::Position& pos=m.game.pos;
 int lastFrom=-1,lastTo=-1;if(m.game.plies){ch::Move last=m.game.moves[m.game.plies-1];lastFrom=ch::moveFrom(last);lastTo=ch::moveTo(last);}
 int checked=pos.inCheck()?pos.king(pos.side):-1;
 d.fillRect(BoardX-2,BoardY-2,Cell*8+4,Cell*8+4,rgb(0x3a2a1c));
 for(int s=0;s<64;s++){
  int x=squareX(s),y=squareY(s);bool light=(ch::fileOf(s)+ch::rankOf(s))&1;
  uint16_t fill=s==chessHeld?(light?heldLight:heldDark):(s==lastFrom||s==lastTo)?(light?lastLight:lastDark):(light?boardLight:boardDark);
  d.fillRect(x,y,Cell,Cell,fill);
  if(s==checked)d.fillCircle(x+Cell/2,y+Cell/2,Cell/2-1,checkFill);
  // Coordinates in the corners of the edge squares, in the other square colour.
  uint16_t label=light?boardDark:boardLight;
  if(squareY(s)==BoardY+Cell*7)text(x+Cell-6,y+Cell-2,String(char('a'+ch::fileOf(s))),label,small);
  if(squareX(s)==BoardX)text(x+1,y+7,String(char('1'+ch::rankOf(s))),label,small);
  drawPiece(pos.board[s],x+2,y+4);
  if(chessHeld>=0&&heldTarget(s)){if(pos.board[s]||(abs(pos.board[chessHeld])==ch::Pawn&&ch::fileOf(s)!=ch::fileOf(chessHeld))){for(int k=0;k<4;k++){int cx=x+(k&1?Cell-1:0),cy=y+(k&2?Cell-1:0);d.fillTriangle(cx,cy,cx+(k&1?-6:6),cy,cx,cy+(k&2?-6:6),moveDot);}}else d.fillCircle(x+Cell/2,y+Cell/2,3,moveDot);}
 }
 if(chessPromo<0){int x=squareX(chessCursor),y=squareY(chessCursor);uint16_t c=chessHeld>=0&&chessCursor!=chessHeld&&!heldTarget(chessCursor)?warn:accent;d.drawRect(x,y,Cell,Cell,c);d.drawRect(x+1,y+1,Cell-2,Cell-2,c);}
}
struct SanRows{const ChessMatch* m=nullptr;uint16_t plies=0xffff,first=0;char san[16][12];} sanRows;
void updateSanRows(const ChessMatch& m){
 if(sanRows.m==&m&&sanRows.plies==m.game.plies)return;sanRows.m=&m;sanRows.plies=m.game.plies;sanRows.first=m.game.plies>16?(m.game.plies-15)&~1:0;
 ch::Position p;p.start();for(unsigned i=0;i<m.game.plies;i++){if(i>=sanRows.first)p.san(m.game.moves[i],sanRows.san[i-sanRows.first],12);p.apply(m.game.moves[i]);}
}
void drawChessSide(){
 const ChessMatch& m=*chessOpen;int x=204,w=110;
 // Opponent on top, the player at the bottom, as on the board.
 panel(x,24,w,24,card,6);colorDisc(x+10,36,m.mine^1);text(x+20,40,fit(m.name,w-24,bold),ink,bold);
 updateSanRows(m);int rows=(m.game.plies-sanRows.first+1)/2;
 for(int r=0;r<rows&&r<8;r++){
  int ply=sanRows.first+r*2,y=64+r*14;String no=String(ply/2+1)+".";text(x+2,y,no,faint,small);
  for(int k=0;k<2&&ply+k<m.game.plies;k++){bool latest=ply+k==m.game.plies-1;String s=localSan(sanRows.san[ply+k-sanRows.first]);text(x+22+k*44,y,fit(s,44),latest?accent:ink);}
 }
 if(!m.game.plies)text(x+2,64,fit(m.state==ChessMatch::Inviting?t("Waiting for reply","Ждём ответа"):m.state==ChessMatch::Invited?t("OK accepts","OK — принять"):t("No moves yet","Ходов ещё нет"),w,small),faint,small);
 int diff=m.game.material(m.mine)-m.game.material(m.mine^1);
 panel(x,176,w,42,card,6);colorDisc(x+10,188,m.mine);text(x+20,192,t("You","Вы"),ink,bold);if(diff)textRight(x+w-6,191,(diff>0?"+":"")+String(diff),diff>0?ok:bad,small);
 String state=chessState(m,true),rowsText[2];unsigned n=wrap(state,rowsText,2,w-10,small);
 // Delivery of the last command matters while waiting, or when it failed.
 String deliver=(m.myTurn()||m.state==ChessMatch::Over)&&m.link()!=ChatMessage::Failed?String():deliveryText(m);
 if(m.drawOffer==ChessMatch::OfferedByMe&&m.state==ChessMatch::Playing)deliver=t("You offered a draw","Вы предложили ничью");
 if(n==1&&deliver.length()){text(x+6,204,rowsText[0],chessStateColor(m),small);text(x+6,214,fit(deliver,w-10,small),m.link()==ChatMessage::Failed?bad:faint,small);}
 else for(unsigned i=0;i<n;i++)text(x+6,204+i*10,rowsText[i],chessStateColor(m),small);
 if(m.drawOffer==ChessMatch::OfferedToMe&&m.state==ChessMatch::Playing){panel(x,150,w,22,rgb(0x14524b),6);textCenter(x+w/2,164,t("Draw? D accepts","Ничья? D — да"),ink,small);}
}
void drawPromotion(){
 auto& d=g();int x=BoardX+Cell*2-10,y=BoardY+Cell*3-16;panel(x,y,Cell*4+20,72,cardHi,8);d.drawRoundRect(x,y,Cell*4+20,72,8,line);
 textCenter(x+(Cell*4+20)/2,y+14,t("Promote to","Превратить в"),ink,small);
 const int8_t kinds[]={ch::Queen,ch::Rook,ch::Bishop,ch::Knight};int8_t sign=chessOpen->mine==ch::White?1:-1;
 for(int i=0;i<4;i++){int bx=x+6+i*(Cell+2),by=y+22;d.fillRect(bx,by,Cell,Cell+4,i==chessPromo?heldLight:boardLight);if(i==chessPromo){d.drawRect(bx,by,Cell,Cell+4,accent);d.drawRect(bx+1,by+1,Cell-2,Cell+2,accent);}drawPiece(kinds[i]*sign,bx+2,by+6);}
 textCenter(x+(Cell*4+20)/2,y+66,t("<> choose  OK put","<> выбор  OK поставить"),dim,small);
}
void drawChessHelp(){
 uint16_t* px=g().getBuffer();for(int i=0;i<320*240;i++)px[i]=(px[i]>>1)&0x7bef;
 panel(6,24,308,194,cardHi,10);g().drawRoundRect(6,24,308,194,10,line);text(16,42,t("Chess keys","Клавиши шахмат"),ink,bold);
 Hint keys[]={{"<>",t("Move the cursor (also ^v)","Курсор по доске (и ^v)")},{"OK",t("Take a piece / put it","Взять фигуру / поставить")},{"BACK",t("Put back / to the games","Вернуть фигуру / к партиям")},
  {"D",t("Offer a draw / accept one","Предложить / принять ничью")},{"X",t("Resign (press twice)","Сдаться (дважды)")},{"R",t("Resend what was not delivered","Повторить недоставленное")},
  {"N",t("Rematch after the game","Реванш после партии")},{"DEL",t("Decline, cancel, remove","Отказ, отмена, удаление")}};
 for(int i=0;i<8;i++){int y=52+i*17;g().fillRoundRect(16,y,30,13,3,line);if(!strcmp(keys[i].key,"<>")){tri(26,y+6,3,3,ink);tri(35,y+6,1,3,ink);}else textCenter(31,y+10,keys[i].key,ink,small);text(54,y+11,keys[i].action,i%2?dim:ink);}
 text(16,202,fit(t("Moves are sent as direct messages with ACK.","Ходы идут личными сообщениями с ACK."),290,small),faint,small);
}
void drawChessBoard(){
 if(!chessOpen||chessOpen->state==ChessMatch::Free){textCenter(160,110,t("The game was removed","Партия удалена"),dim);footer({{"BACK",t("Games","Партии")}});return;}
 const ChessMatch& m=*chessOpen;drawBoard();drawChessSide();if(chessPromo>=0)drawPromotion();
 bool failed=m.link()==ChatMessage::Failed;
 if(chessPromo>=0)footer({{"<>",t("Piece","Фигура")},{"OK",t("Put","Поставить")},{"BACK",t("Cancel","Отмена")}});
 else if(m.state==ChessMatch::Invited)footer({{"OK",t("Accept","Принять")},{"DEL",t("Decline","Отказать")},{"BACK",t("Games","Партии")}});
 else if(m.state==ChessMatch::Inviting)footer({failed?Hint{"R",t("Resend","Повторить")}:Hint{"DEL",t("Cancel","Отменить")},{"H",t("Keys","Клавиши")},{"BACK",t("Games","Партии")}});
 else if(m.state==ChessMatch::Over)footer({failed?Hint{"R",t("Resend","Повторить")}:Hint{"N",t("Rematch","Реванш")},{"DEL",chessRemoveArmed?t("Sure?","Удалить?"):t("Remove","Удалить")},{"BACK",t("Games","Партии")}});
 else if(failed)footer({{"R",t("Resend","Повторить")},{"X",chessResignArmed?t("Sure?","Сдаться?"):t("Resign","Сдаться")},{"BACK",t("Games","Партии")}});
 else if(chessHeld>=0)footer({{"OK",t("Put","Поставить")},{"BACK",t("Put back","Вернуть")},{"H",t("Keys","Клавиши")}});
 else if(m.myTurn())footer({{"OK",t("Take","Взять")},{"D",m.drawOffer==ChessMatch::OfferedToMe?t("Draw: yes","Ничья: да"):t("Draw","Ничья")},{"H",t("Keys","Клавиши")}});
 else footer({{"D",t("Draw","Ничья")},{"X",chessResignArmed?t("Sure?","Сдаться?"):t("Resign","Сдаться")},{"H",t("Keys","Клавиши")}});
 if(chessHelp)drawChessHelp();
}

void chessEnter(ChessMatch* m){
 chessOpen=m;chessHeld=-1;chessPromo=-1;chessHelp=false;chessResignArmed=false;chessRemoveArmed=false;
 // The cursor starts on the last move, or on the king's pawn.
 chessCursor=m->game.plies?ch::moveTo(m->game.moves[m->game.plies-1]):m->mine==ch::White?12:52;
 chessNet.viewed(*m);change(ChessBoard);
}
// Back on the list, the game just left stays selected.
void chessBackToList(){ChessMatch* was=chessOpen;change(ChessList);chessSortRows();for(unsigned i=0;i<chessRowCount;i++)if(chessRows[i]==was)selected=i+1;}
void chessCursorMove(int key){
 int f=ch::fileOf(chessCursor),r=ch::rankOf(chessCursor),dir=flipped()?-1:1;
 if(key==KeyLeft)f=(f-dir+8)%8;if(key==KeyRight)f=(f+dir+8)%8;if(key==KeyUp)r=(r+dir+8)%8;if(key==KeyDown)r=(r-dir+8)%8;chessCursor=r*8+f;
}
void chessPlay(ch::Move mv){
 ChessMatch& m=*chessOpen;
 if(chessNet.move(m,mv)){chessHeld=-1;chessPromo=-1;hardware.ping(1400,12);if(m.state==ChessMatch::Over)hardware.ping(m.won()?1760:600,200);if(m.link()==ChatMessage::Failed)notice(t("Not sent now: will resend automatically","Сейчас не отправлено: повторю автоматически"),warn);}
 else{hardware.ping(260,60);notice(t("Move refused","Ход не принят"),bad);}
}
void chessAct(){
 ChessMatch& m=*chessOpen;
 if(m.state==ChessMatch::Invited){if(chessNet.accept(m))notice(t("Accepted: ","Принято: ")+t("you play ","вы играете ")+colorName(m.mine,true),ok);return;}
 if(!m.myTurn()){notice(m.state==ChessMatch::Playing?t("Wait for their move","Дождитесь хода соперника"):m.state==ChessMatch::Inviting?t("Waiting for the reply","Ждём ответа на вызов"):t("The game is over","Партия окончена"),dim);return;}
 if(chessHeld<0||chessCursor==chessHeld){if(chessCursor==chessHeld){chessHeld=-1;return;}if(canTake(chessCursor))chessHeld=chessCursor;else notice(m.game.pos.board[chessCursor]?t("This piece cannot move","Эта фигура не может ходить"):t("Choose one of your pieces","Выберите свою фигуру"),dim);return;}
 ch::Move mv;
 if(!heldTarget(chessCursor,&mv)){if(canTake(chessCursor)){chessHeld=chessCursor;return;}hardware.ping(260,60);notice(m.game.pos.inCheck()?t("Not allowed: the king is in check","Нельзя: королю шах"):t("Cannot go there","Сюда нельзя"),warn);return;}
 if(ch::movePromotion(mv)){chessPromo=0;return;}
 chessPlay(mv);
}
bool chessKey(int key){
 if(key>='A'&&key<='Z')key+=32;
 if(page==ChessList){
  if(key!=Erase)chessRemoveArmed=false;
  if(key==KeyUp||key==KeyDown){chessSortRows();int total=1+chessRowCount;selected=(selected+(key==KeyUp?-1:1)+total)%total;return true;}
  if(key==Enter||key==KeyHold){chessSortRows();if(!selected){if(chessNet.count()>=ChessNet::MaxMatches&&!chessRowCount)return true;change(ChessPick);}else chessEnter(chessRows[selected-1]);return true;}
  if(key==Erase&&selected){chessSortRows();ChessMatch& m=*chessRows[selected-1];if(m.state!=ChessMatch::Over){notice(t("Only finished games can be removed","Удалить можно только оконченную партию"),dim);return true;}
   if(!chessRemoveArmed){chessRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит партию"),warn);return true;}chessRemoveArmed=false;chessNet.remove(m);notice(t("Game removed","Партия удалена"),ok);return true;}
  if(key==KeyBack){change(Home);return true;}
  return false;
 }
 if(page==ChessPick){
  if(key==KeyLeft||key==KeyRight){chessPickColor=(chessPickColor+(key==KeyLeft?2:1))%3;return true;}
  if(key==KeyUp||key==KeyDown){chessSortPeers();if(chessPeerCount)selected=(selected+(key==KeyUp?-1:1)+chessPeerCount)%chessPeerCount;return true;}
  if(key==Enter||key==KeyHold){chessSortPeers();if(!chessPeerCount)return true;ChessMatch* m=chessNet.invite(meshRadio.peers[chessPeers[selected]].id,chessPickColor);if(!m){notice(chessNet.event,bad);return true;}
   chessNet.events=chessEventsSeen;chessEnter(m);notice(m->link()==ChatMessage::Failed?t("Not sent now: will resend","Сейчас не отправлено: повторю"):t("Challenge sent: you play ","Вызов отправлен: вы играете ")+colorName(m->mine,true),m->link()==ChatMessage::Failed?bad:ok);return true;}
  if(key==KeyBack){change(ChessList);return true;}
  return false;
 }
 // Board.
 if(chessHelp){chessHelp=false;return true;}
 if(!chessOpen||chessOpen->state==ChessMatch::Free){if(key==KeyBack){change(ChessList);return true;}return false;}
 ChessMatch& m=*chessOpen;
 if(key!='x')chessResignArmed=false;if(key!=Erase)chessRemoveArmed=false;
 if(chessPromo>=0){
  if(key==KeyLeft||key==KeyUp)chessPromo=(chessPromo+3)%4;else if(key==KeyRight||key==KeyDown)chessPromo=(chessPromo+1)%4;
  else if(key==Enter||key==KeyHold){const int kinds[]={ch::Queen,ch::Rook,ch::Bishop,ch::Knight};chessPlay(ch::makeMove(chessHeld,chessCursor,kinds[chessPromo]));}
  else if(key==KeyBack)chessPromo=-1;
  return true;
 }
 if(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown){chessCursorMove(key);return true;}
 if(key==Enter||key==KeyHold||key==' '){chessAct();return true;}
 if(key==KeyBack){if(chessHeld>=0)chessHeld=-1;else chessBackToList();return true;}
 if(key=='h'){chessHelp=true;return true;}
 if(key=='r'){if(chessNet.resend(m))notice(t("Sent again","Отправлено повторно"),ok);else notice(m.sending()?t("Already sending","Уже отправляется"):m.link()==ChatMessage::Failed?t("Radio busy, will resend","Радио занято, повторю автоматически"):t("Nothing to resend","Повторять нечего"),dim);return true;}
 if(key=='d'){
  if(m.drawOffer==ChessMatch::OfferedToMe){if(chessNet.acceptDraw(m))notice(t("Draw accepted","Ничья принята"),ok);return true;}
  if(chessNet.offerDraw(m))notice(t("Draw offered","Ничья предложена"),info);else notice(m.drawOffer==ChessMatch::OfferedByMe?t("Draw already offered","Ничья уже предложена"):t("No game in progress","Партия не идёт"),dim);return true;
 }
 if(key=='x'){
  if(m.state!=ChessMatch::Playing){notice(t("No game in progress","Партия не идёт"),dim);return true;}
  if(!chessResignArmed){chessResignArmed=true;notice(t("X again resigns the game","X ещё раз — сдаться"),warn);return true;}
  chessResignArmed=false;chessHeld=-1;chessNet.resign(m);hardware.ping(600,200);notice(t("You resigned","Вы сдались"),dim);return true;
 }
 if(key=='n'){
  if(m.state!=ChessMatch::Over){notice(t("Finish this game first","Сначала завершите партию"),dim);return true;}
  ChessMatch* next=chessNet.invite(m.peer,m.mine^1);if(!next){notice(chessNet.event,bad);return true;}chessNet.events=chessEventsSeen;chessEnter(next);notice(t("Rematch sent: you play ","Реванш: вы играете ")+colorName(next->mine,true),ok);return true;
 }
 if(key==Erase){
  if(m.state==ChessMatch::Invited||m.state==ChessMatch::Inviting||(m.state==ChessMatch::Playing&&!m.game.plies)){bool mineInvite=m.state!=ChessMatch::Invited;if(chessNet.decline(m))notice(mineInvite?t("Challenge cancelled","Вызов отменён"):t("Challenge declined","Вызов отклонён"),dim);return true;}
  if(m.state==ChessMatch::Over){if(!chessRemoveArmed){chessRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит партию"),warn);return true;}chessRemoveArmed=false;chessNet.remove(m);chessOpen=nullptr;change(ChessList);notice(t("Game removed","Партия удалена"),ok);return true;}
  notice(t("Resign with X","Сдаться — клавиша X"),dim);return true;
 }
 return false;
}
void chessTick(){
 if(chessNet.events!=chessEventsSeen){
  chessEventsSeen=chessNet.events;
  // On the open game the news is on the board; elsewhere it is a toast.
  bool here=page==ChessBoard&&chessOpen==chessNet.eventMatch;if(here&&chessOpen)chessNet.viewed(*chessOpen);
  if(!locked&&chessNet.event.length())notice(chessNet.event,here?info:accent);
  if(here&&chessHeld>=0&&!canTake(chessHeld))chessHeld=-1;
 }
 if(chessNet.dirty){chessNet.dirty=false;dirty=true;}
}
void chessStatus(JsonDocument& d){
 if(page==ChessPick){d["chess_color"]=chessPickColor;return;}
 if(page!=ChessBoard||!chessOpen)return;char id[5];snprintf(id,sizeof id,"%04X",chessOpen->id);d["chess_game"]=id;
 char sq[3]={char('a'+ch::fileOf(chessCursor)),char('1'+ch::rankOf(chessCursor)),0};d["chess_cursor"]=sq;
 if(chessHeld>=0){char h[3]={char('a'+ch::fileOf(chessHeld)),char('1'+ch::rankOf(chessHeld)),0};d["chess_held"]=h;}
 d["chess_promotion"]=chessPromo;d["chess_help"]=chessHelp;
}
