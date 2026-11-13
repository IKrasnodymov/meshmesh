// Screens rendered by the preview; the key codes are the production M9 codes.
// Selects an app's tile on the M9 home grid (three a row) from the first one.
static void tile(const char* id){key(0x82);uint8_t o[AppsMax];unsigned n=appsShown(o);for(unsigned i=0;i<n;i++)if(!strcmp(uiApps[o[i]],id)){for(unsigned k=0;k<i/3;k++)key(0xb6);for(unsigned k=0;k<i%3;k++)key(0xb7);return;}printf("FAIL no home tile %s\n",id);exit(1);}
static void scenario(){
#if defined(MM_JOYSTICK)
 // GAT562: joystick arrows 0xb4..0xb7, centre 13 (hold 0xa3), Back 0x86 (hold 0x82).
 for(int i=0;i<6;i++)tick(); // the boot choice times out
 shot("gat562-home");key(13);shot("gat562-home-menu");key(13);
 {int r=0,c=0;auto at=[&](int row,int col){while(r<row){key(0xb6);r++;}while(r>row){key(0xb5);r--;}while(c<col){key(0xb7);c++;}while(c>col){key(0xb4);c--;}key(13);};
  if(config.lang==LangRu||config.lang==LangUk){at(1,2);at(1,3);at(0,8);at(0,2);at(0,5);at(1,5);}else{at(0,7);at(0,4);at(0,11);at(0,11);at(1,1);}
  shot("gat562-keyboard");key(0x82);}
 auto sayId=[](int peer,const char* name,unsigned id,const char* body){char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(peer),name,t);};
 ChessMatch* g=chessNet.invite(peerId(0),chess::White);sayId(0,"Heltec V4",g->id,"yes");
 for(const char* u:{"e2e4","e7e5","g1f3","b8c6","f1c4","f8c5"}){if(g->myTurn()){chess::Move mv;if(g->game.pos.parseUci(u,mv))chessNet.move(*g,mv);}else{char b[16];snprintf(b,sizeof b,"%u %s",g->game.plies+1,u);sayId(0,"Heltec V4",g->id,b);}}
 chessNet.events=0;for(int i=0;i<9;i++)tick(); // no news popup over the board
 key(0x86);for(int i=0;i<16&&uiStatus().indexOf("\"page\":\"chess\"")<0;i++)key(0xb7);shot("gat562-chess-list");key(13);key(13);shot("gat562-chess");key(0x86);key(0x86);
 for(int i=0;i<16&&uiStatus().indexOf("\"page\":\"radar\"")<0;i++)key(0xb7);for(int i=0;i<3;i++)tick();shot("gat562-radar");
 for(int i=0;i<16&&uiStatus().indexOf("\"page\":\"nodes\"")<0;i++)key(0xb4);key(13);shot("gat562-node-menu");key(0x86);
 // Radio settings: "Radio..." in the Settings menu; the frequency by digits (OK, left/right, up/down), SF by right, Save.
 {auto expect=[](const char* what,const char* part){if(uiStatus().indexOf(part)<0){printf("FAIL %s: %s\n",what,uiStatus().c_str());exit(1);}};
  for(int i=0;i<14&&uiStatus().indexOf("\"page\":\"settings\"")<0;i++)key(0xb7);key(13);key(0xb6);key(13);expect("radio screen","\"page\":\"radio\"");shot("gat562-radio");
  key(13);key(0xb5);expect("MHz +1","\"radio_draft\":\"869.731\"");key(0xb7);key(0xb7);key(0xb5);key(0xb5);expect("10 kHz +2","\"radio_draft\":\"869.751\"");shot("gat562-radio-freq");
  key(13);key(0xb6);key(0xb6);key(0xb7);expect("SF9","\"radio_draft\":\"9\"");shot("gat562-radio-sf");
  for(int i=0;i<9;i++)key(0xb6);expect("Network row","\"radio_row\":11");key(13);shot("gat562-radio-net");key(0xb6);expect("Save row","\"radio_row\":12");key(13);expect("saved and closed","\"page\":\"settings\"");
  key(13);key(0xb6);key(13);key(0x86);expect("back leaves","\"page\":\"settings\"");}
#elif defined(MM_BOARD_TDECK)
 // Touch (uiTouch): each step checks the page the production UI reached.
 auto expect=[](const char* what,const char* part){if(uiStatus().indexOf(part)<0){printf("FAIL %s: %s\n",what,uiStatus().c_str());exit(1);}};
 auto touch=[](char gesture,int x,int y){uiTouch(gesture,x,y);tick();};
 // The rightmost footer hint (BACK on most pages), found by tapping from the right edge.
 auto footerRight=[&](const char* page){for(int x=316;x>150&&uiStatus().indexOf(page)>=0;x-=4){uiTouch('t',x,230);tick();}};
 shot("tdeck-role");touch('t',160,10);expect("title bar leaves the boot choice","\"page\":\"home\"");
 touch('t',260,100);expect("Nodes tile","\"page\":\"nodes\"");shot("tdeck-nodes");
 touch('t',160,90);expect("second node row","\"page\":\"node\"");expect("second node","\"selected_node\":\"4A00B2C3D4E5F711\"");
 touch('t',160,10);expect("title bar back","\"page\":\"nodes\"");key(0x83);footerRight("\"page\":\"diagnostics\"");expect("footer BACK to where @ was pressed","\"page\":\"nodes\"");key(0x82);
 touch('d',0,0);expect("swipe down","\"selected\":3");touch('u',0,0);touch('l',0,0);expect("swipe left","\"selected\":2");
 key(0x90);touch('t',160,24+32+10);expect("Settings row","\"page\":\"display\"");
 touch('t',160,25+23+8);expect("first tap selects","\"selected\":1");shot("tdeck-display");
 touch('t',160,10);expect("cancel the editor","\"page\":\"settings\"");
 touch('t',160,24+3*32+10);expect("Connections","\"page\":\"network\"");touch('t',160,25+56+6+44+10);expect("Bluetooth card toggles","\"page\":\"network\"");
 if(!bleActive()){printf("FAIL Bluetooth card did not toggle\n");exit(1);}
 internet.state=Internet::Online;touch('t',160,25+38+6+10);expect("Internet card","\"page\":\"internet\"");
 // Footer: OK (client on/off), DEL (rescan; on the T-Deck keyboard DEL would go back here), BACK.
 {int rescans=previewRescans;for(int x=4;x<316&&previewRescans==rescans&&uiStatus().indexOf("\"page\":\"internet\"")>=0;x+=4){uiTouch('t',x,230);internet.enabled=false;tick();}
  if(previewRescans==rescans){printf("FAIL footer DEL did not rescan: %s\n",uiStatus().c_str());exit(1);}}
 expect("footer DEL stays on the list","\"page\":\"internet\"");
 touch('t',160,10);expect("back to Connections","\"page\":\"network\"");shot("tdeck-network-lan");internet.state=Internet::Off;
 key(0x88);expect("locked","\"locked\":true");touch('t',160,120);expect("a tap does not unlock","\"locked\":true");touch('h',160,120);expect("hold unlocks","\"locked\":false");
 tile("solitaire");key(0x86);for(int i=0;i<3;i++)key(0xb6);key(0x86);
 key(0x90);for(int i=0;i<5;i++)key(0xb6);key(13);shot("tdeck-help");key(0x86);
 // Chess: the colour row, then a game opened by taps and a move made on the board.
 {ChessMatch* g=chessNet.invite(peerId(0),chess::White);char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X yes",g->id);chessNet.receive(peerId(0),"Heltec V4",t);
  chessNet.events=0;for(int i=0;i<9;i++)tick();
  tile("chess");touch('t',160,10);key(0x82);
  tile("chess");key(13);expect("chess list","\"page\":\"chess\"");touch('t',160,46+44+20);expect("game opens","\"page\":\"chess_board\"");
  touch('t',4+4*24+12,25+6*24+12);expect("tap e2 takes the pawn","\"chess_held\":\"e2\"");touch('t',4+4*24+12,25+4*24+12);
  if(g->game.plies!=1){printf("FAIL e2-e4 by taps: %u plies\n",g->game.plies);exit(1);}shot("tdeck-chess");
  touch('t',160,10);touch('t',160,10);expect("chess list back to menu","\"page\":\"home\"");
  tile("chess");key(13);touch('t',160,60);expect("new game","\"page\":\"chess_pick\"");touch('t',160,35);expect("colour row","\"chess_color\":0");
  touch('t',160,88+44+20);expect("first tap on another contact selects","\"selected\":1");expect("no challenge yet","\"page\":\"chess_pick\"");}
 // Channels: "add a channel" row, the hashtag editor, a channel chat, its card from the footer "<" hint.
 key(0x82);key(0x81);expect("chats","\"page\":\"threads\"");touch('t',160,24+3*48+20);expect("add row","\"page\":\"channel_add\"");
 touch('t',160,24+20);expect("hashtag editor","\"editing\":true");touch('t',160,10);expect("editor cancelled","\"editing\":false");touch('t',160,10);expect("back to chats","\"page\":\"threads\"");
 touch('t',160,24+48+20);expect("channel chat","\"page\":\"chat\"");
 for(int x=4;x<316&&uiStatus().indexOf("\"page\":\"chat\"")>=0;x+=4){uiTouch('t',x,230);tick();}
 expect("footer < opens the card","\"page\":\"channel\"");shot("tdeck-channel");
 {unsigned before=meshRadio.historyCount;touch('t',250,158+14);if(meshRadio.historyCount!=before+1){printf("FAIL invite row did not send\n");exit(1);}}
 touch('t',160,10);expect("card back to the chat","\"page\":\"chat\"");key(0x82);
 // Storage left by another firmware: OK on Module health, then OK again, formats and restarts.
 hardware.fsOk=false;key(0x82);key(0x83);shot("tdeck-format");previewCommands="";key(13);key(13);
 if(previewCommands!="fsformat;restart;"){printf("FAIL create storage: %s\n",previewCommands.c_str());exit(1);}hardware.fsOk=true;
 // Dice: taps on a tab, a grid die, the roll card and a sheet block (a tap chooses, a second one rolls its check).
 {dicer.begin();dicer.applyTemplate(dice::DndTemplate);tile("dice");key(13);expect("dice tile","\"page\":\"dice\"");
  touch('t',80,34);if(dicer.s.mode!=dice::Grid){printf("FAIL dice tab tap: %s\n",dicer.json().c_str());exit(1);}
  touch('t',20,60);touch('t',50,60);if(dicer.s.gridChosen!=2){printf("FAIL dice grid tap: %s\n",dicer.json().c_str());exit(1);}
  touch('t',20,34);uint32_t rolls=dicer.rolls;touch('t',100,120);if(dicer.rolls!=rolls+1){printf("FAIL dice roll tap: %s\n",dicer.json().c_str());exit(1);}shot("tdeck-dice");
  dicer.setMode(dice::Sheet);tick();touch('t',8+60,60);touch('t',8+60,60);if(dicer.rolls!=rolls+2||String(dicer.last()->formula)!="d20"){printf("FAIL sheet block taps: %s\n",dicer.json().c_str());exit(1);}shot("tdeck-dice-sheet");
  key('e');touch('t',100,74+5*19+8);if(uiStatus().indexOf("\"block_field\":5")<0){printf("FAIL builder row tap: %s\n",uiStatus().c_str());exit(1);}
  touch('t',100,74+5*19+8);if(dicer.sheet.block[1].shape!=dice::Square){printf("FAIL builder second tap: %s\n",uiStatus().c_str());exit(1);}shot("tdeck-dice-builder");key(0x86);
  touch('t',160,10);expect("dice back","\"page\":\"home\"");}
 // Quick Send: the pinned recipient card and first/last templates use their own touch targets.
 {strlcpy(config.apps,"quick people dice",sizeof config.apps);config.dimAfter=0;config.autoLock=0;key(0x82);key(13);expect("Quick tile","\"page\":\"quick\"");shot("tdeck-quick");unsigned before=meshRadio.historyCount;
  touch('t',160,45);expect("recipient card opens chooser","\"quick_targets\":true");shot("tdeck-quick-targets");if(meshRadio.historyCount!=before){printf("FAIL recipient card sent a template\n");exit(1);}
  touch('t',160,10);expect("cancel returns to card","\"selected\":0");touch('t',160,45);touch('t',160,90);expect("recipient confirmation selects first preset","\"quick_index\":0");shot("tdeck-quick-preset");
  touch('t',160,90);if(meshRadio.historyCount!=before+1||String(meshRadio.history[before].text)!=quickSend::state.text[0]||meshRadio.history[before].destination!=quickSend::state.to){printf("FAIL first Quick template touch\n");exit(1);}
  key(0xb5);key(0xb5);shot("tdeck-quick-last");touch('t',160,190);if(meshRadio.historyCount!=before+2||String(meshRadio.history[before+1].text)!=quickSend::state.text[quickSend::Count-1]){printf("FAIL last Quick template touch\n");exit(1);}
  touch('t',160,45);expect("pinned card after scrolling","\"quick_targets\":true");if(meshRadio.historyCount!=before+2){printf("FAIL pinned card sent a template\n");exit(1);}touch('t',160,10);touch('t',160,10);config.apps[0]=0;}
 printf("tdeck touch checks passed\n");return;
