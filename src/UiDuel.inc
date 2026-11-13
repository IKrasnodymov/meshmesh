// Checkers and sea battle with contacts on the M9 screen (Duel.h). Included by Ui.cpp inside its anonymous namespace,
// after UiChess.inc: the board geometry and colours, the colour discs and the sorted chat contacts are shared.
// The network, the rules and saving live in DuelNet; this file draws the games of one kind, the choice of a contact
// and the two game pages. No rating: just a game. Checkers: arrows move the cursor, OK takes a man and puts it on a
// marked square. Sea battle: arrows aim at their field, OK fires; my fleet is the small field on the right.
namespace ck=checkers;
Duel* duelOpen=nullptr;              // the game on its page
int ckCursor=12,duelHeld=-1,seaCursor=44,duelPickColor=2; // checkers: the cursor (row*8+col) and the taken man (a dark square); sea: a cell of their field
bool duelHelp=false,duelResignArmed=false,duelRemoveArmed=false,seaAimed=false; // seaAimed: the cursor was moved there (a tap on it fires)
uint32_t duelEventsSeen=0;
Duel* duelRows[DuelNet::Max];unsigned duelRowCount=0;
const uint16_t seaWater=rgb(0x0f2a40),seaGrid=rgb(0x1f4a68),seaKnown=rgb(0x1a3d56),seaShip=rgb(0x9aa6b2),seaHit=rgb(0xe5564d),seaSunk=rgb(0x8f2f29),seaShown=rgb(0x51606e);
const uint16_t manWhiteRim=rgb(0xb8b0a0),manBlackRim=rgb(0x5d6672),manShadow=rgb(0x2a1c10),crown=rgb(0xe9b13b);
constexpr int SeaX=18,SeaY=35,SeaCell=17,HomeX=210,HomeY=37,HomeCell=9;

bool duelPage(){return page==CheckersList||page==SeaList||page==DuelPick||page==CheckersBoard||page==SeaBoard;}
bool duelNeeds(const Duel& g){return g.state==Duel::Invited||g.myTurn()||(g.state==Duel::Playing&&g.drawOffer==Duel::OfferedToMe);}
String duelTileDetail(int kind){unsigned w=duels.waiting(kind),n=duels.count(kind);if(w)return count(w,"game waits","games wait","ждёт хода","ждут хода","ждут хода");if(n)return count(n,"game","games","игра","игры","игр");return t("With contacts","С контактами");}
String duelTitle(){
 if(page==DuelPick)return duelKind==Duel::Sea?t("New sea battle","Новый морской бой"):t("New checkers game","Новая партия в шашки");
 bool sea=page==SeaList||page==SeaBoard;String name=sea?t("Sea battle","Морской бой"):t("Checkers","Шашки");
 return (page==CheckersBoard||page==SeaBoard)&&duelOpen&&duelOpen->state!=Duel::Free?name+" · "+duelOpen->name:name;
}
// One line about the state of a game, for the list and for the game page (board: the draw offer has its own strip).
String duelState(const Duel& g,bool board=false){
 bool sea=g.kind==Duel::Sea;
 switch(g.state){
 case Duel::Inviting:return t("Invitation sent","Приглашение отправлено");
 case Duel::Invited:return sea?t("Invites you to play","Зовёт сыграть"):t("Invites you: you play ","Зовёт: вы играете ")+colorName(g.mine,true);
 case Duel::Playing:
  if(g.drawOffer==Duel::OfferedToMe&&!board)return t("Offers a draw","Предлагает ничью");
  if(sea)return g.aim>=0?t("Waiting for the answer","Ждём ответа"):g.myTurn()?t("Your shot","Ваш выстрел"):t("Their shot","Стреляет соперник");
  return g.myTurn()?t("Your move","Ваш ход"):t("Their move","Ход соперника");
 case Duel::Over:{
  if(g.reason==Duel::Declined)return t("Invitation declined","Приглашение отклонено");
  if(g.reason==Duel::Cancelled)return t("Game cancelled","Игра отменена");
  bool won=g.result==Duel::Won;String r=won?t("You won","Победа"):g.result==Duel::Lost?t("You lost","Поражение"):t("Draw","Ничья");
  switch(g.reason){
  case Duel::Resigned:return r+(won?t(": they resigned",": соперник сдался"):t(": you resigned",": вы сдались"));
  case Duel::Agreed:return r+t(" by agreement"," по соглашению");
  case Duel::Quiet:return r+t(": 15 king moves each",": 15 ходов дамками");
  case Duel::Rules:return r+(sea?(won?t(": their fleet sunk",": флот соперника потоплен"):t(": your fleet sunk",": ваш флот потоплен")):(won?t(": they cannot move",": у соперника нет ходов"):t(": you cannot move",": у вас нет ходов")));
  default:return r;}
 }
 default:return "";
 }
}
uint16_t duelStateColor(const Duel& g){if(duelNeeds(g))return accent;if(g.state==Duel::Over)return g.result==Duel::Won?ok:g.result==Duel::Lost?bad:dim;return dim;}
String duelDelivery(const Duel& g){
 switch(g.link()){case ChatMessage::Queued:return t("sending","отправляется");case ChatMessage::Sent:return t("sent, no ACK yet","отправлено, ждём ACK");case ChatMessage::Delivered:return t("delivered","доставлено");
 case ChatMessage::Failed:{if(g.autoStopped)return t("not delivered (R)","не доставлено (R)");int32_t s=duels.retryIn(g);
  return s<0?t("not delivered, resending","не доставлено, повторяю"):s<60?t("not delivered, retry now","не доставлено, повтор сейчас"):t("not delivered, retry in ","не доставлено, повтор через ")+String((s+59)/60)+t(" min"," мин");}}
 return "";
}
String ckLast(const Duel& g){if(!g.game.plies)return "";char b[40];ck::moveText(g.game.last,b,sizeof b);return b;}
// Sea battle: the answer to a shot in words.
String seaWord(uint8_t answer){return answer==sea::AnswerMiss?t("miss","мимо"):answer==sea::AnswerHit?t("hit","ранен"):answer==sea::AnswerSunk||answer==sea::AnswerWin?t("sunk","потоплен"):String();}
String seaScore(const Duel& g){return String(g.target.sunkShips())+":"+String(sea::Ships-g.home.shipsLeft());} // ships sunk by me : by them

