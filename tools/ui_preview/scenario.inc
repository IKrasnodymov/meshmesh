// Screens rendered by the preview; the key codes are the production M9 codes.
static void scenario(){
#if defined(MM_JOYSTICK)
 // GAT562: joystick arrows 0xb4..0xb7, centre 13 (hold 0xa3), Back 0x86 (hold 0x82).
 for(int i=0;i<6;i++)tick(); // the boot choice times out
 shot("gat562-home");key(13);shot("gat562-home-menu");key(13);
 {int r=0,c=0;auto at=[&](int row,int col){while(r<row){key(0xb6);r++;}while(r>row){key(0xb5);r--;}while(c<col){key(0xb7);c++;}while(c>col){key(0xb4);c--;}key(13);};
  if(config.russian){at(1,2);at(1,3);at(0,8);at(0,2);at(0,5);at(1,5);}else{at(0,7);at(0,4);at(0,11);at(0,11);at(1,1);}
  shot("gat562-keyboard");key(0x82);}
 auto sayId=[](int peer,const char* name,unsigned id,const char* body){char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(peer),name,t);};
 ChessMatch* g=chessNet.invite(peerId(0),chess::White);sayId(0,"Heltec V4",g->id,"yes");
 for(const char* u:{"e2e4","e7e5","g1f3","b8c6","f1c4","f8c5"}){if(g->myTurn()){chess::Move mv;if(g->game.pos.parseUci(u,mv))chessNet.move(*g,mv);}else{char b[16];snprintf(b,sizeof b,"%u %s",g->game.plies+1,u);sayId(0,"Heltec V4",g->id,b);}}
 chessNet.events=0;for(int i=0;i<9;i++)tick(); // no news popup over the board
 key(0x86);key(0xb7);key(0xb7);key(0xb7);shot("gat562-chess-list");key(13);key(13);shot("gat562-chess");key(0x86);key(0x86);
 key(0xb7);for(int i=0;i<3;i++)tick();shot("gat562-radar");
 key(0xb4);key(0xb4);key(13);shot("gat562-node-menu");key(0x86);
#elif defined(MM_HELTEC_V4)
 shot("heltec-role");for(int i=0;i<5;i++)tick(); // the boot choice times out
 for(const char* n:{"home","messages","nodes","radar","gps","wifi","ble","settings","modules"}){shot((std::string("heltec-")+n).c_str());if(!strcmp(n,"radar")){key(0xa3);shot("heltec-radar-menu");key(13);key(13);key(0xa3);for(int i=0;i<4;i++)tick();shot("heltec-homing");key(0xa3);key(0xa3);}if(!strcmp(n,"messages")){key(0xa3);shot("heltec-messages-menu");key(13);key(13);key(0xa3);shot("heltec-messages-older");key(13);key(13);key(0xa3);}if(!strcmp(n,"wifi")){key(0xa3);shot("heltec-wifi-on");}key(13);}
 {ChatMessage m;m.source=0xA1B2C3D40000ULL;m.destination=meshRadio.nodeId;strcpy(m.name,"Heltec V4");strcpy(m.text,"Как слышно? Жду на точке через 5 минут");m.timestamp=time(nullptr);m.id=77;meshRadio.history[meshRadio.historyCount++]=m;}shot("heltec-popup");