#elif defined(MM_HELTEC_V4)
 auto expect=[](const char* what,const char* part){if(uiStatus().indexOf(part)<0){printf("FAIL %s: %s\n",what,uiStatus().c_str());exit(1);}};
 shot("heltec-role");for(int i=0;i<5;i++)tick(); // the boot choice times out
 // Each screen is reached by name: an open menu times out first, then clicks step through the screens.
 auto go=[&](const char* n){char savedApps[sizeof config.apps];strlcpy(savedApps,config.apps,sizeof savedApps);strlcpy(config.apps,"-dice -quick -people",sizeof config.apps);std::string want=std::string("\"page\":\"")+n+"\"";for(int i=0;i<11&&uiStatus().indexOf("\"menu\":true")>=0;i++)tick();
  for(int i=0;i<60&&uiStatus().indexOf(want.c_str())<0;i++){if(uiStatus().indexOf("\"page\":\"quick\"")>=0||uiStatus().indexOf("\"page\":\"dice\"")>=0||uiStatus().indexOf("\"page\":\"people\"")>=0)key(0x86);else key(13);}expect(n,want.c_str());strlcpy(config.apps,savedApps,sizeof config.apps);};
 for(const char* n:{"home","messages","nodes","chess","radar","gps","wifi","ble","settings","modules"}){
#if defined(MM_NO_WIFI)
  if(!strcmp(n,"wifi"))continue; // nRF52 (T114): no Wi-Fi screen
#endif
  go(n);shot((std::string("heltec-")+n).c_str());if(!strcmp(n,"radar")){key(0xa3);shot("heltec-radar-menu");key(13);key(13);key(0xa3);for(int i=0;i<4;i++)tick();shot("heltec-homing");key(0xa3);key(0xa3);}if(!strcmp(n,"messages")){key(0xa3);shot("heltec-messages-menu");key(13);key(13);key(0xa3);shot("heltec-messages-older");key(13);key(13);key(0xa3);}if(!strcmp(n,"wifi")){key(0xa3);shot("heltec-wifi-on");}
  if(!strcmp(n,"ble")){for(int i=0;i<2&&!bleActive();i++)key(0xa3);if(!bleActive()){printf("FAIL BLE on\n");exit(1);}if(uiStatus().indexOf("\"menu\":true")>=0)key(0x86);shot("heltec-ble-on");for(int i=0;i<2&&bleActive();i++)key(0xa3);if(uiStatus().indexOf("\"menu\":true")>=0)key(0x86);} // the MeshCore- name and the PIN
  key(13);}
 {ChatMessage m;m.source=0xA1B2C3D40000ULL;m.destination=meshRadio.nodeId;strcpy(m.name,"Heltec V4");strcpy(m.text,"Как слышно? Жду на точке через 5 минут");m.timestamp=time(nullptr);m.id=77;meshRadio.history[meshRadio.historyCount++]=m;notifications::incoming(m);}shot("heltec-popup");
 // Chess with one button: hold on the list opens the game; click steps through the pieces that can move,
 // hold picks one, click steps through its squares (then "Cancel"), hold plays. Each step checks the UI state.
 {auto sayId=[](unsigned id,const char* body){char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(0),"Heltec V4",t);};
  auto clickTo=[&](const char* what,int sq){char want[32];snprintf(want,sizeof want,"\"chess_cursor\":%d,",sq);for(int i=0;i<40&&uiStatus().indexOf(want)<0;i++)key(13);expect(what,want);};
  key(13); // closes the message popup
  ChessMatch* g=chessNet.invite(peerId(0),chess::White);sayId(g->id,"yes");
  for(const char* u:{"e2e4","e7e5","g1f3","b8c6"}){if(g->myTurn()){chess::Move mv;if(g->game.pos.parseUci(u,mv))chessNet.move(*g,mv);}else{char b[16];snprintf(b,sizeof b,"%u %s",g->game.plies+1,u);sayId(g->id,b);}}
  chessNet.events=0;for(int i=0;i<9;i++)tick();if(uiStatus().indexOf("\"chess_popup\":true")>=0)key(13);
  go("chess");shot("heltec-chess-list");
  key(0xa3);expect("hold opens the only game","\"chess_game\"");shot("heltec-chess-board");
  clickTo("f1 bishop",5);shot("heltec-chess-piece");key(0xa3);expect("bishop picked","\"chess_held\":5,");shot("heltec-chess-targets");
  clickTo("Cancel after the squares",-1);shot("heltec-chess-cancel");key(0xa3);expect("put back","\"chess_held\":-1,");expect("cursor back on f1","\"chess_cursor\":5,");
  key(0xa3);clickTo("c4",26);shot("heltec-chess-move");key(0xa3);
  if(g->game.plies!=5){printf("FAIL Bc4 by the button: %u plies\n",g->game.plies);exit(1);}
  sayId(g->id,"6 g8f6");tick();expect("their move updates the board, no popup","\"chess_popup\":false");
  for(int i=0;i<40&&uiStatus().indexOf("\"chess_cursor\":-1,")<0;i++)key(13);expect("Game menu after the pieces","\"chess_cursor\":-1,");shot("heltec-chess-gamemenu");
  key(0xa3);expect("game menu","\"chess_menu\":true");shot("heltec-chess-menu");
  key(13);key(13);key(0xa3);expect("Game list","\"page\":\"chess\"");if(uiStatus().indexOf("chess_game")>=0){printf("FAIL game list: %s\n",uiStatus().c_str());exit(1);}
  key(13);expect("click leaves the list","\"page\":\"pet\"");
  chessNet.remove(*g);}