// The list: games that need the player first, then the newest.
void duelSortRows(){
 duelRowCount=0;for(auto& g:duels.games)if(g.state!=Duel::Free&&g.kind==duelKind)duelRows[duelRowCount++]=&g;
 auto rank=[](const Duel* g){return duelNeeds(*g)||g->unseen?0:g->active()?1:2;};
 for(unsigned i=1;i<duelRowCount;i++)for(unsigned j=i;j>0;j--){Duel* a=duelRows[j];Duel* b=duelRows[j-1];if(rank(a)<rank(b)||(rank(a)==rank(b)&&int32_t(a->changedAt-b->changedAt)>0))std::swap(duelRows[j],duelRows[j-1]);else break;}
}
void drawDuelList(){
 bool sea=duelKind==Duel::Sea;duelSortRows();int total=1+duelRowCount;if(selected>=total)selected=total-1;int first=max(0,selected-3);
 for(int i=first;i<total&&i<first+4;i++){
  int y=24+(i-first)*48;bool focus=selected==i;listRow(y,45,focus);target(8,y,304,45,i);
  if(i==0){g().fillCircle(32,y+22,15,rgb(0x14524b));g().fillRect(31,y+14,3,17,accent);g().fillRect(24,y+21,17,3,accent);text(56,y+18,t("New game","Новая игра"),ink,bold);
   text(56,y+36,fit(!duels.room()?t("Six games in progress: finish one","Идут шесть игр: завершите одну"):t("Invite a chat contact","Позвать чат-контакт"),244,small),dim,small);continue;}
  const Duel& m=*duelRows[i-1];Peer* p=peerOf(m.peer);avatar(32,y+22,15,m.peer,m.name,p?p->type:-1);
  if(sea){g().fillCircle(45,y+34,7,bg);icon(IcShip,45,y+34,5,info,bg);}else colorDisc(45,y+34,m.mine);
  String right=m.updated?timeText(m.updated):String();int rw=measure(right,small);
  text(58,y+18,fit(m.name,232-rw,bold),ink,bold);textRight(304,y+17,right,faint,small);
  String detail=duelState(m);
  if(m.state==Duel::Playing||m.state==Duel::Over){if(sea){if(m.shots)detail+=" · "+seaScore(m);}else if(m.game.plies)detail+=" · "+ckLast(m);}
  bool failed=m.link()==ChatMessage::Failed&&m.active();if(failed)detail=duelDelivery(m);
  text(58,y+36,fit(detail,232,small),failed?bad:duelStateColor(m),small);
  if(m.unseen||duelNeeds(m))g().fillCircle(300,y+33,4,accent);
 }
 scrollbar(first,4,total,24,190);
 if(!duelRowCount){String rows[4];unsigned n=wrap(sea?t("Shots travel as direct MeshCore messages with delivery confirmation. Ten ships each, placed at random; a hit shoots again. The other player needs MeshMesh with sea battle.","Выстрелы идут личными сообщениями MeshCore с подтверждением доставки. У каждого десять кораблей, расставленных случайно; попал - стреляй ещё. Сопернику нужен MeshMesh с морским боем."):t("Moves travel as direct MeshCore messages with delivery confirmation. Russian draughts: capturing is compulsory, kings fly. The other player needs MeshMesh with checkers.","Ходы идут личными сообщениями MeshCore с подтверждением доставки. Русские шашки: бить обязательно, дамка ходит далеко. Сопернику нужен MeshMesh с шашками."),rows,4,296);
  for(unsigned k=0;k<n;k++)textCenter(160,100+k*16,rows[k],faint);}
 bool over=selected>0&&duelRows[selected-1]->state==Duel::Over;
 if(over)footer({{"OK",t("Open","Открыть")},{"DEL",duelRemoveArmed?t("Sure?","Удалить?"):t("Remove","Удалить")},{"BACK",t("Menu","Меню")}});
 else footer({{"OK",selected?t("Open","Открыть"):t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"BACK",t("Menu","Меню")}});
}
// The contact to play with: chat contacts, the most recently heard first (as chess); checkers also takes the colour.
void drawDuelPick(){
 chessSortPeers();bool sea=duelKind==Duel::Sea;panel(8,24,304,30,card,7);
 if(sea){icon(IcShip,24,39,7,info,card);text(38,43,fit(t("Your fleet is placed at random; S reshuffles it before the first shot","Флот расставится случайно; S перемешает его до первого выстрела"),268,small),dim,small);}
 else{text(18,43,t("You play","Вы играете"),dim);String colors[]={t("White","Белыми"),t("Black","Чёрными"),t("Random","Случайно")};int cx=222;tri(cx-58,39,3,4,accent);tri(cx+58,39,1,4,accent);
  if(duelPickColor<2)colorDisc(cx-42,39,duelPickColor);textCenter(cx+(duelPickColor<2?6:0),44,colors[duelPickColor],ink,bold);target(8,24,304,30,-KeyRight);}
 if(!chessPeerCount){textCenter(160,110,t("No chat contacts yet","Чат-контактов пока нет"),ink,bold);textCenter(160,132,t("Press ADV to announce this node and wait","Нажмите ADV, чтобы объявить узел, и"),dim);textCenter(160,148,t("for the other player's announcement","дождитесь объявления соперника"),dim);footer({{"ADV",t("Announce","Объявить")},{"BACK",t("Back","Назад")}});return;}
 if(selected>=int(chessPeerCount))selected=chessPeerCount-1;int first=max(0,selected-2);
 for(int i=first;i<int(chessPeerCount)&&i<first+3;i++){
  const Peer& p=meshRadio.peers[chessPeers[i]];int y=60+(i-first)*48;bool focus=selected==i;listRow(y,45,focus);target(8,y,304,45,i);avatar(32,y+22,15,p.id,p.name,p.type);
  text(56,y+18,fit(p.name,244,bold),ink,bold);
  String info=(p.heard?(millis()-p.seen<60000?ago(0):ago(millis()-p.seen)+t(" ago"," назад"))+" · ":String())+pathText(p);
  bool busy=false;for(auto& g:duels.games)busy|=g.active()&&g.peer==p.id&&g.kind==duelKind;if(busy)info=t("A game with them is on · ","Игра с ним уже идёт · ")+info;
  text(56,y+36,fit(info,244,small),p.heard&&millis()-p.seen<1800000?ok:dim,small);
 }
 scrollbar(first,3,chessPeerCount,60,154);
 if(sea)footer({{"OK",t("Invite","Позвать")},{"^v",t("Select","Выбор")},{"BACK",t("Back","Назад")}});
 else footer({{"OK",t("Invite","Позвать")},{"<>",t("Colour","Цвет")},{"^v",t("Select","Выбор")},{"BACK",t("Back","Назад")}});
}

