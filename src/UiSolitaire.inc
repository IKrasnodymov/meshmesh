// Klondike solitaire ("Косынка") on the M9 screen. Included by Ui.cpp inside its anonymous
// namespace: it uses the same drawing helpers, fonts and toasts as the other pages.
// Arrows move the cursor, Up/Down in a column choose how many cards to take, OK takes and places.
namespace sol=solitaire;
sol::Game game;
bool gameReady=false,gameWinRecorded=false,gameHelp=false,gameNewArmed=false,gameFinishing=false,gameUnsaved=false;
int8_t gameCursor=sol::Tableau,gameHeld=-1;uint8_t gameDepth=1,gameHeldCount=0,gameNextDraw=1;
uint32_t gameClockAt=0,gameChangedAt=0,gameStepAt=0,gameHintAt=0,gameWonAt=0,gamePlayed=0,gameWins=0,gameBest=0;
sol::Move gameHint{};

const uint16_t felt=rgb(0x0d3527),feltDeep=rgb(0x0a2b20),feltLine=rgb(0x2c6a53),shadow=rgb(0x04150e),cardFace=rgb(0xfbf8f0),cardEdge=rgb(0xb9b2a2);
const uint16_t suitRed=rgb(0xd62f28),suitBlack=rgb(0x1f242b),tintRed=rgb(0xf7ddd6),tintBlack=rgb(0xdfe4ec),gold=rgb(0xe4a82a),goldDeep=rgb(0x9c6b12),backFill=rgb(0x1d3a7a),backLine=rgb(0x3457a6);
constexpr int CardW=42,CardH=48,TopY=25,TabY=78,TabBottom=218,FanStep=20;
const Face cardIndex{u8g2_font_9x15_t_cyrillic,u8g2_font_9x15_tf};
// 9x9 corner suits (spades, hearts, diamonds, clubs); bit 8 is the left column.
const uint16_t suitBits[4][9]={
 {0x010,0x038,0x07c,0x0fe,0x1ff,0x1ff,0x0d6,0x010,0x038},
 {0x0c6,0x1ef,0x1ff,0x1ff,0x0fe,0x07c,0x038,0x010,0x000},
 {0x010,0x038,0x07c,0x0fe,0x1ff,0x0fe,0x07c,0x038,0x010},
 {0x038,0x07c,0x07c,0x0d6,0x1ff,0x1ff,0x0d6,0x010,0x038}};
// 5x6 suits for the compact index of tightly stacked cards; bit 4 is the left column.
const uint8_t miniBits[4][6]={{0x04,0x0e,0x1f,0x1f,0x04,0x0e},{0x0a,0x1f,0x1f,0x0e,0x04,0x00},{0x04,0x0e,0x1f,0x0e,0x04,0x00},{0x0e,0x0e,0x1f,0x15,0x04,0x0e}};
void miniSuit(uint8_t s,int x,int y,uint16_t c){for(int r=0;r<6;r++)for(int b=0;b<5;b++)if(miniBits[s][r]>>(4-b)&1)g().drawPixel(x+b,y+r,c);}
void suitIcon(uint8_t s,int x,int y,uint16_t c){for(int r=0;r<9;r++)for(int b=0;b<9;b++)if(suitBits[s][r]>>(8-b)&1)g().drawPixel(x+b,y+r,c);}
// Large suits from circles and triangles; a is the lobe radius, the shape is about 4a wide.
void bigSuit(uint8_t s,int cx,int cy,int a,uint16_t c){
 auto& d=g();
 switch(s){
 case 0:d.fillTriangle(cx,cy-2*a-1,cx-2*a,cy+a/2,cx+2*a,cy+a/2,c);d.fillCircle(cx-a,cy+a/2,a,c);d.fillCircle(cx+a,cy+a/2,a,c);d.fillTriangle(cx,cy+a/2,cx-a,cy+2*a+1,cx+a,cy+2*a+1,c);break;
 case 1:d.fillCircle(cx-a,cy-a/2,a,c);d.fillCircle(cx+a,cy-a/2,a,c);d.fillTriangle(cx-2*a,cy-a/2+1,cx+2*a,cy-a/2+1,cx,cy+2*a,c);break;
 case 2:d.fillTriangle(cx,cy-2*a-1,cx-a*3/2-1,cy,cx+a*3/2+1,cy,c);d.fillTriangle(cx,cy+2*a+1,cx-a*3/2-1,cy,cx+a*3/2+1,cy,c);break;
 case 3:{int r=a*9/10;d.fillCircle(cx,cy-a,r,c);d.fillCircle(cx-a-1,cy+a/2,r,c);d.fillCircle(cx+a+1,cy+a/2,r,c);d.fillCircle(cx,cy,max(1,a/2),c);d.fillTriangle(cx,cy,cx-a,cy+2*a+1,cx+a,cy+2*a+1,c);break;}
 }
}
String rankName(uint8_t r){static const char* en[]={"A","2","3","4","5","6","7","8","9","10","J","Q","K"};static const char* ru[]={"Т","2","3","4","5","6","7","8","9","10","В","Д","К"};return config.lang==LangRu||config.lang==LangUk?ru[r]:en[r];}
String playTime(uint32_t s){char b[12];if(s>=3600)snprintf(b,sizeof b,"%lu:%02lu:%02lu",(unsigned long)(s/3600),(unsigned long)(s/60%60),(unsigned long)(s%60));else snprintf(b,sizeof b,"%lu:%02lu",(unsigned long)(s/60),(unsigned long)(s%60));return b;}