#else
 shot("role-boot");key(0x86);shot("home");key(0x81);shot("threads");key(13);shot("chat-public");key(0x86);
 // Channels: the list, a hashtag channel's card, a private channel and its card, adding one, an invitation.
 key(0xb6);shot("threads-channels");key(0xb7);shot("channel-hashtag");key(0x86);
 key(0xb6);key(0xb6);key(13);shot("chat-channel");key(0xb4);shot("channel-private");key(0xb7);shot("channel-region");key(0xb4);key(0xb6);key(0xb6);key(13);shot("channel-leave");key(0x86);key(0x86);
 key(0xb6);key(0xb6);key(0xb6);key(13);shot("channel-add");key(13);key(0x83);for(char ch:std::string("test"))key(ch);shot("channel-add-tag");key(0x83);key(0x86);key(0x86);
 for(int i=0;i<4;i++)key(0xb6);key(13);shot("chat-invite");key(0xb4);shot("chat-invite-joined");key(0x86);
 if(!meshRadio.channel(channels::idOf(meshRadio.channelList[3].secret))||strcmp(meshRadio.channelList[3].name,"Походы")){printf("FAIL invitation not joined\n");exit(1);}
 for(int i=0;i<6;i++)key(0xb6);key(13);shot("chat");
 for(char ch:std::string("Privet! Kak dela? 5to novogo @ 5"))key(ch);key(0xb7);shot("compose");key(0xa3);shot("layout");key(0x86);key(0x86);key(0x86);
 key(0x85);shot("map");key(0xb7);key(0xb6);shot("map-browse");key('L');shot("library");key(0x86);
 key(0x82);key(0xb7);key(0xb7);key(13);shot("nodes");key(13);shot("node");key(0xb7);key(0xb7);key(0xb7);key(13);shot("node-forget");key(0x86);key(0x86);
 // A repeater's card and its server page: login, status, CLI replies and the trace (fake answers of the preview).
 key(0x82);key(0xb7);key(0xb7);key(13);
 {String want=String("\"selected_node\":\"")+meshRadio.idText(peerId(1))+"\"";for(int i=0;i<8&&uiStatus().indexOf(want)<0;i++)key(0xb6);
  if(uiStatus().indexOf(want)<0){printf("FAIL repeater not in the node list: %s\n",uiStatus().c_str());exit(1);}}
 key(13);shot("node-repeater");key(13);if(uiStatus().indexOf("\"page\":\"remote\"")<0){printf("FAIL server page: %s\n",uiStatus().c_str());exit(1);}shot("remote");key(0x86);key(0x86);key(0x86);
 key(0x82);key(0xb6);key(13);shot("sensors");
 // Navigation: the GPS switch, then BACK returns where the page was opened (the menu tile, the settings row).
 {bool gps=config.gps;key(13);if(config.gps==gps){printf("FAIL GPS switch in navigation\n");exit(1);}shot("sensors-gps");key(13);
  key(0x86);if(uiStatus().indexOf("\"page\":\"home\"")<0||uiStatus().indexOf("\"selected\":3,")<0){printf("FAIL back from navigation to the menu: %s\n",uiStatus().c_str());exit(1);}
  key(0x90);key(0xb6);key(0xb6);key(13);key(0x86);if(uiStatus().indexOf("\"page\":\"settings\"")<0||uiStatus().indexOf("\"selected\":2,")<0){printf("FAIL back from navigation to the settings: %s\n",uiStatus().c_str());exit(1);}}
 // Network of your city (site feature card): a catalogue profile under test, the saved one comes back on a restart.
 {RadioProfile saved=config.radio(),test;test.frequency=868.731f;test.bandwidth=62.5f;test.sf=7;test.cr=7;
  RadioProfile before;before.frequency=869.525f;before.bandwidth=250;before.sf=11;before.cr=5;
  config.trial=before;config.setRadio(test);key(0x90);key(13);for(int i=0;i<10;i++)key(0xb6);shot("radio-test");
  key(0x86);config.trial=RadioProfile();config.setRadio(saved);}
 // The network search found the Moscow profile (the preview's netfind): the row offers to test it.
 key(0x90);key(13);for(int i=0;i<11;i++)key(0xb6);key(13);shot("radio-net");netfind::state.phase=netfind::State::Idle;key(0x86);
 key(0x90);shot("settings");key(13);key(0xb7);shot("radio");for(int i=0;i<7;i++)key(0xb6);key(0xb7);shot("radio-hash");key(0xb6);key(0xb6);key(13);shot("radio-regions");key(0xb5);key(0xb7);key(0xb7);key(0xb7);shot("radio-region");key(13);for(int i=0;i<3;i++)key(8);for(char ch:std::string("sao"))key(ch);shot("radio-region-type");key(0x86);key(0x86);key(0xb6);key(13);for(int i=0;i<8;i++)key(0xb6);key(0xb7);shot("display-battery");for(int i=0;i<8;i++)key(0xb5);shot("display");key(13);shot("display-name");key(0x86);key(0x86);
 tile("connect");key(13);key(13);key(0xb6);key(13);shot("network");key(13);key(0xb6);key(13);key(0x83);shot("diagnostics");key(0x90);for(int i=0;i<5;i++)key(0xb6);key(13);shot("help");key(13);shot("help-layout");key(0x86);key(13);shot("help-layout");key(0x86);
 // Connections with Bluetooth on: the MeshCore- name the phone apps see, then off again.
 key(0x90);for(int i=0;i<3;i++)key(0xb6);key(13);key(0xb6);key(0xb6);key(13);
 if(!bleActive()||uiStatus().indexOf("\"page\":\"network\"")<0){printf("FAIL Bluetooth on: %s\n",uiStatus().c_str());exit(1);}shot("network-ble");key(13);key(0x86);key(0x86);
 key(0x88);shot("locked");
 // Two OK presses within a second unlock as the hold does; one OK, or two far apart, do not.
 key(13);if(uiStatus().indexOf("\"locked\":true")<0){printf("FAIL one OK unlocked\n");exit(1);} // key() ticks 1 s
 key(13);if(uiStatus().indexOf("\"locked\":true")<0){printf("FAIL slow OK presses unlocked\n");exit(1);}fakeMillis+=1500;
 uiKey(13);fakeMillis+=300;uiKey(13);tick();if(uiStatus().indexOf("\"locked\":false")<0){printf("FAIL two OK presses did not unlock: %s\n",uiStatus().c_str());exit(1);}
 key(0x88);key(0xa3);key(0x82);
 key(0xb6);key(0xb7);key(0xb7);shot("home-radar");key(0xb6);shot("home-scrolled");key(0xb5);key(13);shot("radar");key(0xb6);key(0xb6);shot("radar-select");key(0xb5);key(0xb5);key(13);shot("homing");key(0x86);shot("radar-back");key(0xb7);shot("motion");key('b');for(int i=0;i<4;i++)tick();shot("motion-beacon");key('b');key(0xb7);key(0x86);
 // Solitaire: the ninth home tile; hints, a held run and the key sheet.
 tile("solitaire");shot("home-solitaire");key(13);shot("solitaire");for(int i=0;i<12;i++){key('p');key(' ');}shot("solitaire-played");
 key(0xb5);key(0xb5);key(0xb6);key(0xb5);key(13);key(0xb7);shot("solitaire-held");key(0x86);key('h');shot("solitaire-help");key(0x86);key(0x86);
 // Chess: the tenth home tile, the list, a challenge, a game with a held piece, promotion and a won game.
 {auto sayId=[](int peer,const char* name,unsigned id,const char* body){char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(peer),name,t);};
  auto play=[&](int peer,const char* name,ChessMatch* m,std::initializer_list<const char*> moves){for(const char* u:moves){if(m->myTurn()){chess::Move mv;if(m->game.pos.parseUci(u,mv))chessNet.move(*m,mv);}else{char b[16];snprintf(b,sizeof b,"%u %s",m->game.plies+1,u);sayId(peer,name,m->id,b);}}};
  tile("chess");shot("home-chess");key(13);shot("chess-empty");key(13);shot("chess-pick");key(0xb7);shot("chess-pick-white");key(13);shot("chess-inviting");
  ChessMatch* a=nullptr;for(auto& m:chessNet.matches)if(m.state==ChessMatch::Inviting)a=&m;
  sayId(0,"Heltec V4",a->id,"yes");play(0,"Heltec V4",a,{"e2e4","e7e5","g1f3","b8c6","f1c4","g8f6"});
  shot("chess-italian");key(0x86);key(13);for(int i=0;i<3;i++)key(0xb6);key(13);key(0xb7);key(0xb5);key(0xb5);shot("chess-held");key(0x86);key('h');shot("chess-help");key(0x86);key(0x86);
  ChessMatch* b=chessNet.invite(peerId(5),chess::White);sayId(5,"Марат",b->id,"yes");play(5,"Марат",b,{"e2e4","e7e5","d1h5","b8c6","f1c4","g8f6","h5f7"});
  ChessMatch* c=chessNet.invite(peerId(0),chess::White);sayId(0,"Heltec V4",c->id,"yes");play(0,"Heltec V4",c,{"a2a4","b7b5","a4b5","a7a6","b5a6","c8b7","a6b7","b8c6"});
  sayId(3,"Игорь T-Deck",0x4B1D,"new w · шахматы MeshMesh");
  auto openList=[&](int row){tile("chess");key(13);for(int k=0;k<row;k++)key(0xb6);};
  openList(0);shot("chess-list");
  for(int i=1;i<=4;i++){openList(i);key(13);char n[32];snprintf(n,sizeof n,"chess-open%d",i);shot(n);}
  // An undelivered move and a draw offer from the other player.
  a->outStatus=ChatMessage::Failed;for(int i=1;i<=4;i++){openList(i);key(13);if(uiStatus().indexOf("\"chess_cursor\":\"f6\"")>=0){shot("chess-failed");break;}}
  a->outStatus=ChatMessage::Delivered;sayId(0,"Heltec V4",a->id,"draw?");shot("chess-draw-offer");
  // The game with the pawn on b7 opens with the cursor on c6: take b7, go to a8 and choose the piece.
  for(int i=1;i<=4;i++){openList(i);key(13);if(uiStatus().indexOf("\"chess_cursor\":\"c6\"")>=0){key(0xb4);key(0xb5);key(13);key(0xb4);key(0xb5);shot("chess-promo-target");key(13);key(0xb7);shot("chess-promotion");key(13);shot("chess-promoted");break;}}
  key(0x82);
  // Rated results: my signature (made in ChessNet::tick), theirs as a message, the change on the board and
  // the rating tab with two games.
  {auto sayLong=[](int peer,const char* name,unsigned id,const char* body){char t[160];snprintf(t,sizeof t,"\xe2\x99\x9f%04X %s",id,body);chessNet.receive(peerId(peer),name,t);};
   char sig[rating::SigTextSize+1],body[128];uint8_t raw[64];memset(raw,0x33,64);rating::sigText(raw,sig);
   ChessMatch* d=chessNet.invite(peerId(3),chess::White);sayId(3,"Игорь T-Deck",d->id,"yes");play(3,"Игорь T-Deck",d,{"f2f3","e7e5","g2g4","d8h4"});
   for(int i=0;i<2;i++){chessNet.tick();tick();}
   snprintf(body,sizeof body,"sig 1791140000 %s",sig);sayLong(5,"Марат",b->id,body);snprintf(body,sizeof body,"sig 1791150000 %s",sig);sayLong(3,"Игорь T-Deck",d->id,body);
   for(int i=0;i<2;i++){chessNet.tick();tick();}
   if(b->sign!=ChessMatch::SignStored||d->sign!=ChessMatch::SignStored||rating::book.myElo()==rating::Start){printf("FAIL rated results: %d %d %d\n",b->sign,d->sign,rating::book.myElo());exit(1);}
   // PGN of the won game, and the archive on the SD card.
   {std::string pg=chessNet.pgn(*b).c_str();if(pg.find("1. e4 e5 2. Qh5 Nc6 3. Bc4 Nf6 4. Qxf7# {checkmate} 1-0")==std::string::npos||pg.find("[White \"M9\"]")==std::string::npos||pg.find("[Black \"Марат\"]")==std::string::npos||pg.find("[Result \"1-0\"]")==std::string::npos){printf("FAIL pgn:\n%s\n",pg.c_str());exit(1);}
    chessNet.tick();String list=chessArchiveList();char want[16];snprintf(want,sizeof want,"-%04X.pgn",b->id);int at=list.indexOf(want);
    if(at<0){printf("FAIL archive: %s\n",list.c_str());exit(1);}int from=at;while(from>0&&list[from-1]!='"')from--;String name=list.substring(from,at+9);
    StaticJsonDocument<4096> d;deserializeJson(d,chessNet.command("chess archive "+name));if(chessNet.pgn(*b)!=(d["pgn"]|"")){printf("FAIL archive read %s\n",name.c_str());exit(1);}}
   openList(0);key(0xb7);shot("chess-rating");key(0xb4);
   char want[32];snprintf(want,sizeof want,"\"chess_game\":\"%04X\"",b->id);
   for(int i=1;i<=5;i++){openList(i);key(13);if(uiStatus().indexOf(want)>=0){shot("chess-rated-result");break;}}
   key(0x82);tile("chess");key(13);key(13);shot("chess-pick-rated");key(0x86);key(0x82);}
  // Locked: a challenge arrives; its card stands above the messages, without names when details are off,
  // and holding OK unlocks straight into the game.
  key(0x88);sayId(5,"Марат",0x5C3E,"new b · шахматы MeshMesh");tick();shot("locked-chess");
  config.lockDetails=false;tick();shot("locked-chess-hidden");config.lockDetails=true;
  key(0xa3);if(uiStatus().indexOf("\"chess_game\":\"5C3E\"")<0||uiStatus().indexOf("\"locked\":false")<0){printf("FAIL hold on the locked chess card: %s\n",uiStatus().c_str());exit(1);}key(0x86);key(0x82);
  // Swiss tournament, organiser side: the wizard with six contacts, everyone joins, the start comes by
  // itself; four rounds with the reports of both players, then the checks of the pairings and standings.
  // Boards: finished games nobody opened give their place to a new game, the oldest settled first; a finished
  // game still waiting for its rating signature stays, and six games in progress refuse a seventh.
  {auto fail=[](const char* what){printf("FAIL boards: %s\n",what);exit(1);};
   auto game=[&](int peer,const char* name,bool rated){ChessMatch* g=chessNet.invite(peerId(peer),chess::White,rated);if(!g)fail("a free board");char t[48];snprintf(t,sizeof t,"\xe2\x99\x9f%04X yes",g->id);chessNet.receive(peerId(peer),name,t);return g;};
   for(auto& m:chessNet.matches)m=ChessMatch();
   ChessMatch* old[6];for(int i=0;i<6;i++){old[i]=game(0,"Heltec V4",false);chessNet.resign(*old[i]);old[i]->unseen=true;old[i]->changedAt=1000+i;}
   old[0]->rated=true;old[0]->sign=ChessMatch::SignSent;old[0]->theirSigned=false; // the oldest waits for a signature
   uint16_t pendingId=old[0]->id,evicted=old[1]->id;
   ChessMatch* fresh=chessNet.invite(peerId(3),chess::White,false);if(!fresh)fail("a finished game gives its board");
   if(!chessNet.find(pendingId))fail("the game waiting for a signature stays");if(chessNet.find(evicted)&&chessNet.find(evicted)!=fresh)fail("the oldest settled game gives its board");
   for(auto& m:chessNet.matches)m=ChessMatch();
   for(int i=0;i<6;i++)game(i%2?3:0,i%2?"Игорь T-Deck":"Heltec V4",false);
   if(chessNet.room()||chessNet.invite(peerId(5),chess::White,false))fail("six games in progress refuse a seventh");
   previewSent.clear();char t[64];snprintf(t,sizeof t,"\xe2\x99\x9f" "7777 new w r · шахматы MeshMesh");chessNet.receive(peerId(5),"Марат",t);
   if(chessNet.find(0x7777)||previewSent.empty()||previewSent.back().text.find("7777 no")==std::string::npos)fail("a challenge to a full device is declined");}
  {for(auto& m:chessNet.matches)m=ChessMatch(); // free boards for the tournament games
   const char* extra[]={"Лиля","Ринат","Тимур"};for(int i=0;i<3;i++){auto& p=meshRadio.peers[meshRadio.peerCount++];p=Peer();p.id=peerId(6+i);strcpy(p.name,extra[i]);p.type=1;p.heard=true;p.seen=fakeMillis-60000*(i+1);for(int k=0;k<8;k++)p.publicKey[k]=uint8_t(p.id>>(56-8*k));p.publicKey[8]=0x70+i;}
   auto fail=[](const char* what){printf("FAIL tournament: %s\n",what);exit(1);};
   tile("chess");key(13);key('2');shot("tour-empty");
   key(13);shot("tour-new-name");key(0xb7);for(int i=0;i<6;i++){key(' ');key(0xb6);}shot("tour-new-players");key(13);shot("tour-new-rules");key(13);shot("tour-new-check");
   previewSent.clear();key(13);if(uiStatus().indexOf("\"page\":\"chess_tour\"")<0)fail("the wizard did not open the tournament");shot("tour-gathering");
   tour::Tour* T=nullptr;for(auto& x:tour::net.tours)if(x.state==tour::Inviting)T=&x;if(!T||T->playerCount!=7)fail("seven players asked");
   unsigned invites=0;for(auto& m:previewSent)invites+=m.text.find(" inv ")!=std::string::npos;if(invites!=6)fail("six invitations");
   auto openTour=[&](uint16_t id){char want[24];snprintf(want,sizeof want,"\"tour\":\"%04X\"",id);for(int row=1;row<=3;row++){tile("chess");key(13);key('2');for(int k=0;k<row;k++)key(0xb6);key(13);if(uiStatus().indexOf(want)>=0)return;}fail("tournament page");};
   char cmd[200];auto say=[&](int peer,const char* name,const char* body){snprintf(cmd,sizeof cmd,"\xe2\x99\x9e%04X %s",T->id,body);tour::net.receive(peerId(peer),name,cmd);};
   const char* names[]={"Heltec V4","","","Игорь T-Deck","","Марат","Лиля","Ринат","Тимур"};int who[]={0,3,5,6,7,8};
   for(int i=0;i<5;i++)say(who[i],names[who[i]],"yes");tour::net.tick();if(T->state!=tour::Inviting)fail("started before everyone answered");
   say(who[5],names[who[5]],"yes");if(T->state!=tour::Inviting)fail("started inside the radio callback");tour::net.tick();if(T->state!=tour::Running||T->round!=1)fail("automatic start");
   auto playerPeer=[&](uint8_t idx)->int{for(int w:who)if(T->players[idx].id==peerId(w))return w;return -1;};
   bool met[tour::MaxPlayers][tour::MaxPlayers]={};unsigned byes[tour::MaxPlayers]={},whites[tour::MaxPlayers]={},blacks[tour::MaxPlayers]={};
   for(unsigned round=1;round<=T->rounds;round++){
    if(T->round!=round)fail("round order");unsigned r=round-1,bye=0;
    for(unsigned k=0;k<T->pairCount[r];k++){const tour::Pair& p=T->pairs[r][k];
     if(p.black==tour::NoPlayer){bye++;byes[p.white]++;continue;}
     if(met[p.white][p.black]){printf("round %u: %s and %s again\n",round,T->players[p.white].name,T->players[p.black].name);fail("a repeated pairing");}
     met[p.white][p.black]=met[p.black][p.white]=true;whites[p.white]++;blacks[p.black]++;}
    if(bye!=1||T->pairCount[r]!=4)fail("three games and one bye a round");
    if(round==1){chessNet.tick();for(int i=0;i<2;i++)tick();shot("tour-round1");}
    // Results: White wins the first game, Black the second, a draw in the third; my own game ends by resigning.
    for(unsigned k=0;k<T->pairCount[r];k++){tour::Pair p=T->pairs[r][k];if(p.black==tour::NoPlayer)continue;const char* res=k%3==0?"w":k%3==1?"b":"d";
     if(T->players[p.white].id==meshRadio.nodeId||T->players[p.black].id==meshRadio.nodeId){ChessMatch* m=nullptr;for(auto& x:chessNet.matches)if(x.state==ChessMatch::Playing&&x.tour==T->id&&x.id==p.game)m=&x;if(!m)fail("my tournament game");
      bool white=T->players[p.white].id==meshRadio.nodeId;chessNet.resign(*m);chessNet.tick();
      char body[40];snprintf(body,sizeof body,"res %04X %s",p.game,white?"b":"w");int op=playerPeer(white?p.black:p.white);say(op,names[op],body);continue;}
     char body[40];snprintf(body,sizeof body,"res %04X %s",p.game,res);int pw=playerPeer(p.white),pb=playerPeer(p.black);say(pw,names[pw],body);if(round==2&&k==2)snprintf(body,sizeof body,"res %04X w",p.game); // a dispute
     say(pb,names[pb],body);}
    tour::net.tick();
    if(round==2&&T->round==2){openTour(T->id);key(0xb7);for(unsigned k=0;k<T->pairCount[1];k++)if(T->pairs[1][k].result==tour::Disputed){for(unsigned j=0;j<k;j++)key(0xb6);shot("tour-dispute");key('d');tour::net.tick();break;}
     if(T->round!=3)fail("the dispute settled by the organiser");key(0xb4);}
   }
   if(T->state!=tour::Over)fail("finished after the last round");
   for(unsigned i=0;i<T->playerCount;i++){if(byes[i]>1)fail("two byes for one player");if(abs(int(whites[i])-int(blacks[i]))>2)fail("colours not alternated");}
   tour::Row rows[tour::MaxPlayers];unsigned n=tour::net.standings(*T,rows),total=0;for(unsigned i=0;i<n;i++)total+=rows[i].half;if(n!=7||total!=2*4*4)fail("standings: 4 points a round");
   for(unsigned i=1;i<n;i++)if(rows[i].half>rows[i-1].half)fail("standings order");
   openTour(T->id);for(int i=0;i<3;i++)tick();shot("tour-final");key(0xb7);shot("tour-final-games");key(0x86);
   // Player side: an invitation from Игорь, the player list, a pairing with Марат's key, the standings.
   uint16_t pid=0xA7C1;auto org=[&](const char* body){snprintf(cmd,sizeof cmd,"\xe2\x99\x9e%04X %s",pid,body);tour::net.receive(peerId(3),"Игорь T-Deck",cmd);};
   org("inv 5 24 6 Осенняя швейцарка");tour::Tour* P=tour::net.find(pid);if(!P||P->state!=tour::Invited)fail("invitation received");
   openTour(pid);for(int i=0;i<2;i++)tick();shot("tour-invite");previewSent.clear();key(13);if(P->state!=tour::Running||previewSent.empty()||previewSent.back().text.find(" yes")==std::string::npos)fail("joined");
   char list[160];snprintf(list,sizeof list,"pl %06lX:Игорь,%06lX:M9,%06lX:Марат,%06lX:Лиля",(unsigned long)(peerId(3)>>40),(unsigned long)(meshRadio.nodeId>>40),(unsigned long)(peerId(5)>>40),(unsigned long)(peerId(6)>>40));org(list);
   const Peer* marat=nullptr;for(unsigned i=0;i<meshRadio.peerCount;i++)if(meshRadio.peers[i].id==peerId(5))marat=&meshRadio.peers[i];
   const char* b64="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string key64;
   for(int i=0;i<32;i+=3){uint32_t v=uint32_t(marat->publicKey[i])<<16|(i+1<32?marat->publicKey[i+1]<<8:0)|(i+2<32?marat->publicKey[i+2]:0);key64+=b64[v>>18&63];key64+=b64[v>>12&63];key64+=i+1<32?b64[v>>6&63]:'=';key64+=i+2<32?b64[v&63]:'=';}
   snprintf(list,sizeof list,"r1 w 6D21 %s Марат",key64.c_str());org(list);
   ChessMatch* g=chessNet.find(0x6D21);if(!g||g->tour!=pid||g->mine!=chess::White||g->state!=ChessMatch::Playing||g->peer!=peerId(5))fail("the pairing starts my game");
   snprintf(list,sizeof list,"st 1 %06lX:2:3,%06lX:1:4,%06lX:1:2,%06lX:0:3",(unsigned long)(peerId(6)>>40),(unsigned long)(meshRadio.nodeId>>40),(unsigned long)(peerId(3)>>40),(unsigned long)(peerId(5)>>40));org(list);
   for(int i=0;i<2;i++)tick();shot("tour-player");key(0x86);key('1');key(0x82);}
 }