#else
 shot("role-boot");key(0x86);shot("home");key(0x81);shot("threads");key(13);shot("chat-public");key(0x86);key(0xb6);key(13);shot("chat");
 for(char ch:std::string("Privet! Kak dela? 5to novogo @ 5"))key(ch);key(0xb7);shot("compose");key(0xa3);shot("layout");key(0x86);key(0x86);key(0x86);
 key(0x85);shot("map");key(0xb7);key(0xb6);shot("map-browse");key('L');shot("library");key(0x86);
 key(0x82);key(0xb7);key(0xb7);key(13);shot("nodes");key(13);shot("node");key(0xb7);key(0xb7);key(0xb7);key(13);shot("node-forget");key(0x86);key(0x86);
 key(0x82);key(0xb6);key(13);shot("sensors");key(0x90);shot("settings");key(13);key(0xb7);shot("radio");key(0x86);key(0xb6);key(13);for(int i=0;i<8;i++)key(0xb6);key(0xb7);shot("display-battery");for(int i=0;i<8;i++)key(0xb5);shot("display");key(13);shot("display-name");key(0x86);key(0x86);
 key(0x82);key(0xb6);key(0xb7);key(13);key(13);key(0xb6);key(13);shot("network");key(13);key(0xb6);key(13);key(0x83);shot("diagnostics");key(0x90);for(int i=0;i<5;i++)key(0xb6);key(13);shot("help");key(13);shot("help-layout");key(0x86);key(13);shot("help-layout");key(0x86);
 key(0x88);shot("locked");key(0xa3);key(0x82);
 key(0xb6);key(0xb7);key(0xb7);shot("home-radar");key(0xb6);shot("home-scrolled");key(0xb5);key(13);shot("radar");key(0xb6);key(0xb6);shot("radar-select");key(0xb5);key(0xb5);key(13);shot("homing");key(0x86);shot("radar-back");key(0xb7);shot("motion");key('b');for(int i=0;i<4;i++)tick();shot("motion-beacon");key('b');key(0xb7);key(0x86);
 // Solitaire: the ninth home tile; hints, a held run and the key sheet.
 key(0x82);key(0xb6);key(0xb6);key(0xb7);key(0xb7);shot("home-solitaire");key(13);shot("solitaire");for(int i=0;i<12;i++){key('p');key(' ');}shot("solitaire-played");
 key(0xb5);key(0xb5);key(0xb6);key(0xb5);key(13);key(0xb7);shot("solitaire-held");key(0x86);key('h');shot("solitaire-help");key(0x86);key(0x86);
 // Chess: the tenth home tile, the list, a challenge, a game with a held piece, promotion and a won game.
 {auto sayId=[](int peer,const char* name,unsigned id,const char* body){char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(peer),name,t);};
  auto play=[&](int peer,const char* name,ChessMatch* m,std::initializer_list<const char*> moves){for(const char* u:moves){if(m->myTurn()){chess::Move mv;if(m->game.pos.parseUci(u,mv))chessNet.move(*m,mv);}else{char b[16];snprintf(b,sizeof b,"%u %s",m->game.plies+1,u);sayId(peer,name,m->id,b);}}};
  key(0x82);for(int i=0;i<3;i++)key(0xb6);shot("home-chess");key(13);shot("chess-empty");key(13);shot("chess-pick");key(0xb7);shot("chess-pick-white");key(13);shot("chess-inviting");
  ChessMatch* a=nullptr;for(auto& m:chessNet.matches)if(m.state==ChessMatch::Inviting)a=&m;
  sayId(0,"Heltec V4",a->id,"yes");play(0,"Heltec V4",a,{"e2e4","e7e5","g1f3","b8c6","f1c4","g8f6"});
  shot("chess-italian");key(0x86);key(13);for(int i=0;i<3;i++)key(0xb6);key(13);key(0xb7);key(0xb5);key(0xb5);shot("chess-held");key(0x86);key('h');shot("chess-help");key(0x86);key(0x86);
  ChessMatch* b=chessNet.invite(peerId(5),chess::White);sayId(5,"Марат",b->id,"yes");play(5,"Марат",b,{"e2e4","e7e5","d1h5","b8c6","f1c4","g8f6","h5f7"});
  ChessMatch* c=chessNet.invite(peerId(0),chess::White);sayId(0,"Heltec V4",c->id,"yes");play(0,"Heltec V4",c,{"a2a4","b7b5","a4b5","a7a6","b5a6","c8b7","a6b7","b8c6"});
  sayId(3,"Игорь T-Deck",0x4B1D,"new w · шахматы MeshMesh");
  auto openList=[&](int row){key(0x82);for(int i=0;i<3;i++)key(0xb6);key(13);for(int k=0;k<row;k++)key(0xb6);};
  openList(0);shot("chess-list");
  for(int i=1;i<=4;i++){openList(i);key(13);char n[32];snprintf(n,sizeof n,"chess-open%d",i);shot(n);}
  // An undelivered move and a draw offer from the other player.
  a->outStatus=ChatMessage::Failed;for(int i=1;i<=4;i++){openList(i);key(13);if(uiStatus().indexOf("\"chess_cursor\":\"f6\"")>=0){shot("chess-failed");break;}}
  a->outStatus=ChatMessage::Delivered;sayId(0,"Heltec V4",a->id,"draw?");shot("chess-draw-offer");
  // The game with the pawn on b7 opens with the cursor on c6: take b7, go to a8 and choose the piece.
  for(int i=1;i<=4;i++){openList(i);key(13);if(uiStatus().indexOf("\"chess_cursor\":\"c6\"")>=0){key(0xb4);key(0xb5);key(13);key(0xb4);key(0xb5);shot("chess-promo-target");key(13);key(0xb7);shot("chess-promotion");key(13);shot("chess-promoted");break;}}
  key(0x82);
 }
#endif
 // Device modes: the repeater and room pages, the mode choice from Settings.
#if defined(MM_JOYSTICK)
#elif defined(MM_HELTEC_V4)
 auto home=[]{for(int i=0;i<12&&uiStatus().indexOf("\"page\":\"home\"")<0;i++)key(13);};
 for(int i=0;i<9;i++)tick(); // the message popup closes
 config.role=RoleRepeater;home();shot("heltec-repeater");key(0xa3);shot("heltec-repeater-menu");key(0x82);key(0x82);key(0xa3);shot("heltec-role-menu");key(0xa3); // holding the current mode closes the list
 config.role=RoleRoom;home();tick();tick();shot("heltec-room");config.role=RoleNormal;
#else
 config.role=RoleRepeater;key(0x82);shot("repeater");key(0x86);
 config.role=RoleRoom;key(0x82);shot("room");key(13);for(char ch:std::string(config.russian?"Vstre1a u vhoda posle obeda":"Meet at the gate after lunch"))key(ch);shot("room-post");key(0x86);
 key(0x90);key(0xb5);key(13);key(0xb6);shot("role-pick");key(0x86);config.role=RoleNormal;key(0x82);
#endif
}
