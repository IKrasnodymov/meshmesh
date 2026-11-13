// Checkers, sea battle and the app folders on the 128x64 interface (Heltec, GAT562 joystick) and the T114 TFT.
// The other player is faked: their commands go in through duels.receive, what we send is in previewSent.
 {previewOff=false; // the screens above ended with the power off
  auto must=[](bool ok,const char* why){if(!ok){printf("FAIL games %s: %s\n",why,uiStatus().c_str());exit(1);}};
  auto has=[](const char* s){return uiStatus().indexOf(s)>=0;};
#if defined(MM_JOYSTICK)
  const char* prefix="gat562-";const int next=0xb7,enter=13,menu=0xa3,run=13;
#else
  const char* prefix="heltec-";const int next=13,enter=0xa3,menu=0xa3,run=0xa3;
#endif
  auto snap=[&](const char* n){shot((std::string(prefix)+n).c_str());};
  auto home=[&]{for(int i=0;i<11&&(has("\"menu\":true")||has("\"chess_popup\":true"));i++)tick();for(int i=0;i<4&&!has("\"page\":\"home\"");i++)key(0x86);must(has("\"page\":\"home\""),"home");};
  // A page by name: clicks (joystick: right) through the cycle; a hold (OK) enters the games folder when the page is in it.
  auto go=[&](const char* n,bool games){std::string w=std::string("\"page\":\"")+n+"\"";
   for(int i=0;i<40&&!has(w.c_str());i++)key(has("\"page\":\"folder\"")&&games?enter:next);must(has(w.c_str()),n);};
  auto sent=[]{return previewSent.empty()?std::string():previewSent.back().text;};
  auto idOf=[](const Duel* g){char b[5];snprintf(b,sizeof b,"%04X",g->id);return std::string(b);};
  auto say=[&](const Duel* g,const std::string& body){std::string t=std::string(g->kind==Duel::Sea?"\xf0\x9f\x9a\xa2":"\xf0\x9f\x8f\x81")+idOf(g)+" "+body;
   must(duels.receive(peerId(0),"Heltec V4",t.c_str()),"a game command");};
  Duel* found=nullptr;auto latest=[&](Duel::Kind k){found=nullptr;for(auto& g:duels.games)if(g.state!=Duel::Free&&g.kind==k&&(!found||int32_t(g.changedAt-found->changedAt)>0))found=&g;return found;};
  config.apps[0]=0;config.dimAfter=0;config.autoLock=0;config.role=RoleNormal;config.doubleBack=false;
  bool ru=config.lang==LangRu||config.lang==LangUk;(void)ru;
  for(int i=0;i<11;i++)tick();home();

  // The games folder: a screen of the cycle with its apps; a hold (OK) enters it, clicks go through its apps and
  // after the last one back to the folder screen.
  go("folder",false);must(has("\"in_folder\":false"),"the folder screen");snap("folder-games");
#if defined(MM_JOYSTICK)
  key(0xb6);must(has("\"folder_sel\":1"),"down chooses the second app");snap("folder-choose");key(enter);must(has("\"page\":\"checkers\"")&&has("\"in_folder\":true"),"OK opens the chosen app");
  key(0xb4);must(has("\"page\":\"chess\""),"left: the previous app of the folder");key(0xb4);must(has("\"page\":\"folder\""),"left of the first app: the folder screen");
  key(enter);key(0x86);must(has("\"page\":\"folder\""),"Back leaves the folder for its screen");
#else
  key(enter);must(has("\"page\":\"chess\"")&&has("\"in_folder\":true"),"a hold enters the folder: its first app");snap("folder-enter");
  key(next);must(has("\"page\":\"checkers\""),"a click: the next app of the folder");snap("checkers-list");
  key(next);must(has("\"page\":\"sea\""),"then sea battle");snap("sea-list");
  for(int i=0;i<6&&!has("\"page\":\"folder\"");i++)key(next);must(has("\"page\":\"folder\""),"after the last app: the folder screen");
  key(next);must(!has("\"page\":\"folder\"")&&!has("\"in_folder\":true"),"a click on the folder screen: the next screen");
  // Double click back (when switched on): from an app of the folder to the folder screen.
  config.doubleBack=true;home();go("folder",false);key(enter);key(next);must(has("\"page\":\"checkers\""),"checkers again");
  uiKey(13);fakeMillis+=200;uiKey(13);tick();must(has("\"page\":\"folder\""),"double click: the folder screen");config.doubleBack=false;
#endif
  // Hidden folder: its apps leave the cycle; a folder of the user's own, named with spaces.
  strlcpy(config.apps,"chats {Мои_игры sea checkers} nodes -{games chess pet dice}",sizeof config.apps);home();
  go("folder",false);must(has("\"folder\":0"),"the user's folder");snap("folder-own");
  for(int i=0;i<20&&!has("\"page\":\"home\"");i++){key(next);must(!has("\"page\":\"chess\"")&&!has("\"page\":\"pet\""),"a hidden folder is not in the cycle");}
  config.apps[0]=0;home();

  // Invitations from the node card: "Checkers: invite" and "Sea battle: invite" follow "Chess: invite".
  go("nodes",false);must(has("\"page\":\"nodes\""),"nodes");key(menu);
#if defined(MM_JOYSTICK)
  key(0xb6);key(0xb6);snap("node-checkers");key(0xb6);snap("node-sea");
#else
  key(13);key(13);snap("node-checkers");key(13);snap("node-sea");
#endif
  previewSent.clear();key(run);must(has("\"page\":\"sea\"")&&has("\"duel_game\""),"the invitation opens its game");
  Duel* sg=latest(Duel::Sea);must(sg&&sg->state==Duel::Inviting&&sent().find(" new")!=std::string::npos,"a sea battle invitation sent");
  for(int i=0;i<4;i++)tick();snap("sea-invited");
  // A checkers challenge from the node card too, then cancelled from the game menu.
  key(0x86);
#if defined(MM_JOYSTICK)
  key(0x86);
#endif
  home();go("nodes",false);key(menu);
#if defined(MM_JOYSTICK)
  key(0xb6);key(0xb6);
#else
  key(13);key(13);
#endif
  key(run);Duel* declined=latest(Duel::Checkers);must(declined&&declined->state==Duel::Inviting&&has("\"page\":\"checkers\""),"a checkers invitation");snap("checkers-invited");
  key(menu);must(has("\"duel_menu\":true"),"the game menu");snap("checkers-invited-menu");key(run);must(declined->state==Duel::Over&&declined->reason==Duel::Cancelled,"cancelled from the menu");

  // Checkers: they accept (we play White), a few moves each; then our move is a compulsory double capture.
  key(0x86);home();Duel* g=duels.invite(peerId(0),Duel::Checkers,checkers::White);must(g,"invite");say(g,"yes");must(g->state==Duel::Playing,"accepted");
  for(const char* mv:{"a3-b4","h6-g5","b4-c5","d6:b4","c3:a5","e7-d6","d2-c3","f6-e5","e3-f4","g5:e3"}){
   if(g->myTurn()){checkers::Move m;must(g->game.pos.parse(mv,m)&&duels.move(*g,m),mv);duels.delivery(previewSent.back().id,ChatMessage::Delivered);}
   else say(g,std::to_string(g->game.plies+1)+" "+mv);}
  tick();must(has("\"chess_popup\":true")&&has("\"news_kind\":1"),"their move: checkers news");snap("checkers-news");
  key(enter);must(has("\"page\":\"checkers\"")&&has(("\"duel_game\":\""+idOf(g)+"\"").c_str())&&!g->unseen,"the news opens the game, seen");
  std::string f2=std::to_string(checkers::parseSquare("f2"));
#if defined(MM_JOYSTICK)
  must(has("\"duel_cursor\":58"),"the cursor starts on the own bottom row");snap("checkers-board");
  key(0xb5);key(0xb7);key(0xb7);key(0xb7);must(has("\"duel_cursor\":53"),"the cursor on f2");key(13);must(has(("\"duel_held\":"+f2+",").c_str()),"f2 picked");snap("checkers-move");
  key(0xb5);key(0xb5);key(0xb5);key(0xb5);key(13);
#else
  must(has(("\"duel_cursor\":"+f2+",").c_str()),"the only piece that can move: f2 (it must capture)");snap("checkers-board");
  key(0xa3);must(has(("\"duel_held\":"+f2+",").c_str())&&has("\"duel_choice\":0"),"hold picks it, its first move offered");snap("checkers-move");
  key(13);must(has("\"duel_choice\":-1"),"a click: Cancel");snap("checkers-cancel");key(13);must(has("\"duel_choice\":0"),"a click: the move again");
  key(0xa3);
#endif
  must(g->game.plies==11&&sent().find(" 11 f2:d4:f6")!=std::string::npos,"the double capture played and sent");for(int i=0;i<2;i++)tick();snap("checkers-their-move");
  {checkers::Move l[checkers::MaxMoves];unsigned n=g->game.pos.legal(l);must(n>0,"their reply");char t[40];checkers::moveText(l[0],t,sizeof t);say(g,std::string("12 ")+t);}
  tick();must(has("\"chess_popup\":false")&&!g->unseen,"their move on the open board: no popup");
  // The game menu: hold OK (joystick); one button on its move: "Game menu" after the pieces.
#if defined(MM_JOYSTICK)
  key(0xa3);
#else
  for(int i=0;i<40&&!has("\"duel_cursor\":-1,");i++)key(13);snap("checkers-gamemenu");key(0xa3);
#endif
  must(has("\"duel_menu\":true"),"the game menu");snap("checkers-menu");
#if defined(MM_JOYSTICK)
  key(0x86);
#else
  for(int i=0;i<12&&has("\"duel_menu\":true");i++){StaticJsonDocument<1024>d;deserializeJson(d,uiStatus());if((d["duel_act"]|-1)==10){key(0xa3);break;}key(13);} // "< Back to the game"
#endif
  must(!has("\"duel_menu\":true"),"the menu closes");
  // Kings: a later position of another game, drawn as the T114 and the OLED show them (their move).
  {Duel* k=duels.invite(peerId(3),Duel::Checkers,checkers::Black);must(k&&k->peer==peerId(3),"second game");
   auto& b=k->game.pos.board;memset(b,0,sizeof b);for(const char* w:{"d4K","c3","a1","e7k","f6b","g7b","h8k"}){int q=checkers::parseSquare(w);must(q>=0,w);b[q]=w[2]=='K'?checkers::King:w[2]=='k'?-checkers::King:w[2]=='b'?-checkers::Man:checkers::Man;}
   k->game.pos.side=checkers::White;k->state=Duel::Playing;
#if defined(MM_JOYSTICK)
   key(0x86);must(!has("\"duel_game\""),"Back: the list");key(13); // the latest game first
#else
   for(int i=0;i<40&&!has("\"duel_cursor\":-1,");i++)key(13);key(0xa3);for(int i=0;i<12&&has("\"duel_act\":")&&!has("\"duel_act\":9");i++)key(13);key(0xa3); // the game menu: "Game list"
   must(!has("\"duel_game\""),"the list");key(0xa3);key(0xa3); // the list menu: "Open game" (the latest first)
#endif
   must(has(("\"duel_game\":\""+idOf(k)+"\"").c_str()),"the latest game opens");for(int i=0;i<2;i++)tick();snap("checkers-kings");
   k->state=Duel::Over;k->result=Duel::Won;k->reason=Duel::Resigned;duels.remove(*k);tick();must(!has("\"duel_game\""),"a deleted game: the list");}

  // Sea battle (we shoot first): misses, a sunk ship, their hits on us; then a ship hit but not sunk - "finish it".
  sea::Home them;them.fleet.random(12345);
  sg=latest(Duel::Sea);say(sg,"yes");must(sg->state==Duel::Playing&&sg->myTurn(),"they accept: our shot");
  auto water=[&](int skip){for(int c=0;c<sea::Cells;c++)if(them.fleet.shipAt(c)<0&&sg->target.open(c)&&!them.isShot(c)){bool near=false;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int d=sea::cellAt(c%10+dx,c/10+dy);near|=d>=0&&them.fleet.shipAt(d)>=0;}if(!near&&skip--<=0)return c;}return -1;};
  auto fire=[&](int c){must(duels.shoot(*sg,c),"shoot");duels.delivery(previewSent.back().id,ChatMessage::Delivered);say(sg,std::to_string(sg->shots+1)+" = "+sea::answerWord(them.answer(c)));};
  auto incoming=[&](int c){char b[4];sea::cellName(c,b);say(sg,std::to_string(sg->shots+1)+" "+b);};
  // Their misses: our water away from our ships, from the start or the end of the field.
  auto ourWater=[&](bool last){for(int k=0;k<sea::Cells;k++){int c=last?99-k:k;if(sg->home.fleet.shipAt(c)>=0||sg->home.isShot(c))continue;bool near=false;
   for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int d=sea::cellAt(c%10+dx,c/10+dy);near|=d>=0&&sg->home.fleet.shipAt(d)>=0;}if(!near)return c;}return 0;};
  fire(water(3));incoming(sg->home.fleet.cell(0,1));incoming(sg->home.fleet.cell(6,0));incoming(ourWater(false));
  must(sg->myTurn()&&sg->home.shipsLeft()==9,"their two hits (one ship sunk), then a miss: our shot");
  fire(them.fleet.cell(3,0));fire(them.fleet.cell(3,1));must(sg->target.sunkShips()==1,"a two-cell ship sunk");
  fire(water(9));incoming(ourWater(true));
  fire(them.fleet.cell(0,1));must(sg->myTurn()&&sg->target.mark[them.fleet.cell(0,1)]==sea::Hit,"the four-cell ship hit, not sunk: shoot again");
  home();go("sea",true);key(enter);must(has("\"page\":\"sea\"")&&has("\"duel_game\""),"the game opens from the list");
  auto nextToHit=[&]{StaticJsonDocument<1024>d;deserializeJson(d,uiStatus());int cur=d["duel_cursor"]|-9,c=them.fleet.cell(0,1);return cur==c-1||cur==c+1||cur==c-10||cur==c+10;};