#endif
 // Device modes: the repeater and room pages, the mode choice from Settings.
#if defined(MM_JOYSTICK)
#elif defined(MM_HELTEC_V4)
 auto home=[]{for(int i=0;i<11&&uiStatus().indexOf("\"menu\":true")>=0;i++)tick();for(int i=0;i<4&&uiStatus().indexOf("\"page\":\"home\"")<0;i++)key(0x86);};
 for(int i=0;i<9;i++)tick(); // the message popup closes
 config.role=RoleRepeater;home();shot("heltec-repeater");key(0xa3);shot("heltec-repeater-menu");key(0x82);key(0x82);key(0xa3);shot("heltec-role-menu");key(0xa3); // holding the current mode closes the list
 config.role=RoleRoom;home();tick();tick();shot("heltec-room");config.role=RoleNormal;
 // An invitation to a channel: its card in Messages, "Join" first in the menu; a channel message names its channel.
 {channels::Channel c{};strcpy(c.name,"Рыбалка");for(int i=0;i<16;i++)c.secret[i]=uint8_t(i*29+3);ChatMessage m;m.source=0xA1B2C3D40000ULL;m.destination=meshRadio.nodeId;strcpy(m.name,"Heltec V4");
  strcpy(m.text,channels::link(c).c_str());m.timestamp=time(nullptr);m.id=78;meshRadio.history[meshRadio.historyCount++]=m;}
 home();key(13);shot("heltec-invite");key(0xa3);shot("heltec-invite-menu");key(0xa3);
 if(!meshRadio.channel(channels::idOf(meshRadio.channelList[meshRadio.channelCount-1].secret))||strcmp(meshRadio.channelList[meshRadio.channelCount-1].name,"Рыбалка")){printf("FAIL heltec invitation not joined\n");exit(1);}
 {ChatMessage m;m.source=0xA1B2C3D45555ULL;m.destination=meshRadio.channelList[1].id;strcpy(m.name,"Марат");strcpy(m.text,"Лёд на Кабане держит?");m.timestamp=time(nullptr);m.id=79;meshRadio.history[meshRadio.historyCount++]=m;}
 home();key(13);shot("heltec-channel-message");
 // Storage left by another firmware: "Create storage..." heads the home menu, a second hold formats.
 hardware.fsOk=false;home();previewCommands="";key(0xa3);shot("heltec-format-menu");key(0xa3);shot("heltec-format-armed");key(0xa3);
 if(previewCommands!="fsformat;restart;"){printf("FAIL create storage: %s\n",previewCommands.c_str());exit(1);}hardware.fsOk=true;
 // Radio settings with one button: hold on "Radio...", the frequency digit by digit (click +1, hold next), SF, Save.
 for(int i=0;i<11;i++)tick();go("settings");
 key(0xa3);expect("settings menu","\"menu\":true");key(0xa3);expect("radio screen","\"page\":\"radio\"");shot("heltec-radio");
 key(0xa3);key(13);expect("MHz +1","\"radio_draft\":\"869.731\"");key(0xa3);key(0xa3);key(13);key(13);expect("10 kHz +2","\"radio_draft\":\"869.751\"");shot("heltec-radio-freq");
 key(0xa3);key(0xa3);expect("digits done","\"radio_editing\":false");key(13);key(13);key(0xa3);key(13);expect("SF9","\"radio_draft\":\"9\"");shot("heltec-radio-sf");key(0xa3);
 for(int i=0;i<7;i++)key(13);expect("Find row","\"radio_row\":9");key(0xa3);shot("heltec-radio-find");
 for(int i=0;i<13;i++)key(13);expect("Region row","\"radio_row\":8");key(0xa3);key(13);key(13);key(13);key(0xa3);shot("heltec-radio-region");
 for(int i=0;i<2;i++)key(13);expect("Previous row","\"radio_row\":10");shot("heltec-radio-prev");key(13);expect("Network row","\"radio_row\":11");key(0xa3);shot("heltec-radio-net");key(13);expect("Save row","\"radio_row\":12");shot("heltec-radio-save");key(0xa3);expect("saved and closed","\"page\":\"settings\"");