// Checkers board: my side at the bottom (White: a1 bottom left; Black: a1 top right). Pieces stand on the dark squares.
bool ckFlip(){return duelOpen&&duelOpen->mine==ck::Black;}
int ckX(int col){return BoardX+Cell*(ckFlip()?7-col:col);}
int ckY(int row){return BoardY+Cell*(ckFlip()?row:7-row);}
int ckSquare(){return ck::squareAt(ckCursor&7,ckCursor>>3);}
// A man: a disc with a groove; a king carries a crown.
void drawMan(int8_t p,int cx,int cy){
 if(!p)return;auto& d=g();bool white=p>0;
 d.fillCircle(cx,cy+2,9,manShadow);d.fillCircle(cx,cy,9,white?pieceWhiteInk:pieceBlackEdge);d.fillCircle(cx,cy,8,white?pieceWhite:pieceBlack);
 d.drawCircle(cx,cy,5,white?manWhiteRim:manBlackRim);
 if(p==ck::King||p==-ck::King){d.fillRect(cx-5,cy+1,11,3,crown);d.fillTriangle(cx-5,cy+1,cx-5,cy-5,cx-2,cy+1,crown);d.fillTriangle(cx-2,cy+1,cx,cy-6,cx+2,cy+1,crown);d.fillTriangle(cx+2,cy+1,cx+5,cy-5,cx+5,cy+1,crown);
  d.drawFastHLine(cx-5,cy+4,11,white?pieceWhiteInk:pieceBlackEdge);}
}
// The moves of the side to move when it is mine (captures only when there is one).
unsigned ckMoves(ck::Move* list){return duelOpen&&duelOpen->myTurn()?duelOpen->game.pos.legal(list):0;}
bool ckCanTake(int s){if(s<0)return false;ck::Move list[ck::MaxMoves];unsigned n=ckMoves(list);for(unsigned i=0;i<n;i++)if(list[i].from()==s)return true;return false;}
void drawCheckers(){
 auto& d=g();const Duel& m=*duelOpen;const ck::Position& pos=m.game.pos;
 ck::Move list[ck::MaxMoves];unsigned n=ckMoves(list);
 bool last[32]={};if(m.game.plies)for(unsigned i=0;i<m.game.last.n;i++)last[m.game.last.path[i]]=true;
 int cs=ckSquare();const ck::Move* chosen=nullptr; // the move the cursor would play: its path is drawn
 if(duelHeld>=0&&cs>=0)for(unsigned i=0;i<n&&!chosen;i++)if(list[i].from()==duelHeld&&list[i].to()==cs)chosen=&list[i];
 d.fillRect(BoardX-2,BoardY-2,Cell*8+4,Cell*8+4,rgb(0x3a2a1c));
 for(int row=0;row<8;row++)for(int col=0;col<8;col++){
  int x=ckX(col),y=ckY(row),s=ck::squareAt(col,row);
  d.fillRect(x,y,Cell,Cell,s<0?boardLight:s==duelHeld?heldDark:last[s]?lastDark:boardDark);
  uint16_t label=s<0?boardDark:boardLight;
  if(y==BoardY+Cell*7)text(x+Cell-6,y+Cell-2,String(char('a'+col)),label,small);
  if(x==BoardX)text(x+1,y+7,String(char('1'+row)),label,small);
 }
 if(chosen&&chosen->n>2)for(unsigned i=1;i<chosen->n;i++){int a=chosen->path[i-1],b=chosen->path[i];thickLine(ckX(ck::colOf(a))+Cell/2,ckY(ck::rowOf(a))+Cell/2,ckX(ck::colOf(b))+Cell/2,ckY(ck::rowOf(b))+Cell/2,heldLight);}
 for(int s=0;s<32;s++){
  int x=ckX(ck::colOf(s)),y=ckY(ck::rowOf(s));drawMan(pos.board[s],x+Cell/2,y+Cell/2);
  if(duelHeld<0)continue;bool dest=false,take=false;for(unsigned i=0;i<n;i++)if(list[i].from()==duelHeld&&list[i].to()==s){dest=true;take|=list[i].capture;}
  if(!dest)continue;
  if(take)for(int k=0;k<4;k++){int cx=x+(k&1?Cell-1:0),cy=y+(k&2?Cell-1:0);d.fillTriangle(cx,cy,cx+(k&1?-6:6),cy,cx,cy+(k&2?-6:6),moveDot);}else d.fillCircle(x+Cell/2,y+Cell/2,4,moveDot);
 }
 if(chosen&&chosen->n>2)for(unsigned i=1;i+1<chosen->n;i++){int s=chosen->path[i];d.fillCircle(ckX(ck::colOf(s))+Cell/2,ckY(ck::rowOf(s))+Cell/2,3,heldLight);} // the stops of a capture chain
 if(m.state==Duel::Playing){int x=ckX(ckCursor&7),y=ckY(ckCursor>>3);uint16_t c=duelHeld>=0&&cs!=duelHeld&&!chosen&&!ckCanTake(cs)?warn:accent;d.drawRect(x,y,Cell,Cell,c);d.drawRect(x+1,y+1,Cell-2,Cell-2,c);}
}
// Pieces of a colour: "12" or "9 + 2 kings".
String ckPieces(const Duel& m,uint8_t color){unsigned all=m.game.pos.count(color),kings=0;for(int8_t p:m.game.pos.board)kings+=p==(color==ck::White?ck::King:-ck::King);
 return kings?String(all-kings)+" + "+count(kings,"king","kings","дамка","дамки","дамок"):count(all,"man","men","шашка","шашки","шашек");}
