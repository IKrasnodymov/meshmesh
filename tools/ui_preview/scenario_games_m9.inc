// Checkers, sea battle and the app folders on the M9 interface (and the T-Deck touch checks). The other player is
// faked: its commands go straight into duels.receive(), what we send is in previewSent.
{
 auto must=[](bool ok,const char* why){if(!ok){printf("FAIL games %s: %s\n",why,uiStatus().c_str());exit(1);}};
 auto has=[](const char* part){return uiStatus().indexOf(part)>=0;};
 auto field=[](const char* name){static String v;StaticJsonDocument<2048>d;deserializeJson(d,uiStatus());v=d[name]|"";return v;};
 // A command of the other player: "🏁ID ..." or "🚢ID ...".
 auto say=[](const Duel* g,const char* body){char t[96];snprintf(t,sizeof t,"%s%04X %s",g->kind==Duel::Sea?"\xf0\x9f\x9a\xa2":"\xf0\x9f\x8f\x81",g->id,body);
  if(!duels.receive(g->peer,g->name,t)){printf("FAIL games: not a game command %s\n",t);exit(1);}};
 auto peer=[](int i)->Peer&{for(unsigned k=0;k<meshRadio.peerCount;k++)if(meshRadio.peers[k].id==peerId(i))return meshRadio.peers[k];printf("FAIL games: no contact %d\n",i);exit(1);};
 // The row of a contact in a game's contact choice (chat contacts, the most recently heard first, as the screen sorts them).
 auto pickRow=[](uint64_t id){std::vector<const Peer*> v;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].type==1)v.push_back(&meshRadio.peers[i]);uint32_t now=millis();
  std::stable_sort(v.begin(),v.end(),[&](const Peer* a,const Peer* b){if(a->heard!=b->heard)return a->heard;return a->heard&&now-a->seen<now-b->seen;});
  for(size_t i=0;i<v.size();i++)if(v[i]->id==id)return int(i);return -1;};
 // Nodes: the card of a contact (the list follows the node; down until it is selected).
 auto nodeCard=[&](int i){key(0x82);key(0xb7);key(0xb7);key(13);String want=String("\"selected_node\":\"")+meshRadio.idText(peerId(i))+"\"";for(int k=0;k<12&&!has(want.c_str());k++)key(0xb6);must(has(want.c_str()),"the contact in the node list");key(13);};(void)nodeCard;
 auto lastSent=[](){return previewSent.empty()?std::string():previewSent.back().text;};
 previewOff=false; // the M9 scenario turned the board off just before
 config.dimAfter=0;config.autoLock=0;config.apps[0]=0;bool ru=config.lang==LangRu||config.lang==LangUk;(void)ru;
 for(auto& g:duels.games)g=Duel();
 // The game on the screen (uiStatus "duel_game").
 auto shown=[&]()->Duel*{String id=field("duel_game");return id.length()?duels.find(uint16_t(strtoul(id.c_str(),nullptr,16))):nullptr;};
 // The cursor of a game page to a square or cell by the arrow keys (checkers: White up, Black down; sea: rows go down).
 auto cursorTo=[&](const char* want){
  for(int i=0;i<24;i++){String c=field("duel_cursor");if(c==want)return;bool sea=has("\"page\":\"sea_board\"");Duel* g=shown();bool black=!sea&&g&&g->mine==checkers::Black;
   int dc=want[0]-c[0],dr=atoi(want+1)-atoi(c.c_str()+1);if(black){dc=-dc;dr=-dr;}
   key(dc>0?0xb7:dc<0?0xb4:(dr>0)!=sea?0xb5:0xb6);}
  must(field("duel_cursor")==want,"cursor");};
 // A game opened from its list, row by row (the list puts the games that need the player first).
 auto openGame=[&](const Duel* g){char want[24];snprintf(want,sizeof want,"\"duel_game\":\"%04X\"",g->id);
  for(int row=1;row<=int(DuelNet::Max);row++){tile(g->kind==Duel::Sea?"sea":"checkers");key(13);for(int k=0;k<row;k++)key(0xb6);key(13);if(has(want))return;}
  must(false,"open the game from the list");};