#else
 config.role=RoleRepeater;key(0x82);shot("repeater");key(0x86);
 config.role=RoleRoom;key(0x82);shot("room");key(13);for(char ch:std::string(config.lang==LangRu||config.lang==LangUk?"Vstre1a u vhoda posle obeda":"Meet at the gate after lunch"))key(ch);shot("room-post");key(0x86);
 key(0x90);key(0xb5);key(13);key(0xb6);shot("role-pick");key(0x86);config.role=RoleNormal;key(0x82);
#endif
 // The pet: an egg, a happy kid with a new friend, a hungry adult, an ill teen and a grave with its memory.
 {auto look=[](uint8_t stage,uint16_t food,uint16_t joy,uint16_t health,uint32_t xp,const char* words,pet::Effect e){auto& s=creature.s;s.stage=stage;s.food=food;s.joy=joy;s.health=health;s.xp=xp;s.age=stage==pet::Egg?420:stage*86400+5*3600;
   s.snacks=3;s.friends=4;s.packets=1873;creature.speech=words?words:"";creature.speechAt=words?fakeMillis:0;creature.effect=e;creature.effectAt=fakeMillis;};
  strcpy(creature.s.name,"Ping");creature.s.species=0;uint16_t dim=config.dimAfter,lock=config.autoLock;config.dimAfter=config.autoLock=0; // dozens of shots outlast the screen timeout
  int16_t offset=config.utcOffset;{time_t now=time(nullptr);config.utcOffset=int16_t(((12-int(now/3600%24))+24)%24*60);} // noon: awake at any hour of the build
#if defined(MM_HELTEC_V4)
  for(int i=0;i<11;i++)tick(); // menus and popups close
  for(int i=0;i<14&&uiStatus().indexOf("\"page\":\"pet\"")<0;i++)
#if defined(MM_JOYSTICK)
   key(0xb7);
#else
   key(13);
#endif
  const char* prefix=
#if defined(MM_JOYSTICK)
   "gat562-";
#else
   "heltec-";
#endif
  auto snap=[&](const char* n){shot((std::string(prefix)+n).c_str());};
#else
  tile("pet");auto snap=[&](const char* n){shot(n);};snap("home-pet");key(13);
#endif
  if(uiStatus().indexOf("\"page\":\"pet\"")<0){printf("FAIL pet page: %s\n",uiStatus().c_str());exit(1);}
  // Every species, young and grown, awake at noon (the gallery of tools/pet_sprites.py on the real screens).
  {for(unsigned i=0;i<pet::Species;i++){creature.s.species=i;char n[32];
    look(pet::Child,850,900,1000,190,nullptr,pet::NoEffect);snprintf(n,sizeof n,"pet-kind%02u-young",i);snap(n);
    look(pet::Adult,850,900,1000,2600,nullptr,pet::NoEffect);snprintf(n,sizeof n,"pet-kind%02u-grown",i);snap(n);}
   creature.s.species=0;}
  look(pet::Egg,700,700,1000,0,nullptr,pet::NoEffect);snap("pet-egg");
  look(pet::Child,820,900,1000,190,config.lang==LangRu?"Новый друг: Марат":"New friend: Marat",pet::Hearts);snap("pet-happy");
  creature.speechAt=0;snap("pet-stats");
  creature.s.species=3;look(pet::Adult,120,520,700,2600,nullptr,pet::NoEffect);snap("pet-hungry");
  creature.s.species=5;look(pet::Teen,420,300,260,900,nullptr,pet::NoEffect);snap("pet-ill");
#if defined(MM_HELTEC_V4)
  key(0xa3);snap("pet-menu");
#if defined(MM_JOYSTICK)
  key(0x86);
#else
  for(int i=0;i<11;i++)tick();
#endif
#endif
  auto& s=creature.s;s.food=0;s.joy=0;s.health=0;creature.speech="";creature.command("pet skip 3600");
  if(creature.alive()){printf("FAIL the pet outlived zero health: %s\n",creature.json().c_str());exit(1);}
  snap("pet-dead");
  creature.command("pet egg");if(!creature.alive()||creature.s.stage!=pet::Egg||creature.s.graveCount!=1){printf("FAIL new egg: %s\n",creature.json().c_str());exit(1);}
  creature.command("pet skip 900");if(creature.s.stage!=pet::Baby){printf("FAIL hatching: %s\n",creature.json().c_str());exit(1);}
  creature.command("pet mortal off");s.food=0;s.joy=0;creature.command("pet skip 864000");if(!creature.alive()||creature.s.health!=1){printf("FAIL death switched off: %s\n",creature.json().c_str());exit(1);}
  creature.command("pet mortal on");s.food=s.joy=800;s.health=1000;
  // A pet is optional: letting it go leaves no pet (and no new grave), the page offers to start one.
  {uint8_t graves=creature.s.graveCount;creature.command("pet release");
   if(creature.has()||creature.s.graveCount!=graves){printf("FAIL release: %s\n",creature.json().c_str());exit(1);}
   snap("pet-none");creature.command("pet adopt");if(creature.s.stage!=pet::Egg){printf("FAIL adopt: %s\n",creature.json().c_str());exit(1);}}config.dimAfter=dim;config.autoLock=lock;config.utcOffset=offset;
  printf("pet checks passed\n");}
 // Dice (DIC3R, Dice.h): the rules of formulas, rolls, the grid, the sheet and the commands first, then the screens.
 {uint16_t dim=config.dimAfter,lock=config.autoLock;config.dimAfter=config.autoLock=0;srand(11);dicer.begin();
  auto must=[](bool v,const char* what){if(!v){printf("FAIL dice %s: %s\n",what,dicer.json().c_str());exit(1);}};
  using dice::Dicer;
  // A fresh device gets a D&D and a R.E.T. character once, the general set stays current.
  must(dicer.s.characters==3&&dicer.s.character==0&&String(dicer.s.list[1].name)=="D&D 5e"&&(dicer.s.flags&dice::ExamplesAdded),"the two systems by default");
  must(dicer.useCharacter(2)&&dicer.find("body")>=0&&dicer.useCharacter(1)&&dicer.find("dex")>=0,"the examples are full sheets");
  dicer.deleteCharacter();dicer.useCharacter(1);dicer.deleteCharacter();dicer.addExamples();
  must(dicer.s.characters==3,"added again only by addExamples");dicer.useCharacter(2);dicer.deleteCharacter();dicer.useCharacter(1);dicer.deleteCharacter();
  dicer.applyTemplate(dice::Blank);dicer.deleteBlock(0);
  must(Dicer::valid("2d6+1")&&Dicer::valid("d20")&&Dicer::valid("4к6-2")&&Dicer::valid("d%")&&Dicer::valid("3D8 + 2d4 - 1")&&Dicer::valid("-1d4+3")&&Dicer::valid("99d1000")&&Dicer::valid("r3+2")&&Dicer::valid("r0"),"good formulas");
  must(!Dicer::valid("")&&!Dicer::valid("2d")&&!Dicer::valid("d1")&&!Dicer::valid("100d6")&&!Dicer::valid("2d6+")&&!Dicer::valid("abc")&&!Dicer::valid("1+1+1+1+1+1+1+1+1")&&!Dicer::valid("2d6 3")&&!Dicer::valid("0d6")&&!Dicer::valid("r9")&&!Dicer::valid("2r3"),"bad formulas");
  for(int i=0;i<400;i++){must(dicer.roll("3d6+2"),"3d6+2");const dice::Result* r=dicer.last();must(r->total>=5&&r->total<=20&&r->parts==4&&!r->special,"3d6+2 range");}
  {bool ends=false;for(int i=0;i<400;i++){dicer.roll("d%");unsigned v=dicer.last()->value[0];must(v>=1&&v<=100&&dicer.last()->special,"d% range");ends|=v==1||v==100;
    dicer.roll("d66");v=dicer.last()->value[0];must(v/10>=1&&v/10<=6&&v%10>=1&&v%10<=6,"d66 digits");}must(ends,"d% reaches its ends");}
  {must(dicer.roll("2d6-1d4-1"),"subtraction");const dice::Result* r=dicer.last();must(r->negative[2]&&r->negative[3]&&r->total==int(r->value[0]+r->value[1])-int(r->value[2])-1,"subtraction sum");}
  must(dicer.roll("40d6")&&dicer.last()->more&&dicer.last()->parts==dice::Parts&&dicer.resultCount==dice::Results,"many dice, eight results kept");
  // R.E.T. dice: the top value adds the die of the next level; a 1 first is a failure that drops the bonus.
  {bool fail=false,chain=false,plain=false;for(int i=0;i<3000;i++){must(dicer.roll("r3+2"),"r3+2");const dice::Result& r=*dicer.last();
    if(r.fail){fail=true;must(r.fail>=1&&r.fail<=4&&r.total==1&&r.value[0]==1,"failure: the die alone");continue;}
    if(r.successes){chain=true;must(r.value[0]==6&&r.shape[1]==dice::D8&&r.total>=6+1+2,"success adds the d8");if(r.successes>=2)must(r.value[1]==8&&r.shape[2]==dice::D10,"a second success adds the d10");continue;}
    plain=true;must(r.value[0]>=2&&r.value[0]<=5&&r.total==int(r.value[0])+2,"ordinary r3");}
   must(fail&&chain&&plain,"failures, successes and ordinary rolls");
   for(int i=0;i<200;i++){dicer.roll("r0");const dice::Result& r=*dicer.last();if(r.fail)continue;must(r.kind[1]==dice::PartNumber&&r.negative[1]&&r.value[1]==3&&r.total>=-1,"d6-3");}
   for(int i=0;i<500;i++){dicer.roll("r8");const dice::Result& r=*dicer.last();must(r.parts==1||r.fail,"d100 adds no die");}}
  dicer.gridReset();dicer.gridChoose(12);dicer.gridRoll();for(unsigned i=0;i+1<12;i++)must(dicer.grid[i]>=dicer.grid[i+1],"grid from high to low");
  dicer.gridSetThreshold(true,4);{unsigned n=0;for(unsigned i=0;i<12;i++)n+=dicer.grid[i]>=4;must(dicer.gridSuccesses()==n,"successes");}
  dicer.gridSetThreshold(true,99);must(dicer.s.threshold==6,"threshold within the die");
  for(int i=0;i<8;i++)dicer.gridAddRow();must(dicer.s.gridSize==dice::GridMax,"50 dice at most");
  must(dicer.command("dice saved add 8d6 Fire ball").startsWith("OK")&&dicer.savedCount()==1&&String(dicer.sheet.block[dicer.savedBlock(0)].name)=="Fire ball","saved add");
  must(dicer.command("dice saved add 2d Bad").startsWith("ERR"),"a bad saved roll");
  must(dicer.command("dice saved roll 0").startsWith("OK")&&String(dicer.last()->label)=="Fire ball","saved roll");
  must(dicer.command("dice char add Hero").startsWith("OK")&&dicer.s.character==1&&dicer.sheet.count==1&&dicer.sheet.block[0].kind==dice::Track&&!dicer.savedCount(),"character: one counter");
  must(dicer.command("dice sheet 0 -3").startsWith("OK")&&dicer.value(0)==17,"counter -3");
  must(dicer.command("dice char use 0").startsWith("OK")&&dicer.savedCount()==1,"general set: its sheet comes back");
  must(dicer.command("dice char use 1").startsWith("OK")&&dicer.value(0)==17,"the character's sheet comes back");
  must(dicer.command("dice pool 3 d8 -2").startsWith("OK")&&dicer.poolText()=="3d8-2","pool");
  // D&D: modifiers, derived values, skills, spending.
  bool ru=config.lang==LangRu||config.lang==LangUk;
  must(dicer.command("dice char new dnd Arwen").startsWith("OK")&&dicer.sheet.count>=40,"D&D template");
  int dex=dicer.find("dex"),ac=dicer.find("ac"),init=dicer.find("init"),sl1=dicer.find("sl1");must(dex>=0&&ac>=0&&init>=0&&sl1>=0,"D&D codes");
  must(dicer.value(ac)==10&&dicer.badge(dex)=="+0","AC 10 at DEX 10");
  dicer.setValue(dex,16);must(dicer.value(ac)==13&&dicer.badge(dex)=="+3"&&dicer.rollFormula(init)=="d20+3"&&dicer.rollFormula(dex)=="d20+3","DEX 16: +3 everywhere");
  dicer.setValue(dex,7);must(dicer.badge(dex)=="-2"&&dicer.value(ac)==8&&dicer.rollFormula(dex)=="d20-2","DEX 7: -2, rounded down");
  {int stealth=-1;for(unsigned i=0;i<dicer.sheet.count;i++)if(String(dicer.sheet.block[i].formula)=="d20+@dex"&&dicer.sheet.block[i].kind==dice::Ability)stealth=i;must(stealth>=0&&dicer.rollFormula(stealth)=="d20-2","a DEX skill");
   must(dicer.setField(stealth,"formula","d20+@dex+@pb")&&dicer.rollFormula(stealth)=="d20","a proficient skill");
   must(dicer.setField(stealth,"use","sl1")&&dicer.setField(stealth,"spend","1"),"a cost");int slots=dicer.value(sl1);
   must(dicer.act(stealth)&&dicer.value(sl1)==slots-1&&dicer.act(stealth)&&dicer.value(sl1)==slots-2&&!dicer.act(stealth)&&dicer.why.length(),"spending until none is left");}
  must(!dicer.setField(ac,"formula","10+(@dex")&&!dicer.setField(0,"code","dex")&&!dicer.setField(0,"code","1x"),"bad formula and codes refused");
  // A loop of references does not hang: the values show as wrong.
  {int a=dicer.addBlock(dice::Stat),b=dicer.addBlock(dice::Stat);must(dicer.setField(a,"code","la")&&dicer.setField(b,"code","lb")&&dicer.setField(a,"formula","@lb+1")&&dicer.setField(b,"formula","@la+1"),"a loop is accepted");
   must(!dicer.derivedOk(a)&&dicer.badge(a)=="?"&&dicer.sheetJson().indexOf("\"bad\":true")>0,"a loop shows as wrong");dicer.deleteBlock(b);dicer.deleteBlock(a);}
  {DynamicJsonDocument d(32768);DeserializationError e=deserializeJson(d,dicer.sheetJson());if(e)printf("%s\n%s\n",e.c_str(),dicer.sheetJson().c_str());must(!e&&d["blocks"].size()==dicer.sheet.count,"sheet JSON parses");}
  // R.E.T.: the tree halves upwards, dice by level, derived tracks.
  must(dicer.command("dice char new ret Yan").startsWith("OK"),"R.E.T. template");
  int body=dicer.find("body"),hp=dicer.find("hp"),phys=dicer.find("phys"),endu=dicer.find("endu"),lvl=dicer.find("lvl");must(body>=0&&hp>=0&&phys>=0&&endu>=0&&lvl>=0,"R.E.T. codes");
  must(dicer.value(body)==3&&dicer.maxOf(hp)==6&&dicer.badge(body)=="d6"&&dicer.rollFormula(body)=="r3","R.E.T. start");
  dicer.setValue(phys,5);dicer.setValue(endu,5);must(dicer.value(dicer.find("str"))==5&&dicer.value(body)==4&&dicer.badge(body)=="d8"&&dicer.maxOf(hp)==8,"R.E.T. halves upwards");
  dicer.setValue(phys,0);dicer.setValue(endu,1);must(dicer.badge(dicer.find("str"))=="d6-3"&&dicer.rollFormula(dicer.find("str"))=="r0","a penalty level");
  {int skill=-1;for(unsigned i=0;i<dicer.sheet.count;i++)if(String(dicer.sheet.block[i].formula)=="@cha+3")skill=i;must(skill>=0&&dicer.rollFormula(skill)=="r3+3","a R.E.T. skill");
   int talent=-1;for(unsigned i=0;i<dicer.sheet.count;i++)if(String(dicer.sheet.block[i].formula)=="r3+@fine!")talent=i;must(talent>=0&&dicer.rollFormula(talent)=="r3+3","a R.E.T. talent");}
  must(dicer.sheet.block[dicer.find("luck")].noteLen>0,"notes kept");
  // The sheet as one line and back: names, notes, escapes.
  {must(dicer.setField(0,"name","A|B~C\\D")&&dicer.setField(0,"note","line 1\nline | 2 ~ end"),"escaped fields");String text=dicer.exportText();unsigned count=dicer.sheet.count;String note=dicer.noteText(0);
   must(text.indexOf('\n')<0&&text.startsWith("MMS1~N|Yan~"),"one line");
   must(dicer.command("dice sheet import "+text).startsWith("OK")&&dicer.sheet.count==count&&String(dicer.sheet.block[0].name)=="A|B~C\\D"&&dicer.noteText(0)==note&&dicer.exportText()==text,"import gives the same sheet");
   {int cut=text.indexOf(' ',60);String a=text.substring(0,cut),b=text.substring(cut);must(dicer.command("dice sheet put 1/2 "+a).startsWith("OK put 1/2")&&dicer.command("dice sheet put 2/2 "+b).startsWith("OK")&&dicer.exportText()==text,"import in pieces");dicer.command("dice char del");dicer.useCharacter(dicer.s.characters-1);
    must(dicer.command("dice sheet put 2/2 x").startsWith("ERR"),"a piece out of order");}
   must(dicer.command("dice sheet import MMS1~N|X~9|0").startsWith("ERR")&&dicer.command("dice sheet import hello").startsWith("ERR"),"bad text refused");
   must(dicer.command("dice char del").startsWith("OK"),"delete the copy");}
  // Sending to a contact: acknowledged parts in order; the contact gets it as an offer.
  {dicer.useCharacter(3);previewSent.clear();String sent=dicer.exportText();must(dicer.command("dice sheet send "+meshRadio.idText(peerId(0))).startsWith("OK"),"send starts");size_t done=0;unsigned parts=0;
   for(int guard=0;guard<200&&done<previewSent.size();guard++){PreviewSent p=previewSent[done++];must(p.text.size()<=151&&meshmesh::validUtf8((const uint8_t*)p.text.data(),p.text.size())&&p.text.rfind("\xf0\x9f\x8e\xb2S ",0)==0,"a part fits a message");
    parts++;must(dicer.delivery(p.id,ChatMessage::Delivered),"its ACK");must(dicer.receive(peerId(0),"Heltec V4",p.text.c_str()),"taken as a sheet part");}
   dicer.tick();must(dicer.out.done&&parts==dicer.out.parts&&parts>1&&dicer.offered&&String(dicer.offer.name)=="Yan"&&dicer.offer.text==sent,"delivered and offered");
   uint8_t before=dicer.s.characters;must(dicer.command("dice sheet offer accept").startsWith("OK")&&dicer.s.characters==before+1&&dicer.exportText()==sent&&!dicer.offered,"accepted as a new character");
   dicer.command("dice char del");previewSent.clear();
   must(dicer.receive(peerId(0),"x","\xf0\x9f\x8e\xb2S 1234 1/2 MMS1~N|half")&&!dicer.offered,"half a sheet is not offered");dicer.tick();
   must(!dicer.receive(peerId(0),"x","hello")&&!dicer.receive(peerId(0),"x","\xe2\x99\x9f""1234 1"),"other messages pass");}
  // dice.bin of 0.7-0.24: counters become tracks, saved rolls abilities.
  {struct R{char name[24];char formula[28];uint8_t color;};struct C{char name[24];int16_t value;uint8_t color;};struct H{char name[24];uint8_t rolls,counters;R roll[10];C counter[6];};
   struct V{uint32_t version;uint8_t character,characters,mode;uint8_t type,count;int8_t mod;uint8_t gridType,gridSize,gridChosen,threshold;bool thresholdOn;H list[6];uint32_t check;};
   V* v=new V();v->version=1;v->characters=2;v->character=1;v->mode=2;v->type=dice::D20;v->count=1;v->gridType=dice::D6;v->gridSize=20;v->threshold=4;
   strcpy(v->list[0].roll[0].name,"Fireball");strcpy(v->list[0].roll[0].formula,"8d6");v->list[0].rolls=1;
   strcpy(v->list[1].name,"Hero");strcpy(v->list[1].counter[0].name,"Life");v->list[1].counter[0].value=17;v->list[1].counter[0].color=3;v->list[1].counters=1;
   uint32_t h=2166136261u;const uint8_t* p=(const uint8_t*)v;for(size_t k=0;k<offsetof(V,check);k++)h=(h^p[k])*16777619u;v->check=h;
   must(sizeof(V)==4304||sizeof(V)>4000,"V1 layout");must(dicer.migrate((const uint8_t*)v,sizeof(V)),"V1 migrates");delete v;
   must(dicer.s.characters==2&&dicer.s.character==1&&dicer.s.mode==dice::Sheet&&dicer.sheet.count==1&&dicer.sheet.block[0].kind==dice::Track&&dicer.value(0)==17&&!dicer.maxOf(0),"counters become tracks");
   must(dicer.command("dice char use 0").startsWith("OK")&&dicer.savedCount()==1&&dicer.rollFormula(dicer.savedBlock(0))=="8d6","saved rolls become abilities");}
  must(dicer.json().indexOf("\"overflowed\"")<0,"the reply fits");
  // The screens: the general set with saved rolls, a D&D and a R.E.T. character.
  dicer.s=dice::State();dicer.clearResults();dicer.gridReset();dicer.applyTemplate(dice::Blank);dicer.deleteBlock(0);
  dicer.addRoll("8d6",ru?"Огненный шар":"Fireball");dicer.addRoll("d20+5",ru?"Атака мечом":"Sword attack");dicer.addRoll("2d6+3",ru?"Урон":"Damage");dicer.addRoll("d20+7",ru?"Скрытность":"Stealth");
  dicer.colorRoll(1,1);dicer.colorRoll(2,6);dicer.colorRoll(3,9);
  {int k=dicer.addBlock(dice::Track,0);dicer.setField(k,"name",ru?"Жизни":"Life");dicer.setValue(k,24);dicer.setField(k,"max","30");}
  dicer.addCharacter(ru?"Арвен":"Arwen",dice::DndTemplate);
  {const char* v[6]={"8","16","14","12","13","10"};const char* c[6]={"str","dex","con","int","wis","cha"};for(int k=0;k<6;k++)dicer.setValue(dicer.find(c[k]),atoi(v[k]));
   dicer.setValue(dicer.find("hp"),19);dicer.setField(dicer.find("hp"),"max","27");dicer.setValue(dicer.find("dss"),1);dicer.setValue(dicer.find("sl1"),1);}
  dicer.addCharacter(ru?"Ян":"Yan",dice::RetTemplate);dicer.setValue(dicer.find("cha"),5);dicer.setValue(dicer.find("insp"),4);dicer.setValue(dicer.find("hp"),4);
  dicer.useCharacter(0);
#if defined(MM_HELTEC_V4)
#if defined(MM_JOYSTICK)
  const int next=0xb7,hold=13,run=13;const char* prefix="gat562-";
#else
  const int next=13,hold=0xa3,run=0xa3;const char* prefix="heltec-";
#endif
  auto snap=[&](const char* n){shot((std::string(prefix)+n).c_str());};
  for(int i=0;i<11;i++)tick();for(int i=0;i<14&&uiStatus().indexOf("\"page\":\"dice\"")<0;i++)key(next);
  if(uiStatus().indexOf("\"page\":\"dice\"")<0){printf("FAIL dice page: %s\n",uiStatus().c_str());exit(1);}
#if !defined(MM_JOYSTICK)
  snap("dice-closed");key(hold);must(uiStatus().indexOf("\"app_open\":true")>=0,"hold opens dice");
#endif
  snap("dice-empty");key(hold);snap("dice-menu");key(run);must(dicer.resultCount==1&&uiStatus().indexOf("\"menu\":false")>=0,"hold, hold rolls the pool");snap("dice-rpg");
  dicer.roll("d20+5",ru?"Атака мечом":"Sword attack");snap("dice-saved");dicer.roll("d4+d6+d8+d10+d12");snap("dice-shapes");
  dicer.setMode(dice::Grid);dicer.gridChoose(10);dicer.gridSetThreshold(true,4);dicer.gridRoll();snap("dice-grid");
  dicer.setMode(dice::Sheet);tick();snap("dice-counters");key(hold);
#if defined(MM_JOYSTICK)
  key(run);must(dicer.value(0)==25,"+1 from the menu");snap("dice-counter-menu");
  key(0x86);key(0xb5);must(dicer.value(0)==26,"joystick up");
#else
  // Count with button: click +1, hold -1, a 5 s hold leaves and takes that hold's -1 back.
  key(run);must(uiStatus().indexOf("\"dice_tapping\":true")>=0,"count with button opens");snap("dice-counting");
  key(13);key(13);must(dicer.value(0)==26,"click +1");key(0xa3);must(dicer.value(0)==25,"hold -1");
  key(0xa4);must(dicer.value(0)==26&&uiStatus().indexOf("\"dice_tapping\":false")>=0,"5 s hold leaves, -1 taken back");
  for(int i=0;i<11;i++)tick();
#endif
  // The D&D sheet: a click goes to the next block, the menu rolls the shown stat's check.
  dicer.useCharacter(1);tick();snap("dice-sheet");
#if !defined(MM_JOYSTICK)
  key(13);must(uiStatus().indexOf("\"sheet_block\":1")>=0,"a click: the next block");
#endif
  {uint32_t rolls=dicer.rolls;key(hold);key(run);must(dicer.rolls==rolls+1&&String(dicer.last()->formula).startsWith("d20"),"the menu rolls the check");snap("dice-sheet-roll");}
  dicer.useCharacter(2);for(int i=0;i<11;i++)tick();snap("dice-sheet-ret");
#if !defined(MM_JOYSTICK)
  {int want=dicer.find("cha");for(int k=0;k<80&&uiStatus().indexOf((std::string("\"sheet_block\":")+std::to_string(want)+",").c_str())<0;k++)key(13);}
#endif
  dicer.roll("r5+3",dicer.sheet.block[dicer.find("cha")].name);snap("dice-sheet-ret-roll");
  dicer.useCharacter(0);dicer.setMode(dice::Rpg);
#else
  tile("dice");shot("home-dice");key(13);
  if(uiStatus().indexOf("\"page\":\"dice\"")<0){printf("FAIL dice page: %s\n",uiStatus().c_str());exit(1);}
  shot("dice-empty");key(13);shot("dice-rpg");must(dicer.resultCount==1&&dicer.last()->parts==2,"OK rolls the pool");
  dicer.roll("d4+d6+d8+d10+d12+d20");shot("dice-shapes");
  dicer.roll("d66+d%+d66");shot("dice-d66");
  key('s');shot("dice-saved");key(0xb6);key(13);must(String(dicer.last()->label)==(ru?"Атака мечом":"Sword attack"),"saved roll from the list");shot("dice-saved-roll");
  key('e');for(int i=0;i<5;i++)key(8);for(char ch:std::string("4d6-2"))key(ch);shot("dice-formula");key(13);must(String(dicer.last()->formula)=="4d6-2","typed formula");
  key('m');for(int i=0;i<10;i++)key(0xb7);key('t');key(13);must(dicer.gridRolled&&dicer.s.gridChosen==10&&dicer.s.thresholdOn,"grid roll");shot("dice-grid");
  key('m');shot("dice-counters");key('+');must(dicer.value(0)==25,"+ on the sheet");key('v');for(int i=0;i<3;i++)key(8);key('-');key('7');key(13);must(dicer.value(0)==18,"V: -7");
  // Characters: a new one from the R.E.T. template; the D&D sheet: arrows, OK rolls a check.
  key('p');shot("dice-characters");key(0xb6);key(13);must(dicer.s.character==1,"choose Arwen");shot("dice-sheet");
  key(0xb7);{uint32_t rolls=dicer.rolls;key(13);must(dicer.rolls==rolls+1&&String(dicer.last()->formula)=="d20+3"&&String(dicer.last()->label)==dicer.sheet.block[1].name,"OK rolls the DEX check");}shot("dice-sheet-roll");
  for(int i=0;i<4;i++)tick();key(0xb6);key(0xb6);shot("dice-sheet-down");
  auto choose=[&](int want){for(int k=0;k<80&&uiStatus().indexOf((std::string("\"sheet_block\":")+std::to_string(want)+",").c_str())<0;k++)key(0xb7);must(uiStatus().indexOf((std::string("\"sheet_block\":")+std::to_string(want)+",").c_str())>=0,"choose a block");};
  {int atk=-1;for(unsigned i=0;i<dicer.sheet.count;i++)if(String(dicer.sheet.block[i].formula)=="d20+@str+@pb")atk=i;choose(atk);key('i');must(uiStatus().indexOf("\"sheet_note\":true")>=0,"I opens the note");shot("dice-sheet-note");key('i');}
  // The builder: E on a block, the arrows change its kind/rule/shape, OK types a formula, A adds a block, DEL twice deletes.
  {choose(0);unsigned blocks=dicer.sheet.count;key('e');must(uiStatus().indexOf("\"dice_sheet\":\"block\"")>=0,"E opens the builder");
   for(int k=0;k<6;k++)key(0xb6);key(0xb7);must(dicer.sheet.block[0].rule==dice::Bonus,"the rule by the arrow");key(0xb4);
   key(0xb5);key(0xb7);must(dicer.sheet.block[0].shape==dice::Square,"the shape by the arrow");shot("dice-builder");key(0xb4);
   for(int k=0;k<3;k++)key(0xb6);key(13);must(uiStatus().indexOf("\"editing\":true")>=0,"OK types the formula");for(char ch:std::string("10+@dex"))key(ch);shot("dice-builder-formula");key(13);
   must(String(dicer.sheet.block[0].formula)=="10+@dex"&&dicer.value(0)==13,"a typed formula computes");
   key(13);for(int k=0;k<10;k++)key(8);key(13);must(!dicer.sheet.block[0].formula[0],"the formula cleared");
   key('a');must(dicer.sheet.count==blocks+1&&uiStatus().indexOf("\"sheet_block\":1,")>=0,"A adds a block after it");key(0xb7);key(0xb7);must(dicer.sheet.block[1].kind==dice::Pips&&dicer.fresh(1)&&dicer.sheet.block[1].max==3,"a new block takes the defaults of its kind");shot("dice-builder-new");
   key(8);key(8);must(dicer.sheet.count==blocks&&uiStatus().indexOf("\"dice_sheet\":\"main\"")>=0,"DEL twice deletes");}
  key('p');key(0xb6);key(13);must(dicer.s.character==2,"choose Yan");choose(0);dicer.roll("r5+3",dicer.sheet.block[dicer.find("cha")].name);shot("dice-sheet-ret");
  choose(dicer.find("insp"));for(int k=0;k<3;k++)tick();shot("dice-sheet-ret-low");
  key('p');key('a');for(char ch:std::string(ru?"Mira":"Mira"))key(ch);key(13);must(uiStatus().indexOf("\"dice_sheet\":\"templates\"")>=0,"name, then the template");shot("dice-templates");
  key(0xb6);key(0xb6);key(13);must(dicer.s.characters==4&&dicer.find("mind")>=0,"a R.E.T. character from the screen");
  dicer.command("dice char del");
  // A sheet from a contact: the mark by the name, the offer page.
  {dicer.useCharacter(1);previewSent.clear();dicer.command("dice sheet send "+meshRadio.idText(peerId(0)));size_t done=0;
   while(done<previewSent.size()){PreviewSent p=previewSent[done++];dicer.delivery(p.id,ChatMessage::Delivered);dicer.receive(peerId(0),"Heltec V4",p.text.c_str());}dicer.tick();
   key('p');shot("dice-characters-offer");key('o');shot("dice-offer");key(13);must(dicer.s.characters==4&&!dicer.offered,"accepted from the screen");dicer.command("dice char del");
   dicer.useCharacter(1);key('p');key('s');shot("dice-send");key(0x86);key(0x86);}
  dicer.useCharacter(0);key('1');
#endif
  config.dimAfter=dim;config.autoLock=lock;printf("dice checks passed\n");}