void drawCheckersSide(){
 const Duel& m=*duelOpen;int x=204,w=110;
 // The opponent on top, the player at the bottom, as on the board.
 panel(x,24,w,38,card,6);colorDisc(x+10,36,m.mine^1);text(x+20,40,fit(m.name,w-24,bold),ink,bold);text(x+6,55,fit(ckPieces(m,m.mine^1),w-10,small),dim,small);
 text(x+2,76,t("Last move","Последний ход"),faint,small);
 if(m.game.plies){colorDisc(x+7,88,m.game.pos.side^1);String rows[3];unsigned n=wrap(ckLast(m),rows,3,w-20);for(unsigned i=0;i<n;i++)text(x+16,92+i*14,rows[i],ink);}
 else text(x+2,92,fit(m.state==Duel::Inviting?t("Waiting for reply","Ждём ответа"):m.state==Duel::Invited?t("OK accepts","OK - принять"):t("No moves yet","Ходов ещё нет"),w-4,small),faint,small);
 if(m.state==Duel::Playing){
  if(m.drawOffer==Duel::OfferedToMe){panel(x,150,w,22,rgb(0x14524b),6);textCenter(x+w/2,164,t("Draw? D accepts","Ничья? D - да"),ink,small);}
  else if(m.drawOffer==Duel::OfferedByMe){panel(x,150,w,22,card,6);textCenter(x+w/2,164,fit(t("You offered a draw","Вы предложили ничью"),w-8,small),dim,small);}
  else if(m.myTurn()){ck::Move list[ck::MaxMoves];unsigned n=ckMoves(list);if(n&&list[0].capture){panel(x,150,w,22,rgb(0x4a3410),6);textCenter(x+w/2,164,fit(t("You must capture","Бить обязательно"),w-8,small),warn,small);}}
 }
 panel(x,176,w,42,card,6);colorDisc(x+10,188,m.mine);text(x+20,192,t("You","Вы"),ink,bold);textRight(x+w-6,191,String(m.game.pos.count(m.mine)),dim,small);
 String rows[2];unsigned n=wrap(duelState(m,true),rows,2,w-10,small);String deliver=duelDelivery(m);bool failed=m.link()==ChatMessage::Failed;
 if(n==1&&deliver.length()&&(failed||!m.myTurn())){text(x+6,204,rows[0],duelStateColor(m),small);text(x+6,214,fit(deliver,w-10,small),failed?bad:faint,small);}
 else for(unsigned i=0;i<n;i++)text(x+6,204+i*10,rows[i],duelStateColor(m),small);
}