// Screen positions: stock, waste, a gap and the foundations on top; seven columns below.
int pileColumn(uint8_t p){return p==sol::Stock?0:p==sol::Waste?1:sol::isFoundation(p)?3+p-sol::Foundation:p-sol::Tableau;}
int pileX(uint8_t p){return 4+45*pileColumn(p);}
// Long columns squeeze their spacing so the last card always fits above the footer.
void columnSteps(const sol::Pile& p,int& down,int& face){
 int d=0;while(d<p.n&&!sol::up(p.c[d]))d++;int u=p.n-d,room=TabBottom-TabY-CardH;down=5;face=16;
 if(u>1&&d*down+(u-1)*face>room){down=3;face=max(6,(room-d*down)/(u-1));}
}
int cardY(uint8_t p,int i){if(!sol::isTableau(p))return TopY;const sol::Pile& s=game.pile[p];int down,face,y=TabY;columnSteps(s,down,face);for(int k=0;k<i;k++)y+=sol::up(s.c[k])?face:down;return y;}
int wasteX(int i,int shown){return pileX(sol::Waste)+FanStep*(i-(game.pile[sol::Waste].n-shown));} // draw-3 fan

// A card: soft shadow on the felt, rounded paper, thin edge. The shadow also separates stacked cards.
void paper(int x,int y,uint16_t fill){auto& d=g();d.fillRoundRect(x+1,y+2,CardW,CardH-1,4,shadow);d.fillRoundRect(x,y,CardW,CardH,4,fill);d.drawRoundRect(x,y,CardW,CardH,4,cardEdge);}
void cardBack(int x,int y){
 auto& d=g();paper(x,y,cardFace);d.fillRoundRect(x+2,y+2,CardW-4,CardH-4,2,backFill); // navy shows even in 3 px strips
 for(int yy=y+3;yy<y+CardH-3;yy++)for(int xx=x+3;xx<x+CardW-3;xx++)if((xx-x+yy-y)%6==0||(xx-x-(yy-y)+60)%6==0)d.drawPixel(xx,yy,backLine); // lattice
 int cx=x+CardW/2,cy=y+CardH/2;d.fillTriangle(cx,cy-10,cx-7,cy,cx+7,cy,backFill);d.fillTriangle(cx,cy+10,cx-7,cy,cx+7,cy,backFill); // emblem
 d.fillTriangle(cx,cy-8,cx-5,cy,cx+5,cy,gold);d.fillTriangle(cx,cy+8,cx-5,cy,cx+5,cy,gold);d.fillTriangle(cx,cy-4,cx-2,cy,cx+2,cy,backFill);d.fillTriangle(cx,cy+4,cx-2,cy,cx+2,cy,backFill);
}
// Court cards: a tinted portrait with a crown (king), a tiara (queen) or a cap with a plume (jack).
void portrait(int x,int y,uint8_t r,uint16_t ink,uint16_t tint){
 auto& d=g();int px=x+13,py=y+16,cx=px+12;d.fillRoundRect(px,py,25,29,3,tint);d.drawRoundRect(px,py,25,29,3,ink);
 int top=py+3;
 if(r==12){d.fillRect(cx-6,top+5,13,4,gold);d.fillTriangle(cx-6,top+5,cx-6,top,cx-2,top+5,gold);d.fillTriangle(cx-3,top+5,cx,top-1,cx+3,top+5,gold);d.fillTriangle(cx+2,top+5,cx+6,top,cx+6,top+5,gold);d.drawFastHLine(cx-6,top+8,13,goldDeep);d.drawPixel(cx,top+6,suitRed);d.drawPixel(cx-4,top+6,suitRed);d.drawPixel(cx+4,top+6,suitRed);}
 else if(r==11){d.fillRoundRect(cx-6,top+5,13,3,1,gold);for(int k=-1;k<=1;k++)d.fillCircle(cx+k*5,top+3-(k==0),k==0?2:1,gold);d.drawFastHLine(cx-5,top+7,11,goldDeep);}
 else{d.fillCircle(cx,top+6,5,gold);d.fillRect(cx-6,top+6,13,3,gold);d.drawFastHLine(cx-6,top+8,13,goldDeep);d.drawLine(cx+3,top+2,cx+7,top-2,ink);d.drawLine(cx+4,top+2,cx+8,top-1,ink);}
 textCenter(cx+1,py+27,rankName(r),ink,big);
}
// shown: how much of the card is visible above the next one in a column; a squeezed column gets a smaller index.
void cardFaceAt(int x,int y,uint8_t c,int shown=CardH){
 uint8_t r=sol::rank(c),s=sol::suit(c);bool isRed=sol::red(c);uint16_t ink=isRed?suitRed:suitBlack;
 paper(x,y,cardFace);
 if(shown<11){String name=rankName(r);text(x+3,y+8,name,ink,small);text(x+4,y+8,name,ink,small);miniSuit(s,x+CardW-9,y+2,ink);return;}
 if(shown<15){String name=rankName(r);text(x+3,y+11,name,ink,bold);text(x+4,y+11,name,ink,bold);suitIcon(s,x+CardW-12,y+2,ink);return;}
 // Index: bold rank and the suit in the corner (visible in stacked columns), the suit again under the rank (visible in the fan).
 String name=rankName(r);if(r==9){text(x+1,y+14,"1",ink,cardIndex);text(x+2,y+14,"1",ink,cardIndex);text(x+8,y+14,"0",ink,cardIndex);text(x+9,y+14,"0",ink,cardIndex);}else{text(x+3,y+14,name,ink,cardIndex);text(x+4,y+14,name,ink,cardIndex);}
 suitIcon(s,x+CardW-12,y+3,ink);suitIcon(s,x+4,y+18,ink);
 if(r>=10)portrait(x,y,r,ink,isRed?tintRed:tintBlack);
 else if(r==0){bigSuit(s,x+25,y+30,6,ink);}
 else bigSuit(s,x+25,y+31,4,ink);
}
void drawCardAt(int x,int y,uint8_t c,int shown=CardH){if(sol::up(c))cardFaceAt(x,y,c,shown);else cardBack(x,y);}
void slot(int x,int y,const String& mark){auto& d=g();d.fillRoundRect(x,y,CardW,CardH,4,feltDeep);d.drawRoundRect(x,y,CardW,CardH,4,feltLine);if(mark.length())textCenter(x+CardW/2,y+CardH/2+7,mark,feltLine,big);}
// The cards of pile p from index first to the top, as one rectangle.
void pileRect(uint8_t p,int first,int& x,int& y,int& w,int& h){
 const sol::Pile& s=game.pile[p];x=pileX(p);w=CardW;y=cardY(p,max(0,first));h=s.n?cardY(p,s.n-1)+CardH-y:CardH;
 if(p==sol::Waste&&s.n&&game.draw==3){int shown=min<int>(3,s.n);x=wasteX(s.n-1,shown);}
}
void mark(uint8_t p,int count,uint16_t color){int x,y,w,h;pileRect(p,game.pile[p].n-max(1,count),x,y,w,h);ring(x-2,y-2,w+4,h+4,6,color);}