#if defined(MM_BOARD_TDECK)
 auto expect=[&](const char* what,const char* part){must(has(part),what);};
 auto touch=[](char gesture,int x,int y){uiTouch(gesture,x,y);tick();};
 // Home: the games folder's tile is selected by the keys and tapped; in the folder a tap opens Checkers.
 key(0x82);{uint8_t o[AppEntriesMax];unsigned n=appsShown(o);int at=-1;for(unsigned i=0;i<n;i++)if(o[i]&AppFolder)at=i;must(at>=0,"games folder tile");
  for(int k=0;k<at/3;k++)key(0xb6);for(int k=0;k<at%3;k++)key(0xb7);int first=max(0,at/3-1);touch('t',8+(at%3)*104+48,71+(at/3-first)*74+34);}
 expect("folder by a tap","\"page\":\"folder\"");touch('t',8+104+48,71+34);expect("checkers tile","\"page\":\"checkers\"");
 touch('t',160,24+20);expect("new game row","\"page\":\"duel_pick\"");touch('t',160,35);expect("colour row","\"duel_color\":0");
 touch('t',160,60+48+20);expect("first tap on a contact selects","\"selected\":1");expect("no invitation yet","\"page\":\"duel_pick\"");
 touch('t',160,60+20);touch('t',160,60+20);expect("second tap invites","\"page\":\"checkers_board\"");
 {Duel* g=shown();must(g&&g->mine==checkers::White&&g->state==Duel::Inviting,"white invitation");say(g,"yes");tick();
  touch('t',4+2*24+12,25+5*24+12);expect("tap c3 takes the man","\"duel_held\":\"c3\"");touch('t',4+3*24+12,25+4*24+12);must(g->game.plies==1,"c3-d4 by taps");
  say(g,"2 f6-e5");tick();touch('t',4+3*24+12,25+4*24+12);touch('t',4+5*24+12,25+2*24+12);must(g->game.plies==3&&g->game.last.capture,"capture d4:f6 by taps");shot("tdeck-checkers");
  touch('t',160,10);expect("board back to the list","\"page\":\"checkers\"");touch('t',160,10);expect("list back to the folder","\"page\":\"folder\"");}
 // Sea battle: the tile, a contact, then a tap aims and a second tap on the same cell fires.
 touch('t',8+2*104+48,71+34);expect("sea tile","\"page\":\"sea\"");touch('t',160,24+20);expect("sea contact choice","\"page\":\"duel_pick\"");
 touch('t',160,60+20);touch('t',160,60+20);expect("sea invitation","\"page\":\"sea_board\"");
 {Duel* g=shown();must(g&&g->kind==Duel::Sea,"sea game");say(g,"yes");tick();touch('t',18+4*17+8,35+4*17+8);expect("tap aims","\"duel_cursor\":\"e5\"");must(g->aim<0,"one tap only aims");
  touch('t',18+4*17+8,35+4*17+8);must(g->aim==44&&lastSent().find(" 1 e5")!=std::string::npos,"second tap fires");shot("tdeck-sea");
  say(g,"1 = miss");tick();touch('t',160,10);touch('t',160,10);expect("sea back to the folder","\"page\":\"folder\"");}
 // The node card: Play, then a game's row opens its contact choice with this contact.
 nodeCard(0);expect("node card","\"page\":\"node\"");touch('t',8+97+6+48,160+12); // Play: the second of five buttons in two rows
 expect("play menu","\"play_menu\":true");touch('t',160,76+36+16);expect("checkers from the card","\"page\":\"duel_pick\"");{char want[24];snprintf(want,sizeof want,"\"selected\":%d,",pickRow(peerId(0)));expect("this contact",want);}
 touch('t',160,10);expect("back to the card","\"page\":\"node\"");key(0x82);
 for(auto& g:duels.games)g=Duel();