// Apps in the chosen order ("apps" setting): hidden ones get no tile and no click.
 {auto page=[](const char* want){std::string w=std::string("\"page\":\"")+want+"\"";if(uiStatus().indexOf(w.c_str())<0){printf("FAIL apps order, want %s: %s\n",want,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"settings -chats -nodes map -dice -quick -people",sizeof config.apps);
#if defined(MM_HELTEC_V4)
  const int next=
#if defined(MM_JOYSTICK)
   0xb7; // joystick right
#else
   13;
#endif
  for(int i=0;i<4&&uiStatus().indexOf("\"page\":\"home\"")<0;i++)key(0x86);page("home");shot("heltec-apps-home");key(next);page("settings");key(next);page("gps"); // the apps the setting does not name follow in the default order
  for(int i=0;i<14&&uiStatus().indexOf("\"page\":\"home\"")<0;i++){key(next);if(uiStatus().indexOf("\"page\":\"messages\"")>=0||uiStatus().indexOf("\"page\":\"nodes\"")>=0){printf("FAIL hidden page shown\n");exit(1);}}page("home");
#else
  key(0x82);shot("home-apps");key(13);page("settings");key(0x82);key(0xb7);key(13);page("map");key(0x82);
#endif
  config.apps[0]=0;printf("apps order checks passed\n");}
#if defined(MM_ONE_BUTTON)
// Double click = back (config.doubleBack): a click waits for a second one; counting keeps its clicks immediate.
 {auto must=[](bool ok,const char* why){if(!ok){printf("FAIL double click %s: %s\n",why,uiStatus().c_str());exit(1);}};
  auto has=[](const char* s){return uiStatus().indexOf(s)>=0;};
  auto twice=[](){uiKey(13);fakeMillis+=200;uiKey(13);tick();};
  strlcpy(config.apps,"quick people dice",sizeof config.apps);config.dimAfter=0;config.autoLock=0;
  key(0x86);twice();must(has("\"page\":\"people\""),"off: two quick clicks are two clicks");
  key(0x86);config.doubleBack=true;
  uiKey(13);must(has("\"page\":\"home\"")&&has("\"click_wait\":true"),"a click waits for a second one");
  tick();must(has("\"page\":\"quick\"")&&has("\"click_wait\":false"),"a single click acts after the wait");
  key(0xa3);key(0xa3);must(has("\"app_open\":true")&&has("\"quick_targets\":true"),"hold opens Quick Send and its recipients");
  twice();must(has("\"quick_targets\":false")&&has("\"app_open\":true"),"back closes the recipients");
  twice();must(has("\"page\":\"quick\"")&&has("\"app_open\":false"),"back closes the app");
  twice();must(has("\"page\":\"home\""),"back: the previous page");
  uiKey(13);fakeMillis+=200;uiKey(0xa3);tick();must(has("\"page\":\"quick\"")&&has("\"app_open\":true"),"a hold after a click: the click first");
  key(0x86);key(13);key(13);must(has("\"page\":\"people\""),"people");key(0xa3);int32_t before=people::value();
  uiKey(13);must(people::value()==before+1&&has("\"click_wait\":false"),"counting: a click counts at once");
  twice();must(people::value()==before+3&&has("\"app_open\":true"),"counting: two quick clicks count twice");
  people::change(-3);config.doubleBack=false;key(0x86);config.apps[0]=0;
  for(int i=0;i<20&&!has("\"page\":\"settings\"");i++)key(13);key(0xa3);for(int i=0;i<5;i++)key(13);shot("heltec-settings-double");
  key(0xa3);must(config.doubleBack&&has("\"menu\":true"),"the settings menu switches it on");shot("heltec-settings-double-on");key(0xa3);
  must(!config.doubleBack,"and off");for(int i=0;i<11;i++)tick();key(0x86);printf("double click checks passed\n");}
#endif
// Quick Send, manual counting and a persistent roll mode: the production key paths.
 {auto must=[](bool ok,const char* why){if(!ok){printf("FAIL wishlist UI %s: %s\n",why,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"quick people dice",sizeof config.apps);config.dimAfter=0;config.autoLock=0;
#if defined(MM_HELTEC_V4)
#if defined(MM_JOYSTICK)
  const int step=0xb7,hold=13,choose=0xb6;
#else
  const int step=13,hold=0xa3,choose=13;
#endif
#if !defined(MM_JOYSTICK)
  key(0x86);key(step);must(uiStatus().indexOf("\"page\":\"quick\"")>=0&&uiStatus().indexOf("\"app_open\":false")>=0,"Quick Send waits for a hold");shot("heltec-quick-closed");unsigned passed=meshRadio.historyCount;key(step);
  must(uiStatus().indexOf("\"page\":\"people\"")>=0&&uiStatus().indexOf("\"app_open\":false")>=0,"click passes Quick Send");int32_t untouched=people::value();key(step);must(uiStatus().indexOf("\"page\":\"dice\"")>=0&&people::value()==untouched,"click passes the counter without counting");
  unsigned unrolled=dicer.rolls;key(step);must(uiStatus().indexOf("\"page\":\"dice\"")<0&&dicer.rolls==unrolled&&meshRadio.historyCount==passed,"click passes dice without rolling");
  key(0x86);key(step);key(hold);
#else
  key(0x86);key(step);
#endif
  must(uiStatus().indexOf("\"page\":\"quick\"")>=0&&uiStatus().indexOf("\"quick_row\":0")>=0,"open Quick Send on recipient card");shot("heltec-quick");unsigned sent=meshRadio.historyCount;key(hold);
  must(uiStatus().indexOf("\"quick_targets\":true")>=0&&meshRadio.historyCount==sent,"recipient card opens chooser without sending");shot("heltec-quick-targets");key(0x86);
  must(uiStatus().indexOf("\"page\":\"quick\"")>=0&&uiStatus().indexOf("\"quick_row\":0")>=0,"cancel returns to recipient card");key(hold);key(choose);uint64_t to=quickSend::target(1);key(hold);
  must(quickSend::state.to==to&&uiStatus().indexOf("\"quick_index\":0")>=0&&meshRadio.historyCount==sent,"recipient confirmation selects first preset without sending");for(int i=0;i<4;i++)tick();shot("heltec-quick-preset");key(hold);
  must(meshRadio.historyCount==sent+1&&String(meshRadio.history[sent].text)==quickSend::state.text[0]&&meshRadio.history[sent].destination==to,"first preset still sends its own text to chosen recipient");
  for(unsigned i=1;i<quickSend::Count;i++)key(choose);for(int i=0;i<4;i++)tick();shot("heltec-quick-last");key(hold);
  must(meshRadio.historyCount==sent+2&&String(meshRadio.history[sent+1].text)==quickSend::state.text[quickSend::Count-1]&&meshRadio.history[sent+1].destination==to,"last preset remains reachable and sends its own text");
#if defined(MM_JOYSTICK)
  key(0x86);key(step);key(0xb5);must(uiStatus().indexOf("\"quick_row\":11")>=0,"joystick up wraps to exit");key(0xb5);must(uiStatus().indexOf("\"quick_index\":9")>=0,"joystick up reaches last preset");key(0xb6);key(0xb6);must(uiStatus().indexOf("\"quick_row\":0")>=0,"joystick down wraps to recipient card");
  key(0x86);key(step);key(step);int32_t manual=people::value();key(0xb5);must(people::value()==manual+1,"joystick up counts +1");key(0xb6);must(people::value()==manual,"joystick down counts -1");
#else
  key(13);key(hold);must(uiStatus().indexOf("\"page\":\"people\"")>=0,"Quick exit advances to counter");shot("heltec-people-closed");key(hold);must(uiStatus().indexOf("\"app_open\":true")>=0,"hold opens the counter");int32_t manual=people::value();key(13);must(people::value()==manual+1,"manual +1");key(hold);shot("heltec-people-menu");key(hold);must(people::value()==manual,"manual -1 menu");
  key(hold);key(13);key(hold);must(people::tally.count==2&&people::tally.current==1,"menu adds a card and opens it");key(13);key(13);must(people::value()==2,"the new card counts");
  people::rename(1,config.lang==LangRu?"Кофе":"Coffee");fakeMillis+=4000;tick();shot("heltec-people-two");key(hold);key(13);key(hold);must(people::tally.current==0&&people::value()==manual,"menu turns to the next card");
  people::select(1);key(hold);for(int i=0;i<4;i++)key(13);key(hold);must(people::tally.count==2,"delete asks again");shot("heltec-people-delete");key(hold);must(people::tally.count==1,"second hold deletes");
#endif
  shot("heltec-people");
#if !defined(MM_JOYSTICK)
  key(hold);StaticJsonDocument<768>d;deserializeJson(d,uiStatus());unsigned n=d["menu_count"]|0;must(n>=3,"counter exit menu");for(unsigned i=0;i<n-2;i++)key(13);key(hold);must(uiStatus().indexOf("\"page\":\"dice\"")>=0,"counter exit advances to dice");key(hold);
  dicer.setMode(dice::Rpg);unsigned rolls=dicer.rolls;key(13);must(dicer.rolls==rolls+1,"short press rolls");ChatMessage m;m.source=peerId(0);strcpy(m.name,"Preview");strcpy(m.text,"message while rolling");notifications::incoming(m);tick();must(uiStatus().indexOf("\"popup\":false")>=0,"message does not take the roll button");key(13);must(dicer.rolls==rolls+2,"next short press rolls again");config.dimAfter=10;fakeMillis+=12000;tick();must(uiScreenOff(),"dice screen dims");key(13);must(!uiScreenOff()&&dicer.rolls==rolls+3,"dark press wakes and rolls");key(hold);deserializeJson(d,uiStatus());n=d["menu_count"]|0;for(unsigned i=0;i<n-2;i++)key(13);key(hold);must(uiStatus().indexOf("\"page\":\"dice\"")<0,"exit through menu");
#endif
#else
  key(0x82);key(13);must(uiStatus().indexOf("\"page\":\"quick\"")>=0,"open Quick Send");shot("quick");
  unsigned sent=meshRadio.historyCount;key(13);must(uiStatus().indexOf("\"quick_targets\":true")>=0&&meshRadio.historyCount==sent,"first row opens recipients without sending");shot("quick-targets");
  key(0x86);must(uiStatus().indexOf("\"selected\":0")>=0,"cancel returns to recipient card");key(13);key(13);
  must(uiStatus().indexOf("\"quick_index\":0")>=0,"recipient confirmation selects first preset");shot("quick-preset");key(13);
  must(meshRadio.historyCount==sent+1&&String(meshRadio.history[sent].text)==quickSend::state.text[0],"first preset still sends its own text");
  key(0xb5);key(0xb5);shot("quick-last");key(13);
  must(meshRadio.historyCount==sent+2&&String(meshRadio.history[sent+1].text)==quickSend::state.text[quickSend::Count-1],"last preset remains reachable and sends its own text");
  key(0x86);key(0xb7);key(13);int32_t manual=people::value();key(13);must(people::value()==manual+1,"counter +1");key(0xb6);must(people::value()==manual,"counter -1");
  bool ru=config.lang==LangRu||config.lang==LangUk;people::rename(0,ru?"Гости":"Guests");people::set(0,27);shot("people");
  key(0xb7);must(uiStatus().indexOf("\"tally_new\":true")>=0,"right of the last card: a new one");shot("people-new");key(13);must(people::tally.count==2&&people::tally.current==1,"OK adds a card");
  key('n');for(char ch:std::string(ru?"Kofe":"Coffee"))key(ch);shot("people-name");key(13);key(13);key(13);must(people::value()==2,"the new card counts on its own");
  key(0xb4);must(people::tally.current==0&&people::value()==27,"left turns back to the first card");key(0xb7);key(0x08);key(0x08);must(people::tally.count==1,"DEL twice deletes");
  people::add(ru?"Круги":"Laps");people::set(1,12);people::recolor(1,7);people::add(ru?"Подходы":"Sets");people::set(2,4);people::recolor(2,5);people::select(0);fakeMillis+=4000;tick();shot("people-cards");
  people::remove(2);people::remove(1);key(0x86);
#endif
  config.apps[0]=0;printf("wishlist UI checks passed\n");}
// Wardrive: off, then listening and pings along a street; its points on the map.
 {auto must=[](bool ok,const char* why){if(!ok){printf("FAIL wardrive UI %s: %s\n",why,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"wardrive",sizeof config.apps);config.dimAfter=0;config.autoLock=0;
#if defined(MM_HELTEC_V4)&&!defined(MM_JOYSTICK)
  for(int i=0;i<4&&uiStatus().indexOf("\"page\":\"home\"")<0;i++)key(0x86);key(13);must(uiStatus().indexOf("\"page\":\"wardrive\"")>=0,"open wardrive");shot("heltec-wardrive-off");
  key(0xa3);shot("heltec-wardrive-menu");key(0xa3);must(wardrive::running(),"menu starts it");wardriveData();tick();shot("heltec-wardrive");
  key(0xa3);key(13);key(13);key(0xa3);shot("heltec-wardrive-repeaters");key(0xa3);key(13);key(13);key(0xa3);key(0x86);
#elif defined(MM_HELTEC_V4)
  wardriveData(); // the joystick board: the same screens, reached as on the other one-button boards
#else
  key(0x86);key(0x82);key(13);must(uiStatus().indexOf("\"page\":\"wardrive\"")>=0,"open wardrive");shot("wardrive-off");
  key(13);must(wardrive::settings.passive&&wardrive::settings.ping,"OK starts listening and pings");wardriveData();tick();shot("wardrive");
  key(0x85);bool follow=maps.follow;maps.follow=false;maps.center(55.7975,49.1137);shot("wardrive-map");maps.follow=follow;key(0x82);
#endif
  {StaticJsonDocument<64>d;d["action"]="stop";wardrive::command(d.as<JsonObjectConst>());}config.apps[0]=0;printf("wardrive UI checks passed\n");}
// Power off last: the screens stop taking keys once it is requested.
#if defined(MM_HELTEC_V4)
 powerOff();shot("heltec-poweroff");uiFarewell();shot("heltec-poweroff-done");
#else
 key(0x90);key(0xb5);shot("settings-poweroff");key(13);key(13);
 if(!powerOffPending()){printf("FAIL Turn off needs two OK\n");exit(1);}
 shot("poweroff");uiFarewell();shot("poweroff-done");
#endif
}