// Winning cascade: the foundation cards leave one by one and bounce, leaving a trail.
void drawCascade(uint32_t age){
 for(int k=0;k<52;k++){
  int32_t life=int32_t(age)-k*260;if(life<0)break;uint8_t f=sol::Foundation+k%4;uint8_t c=(k%4)*13+12-k/4;
  float x=pileX(f),y=TopY,vx=(k*7%5-2.5f)*1.6f;if(fabsf(vx)<1.2f)vx=vx<0?-1.5f:1.5f;float vy=-2-(k*3%4);
  int steps=life/25;
  for(int i=0;i<=steps&&i<400;i++){
   if(i>=steps-40&&(steps-i)%5==0&&x>-CardW&&x<320){cardFaceAt(int(x),int(y),c|sol::FaceUp);}
   x+=vx;vy+=.5f;y+=vy;if(y>TabBottom-CardH){y=TabBottom-CardH;vy=-vy*.72f;}
  }
 }
}
void gameStatsLoad(){Preferences p;if(p.begin("meshmesh-game",true)){gamePlayed=p.getUInt("played",0);gameWins=p.getUInt("won",0);gameBest=p.getUInt("best",0);gameNextDraw=p.getUChar("draw",1)==3?3:1;p.end();}}
void gameStatsSave(){Preferences p;if(p.begin("meshmesh-game",false)){p.putUInt("played",gamePlayed);p.putUInt("won",gameWins);p.putUInt("best",gameBest);p.putUChar("draw",gameNextDraw);p.end();}}
void gameSave(){if(!gameUnsaved)return;uint8_t b[sol::SavedSize];game.save(b);Preferences p;if(p.begin("meshmesh-game",false)){gameUnsaved=p.putBytes("state",b,sizeof b)!=sizeof b;p.end();}}
void gameDeal(bool store=true){game.deal(uint32_t(random(0x7fffffff))^millis(),gameNextDraw);gameCursor=sol::Tableau;gameDepth=1;gameHeld=-1;gameFinishing=false;gameNewArmed=false;gameWonAt=0;gameWinRecorded=false;gameUnsaved=true;if(store)gameSave();}
void gameLoad(){
 if(gameReady)return;gameReady=true;gameStatsLoad();
 uint8_t b[sol::SavedSize];size_t n=0;Preferences p;if(p.begin("meshmesh-game",true)){if(p.getBytesLength("state")==sizeof b)n=p.getBytes("state",b,sizeof b);p.end();}
 if(!n||!game.load(b,n))gameDeal(false); // no game yet, or a damaged record; saved once played
 gameWinRecorded=game.won();gameWonAt=game.won()?(millis()-20000)|1:0; // a finished game shows its result, without the cascade
}
void gameOpen(){gameLoad();gameClockAt=millis();gameHeld=-1;gameHelp=false;gameNewArmed=false;if(game.finishable()){gameFinishing=true;gameStepAt=millis();}}
void gameLeave(){gameHeld=-1;gameFinishing=false;gameSave();}
String gameTitle(){return t("Solitaire","Косынка")+(game.draw==3?t(" · draw 3"," · по 3"):String());}
String gameTileDetail(){gameLoad();if(game.counted&&!game.won())return count(game.moves,"move","moves","ход","хода","ходов")+" · "+playTime(game.seconds);if(gamePlayed)return t("Won ","Побед ")+String(gameWins)+t(" of "," из ")+String(gamePlayed);return t("Klondike","Классика");}
bool gameAnimating(){return gameFinishing||(gameWonAt&&millis()-gameWonAt<16000);}