// Sea battle: their field large on the left (letters a..j across, rows 1..10 down), my fleet small on the right.
void drawSeaTarget(){
 auto& d=g();const Duel& m=*duelOpen;bool aiming=m.state==Duel::Playing;
 for(int c=0;c<sea::Size;c++)textCenter(SeaX+c*SeaCell+SeaCell/2,SeaY-3,String(char('a'+c)),aiming&&c==seaCursor%sea::Size?accent:dim,small);
 for(int r=0;r<sea::Size;r++)textRight(SeaX-3,SeaY+r*SeaCell+12,String(r+1),aiming&&r==seaCursor/sea::Size?accent:dim,small);
 d.fillRect(SeaX,SeaY,SeaCell*sea::Size+1,SeaCell*sea::Size+1,seaGrid);
 for(int c=0;c<sea::Cells;c++){
  int x=SeaX+(c%sea::Size)*SeaCell+1,y=SeaY+(c/sea::Size)*SeaCell+1,s=SeaCell-1,cx=x+s/2,cy=y+s/2;uint8_t mark=m.target.mark[c];
  bool ship=m.revealed&&m.theirs.shipAt(c)>=0,shown=ship&&mark==sea::Unknown,lie=ship&&mark==sea::Miss; // lie: answered "miss" on a ship
  d.fillRect(x,y,s,s,mark==sea::Hit?seaHit:mark==sea::Sunk?seaSunk:shown||lie?seaShown:m.target.around(c)?seaKnown:seaWater);
  if(mark==sea::Miss)d.fillCircle(cx,cy,2,lie?bad:dim);if(lie)d.drawRect(x,y,s,s,bad);
  else if(mark==sea::Hit||mark==sea::Sunk){thickLine(x+3,y+3,x+s-5,y+s-4,ink);thickLine(x+s-5,y+3,x+3,y+s-4,ink);}
  else if(c==m.aim){d.drawCircle(cx,cy,5,warn);d.drawCircle(cx,cy,4,warn);d.drawFastHLine(cx-7,cy,15,warn);d.drawFastVLine(cx,cy-7,15,warn);} // my shot waits for the answer
  if(c==m.lastOut&&c!=m.aim)d.drawRect(x-1,y-1,s+2,s+2,ink);
 }
 if(aiming){int x=SeaX+(seaCursor%sea::Size)*SeaCell,y=SeaY+(seaCursor/sea::Size)*SeaCell;uint16_t col=m.myTurn()&&!m.target.open(seaCursor)?warn:accent;d.drawRect(x-1,y-1,SeaCell+3,SeaCell+3,col);d.drawRect(x,y,SeaCell+1,SeaCell+1,col);}
}
void drawSeaHome(){
 auto& d=g();const Duel& m=*duelOpen;d.fillRect(HomeX,HomeY,HomeCell*sea::Size+1,HomeCell*sea::Size+1,seaGrid);
 for(int c=0;c<sea::Cells;c++){
  int x=HomeX+(c%sea::Size)*HomeCell+1,y=HomeY+(c/sea::Size)*HomeCell+1,s=HomeCell-1,ship=m.home.fleet.shipAt(c);bool shot=m.home.isShot(c);
  d.fillRect(x,y,s,s,ship<0?seaWater:!shot?seaShip:m.home.sunk(ship)?seaSunk:seaHit);
  if(shot&&ship<0)d.fillRect(x+s/2-1,y+s/2-1,2,2,dim);
  if(c==m.lastIn)d.drawRect(x-1,y-1,s+2,s+2,warn);
 }
}
void drawSeaSide(){
 const Duel& m=*duelOpen;int x=198,w=116;
 text(x,31,t("Your fleet","Ваш флот"),dim,small);unsigned left=m.home.shipsLeft();textRight(314,31,String(left)+"/"+String(sea::Ships),left<sea::Ships?warn:ok,small);
 drawSeaHome();
 String rows[2];unsigned n=wrap(duelState(m,true),rows,2,w,bold);for(unsigned i=0;i<n;i++)text(x,145+i*14,rows[i],duelStateColor(m),bold);
 int y=n>1?178:166;
 if(m.state==Duel::Playing||m.state==Duel::Over){
  text(x,y,fit(t("Sunk: ","Потоплено: ")+String(m.target.sunkShips())+t(" of "," из ")+String(sea::Ships),w,small),ink,small);y+=11;
  if(m.lastOut>=0){char c[4];sea::cellName(m.lastOut,c);uint8_t mk=m.target.mark[m.lastOut];text(x,y,fit(t("You: ","Вы: ")+c+" "+seaWord(mk==sea::Miss?sea::AnswerMiss:mk==sea::Hit?sea::AnswerHit:sea::AnswerSunk),w,small),mk==sea::Miss?dim:ok,small);y+=11;}
  if(m.lastIn>=0){char c[4];sea::cellName(m.lastIn,c);text(x,y,fit(t("They: ","Соперник: ")+c+" "+seaWord(m.lastAnswer),w,small),m.lastAnswer==sea::AnswerMiss?dim:bad,small);y+=11;}
 }else{text(x,y,fit(t("S reshuffles your fleet","S - перемешать ваш флот"),w,small),faint,small);y+=11;}
 String deliver=duelDelivery(m);bool failed=m.link()==ChatMessage::Failed;
 if(m.state==Duel::Over&&m.revealed&&!m.fair)text(x,214,fit(t("Answers do not match the fleet!","Ответы не сходятся с флотом!"),w,small),bad,small);
 else if(m.state==Duel::Over&&m.result==Duel::Lost&&!m.revealed&&m.reason==Duel::Rules)text(x,214,fit(t("Waiting for their fleet","Ждём расстановку соперника"),w,small),faint,small);
 else if(deliver.length()&&y<=214)text(x,214,fit(deliver,w,small),failed?bad:faint,small);
}
void drawDuelHelp(){
 uint16_t* px=g().getBuffer();for(int i=0;i<320*240;i++)px[i]=(px[i]>>1)&0x7bef;bool sea=duelOpen&&duelOpen->kind==Duel::Sea;
 panel(6,24,308,194,cardHi,10);g().drawRoundRect(6,24,308,194,10,line);text(16,42,sea?t("Sea battle keys","Клавиши морского боя"):t("Checkers keys","Клавиши шашек"),ink,bold);
 Hint keys[]={{"<>",sea?t("Aim at their field (also ^v)","Прицел по полю соперника (и ^v)"):t("Move the cursor (also ^v)","Курсор по доске (и ^v)")},{"OK",sea?t("Fire; a hit shoots again","Выстрел; попал - стреляйте ещё"):t("Take a man / put it","Взять шашку / поставить")},
  {"BACK",sea?t("To the games","К играм"):t("Put back / to the games","Вернуть шашку / к играм")},{sea?"S":"D",sea?t("Reshuffle the fleet before the first shot","Перемешать флот до первого выстрела"):t("Offer a draw / accept one","Предложить / принять ничью")},
  {"X",t("Resign (press twice)","Сдаться (дважды)")},{"R",t("Resend what was not delivered","Повторить недоставленное")},{"N",t("Play again after the game","Сыграть ещё раз после игры")},{"DEL",t("Decline, cancel, remove","Отказ, отмена, удаление")}};
 for(int i=0;i<8;i++){int y=52+i*17;g().fillRoundRect(16,y,30,13,3,line);if(!strcmp(keys[i].key,"<>")){tri(26,y+6,3,3,ink);tri(35,y+6,1,3,ink);}else textCenter(31,y+10,keys[i].key,ink,small);text(54,y+11,fit(keys[i].action,252),i%2?dim:ink);}
 text(16,202,fit(sea?t("Shots are sent as direct messages with ACK.","Выстрелы идут личными сообщениями с ACK."):t("Moves are sent as direct messages with ACK.","Ходы идут личными сообщениями с ACK."),290,small),faint,small);
}
void drawDuelGame(){
 if(!duelOpen||duelOpen->state==Duel::Free){textCenter(160,110,t("The game was removed","Игра удалена"),dim);footer({{"BACK",t("Games","Игры")}});return;}
 const Duel& m=*duelOpen;bool sea=m.kind==Duel::Sea,failed=m.link()==ChatMessage::Failed;
 if(sea){drawSeaTarget();drawSeaSide();}else{drawCheckers();drawCheckersSide();}
 String resign=duelResignArmed?t("Sure?","Сдаться?"):t("Resign","Сдаться");
 if(m.state==Duel::Invited){if(sea)footer({{"OK",t("Accept","Принять")},{"S",t("Shuffle","Перемешать")},{"DEL",t("Decline","Отказать")},{"BACK",t("Games","Игры")}});else footer({{"OK",t("Accept","Принять")},{"DEL",t("Decline","Отказать")},{"BACK",t("Games","Игры")}});}
 else if(m.state==Duel::Inviting)footer({failed?Hint{"R",t("Resend","Повторить")}:Hint{"DEL",t("Cancel","Отменить")},sea?Hint{"S",t("Shuffle","Перемешать")}:Hint{"H",t("Keys","Клавиши")},{"BACK",t("Games","Игры")}});
 else if(m.state==Duel::Over)footer({failed?Hint{"R",t("Resend","Повторить")}:Hint{"N",t("Again","Ещё раз")},{"DEL",duelRemoveArmed?t("Sure?","Удалить?"):t("Remove","Удалить")},{"BACK",t("Games","Игры")}});
 else if(failed)footer({{"R",t("Resend","Повторить")},{"X",resign},{"BACK",t("Games","Игры")}});
 else if(sea&&m.myTurn())footer({{"OK",t("Fire","Огонь")},{"<>",t("Aim","Прицел")},m.canShuffle()?Hint{"S",t("Shuffle","Перемешать")}:Hint{"X",resign},{"H",t("Keys","Клавиши")}});
 else if(sea)footer({{"X",resign},m.canShuffle()?Hint{"S",t("Shuffle","Перемешать")}:Hint{"H",t("Keys","Клавиши")},{"BACK",t("Games","Игры")}});
 else if(duelHeld>=0)footer({{"OK",t("Put","Поставить")},{"BACK",t("Put back","Вернуть")},{"H",t("Keys","Клавиши")}});
 else if(m.myTurn())footer({{"OK",t("Take","Взять")},{"D",m.drawOffer==Duel::OfferedToMe?t("Draw: yes","Ничья: да"):t("Draw","Ничья")},{"X",resign},{"H",t("Keys","Клавиши")}});
 else footer({{"D",t("Draw","Ничья")},{"X",resign},{"H",t("Keys","Клавиши")},{"BACK",t("Games","Игры")}});
 if(duelHelp)drawDuelHelp();
}