#if defined(MM_JOYSTICK)
  must(nextToHit(),"the cursor next to the hit");snap("sea-hunt");key(13);must(sg->aim>=0,"OK fires at the cursor");for(int i=0;i<2;i++)tick();snap("sea-aim");
  say(sg,std::to_string(sg->shots+1)+" = "+sea::answerWord(them.answer(sg->aim)));
  if(sg->myTurn()){key(0xb6);key(0xb6);snap("sea-cursor");}
#else
  must(nextToHit(),"the first choice: a cell next to the hit");snap("sea-hunt");
  // Through the "finish it" cells to the columns: hold fixes one, then its cells; hold fires.
  for(int i=0;i<8;i++){StaticJsonDocument<1024>d;deserializeJson(d,uiStatus());if((d["duel_cursor"]|0)>=100)break;key(13);}
  must(has("\"duel_cursor\":10"),"a column");snap("sea-column");key(0xa3);must(!has("\"duel_column\":-1"),"hold fixes the column");
  key(13);snap("sea-row");{StaticJsonDocument<1024>d;deserializeJson(d,uiStatus());int cell=d["duel_cursor"]|-1;key(0xa3);must(sg->aim==cell,"hold fires at the chosen cell");}
  for(int i=0;i<2;i++)tick();snap("sea-aim");
  say(sg,std::to_string(sg->shots+1)+" = "+sea::answerWord(them.answer(sg->aim)));