// After every action: statistics, the winning moment, automatic finish and a delayed save.
void gameAfter(bool wasCounted){
 if(game.counted&&!wasCounted){gamePlayed++;gameStatsSave();}
 if(game.won()&&!gameWonAt){gameWonAt=millis();gameFinishing=false;if(!gameWinRecorded){gameWinRecorded=true;gameWins++;if(!gameBest||game.seconds<gameBest)gameBest=game.seconds;gameStatsSave();}hardware.ping(1760,200);gameUnsaved=true;gameSave();return;}
 if(!gameFinishing&&game.finishable()){gameFinishing=true;gameStepAt=millis();notice(t("All cards open: collecting","Все карты открыты: собираю"),ok);}
 gameUnsaved=true;gameChangedAt=millis();gameNewArmed=false;
}
uint8_t gamePick(){uint8_t k=game.movable(gameCursor);return sol::isTableau(gameCursor)?min(gameDepth,k):min<uint8_t>(k,1);}
void gameMoveCursor(int key){
 static const uint8_t topRow[]={sol::Stock,sol::Waste,sol::Foundation,sol::Foundation+1,sol::Foundation+2,sol::Foundation+3};
 bool tableau=sol::isTableau(gameCursor);int col=pileColumn(gameCursor);
 if(key==KeyLeft||key==KeyRight){int dir=key==KeyLeft?-1:1;if(tableau)gameCursor=sol::Tableau+(col+dir+7)%7;else{int i=0;while(topRow[i]!=gameCursor)i++;gameCursor=topRow[(i+dir+6)%6];}gameDepth=1;return;}
 if(key==KeyUp){if(!tableau)return;if(gameHeld<0&&gameDepth<game.movable(gameCursor)){gameDepth++;return;}gameCursor=col==0?sol::Stock:col<=2?sol::Waste:sol::Foundation+col-3;gameDepth=1;return;}
 if(key==KeyDown){if(tableau){if(gameDepth>1)gameDepth--;return;}gameCursor=sol::Tableau+col;gameDepth=1;}
}
void gameAct(bool automatic){
 bool was=game.counted;
 if(gameHeld>=0){
  int to=automatic?game.target(gameHeld,gameHeldCount):gameCursor;
  if(to==gameHeld&&!automatic){gameHeld=-1;return;}
  if(to>=0&&game.move(gameHeld,gameHeldCount,to)){gameHeld=-1;gameDepth=1;hardware.ping(1400,12);gameAfter(was);}
  else{hardware.ping(260,60);notice(t("Cannot go there","Сюда нельзя"),warn);}
  return;
 }
 if(gameCursor==sol::Stock){if(game.turnStock()){hardware.ping(900,8);gameAfter(was);}else notice(t("Stock is empty","Колода пуста"),dim);return;}
 uint8_t n=gamePick();if(!n){notice(t("No cards to take here","Здесь нечего взять"),dim);return;}
 if(!automatic){gameHeld=gameCursor;gameHeldCount=n;return;}
 int to=game.target(gameCursor,n);
 if(to>=0&&game.move(gameCursor,n,to)){gameDepth=1;hardware.ping(1400,12);gameAfter(was);}else{hardware.ping(260,60);notice(t("No place for these cards","Этим картам некуда лечь"),warn);}
}
void gameNew(){
 if(game.counted&&!game.won()&&!gameNewArmed){gameNewArmed=true;notice(t("N again: new game (this one counts as lost)","N ещё раз: новая партия, эта будет проиграна"),warn);return;}
 gameDeal();notice(game.draw==3?t("New game: draw 3","Новая партия: сдача по 3"):t("New game","Новая партия"),ok);
}
bool gameKey(int key){
 if(key>='A'&&key<='Z')key+=32;
 if(gameHelp){gameHelp=false;return true;}
 if(key!='n')gameNewArmed=false;
 if(key==KeyBack){if(gameHeld>=0)gameHeld=-1;else change(Home);return true;}
 if(gameFinishing&&key!=Erase&&key!='u')return true; // cards are flying to the foundations
 if(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown){gameMoveCursor(key);return true;}
 if(key==Enter){gameAct(false);return true;}
 if(key==' '||key==KeyHold){gameAct(true);return true;}
 if(key==Erase||key=='u'){gameFinishing=false;gameHeld=-1;if(game.undo()){gameWonAt=0;gameDepth=1;gameUnsaved=true;gameChangedAt=millis();}else notice(t("Nothing to undo","Нечего отменять"),dim);return true;}
 if(key=='p'){gameHeld=-1;if(game.hint(gameHint)){gameHintAt=millis();gameCursor=gameHint.from;gameDepth=gameHint.count;if(gameHint.from==sol::Stock)notice(game.pile[sol::Stock].n?t("Hint: turn the stock","Подсказка: сдайте из колоды"):t("Hint: turn the waste over","Подсказка: переверните колоду"),info);}else notice(t("No useful moves: undo or start again","Полезных ходов нет: отмените или начните заново"),warn);return true;}
 if(key=='a'){if(game.won())return true;gameHeld=-1;gameFinishing=true;gameStepAt=millis();return true;}
 if(key=='n'){gameNew();return true;}
 if(key=='d'){gameNextDraw=gameNextDraw==1?3:1;gameStatsSave();if(!game.counted){gameDeal();notice(gameNextDraw==3?t("Draw 3: dealt again","Сдача по 3: карты пересданы"):t("Draw 1: dealt again","Сдача по 1: карты пересданы"),ok);}else notice(gameNextDraw==3?t("Next game: draw 3","Следующая партия: сдача по 3"):t("Next game: draw 1","Следующая партия: сдача по 1"),info);return true;}
 if(key=='h'){gameHelp=true;return true;}
 return false;
}
void gameTick(uint32_t now){
 // Play time runs while the game is on screen, started, not won and the screen is awake.
 if(!locked&&!wakeOnly&&game.counted&&!game.won()){while(now-gameClockAt>=1000){game.seconds++;gameClockAt+=1000;dirty=true;}}else gameClockAt=now;
 if(gameFinishing&&now-gameStepAt>=110){gameStepAt=now;bool was=game.counted;if(game.collect())gameAfter(was);else gameFinishing=false;dirty=true;}
 if(gameUnsaved&&now-gameChangedAt>=20000){gameChangedAt=now;gameSave();} // NVS: at most one write per 20 s of play
 if(gameHintAt&&now-gameHintAt>=2500){gameHintAt=0;dirty=true;}
}
void gameStatus(JsonDocument& d){
 d["game_cursor"]=gameCursor;d["game_depth"]=gamePick();d["game_held"]=gameHeld;d["game_held_count"]=gameHeld>=0?gameHeldCount:0;
 d["game_moves"]=game.moves;d["game_score"]=game.score;d["game_seconds"]=game.seconds;d["game_draw"]=game.draw;d["game_foundation"]=game.onFoundations();
 d["game_stock"]=game.pile[sol::Stock].n;d["game_waste"]=game.pile[sol::Waste].n;d["game_won"]=game.won();d["game_undo"]=game.undoCount();
}
void drawGameHelp(){
 uint16_t* px=g().getBuffer();for(int i=0;i<320*240;i++)px[i]=(px[i]>>1)&0x7bef;
 panel(6,24,308,194,cardHi,10);g().drawRoundRect(6,24,308,194,10,line);text(16,42,t("Solitaire keys","Клавиши «Косынки»"),ink,bold);
 Hint keys[]={{"<>",t("Choose a pile","Выбор стопки")},{"^v",t("In a column: how many cards","В столбце: сколько карт взять")},{"OK",t("Take / put; on the stock: turn","Взять / положить; на колоде: сдать")},
  {"SPC",t("Move automatically (or hold OK)","Переложить само (или удерж. OK)")},{"DEL",t("Undo (also U)","Отменить ход (или U)")},{"P",t("Hint","Подсказка")},{"A",t("Collect to foundations","Собрать в дома")},
  {"N",t("New game","Новая партия")},{"D",game.draw==3||gameNextDraw==3?t("Draw 1 / 3 (now: 3)","Сдача по 1 / 3 (сейчас: по 3)"):t("Draw 1 / 3 (now: 1)","Сдача по 1 / 3 (сейчас: по 1)")}};
 for(int i=0;i<9;i++){int y=50+i*16;g().fillRoundRect(16,y,26,13,3,line);if(!strcmp(keys[i].key,"<>")){tri(24,y+6,3,3,ink);tri(33,y+6,1,3,ink);}else if(!strcmp(keys[i].key,"^v")){tri(24,y+7,0,3,ink);tri(33,y+5,2,3,ink);}else textCenter(29,y+10,keys[i].key,ink,small);text(50,y+11,keys[i].action,i%2?dim:ink);}
 text(16,207,fit(t("Played ","Сыграно ")+String(gamePlayed)+t(" · won "," · побед ")+String(gameWins)+(gameBest?t(" · best "," · лучшее ")+playTime(gameBest):String())+t(" · score "," · очки ")+String(game.score),290,small),faint,small);
}
void drawGame(){
 auto& d=g();d.fillRect(0,21,320,199,felt);
 // Stock, waste and foundations.
 const sol::Pile& stock=game.pile[sol::Stock];const sol::Pile& waste=game.pile[sol::Waste];
 if(stock.n){cardBack(pileX(sol::Stock),TopY);String n=String(stock.n);int w=measure(n,small)+6;d.fillRoundRect(pileX(sol::Stock)+CardW/2-w/2,TopY+CardH-13,w,10,3,bar);textCenter(pileX(sol::Stock)+CardW/2,TopY+CardH-5,n,ink,small);}
 else{int x=pileX(sol::Stock);slot(x,TopY,"");if(waste.n){d.drawCircle(x+21,TopY+24,9,feltLine);d.drawCircle(x+21,TopY+24,8,feltLine);tri(x+30,TopY+22,2,4,feltLine);}}
 if(waste.n){int shown=game.draw==3?min<int>(3,waste.n):1;for(int i=waste.n-shown;i<waste.n;i++)cardFaceAt(wasteX(i,shown),TopY,waste.c[i]);}else slot(pileX(sol::Waste),TopY,"");
 for(uint8_t f=sol::Foundation;f<sol::Tableau;f++){const sol::Pile& p=game.pile[f];if(p.n)cardFaceAt(pileX(f),TopY,p.top());else slot(pileX(f),TopY,rankName(0));}
 // Tableau.
 for(uint8_t p=sol::Tableau;p<sol::PileCount;p++){const sol::Pile& s=game.pile[p];if(!s.n){slot(pileX(p),TabY,rankName(12));continue;}for(int i=0;i<s.n;i++){int y=cardY(p,i);drawCardAt(pileX(p),y,s.c[i],i+1<s.n?cardY(p,i+1)-y:CardH);}}
 if(gameWonAt){
  uint32_t age=millis()-gameWonAt;if(age<16000)drawCascade(age);
  panel(40,88,240,92,cardHi,10);d.drawRoundRect(40,88,240,92,10,ok);textCenter(160,114,t("You won!","Победа!"),ok,big);
  textCenter(160,134,playTime(game.seconds)+" · "+count(game.moves,"move","moves","ход","хода","ходов")+" · "+count(game.score,"point","points","очко","очка","очков"),ink);
  textCenter(160,152,t("Won ","Побед ")+String(gameWins)+t(" of "," из ")+String(gamePlayed)+(gameBest?t(" · best "," · лучшее ")+playTime(gameBest):String()),dim,small);
  textCenter(160,170,t("N: new game   BACK: menu","N: новая партия   BACK: меню"),dim,small);
  footer({{"N",t("New game","Новая партия")},{"DEL",t("Undo","Отмена")},{"BACK",t("Menu","Меню")}});return;
 }
 // Hint, held cards and cursor.
 if(gameHintAt){mark(gameHint.from,gameHint.count,info);if(gameHint.from!=sol::Stock)mark(gameHint.to,1,ok);}
 if(gameHeld>=0)mark(gameHeld,gameHeldCount,warn);
 if(!gameFinishing)mark(gameCursor,gameHeld>=0?1:max<int>(1,gamePick()),accent);
 if(gameHeld>=0){int x,y,w,h;pileRect(gameCursor,game.pile[gameCursor].n-1,x,y,w,h);tri(x+w/2,max(24,y-5),2,5,warn);}
 String stats=count(game.moves,"move","moves","ход","хода","ходов")+" · "+playTime(game.seconds);
 if(gameHeld>=0)footer({{"OK",t("Put","Положить")},{"SPC",t("Auto","Само")},{"BACK",t("Cancel","Отмена")}});
 else footer({{"OK",gameCursor==sol::Stock?t("Turn","Сдать"):t("Take","Взять")},{"SPC",t("Auto","Само")},{"H",t("Keys","Клавиши")}});
 d.fillRect(314-measure(stats,small)-8,222,measure(stats,small)+10,18,bar);textRight(314,234,stats,dim,small);
 if(gameHelp)drawGameHelp();
}
