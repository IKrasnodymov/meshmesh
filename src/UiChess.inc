// Chess with contacts on the M9 screen. Included by Ui.cpp inside its anonymous namespace.
// Games, their network commands and saving live in ChessNet; this file draws the list of games, the
// choice of an opponent and the board. Arrows move the cursor, OK takes a piece and puts it.
// The list has two tabs (<>): the games and the rating (ChessRating.h).
namespace ch=chess;
ChessMatch* chessOpen=nullptr;          // the game on the board page
int chessCursor=12,chessHeld=-1,chessPromo=-1,chessPickColor=2;
bool chessHelp=false,chessResignArmed=false,chessRemoveArmed=false;
uint32_t chessEventsSeen=0;
unsigned chessPeers[24],chessPeerCount=0;
ChessMatch* chessRows[ChessNet::MaxMatches];unsigned chessRowCount=0;
int chessTab=0,chessRatingSel=0;bool chessPickRated=true;

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
// The rating line of a rated game: waiting for the signatures, the change, or why it does not count.
String chessRatingText(const ChessMatch& m){
 if(!m.rated)return t("Friendly game","Товарищеская");
 if(m.state!=ChessMatch::Over)return t("Rated game","На рейтинг");
 switch(m.sign){
 case ChessMatch::SignStored:{const rating::Delta* d=rating::book.delta(m.peer,m.id);if(!d)return t("Rated","В рейтинге");int diff=d->after-d->before;
  return d->counted?"ELO "+String(d->after)+" ("+(diff>=0?"+":"")+String(diff)+")":t("Over the daily limit","Сверх дневного предела");}
 case ChessMatch::SignBad:return t("Not rated: signatures","Без рейтинга: подписи");
 case ChessMatch::SignNone:return t("Not rated","Без рейтинга");
 default:return m.theirSigned?t("Rating: checking","Рейтинг: проверка"):t("Rating: their signature","Рейтинг: ждём подпись");
 }
}
String chessTileDetail(){unsigned w=chessNet.waiting(),n=chessNet.count();if(w)return count(w,"game waits","games wait","ждёт хода","ждут хода","ждут хода");if(n)return count(n,"game","games","партия","партии","партий");return t("With contacts","С контактами");}
bool chessPage(){return page==ChessList||page==ChessPick||page==ChessBoard||page==ChessTour||page==ChessTourNew;}

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
// Three tabs: games, tournaments, rating (<> or a tap; 1-3 on the keyboard).
void drawChessTabs(){
 String names[]={t("Games","Партии"),t("Tournaments","Турниры"),t("Rating","Рейтинг")};unsigned badges[]={chessNet.waiting(),tour::net.waiting(),0};
 for(int i=0;i<3;i++){int x=8+i*101,w=98;bool on=i==chessTab;if(on)panel(x,23,w,20,cardHi,6);int tw=measure(names[i],on?bold:body);textCenter(x+w/2,37,names[i],on?ink:dim,on?bold:body);
  if(on)g().fillRoundRect(x+w/2-tw/2,40,tw,2,1,accent);target(x,23,w,20,-('1'+i));
  if(badges[i]){g().fillCircle(x+w/2+tw/2+8,31,6,accent);textCenter(x+w/2+tw/2+8,34,String(badges[i]),bg,small);}}
}
void drawChessRating();void drawTourList();bool tourListKey(int key);bool tourCardKey(int key);bool tourNewKey(int key);
void drawChessList(){
 drawChessTabs();if(chessTab==2){drawChessRating();return;}if(chessTab==1){drawTourList();return;}
 chessSortRows();int total=1+chessRowCount;if(selected>=total)selected=total-1;int first=max(0,selected-3);
 for(int i=first;i<total&&i<first+4;i++){
  int y=46+(i-first)*44;bool focus=selected==i;listRow(y,42,focus);target(8,y,304,42,i);
  if(i==0){g().fillCircle(32,y+21,14,rgb(0x14524b));g().fillRect(31,y+13,3,17,accent);g().fillRect(24,y+20,17,3,accent);text(56,y+17,t("New game","Новая партия"),ink,bold);text(56,y+32,fit(chessNet.count()>=ChessNet::MaxMatches?t("All boards busy: finish a game","Все доски заняты: завершите партию"):t("Challenge a chat contact","Вызвать чат-контакт"),240,small),dim,small);continue;}
  const ChessMatch& m=*chessRows[i-1];Peer* p=peerOf(m.peer);avatar(32,y+21,14,m.peer,m.name,p?p->type:-1);colorDisc(44,y+32,m.mine);
  String right=m.game.plies?moveCount(m):String();if(m.updated)right=timeText(m.updated);if(m.tour)right=t("R","т")+String(m.round)+" · "+right;int rw=measure(right,small);
  text(56,y+17,fit(m.name,232-rw,bold),ink,bold);textRight(304,y+16,right,faint,small);
  String detail=chessState(m);if(m.game.plies&&m.state!=ChessMatch::Invited)detail+=" · "+lastSan(m);if(m.state==ChessMatch::Playing&&m.link()==ChatMessage::Failed)detail=deliveryText(m);
  if(m.state==ChessMatch::Over&&m.rated&&m.sign==ChessMatch::SignStored)if(const rating::Delta* d=rating::book.delta(m.peer,m.id))if(d->counted){int diff=d->after-d->before;detail+=" · "+String(diff>=0?"+":"")+String(diff);}
  text(56,y+32,fit(detail,236,small),m.link()==ChatMessage::Failed&&m.active()?bad:chessStateColor(m),small);
  if(m.unseen||m.state==ChessMatch::Invited||m.myTurn())g().fillCircle(300,y+30,4,accent);
 }
 scrollbar(first,4,total,46,172);
 if(!chessRowCount){String rows[3];unsigned n=wrap(t("Moves travel as direct MeshCore messages with delivery confirmation. Both players need a MeshMesh M9 with chess.","Ходы идут личными сообщениями MeshCore с подтверждением доставки. Сопернику нужен MeshMesh M9 с шахматами."),rows,3,300);for(unsigned k=0;k<n;k++)textCenter(160,116+k*16,rows[k],faint);}
 bool over=selected>0&&chessRows[selected-1]->state==ChessMatch::Over;
 if(over)footer({{"OK",t("Open","Открыть")},{"DEL",chessRemoveArmed?t("Sure?","Удалить?"):t("Remove","Удалить")},{"<>",t("Tabs","Разделы")},{"BACK",t("Menu","Меню")}});
 else footer({{"OK",selected?t("Open","Открыть"):t("Choose","Выбрать")},{"<>",t("Tabs","Разделы")},{"BACK",t("Menu","Меню")}});
}
// Rating tab: my rating and its last changes, then the players with my score against each.
void drawChessRating(){
 const rating::Player& me=rating::book.players[0];bool ready=rating::book.ready;
 panel(8,46,304,74,card,7);text(18,62,t("Your rating","Ваш рейтинг"),dim,small);text(18,88,String(rating::book.myElo()),ink,big);
 unsigned h=rating::book.historyCount;
 if(h>=2){int diff=rating::book.history[h-1]-rating::book.history[0];text(66,87,(diff>=0?"+":"")+String(diff)+t(" in "," за ")+count(h-1,"game","games","партию","партии","партий"),diff>=0?ok:bad,small);}
 int x=18,y=106;
 if(ready&&me.wins+me.draws+me.losses){x=text(x,y,count(me.wins+me.draws+me.losses,"game","games","партия","партии","партий")+": ",dim,small);x=text(x,y,"+"+String(me.wins),ok,small);x=text(x,y," ="+String(me.draws),dim,small);text(x,y," -"+String(me.losses),bad,small);}
 else text(x,y,t("No rated games yet","Партий на рейтинг пока нет"),faint,small);
 int gx=180,gy=56,gw=124,gh=48;g().drawFastHLine(gx,gy+gh,gw,line);
 if(h>=2){int lo=rating::book.history[0],hi=lo;for(unsigned i=0;i<h;i++){lo=min(lo,int(rating::book.history[i]));hi=max(hi,int(rating::book.history[i]));}if(hi-lo<10){lo-=5;hi+=5;}
  for(int k=0;k<3;k++)for(int px=gx;px<gx+gw;px+=4)g().drawPixel(px,gy+k*gh/3,line);
  auto py=[&](int v){return gy+gh-(v-lo)*gh/(hi-lo);};
  for(unsigned i=1;i<h;i++)thickLine(gx+(i-1)*gw/(h-1),py(rating::book.history[i-1]),gx+i*gw/(h-1),py(rating::book.history[i]),accent);
  g().fillCircle(gx+gw,py(rating::book.history[h-1]),3,accent);textRight(gx+gw,gy+gh+10,count(h,"game","games","партия","партии","партий"),faint,small);}
 else textCenter(gx+gw/2,gy+gh/2+4,t("history after 2 games","история после 2 партий"),faint,small);
 // Players: everyone in the book except me, the highest rating first.
 unsigned order[rating::MaxPlayers],n=0;for(unsigned i=1;i<rating::book.playerCount;i++){const rating::Player& p=rating::book.players[i];if(p.games||p.wins+p.draws+p.losses)order[n++]=i;}
 for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&rating::book.players[order[j]].elo>rating::book.players[order[j-1]].elo;j--)std::swap(order[j],order[j-1]);
 text(16,134,t("Players","Игроки"),faint,small);textRight(304,134,t("your score","ваш счёт"),faint,small);
 if(!n)text(16,154,fit(t("Rated games against contacts appear here","Здесь появятся соперники по партиям на рейтинг"),290,small),faint,small);
 if(chessRatingSel>=int(n))chessRatingSel=max(0,int(n)-1);int first=max(0,chessRatingSel-2);
 for(int i=first;i<int(n)&&i<first+3;i++){const rating::Player& p=rating::book.players[order[i]];int ry=138+(i-first)*26;listRow(ry,24,i==chessRatingSel);uint64_t id=rating::idOf(p.key);
  Peer* peer=peerOf(id);String name=peer?String(peer->name):String(p.name[0]?p.name:meshRadio.idText(id).c_str());
  text(18,ry+16,String(i+1),faint,small);avatar(40,ry+12,9,id,name,1);text(56,ry+17,fit(name,130,bold),ink,bold);textRight(222,ry+17,String(int(lroundf(p.elo))),ink,bold);
  textRight(304,ry+16,"+"+String(p.wins)+" ="+String(p.draws)+" -"+String(p.losses),dim,small);}
 scrollbar(first,3,n,138,78);
 footer({{"<>",t("Tabs","Разделы")},{"^v",t("Players","Игроки")},{"BACK",t("Menu","Меню")}});
}
void drawChessPick(){
 chessSortPeers();panel(8,24,304,30,card,7);text(18,43,t("You play","Вы играете"),dim);
 String colors[]={t("White","Белыми"),t("Black","Чёрными"),t("Random","Случайно")};int cx=222;tri(cx-58,39,3,4,accent);tri(cx+58,39,1,4,accent);
 if(chessPickColor<2)colorDisc(cx-42,39,chessPickColor);textCenter(cx+(chessPickColor<2?6:0),44,colors[chessPickColor],ink,bold);
 target(8,24,304,30,-KeyRight); // the colour
 // Rated or friendly (F): a rated game changes both players' ELO once both sign the result.
 panel(8,57,304,28,card,7);text(18,75,t("Game","Партия"),dim);
 text(18+measure(t("Game","Партия"))+10,75,chessPickRated?t("ELO changes for both","ELO меняется у обоих"):t("ELO stays","ELO не меняется"),faint,small);
 textRight(300,76,chessPickRated?t("Rated","На рейтинг"):t("Friendly","Товарищеская"),chessPickRated?accent:ink,bold);target(8,57,304,28,-'f');
 if(!chessPeerCount){textCenter(160,120,t("No chat contacts yet","Чат-контактов пока нет"),ink,bold);textCenter(160,142,t("Press ADV to announce this node and wait","Нажмите ADV, чтобы объявить узел, и"),dim);textCenter(160,158,t("for the other player's announcement","дождитесь объявления соперника"),dim);footer({{"ADV",t("Announce","Объявить")},{"BACK",t("Games","Партии")}});return;}
 if(selected>=int(chessPeerCount))selected=chessPeerCount-1;int first=max(0,selected-2);
 for(int i=first;i<int(chessPeerCount)&&i<first+3;i++){
  const Peer& p=meshRadio.peers[chessPeers[i]];int y=88+(i-first)*44;bool focus=selected==i;listRow(y,42,focus);target(8,y,304,42,i);avatar(32,y+21,14,p.id,p.name,p.type);
  int rw=0;
  if(chessPickRated){int win,draw,loss;rating::book.expected(p.id,win,draw,loss);String w="+"+String(win),l=String(loss);rw=max(measure(w,bold),measure(l,bold))+6;
   textRight(304,y+17,w,ok,bold);textRight(304,y+32,l,bad,bold);}
  text(56,y+17,fit(p.name,240-rw,bold),ink,bold);
  String info=(p.heard?(millis()-p.seen<60000?ago(0):ago(millis()-p.seen)+t(" ago"," назад"))+" · ":String())+pathText(p);if(chessPickRated)info="ELO "+String(rating::book.elo(p.id))+" · "+info;
  text(56,y+32,fit(info,240-rw,small),p.heard&&millis()-p.seen<1800000?ok:dim,small);
 }
 scrollbar(first,3,chessPeerCount,88,130);
 footer({{"OK",t("Challenge","Вызвать")},{"<>",t("Colour","Цвет")},{"F",chessPickRated?t("Friendly","Товарищ."):t("Rated","Рейтинг")},{"BACK",t("Back","Назад")}});
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
 // Delivery of the last command matters while waiting or when it failed; otherwise the rating line.
 String deliver=(m.myTurn()||m.state==ChessMatch::Over)&&m.link()!=ChatMessage::Failed?chessRatingText(m):deliveryText(m);
 if(m.drawOffer==ChessMatch::OfferedByMe&&m.state==ChessMatch::Playing)deliver=t("You offered a draw","Вы предложили ничью");
 if(n==1&&deliver.length()){text(x+6,204,rowsText[0],chessStateColor(m),small);text(x+6,214,fit(deliver,w-10,small),m.link()==ChatMessage::Failed?bad:faint,small);}
 else for(unsigned i=0;i<n;i++)text(x+6,204+i*10,rowsText[i],chessStateColor(m),small);
 if(m.drawOffer==ChessMatch::OfferedToMe&&m.state==ChessMatch::Playing){panel(x,150,w,22,rgb(0x14524b),6);textCenter(x+w/2,164,t("Draw? D accepts","Ничья? D — да"),ink,small);}
 else if(m.tour){const tour::Tour* T=tour::net.find(m.tour);panel(x,150,w,22,rgb(0x2e2450),6);text(x+6,165,fit((T?String(T->name):String(t("Tournament","Турнир")))+" · "+t("R","т")+String(m.round),w-10,small),violet,small);}
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
void chessBackToList(){ChessMatch* was=chessOpen;change(ChessList);chessTab=0;chessSortRows();for(unsigned i=0;i<chessRowCount;i++)if(chessRows[i]==was)selected=i+1;}
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
 if(page==ChessTour)return tourCardKey(key);
 if(page==ChessTourNew)return tourNewKey(key);
 if(key>='A'&&key<='Z')key+=32;
 if(page==ChessList){
  if(key!=Erase)chessRemoveArmed=false;
  if(key==KeyLeft||key==KeyRight){chessTab=constrain(chessTab+(key==KeyRight?1:-1),0,2);chessRemoveArmed=false;return true;}
  if(key>='1'&&key<='3'){chessTab=key-'1';chessRemoveArmed=false;return true;}
  if(chessTab==1)return tourListKey(key);
  if(chessTab==2){ // rating: up/down go through the players
   if(key==KeyUp||key==KeyDown){chessRatingSel=max(0,chessRatingSel+(key==KeyUp?-1:1));return true;}
   if(key==KeyBack){change(Home);return true;}
   return key==Enter||key==KeyHold||key==Erase;
  }
  if(key==KeyUp||key==KeyDown){chessSortRows();int total=1+chessRowCount;selected=(selected+(key==KeyUp?-1:1)+total)%total;return true;}
  if(key==Enter||key==KeyHold){chessSortRows();if(!selected){if(chessNet.count()>=ChessNet::MaxMatches&&!chessRowCount)return true;change(ChessPick);}else chessEnter(chessRows[selected-1]);return true;}
  if(key==Erase&&selected){chessSortRows();ChessMatch& m=*chessRows[selected-1];if(m.state!=ChessMatch::Over){notice(t("Only finished games can be removed","Удалить можно только оконченную партию"),dim);return true;}
   if(!chessRemoveArmed){chessRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит партию"),warn);return true;}chessRemoveArmed=false;chessNet.remove(m);notice(t("Game removed","Партия удалена"),ok);return true;}
  if(key==KeyBack){change(Home);return true;}
  return false;
 }
 if(page==ChessPick){
  if(key==KeyLeft||key==KeyRight){chessPickColor=(chessPickColor+(key==KeyLeft?2:1))%3;return true;}
  if(key=='f'){chessPickRated=!chessPickRated;return true;}
  if(key==KeyUp||key==KeyDown){chessSortPeers();if(chessPeerCount)selected=(selected+(key==KeyUp?-1:1)+chessPeerCount)%chessPeerCount;return true;}
  if(key==Enter||key==KeyHold){chessSortPeers();if(!chessPeerCount)return true;ChessMatch* m=chessNet.invite(meshRadio.peers[chessPeers[selected]].id,chessPickColor,chessPickRated);if(!m){notice(chessNet.event,bad);return true;}
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
  ChessMatch* next=chessNet.invite(m.peer,m.mine^1,m.rated);if(!next){notice(chessNet.event,bad);return true;}chessNet.events=chessEventsSeen;chessEnter(next);notice(t("Rematch sent: you play ","Реванш: вы играете ")+colorName(next->mine,true),ok);return true;
 }
 if(key==Erase){
  if(m.state==ChessMatch::Invited||m.state==ChessMatch::Inviting||(m.state==ChessMatch::Playing&&!m.game.plies)){bool mineInvite=m.state!=ChessMatch::Invited;if(chessNet.decline(m))notice(mineInvite?t("Challenge cancelled","Вызов отменён"):t("Challenge declined","Вызов отклонён"),dim);return true;}
  if(m.state==ChessMatch::Over){if(!chessRemoveArmed){chessRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит партию"),warn);return true;}chessRemoveArmed=false;chessNet.remove(m);chessOpen=nullptr;change(ChessList);notice(t("Game removed","Партия удалена"),ok);return true;}
  notice(t("Resign with X","Сдаться — клавиша X"),dim);return true;
 }
 return false;
}
// Locked screen: the game with the latest unseen news if it needs the player or has ended, else the
// newest game waiting for the player. Without details it names neither the opponent nor the move.
bool lockNeeds(const ChessMatch& m){return m.state==ChessMatch::Invited||m.myTurn()||(m.state==ChessMatch::Playing&&m.drawOffer==ChessMatch::OfferedToMe);}
const ChessMatch* lockChess(){
 const ChessMatch* e=chessNet.eventMatch;if(e&&e->state!=ChessMatch::Free&&e->unseen&&(lockNeeds(*e)||e->state==ChessMatch::Over))return e;
 const ChessMatch* best=nullptr;for(auto& m:chessNet.matches)if(m.state!=ChessMatch::Free&&lockNeeds(m)&&(!best||int32_t(m.changedAt-best->changedAt)>0))best=&m;
 return best;
}
void drawLockChess(int y){
 const ChessMatch& m=*lockChess();unsigned waiting=chessNet.waiting();String head,sub;
 panel(40,y,240,40,card,8);ring(40,y,240,40,8,rgb(0x14524b));icon(IcChess,62,y+20,9,accent,card);
 if(config.lockDetails){bool theirs=m.game.plies&&(m.myTurn()||m.state==ChessMatch::Over);head=String(m.name)+(theirs?": "+lastSan(m):String());
  sub=chessState(m);}
 else{head=t("Chess","Шахматы");sub=waiting?chessTileDetail():t("News in a game","Новости в партии");}
 // Other games waiting, on the right of the first line.
 String more=config.lockDetails&&waiting>(lockNeeds(m)?1u:0u)?t("more: ","ещё ")+String(waiting-(lockNeeds(m)?1:0)):String();int mw=more.length()?measure(more,small)+8:0;
 if(mw)textRight(272,y+15,more,faint,small);text(80,y+16,fit(head,192-mw,bold),ink,bold);text(80,y+32,fit(sub,192,small),chessStateColor(m),small);
}
void chessTick(){
 if(chessNet.events!=chessEventsSeen){
  chessEventsSeen=chessNet.events;
  // News the player has to act on, or a result, lights a dark screen; the locked screen shows it.
  const ChessMatch* e=chessNet.eventMatch;if(e&&(lockNeeds(*e)||e->state==ChessMatch::Over)){lastInput=millis();if(wakeOnly){wakeOnly=false;hardware.brightness(config.brightness);}}
  // On the open game the news is on the board; elsewhere it is a toast.
  bool here=page==ChessBoard&&chessOpen==chessNet.eventMatch;if(here&&chessOpen)chessNet.viewed(*chessOpen);
  if(!locked&&chessNet.event.length())notice(chessNet.event,here?info:accent);
  if(here&&chessHeld>=0&&!canTake(chessHeld))chessHeld=-1;
 }
 if(chessNet.dirty){chessNet.dirty=false;dirty=true;}
}
void chessStatus(JsonDocument& d){
 if(page==ChessList)d["chess_tab"]=chessTab==2?"rating":chessTab?"tournaments":"games";
 if(page==ChessPick){d["chess_color"]=chessPickColor;d["chess_rated"]=chessPickRated;return;}
 if(page!=ChessBoard||!chessOpen)return;char id[5];snprintf(id,sizeof id,"%04X",chessOpen->id);d["chess_game"]=id;
 char sq[3]={char('a'+ch::fileOf(chessCursor)),char('1'+ch::rankOf(chessCursor)),0};d["chess_cursor"]=sq;
 if(chessHeld>=0){char h[3]={char('a'+ch::fileOf(chessHeld)),char('1'+ch::rankOf(chessHeld)),0};d["chess_held"]=h;}
 d["chess_promotion"]=chessPromo;d["chess_help"]=chessHelp;
}