#endif
  if(!sg->myTurn())incoming(ourWater(false));
  for(int i=0;i<11;i++)tick();must(has("\"chess_popup\":false")&&has("\"duel_game\""),"news of the open game: no popup");snap("sea-game");
  // The own fleet from the game menu (one button on its shot: "Game menu" after the choices).
#if defined(MM_JOYSTICK)
  key(0xa3);
#else
  if(sg->myTurn())for(int i=0;i<40&&!has("\"duel_cursor\":-1,");i++)key(13);
  key(0xa3);
#endif
  must(has("\"duel_menu\":true"),"the sea game menu");snap("sea-menu");
  for(int i=0;i<12;i++){StaticJsonDocument<1024>d;deserializeJson(d,uiStatus());if((d["duel_act"]|-1)==5)break;key(
#if defined(MM_JOYSTICK)
   0xb6
#else
   13
#endif
  );}
  key(run);must(has("\"duel_fleet\":true"),"My fleet");snap("sea-fleet");
  key(13);must(has("\"duel_fleet\":false"),"a click (OK) returns to their field");
  // The end: their fleet shown (it does not match one answer: the note), the game list.
  {sea::Fleet f=them.fleet;sg->state=Duel::Over;sg->result=Duel::Lost;sg->reason=Duel::Rules;sg->theirs=f;sg->revealed=true;sg->fair=false;sg->aim=-1;for(int i=0;i<2;i++)tick();snap("sea-over");
   sg->fair=true;}
  key(0x86);
#if defined(MM_JOYSTICK)
  key(0x86);
#endif
  home();go("checkers",true);for(int i=0;i<2;i++)tick();snap("checkers-games");
  printf("games checks passed\n");}