void duelEnter(Duel* m){
 duelOpen=m;duelKind=m->kind;duelHeld=-1;duelHelp=false;duelResignArmed=false;duelRemoveArmed=false;
 // Checkers: the cursor on the last move, or in front of my men; sea: on my last shot, or the middle.
 if(m->kind==Duel::Checkers){int s=m->game.plies?m->game.last.to():m->mine==ck::White?9:22;ckCursor=ck::rowOf(s)*8+ck::colOf(s);}
 else{seaCursor=m->aim>=0?m->aim:m->lastOut>=0?m->lastOut:44;seaAimed=false;}
 duels.viewed(*m);change(m->kind==Duel::Sea?SeaBoard:CheckersBoard);
}
void duelBackToList(){Duel* was=duelOpen;change(was&&was->kind==Duel::Sea?SeaList:CheckersList);duelSortRows();for(unsigned i=0;i<duelRowCount;i++)if(duelRows[i]==was)selected=i+1;}
void duelInvite(uint64_t peer){
 Duel* m=duels.invite(peer,duelKind,duelKind==Duel::Checkers?duelPickColor:2);duelEventsSeen=duels.events;if(!m){notice(duels.event,bad);return;}
 duelEnter(m);bool failed=m->link()==ChatMessage::Failed;
 notice(failed?t("Not sent now: will resend","Сейчас не отправлено: повторю"):m->kind==Duel::Sea?t("Invitation sent","Приглашение отправлено"):t("Invitation sent: you play ","Приглашение отправлено: вы играете ")+colorName(m->mine,true),failed?bad:ok);
}
void ckPlay(const ck::Move& mv){
 Duel& m=*duelOpen;
 if(duels.move(m,mv)){duelHeld=-1;hardware.ping(1400,12);if(m.state==Duel::Over)hardware.ping(m.result==Duel::Won?1760:600,200);if(m.link()==ChatMessage::Failed)notice(t("Not sent now: will resend automatically","Сейчас не отправлено: повторю автоматически"),warn);}
 else{hardware.ping(260,60);notice(t("Move refused","Ход не принят"),bad);}
}
void ckAct(){
 Duel& m=*duelOpen;
 if(m.state==Duel::Invited){if(duels.accept(m))notice(t("Accepted: you play ","Принято: вы играете ")+colorName(m.mine,true),ok);return;}
 if(!m.myTurn()){notice(m.state==Duel::Playing?t("Wait for their move","Дождитесь хода соперника"):m.state==Duel::Inviting?t("Waiting for the reply","Ждём ответа на приглашение"):t("The game is over","Игра окончена"),dim);return;}
 int s=ckSquare();if(s<0){notice(t("Men stand on the dark squares","Шашки стоят на тёмных полях"),dim);return;}
 if(s==duelHeld){duelHeld=-1;return;}
 ck::Move list[ck::MaxMoves];unsigned n=ckMoves(list);
 if(duelHeld>=0)for(unsigned i=0;i<n;i++)if(list[i].from()==duelHeld&&list[i].to()==s){ck::Move mv=list[i];ckPlay(mv);return;} // the first of the moves with this start and end
 if(ckCanTake(s)){duelHeld=s;return;}
 bool mine=m.game.pos.own(s)>0;hardware.ping(260,60);
 notice(n&&list[0].capture?t("Capture is compulsory","Бить обязательно"):duelHeld>=0&&!mine?t("Cannot go there","Сюда нельзя"):mine?t("This man cannot move","Эта шашка не может ходить"):t("Choose one of your men","Выберите свою шашку"),warn);
}
void seaAct(){
 Duel& m=*duelOpen;
 if(m.state==Duel::Invited){if(duels.accept(m))notice(t("Accepted: their shot first","Принято: первым стреляет соперник"),ok);return;}
 if(!m.myTurn()){notice(m.state!=Duel::Playing?(m.state==Duel::Inviting?t("Waiting for the reply","Ждём ответа на приглашение"):t("The game is over","Игра окончена")):m.aim>=0?t("Waiting for the answer to your shot","Ждём ответа на ваш выстрел"):t("Wait for their shot","Дождитесь выстрела соперника"),dim);return;}
 if(!m.target.open(seaCursor)){hardware.ping(260,60);notice(t("Already known: aim elsewhere","Уже известно: цельтесь в другое место"),warn);return;}
 if(duels.shoot(m,seaCursor)){hardware.ping(1400,12);char c[4];sea::cellName(seaCursor,c);notice(m.link()==ChatMessage::Failed?t("Not sent now: will resend automatically","Сейчас не отправлено: повторю автоматически"):t("Fire: ","Огонь: ")+c,m.link()==ChatMessage::Failed?warn:info);}
 else notice(t("Shot refused","Выстрел не принят"),bad);
}
bool duelKey(int key){
 if(key>='A'&&key<='Z')key+=32;
 if(page==CheckersList||page==SeaList){
  if(key!=Erase)duelRemoveArmed=false;
  if(key==KeyUp||key==KeyDown){duelSortRows();int total=1+duelRowCount;selected=(selected+(key==KeyUp?-1:1)+total)%total;return true;}
  if(key==Enter||key==KeyHold){duelSortRows();if(!selected){if(!duels.room()){notice(t("Six games in progress: finish one","Идут шесть игр: завершите одну"),warn);return true;}pickReturn=page;change(DuelPick);}else duelEnter(duelRows[selected-1]);return true;}
  if(key==Erase&&selected){duelSortRows();Duel& m=*duelRows[selected-1];if(m.state!=Duel::Over){notice(t("Only finished games can be removed","Удалить можно только оконченную игру"),dim);return true;}
   if(!duelRemoveArmed){duelRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит игру"),warn);return true;}duelRemoveArmed=false;duels.remove(m);notice(t("Game removed","Игра удалена"),ok);return true;}
  if(key==KeyBack){change(Home,true);return true;}
  return key==KeyLeft||key==KeyRight;
 }
 if(page==DuelPick){
  if((key==KeyLeft||key==KeyRight)&&duelKind==Duel::Checkers){duelPickColor=(duelPickColor+(key==KeyLeft?2:1))%3;return true;}
  if(key==KeyUp||key==KeyDown){chessSortPeers();if(chessPeerCount)selected=(selected+(key==KeyUp?-1:1)+chessPeerCount)%chessPeerCount;return true;}
  if(key==Enter||key==KeyHold){chessSortPeers();if(chessPeerCount)duelInvite(meshRadio.peers[chessPeers[constrain(selected,0,int(chessPeerCount)-1)]].id);return true;}
  if(key==KeyBack){change(pickReturn,true);return true;}
  return false;
 }
 // A game page.
 if(duelHelp){duelHelp=false;return true;}
 if(!duelOpen||duelOpen->state==Duel::Free){if(key==KeyBack){change(duelKind==Duel::Sea?SeaList:CheckersList);return true;}return false;}
 Duel& m=*duelOpen;bool sea=m.kind==Duel::Sea;
 if(key!='x')duelResignArmed=false;if(key!=Erase)duelRemoveArmed=false;
 if(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown){
  if(sea){int c=seaCursor%sea::Size,r=seaCursor/sea::Size;if(key==KeyLeft)c=(c+9)%10;if(key==KeyRight)c=(c+1)%10;if(key==KeyUp)r=(r+9)%10;if(key==KeyDown)r=(r+1)%10;seaCursor=r*sea::Size+c;seaAimed=true;}
  else{int c=ckCursor&7,r=ckCursor>>3,dir=ckFlip()?-1:1;if(key==KeyLeft)c=(c-dir+8)%8;if(key==KeyRight)c=(c+dir+8)%8;if(key==KeyUp)r=(r+dir+8)%8;if(key==KeyDown)r=(r-dir+8)%8;ckCursor=r*8+c;}
  return true;}
 if(key==Enter||key==KeyHold||key==' '){if(sea)seaAct();else ckAct();return true;}
 if(key==KeyBack){if(duelHeld>=0)duelHeld=-1;else duelBackToList();return true;}
 if(key=='h'){duelHelp=true;return true;}
 if(key=='r'){if(duels.resend(m))notice(t("Sent again","Отправлено повторно"),ok);else notice(m.sending()?t("Already sending","Уже отправляется"):m.link()==ChatMessage::Failed?t("Radio busy, will resend","Радио занято, повторю автоматически"):t("Nothing to resend","Повторять нечего"),dim);return true;}
 if(key=='s'&&sea){if(duels.shuffle(m)){hardware.ping(1100,20);notice(t("Fleet reshuffled","Флот перемешан"),ok);}else notice(t("Too late: they have shot at it","Поздно: по флоту уже стреляли"),dim);return true;}
 if(key=='d'&&!sea){
  bool theirs=m.drawOffer==Duel::OfferedToMe;
  if(duels.draw(m))notice(theirs?t("Draw accepted","Ничья принята"):t("Draw offered","Ничья предложена"),theirs?ok:info);
  else notice(m.drawOffer==Duel::OfferedByMe?t("Draw already offered","Ничья уже предложена"):t("No game in progress","Игра не идёт"),dim);return true;
 }
 if(key=='x'){
  if(m.state!=Duel::Playing){notice(t("No game in progress","Игра не идёт"),dim);return true;}
  if(!duelResignArmed){duelResignArmed=true;notice(t("X again resigns the game","X ещё раз - сдаться"),warn);return true;}
  duelResignArmed=false;duelHeld=-1;duels.resign(m);hardware.ping(600,200);notice(t("You resigned","Вы сдались"),dim);return true;
 }
 if(key=='n'){
  if(m.state!=Duel::Over){notice(t("Finish this game first","Сначала завершите игру"),dim);return true;}
  duelKind=m.kind;duelPickColor=sea?2:m.mine^1;duelInvite(m.peer);return true;
 }
 if(key==Erase){
  bool fresh=m.state==Duel::Playing&&(sea?!m.shots&&m.aim<0:!m.game.plies);
  if(m.state==Duel::Invited||m.state==Duel::Inviting||fresh){bool mineInvite=m.state!=Duel::Invited;if(duels.decline(m))notice(mineInvite?t("Game cancelled","Игра отменена"):t("Invitation declined","Приглашение отклонено"),dim);return true;}
  if(m.state==Duel::Over){if(!duelRemoveArmed){duelRemoveArmed=true;notice(t("DEL again removes the game","DEL ещё раз удалит игру"),warn);return true;}duelRemoveArmed=false;Duel::Kind k=m.kind;duels.remove(m);duelOpen=nullptr;change(k==Duel::Sea?SeaList:CheckersList);notice(t("Game removed","Игра удалена"),ok);return true;}
  notice(t("Resign with X","Сдаться - клавиша X"),dim);return true;
 }
 return false;
}
// Touch on a game page: checkers - a square is the cursor and OK; sea - a cell of their field aims, the aimed one fires.
bool duelTap(int x,int y){
 if(!duelOpen||duelOpen->state!=Duel::Playing||duelHelp)return false;
 if(page==CheckersBoard&&x>=BoardX&&x<BoardX+Cell*8&&y>=BoardY&&y<BoardY+Cell*8){int c=(x-BoardX)/Cell,r=7-(y-BoardY)/Cell;if(ckFlip()){c=7-c;r=7-r;}ckCursor=r*8+c;uiKey(Enter);return true;}
 if(page==SeaBoard&&x>=SeaX&&x<SeaX+SeaCell*sea::Size&&y>=SeaY&&y<SeaY+SeaCell*sea::Size){int c=(y-SeaY)/SeaCell*sea::Size+(x-SeaX)/SeaCell;
  if(c==seaCursor&&seaAimed)uiKey(Enter);else{seaCursor=c;seaAimed=true;dirty=true;lastInput=millis();}return true;}
 return false;
}
// The node card: "Play" opens the games; each opens its contact choice with this contact selected.
void openPick(Page p,uint64_t peer){chessSortPeers();pickReturn=Node;change(p);for(unsigned i=0;i<chessPeerCount;i++)if(meshRadio.peers[chessPeers[i]].id==peer)selected=i;}
// Locked screen: as chess, the game with unseen news that needs the player or has ended, else the newest waiting one.
const Duel* lockDuel(){
 const Duel* e=duels.eventGame;if(e&&e->state!=Duel::Free&&e->unseen&&(duelNeeds(*e)||e->state==Duel::Over))return e;
 const Duel* best=nullptr;for(auto& g:duels.games)if(g.state!=Duel::Free&&duelNeeds(g)&&(!best||int32_t(g.changedAt-best->changedAt)>0))best=&g;
 return best;
}
void drawLockDuel(int y){
 const Duel& m=*lockDuel();bool sea=m.kind==Duel::Sea;String head,sub;
 panel(40,y,240,40,card,8);ring(40,y,240,40,8,rgb(0x14524b));icon(sea?IcShip:IcCheckers,62,y+20,9,accent,card);
 if(config.lockDetails){head=String(m.name);if(!sea&&m.game.plies&&(m.myTurn()||m.state==Duel::Over))head+=": "+ckLast(m);sub=duelState(m);}
 else{head=sea?t("Sea battle","Морской бой"):t("Checkers","Шашки");sub=duelTileDetail(m.kind);}
 text(80,y+16,fit(head,192,bold),ink,bold);text(80,y+32,fit(sub,192,small),duelStateColor(m),small);
}
void duelTick(){
 if(duels.events!=duelEventsSeen){
  duelEventsSeen=duels.events;
  // News the player has to act on, or a result, lights a dark screen; the locked screen shows it.
  const Duel* e=duels.eventGame;if(e&&e->state!=Duel::Free&&(duelNeeds(*e)||e->state==Duel::Over)){lastInput=millis();if(wakeOnly){wakeOnly=false;hardware.brightness(config.brightness);}}
  // On the open game the news is on its page; elsewhere it is a toast.
  bool here=(page==CheckersBoard||page==SeaBoard)&&duelOpen&&duelOpen==e;if(here)duels.viewed(*duelOpen);
  if(!locked&&duels.event.length())notice(duels.event,here?info:accent);
  if(here&&duelHeld>=0&&!ckCanTake(duelHeld))duelHeld=-1;
 }
 if(duels.dirty){duels.dirty=false;dirty=true;}
}
void duelStatus(JsonDocument& d){
 if(page==CheckersList||page==SeaList||page==DuelPick)d["duel_kind"]=duelKind==Duel::Sea?"sea":"checkers";
 if(page==DuelPick){d["duel_color"]=duelPickColor;return;}
 if((page!=CheckersBoard&&page!=SeaBoard)||!duelOpen)return;char id[5];snprintf(id,sizeof id,"%04X",duelOpen->id);d["duel_game"]=id;
 if(page==CheckersBoard){char sq[3]={char('a'+(ckCursor&7)),char('1'+(ckCursor>>3)),0};d["duel_cursor"]=sq;if(duelHeld>=0){char h[3];ck::squareName(duelHeld,h);d["duel_held"]=h;}}
 else{char c[4];sea::cellName(seaCursor,c);d["duel_cursor"]=c;}
 d["duel_help"]=duelHelp;
}