#else
 // Home: the games folder (its badge sums the games waiting), the folder page, back with the folder's tile selected.
 key(0x82);uint8_t top[AppEntriesMax];unsigned topN=appsShown(top);int games=-1;for(unsigned i=0;i<topN;i++)if(top[i]&AppFolder)games=i;must(games>=0,"games folder on the home grid");
 for(int k=0;k<games/3;k++)key(0xb6);for(int k=0;k<games%3;k++)key(0xb7);shot("m9-home-folder");
 key(13);must(has("\"page\":\"folder\""),"folder opens");shot("m9-folder");
 key(0x86);{char want[24];snprintf(want,sizeof want,"\"selected\":%d,",games);must(has("\"page\":\"home\"")&&has(want),"BACK: the folder's tile");}
 // Checkers: the list from the folder, a new game with Heltec V4 as White.
 key(13);key(0xb7);key(13);must(has("\"page\":\"checkers\"")&&has("\"from_folder\":0"),"checkers from the folder");shot("m9-checkers-empty");
 key(13);must(has("\"page\":\"duel_pick\""),"contact choice");key(0xb7);must(has("\"duel_color\":0"),"White chosen");for(int k=0;k<pickRow(peerId(0));k++)key(0xb6);shot("m9-checkers-pick");
 key(13);Duel* g1=shown();must(g1&&has("\"page\":\"checkers_board\"")&&g1->state==Duel::Inviting&&g1->mine==checkers::White,"invitation");must(lastSent().find(" new w")!=std::string::npos,"the invitation is sent");shot("m9-checkers-inviting");
 say(g1,"yes");tick();must(g1->state==Duel::Playing&&g1->myTurn(),"accepted");
 // c3-d4 by the keys; their f6-e5: the capture d4:f6 is compulsory.
 cursorTo("c3");key(13);must(field("duel_held")=="c3","c3 taken");cursorTo("d4");key(13);must(g1->game.plies==1&&lastSent().find(" 1 c3-d4")!=std::string::npos,"c3-d4 sent");
 say(g1,"2 f6-e5");tick();cursorTo("b2");key(13);must(field("duel_held")=="","a man without the capture is not taken");
 cursorTo("d4");key(13);cursorTo("f6");shot("m9-checkers-capture");key(13);must(g1->game.plies==3&&g1->game.last.capture&&g1->game.pos.count(checkers::Black)==11,"d4:f6");
 say(g1,"4 g7:e5");say(g1,"5 g3-h4");/* out of turn: refused */must(g1->game.plies==4,"a move out of turn is refused");
 cursorTo("g3");key(13);cursorTo("h4");key(13);say(g1,"6 h6-g5");tick();
 cursorTo("h4");key(13);cursorTo("d4");shot("m9-checkers-double");key(13);must(g1->game.plies==7&&g1->game.pos.count(checkers::Black)==9,"h4:f6:d4 takes two");
 say(g1,"8 b6-a5");tick();cursorTo("d4");key(13);cursorTo("c5");shot("m9-checkers");must(field("duel_held")=="d4","d4 taken");
 key(0x86);must(field("duel_held")==""&&has("\"page\":\"checkers_board\""),"BACK puts it back");key('h');shot("m9-checkers-help");key(0x86);
 key('d');must(g1->drawOffer==Duel::OfferedByMe,"draw offered");cursorTo("e3");key(13);cursorTo("f4");key(13);must(g1->game.plies==9,"e3-f4");shot("m9-checkers-draw");
 // Their news while on another page: a toast; the list shows the games.
 key(0x86);must(has("\"page\":\"checkers\""),"back to the list");say(g1,"10 d6-e5");tick();must(g1->drawOffer==Duel::NoOffer&&g1->myTurn(),"their move: my turn, the offer is gone");shot("m9-checkers-list");
 // A position set up for the screens: a king's capture chain that ends the game (Black has nothing left).
 strcpy(peer(5).name,"Maximilian Longname 0E44");
 Duel* g2=duels.invite(peerId(5),Duel::Checkers,checkers::White);say(g2,"yes");
 {auto& p=g2->game.pos;memset(p.board,0,32);p.board[0]=checkers::King;p.board[2]=p.board[3]=p.board[7]=checkers::Man;p.board[9]=-checkers::Man;p.board[22]=-checkers::King;p.side=checkers::White;
  g2->game.plies=24;g2->game.last=checkers::Move();g2->game.last.n=2;g2->game.last.path[0]=27;g2->game.last.path[1]=22;}
 openGame(g2);tick();cursorTo("a1");key(13);cursorTo("g7");shot("m9-checkers-chain");key(13);
 must(g2->state==Duel::Over&&g2->result==Duel::Won&&g2->reason==Duel::Rules,"the chain takes the last pieces: won");shot("m9-checkers-won");
 key(0x08);key(0x08);must(has("\"page\":\"checkers\"")&&g2->state==Duel::Free,"DEL twice removes the finished game");
 // Invited by Игорь, who plays White: my side (Black) at the bottom; OK accepts, their move, mine by the keys.
 {char t[64];snprintf(t,sizeof t,"\xf0\x9f\x8f\x81%04X new w",0x4B1D);duels.receive(peerId(3),"Игорь T-Deck",t);}
 Duel* g3=duels.find(0x4B1D);must(g3&&g3->state==Duel::Invited&&g3->mine==checkers::Black,"invited as Black");tick();shot("m9-duel-news");
 openGame(g3);shot("m9-checkers-invited");key(13);must(g3->state==Duel::Playing,"accepted by OK");
 say(g3,"1 c3-d4");tick();cursorTo("f6");key(13);cursorTo("e5");shot("m9-checkers-black");key(13);must(g3->game.plies==2,"f6-e5 as Black");
 say(g3,"resign");tick();shot("m9-checkers-over");must(g3->state==Duel::Over&&g3->result==Duel::Won,"they resigned");
 key(0x86);key(0x86);must(has("\"page\":\"folder\""),"list back to the folder");key(0x86);must(has("\"page\":\"home\""),"folder back home");

 // Sea battle with Heltec V4: I shoot first.
 tile("sea");key(13);must(has("\"page\":\"sea\""),"sea list");shot("m9-sea-empty");key(13);must(has("\"page\":\"duel_pick\"")&&has("\"duel_kind\":\"sea\""),"sea contact choice");shot("m9-sea-pick");
 key(13);Duel* s1=shown();must(s1&&s1->kind==Duel::Sea&&s1->state==Duel::Inviting&&has("\"page\":\"sea_board\""),"sea invitation");shot("m9-sea-inviting");
 {char before[64];s1->home.fleet.text(before,sizeof before);key('s');char after[64];s1->home.fleet.text(after,sizeof after);must(strcmp(before,after)&&s1->home.fleet.valid(),"S reshuffles");}
 say(s1,"yes");tick();must(s1->myTurn(),"my shot first");
 cursorTo("e5");key(13);must(s1->aim==44&&lastSent().find(" 1 e5")!=std::string::npos,"e5 fired");shot("m9-sea-aim");key(13);must(s1->aim==44,"no second shot before the answer");
 say(s1,"1 = miss");tick();must(!s1->myTurn(),"a miss: their shot");
 // Their shots: a hit on my four-deck ship, then a miss.
 {char c[4];sea::cellName(s1->home.fleet.cell(0,1),c);std::string b=std::string("2 ")+c;say(s1,b.c_str());must(s1->lastAnswer==sea::AnswerHit,"my ship hit");
  int water=0;while(s1->home.fleet.shipAt(water)>=0||s1->home.isShot(water))water++;sea::cellName(water,c);b=std::string("3 ")+c;say(s1,b.c_str());must(s1->myTurn(),"their miss: my shot");}
 cursorTo("c3");key(13);say(s1,"4 = hit");cursorTo("c4");key(13);say(s1,"5 = sunk");must(s1->target.sunkShips()==1&&s1->target.around(sea::parseCell("d3")),"sunk: the water around is known");
 cursorTo("d4");key(13);must(s1->aim<0,"no shot next to a sunk ship");
 cursorTo("h8");key(13);say(s1,"6 = hit");cursorTo("j1");key(13);say(s1,"7 = miss");
 {char c[4];int water=0;while(s1->home.fleet.shipAt(water)>=0||s1->home.isShot(water))water++;sea::cellName(water,c);std::string b=std::string("8 ")+c;say(s1,b.c_str());}
 cursorTo("h9");key(13);tick();shot("m9-sea");must(s1->aim==sea::parseCell("h9"),"h9 aimed");key('h');shot("m9-sea-help");key(0x86);
 say(s1,"9 = sunk");tick();cursorTo("b9");shot("m9-sea-mid");
 say(s1,"resign");must(s1->state==Duel::Over,"they resigned");must(s1->state==Duel::Over&&lastSent().find(" fleet ")!=std::string::npos,"the winner shows the fleet");shot("m9-sea-won");
 // Invited by Игорь (they shoot first): they sink my whole fleet, then show theirs, which does not match an answer.
 {char t[64];snprintf(t,sizeof t,"\xf0\x9f\x9a\xa2%04X new",0x5EA2);duels.receive(peerId(3),"Игорь T-Deck",t);}
 Duel* s2=duels.find(0x5EA2);must(s2&&s2->state==Duel::Invited&&s2->mine==1,"sea invitation to me");openGame(s2);shot("m9-sea-invited");key(13);must(s2->state==Duel::Playing&&!s2->myTurn(),"accepted: their shot first");
 sea::Fleet theirs;theirs.random(77);
 {char c[4];int water=0;while(s2->home.fleet.shipAt(water)>=0)water++;sea::cellName(water,c);std::string b=std::string("1 ")+c;say(s2,b.c_str());must(s2->myTurn(),"their miss");
  sea::cellName(theirs.cell(0,0),c);cursorTo(c);key(13);say(s2,"2 = miss"); // a ship there, answered "miss"
  unsigned n=2;for(int s=0;s<sea::Ships;s++)for(int k=0;k<sea::Lengths[s];k++){sea::cellName(s2->home.fleet.cell(s,k),c);char b2[16];snprintf(b2,sizeof b2,"%u %s",++n,c);say(s2,b2);}}
 must(s2->state==Duel::Over&&s2->result==Duel::Lost,"the fleet is sunk: lost");shot("m9-sea-lost-wait");
 {char f[64];theirs.text(f,sizeof f);std::string b=std::string("fleet ")+f;say(s2,b.c_str());}must(s2->revealed&&!s2->fair,"their fleet does not match");tick();shot("m9-sea-lost");
 // News on the locked screen: hold opens the game.
 {char t[64];snprintf(t,sizeof t,"\xf0\x9f\x8f\x81%04X new b",0x7C3E);key(0x88);duels.receive(peerId(5),peer(5).name,t);tick();shot("m9-lock-duel");
  key(0xa3);must(has("\"page\":\"checkers_board\"")&&has("\"duel_game\":\"7C3E\""),"hold opens the game");key(0x08);must(duels.find(0x7C3E)->state==Duel::Over,"DEL declines");}
 // The node card: Play, the games, then the contact choice with this contact selected; BACK returns to the card.
 nodeCard(0);must(has("\"page\":\"node\""),"node card");key(0xb7);must(has("\"action\":1"),"Play is the second action");shot("m9-node");
 key(13);must(has("\"play_menu\":true"),"Play opens the games");key(0xb6);shot("m9-node-play");key(13);
 {char want[24];snprintf(want,sizeof want,"\"selected\":%d,",pickRow(peerId(0)));must(has("\"page\":\"duel_pick\"")&&has("\"duel_kind\":\"checkers\"")&&has(want),"checkers with this contact");}shot("m9-node-play-pick");
 key(0x86);must(has("\"page\":\"node\""),"BACK to the card");key(0xb7);key(13);key(0xb6);key(0xb6);key(13);must(has("\"duel_kind\":\"sea\""),"sea from the card");
 key(13);must(has("\"page\":\"sea_board\"")&&shown()&&shown()->peer==peerId(0),"sea invitation from the card");
 nodeCard(0);key(0xb7);key(13);key(13);must(has("\"page\":\"chess_pick\""),"chess from the card");key(0x86);must(has("\"page\":\"node\""),"chess choice BACK to the card");
 // The folder hidden or emptied: its tile goes; a custom folder named on the web page.
 strlcpy(config.apps,"chats {My_games chess checkers sea} -{games pet} nodes",sizeof config.apps);key(0x82);shot("m9-home-folders");
 {uint8_t o[AppEntriesMax];unsigned n=appsShown(o);must(n>=3&&o[1]==(AppFolder|0)&&appsFolderName(0)=="My games","custom folder");}
 key(0xb7);key(13);must(has("\"folder_name\":\"My games\""),"custom folder opens");shot("m9-folder-custom");key(0x82);config.apps[0]=0;
 for(auto& g:duels.games)g=Duel();strcpy(peer(5).name,"Марат");
 printf("games checks passed\n");
#endif
}
