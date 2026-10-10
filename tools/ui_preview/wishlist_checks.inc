// Exercise the production modules with in-memory storage and a deterministic clock.
static void wishlistChecks(){
 auto must=[](bool ok,const char* what){if(!ok){printf("FAIL wishlist %s\n",what);exit(1);}};
 auto runQuick=[](const char* action,int index=-1,const char* text=nullptr,const char* to=nullptr){StaticJsonDocument<512>d;d["action"]=action;if(index>=0)d["index"]=index;if(text)d["text"]=text;if(to)d["to"]=to;return quickSend::command(d.as<JsonObjectConst>());};
 auto runPeople=[](const char* action,const char* key=nullptr,int value=0){StaticJsonDocument<128>d;d["action"]=action;if(key)d[key]=value;return people::command(d.as<JsonObjectConst>());};
 bool fs=hardware.fsOk;hardware.fsOk=true;
 struct Value {uint32_t number;};Value value{};
 must(blob::save("/meshmesh/check",7,Value{1})&&blob::save("/meshmesh/check",7,Value{2}),"two verified writes");
 previewFs::files()["/meshmesh/check.0"][4]^=0x80;
 must(blob::load("/meshmesh/check",7,value)&&value.number==1,"corrupt generation recovers the previous slot");
 must(blob::save("/meshmesh/check",7,Value{3})&&blob::load("/meshmesh/check",7,value)&&value.number==3,"write after interrupted slot");
 must(notifications::mentioned("@[ИГОРЬ] приём","Игорь","")&&!notifications::mentioned("Игорь пришёл","Игорь",""),"Cyrillic explicit mention");
 must(notifications::mentioned("Еду в КАЗАНЬ","Игорь","самара, Казань"),"configured words fold case");
 must(!notifications::enabled(0,true)&&notifications::enabled(2,true)&&!notifications::enabled(2,false),"notification modes");
 unsigned savedCount=meshRadio.historyCount;std::vector<ChatMessage> history(meshRadio.history,meshRadio.history+savedCount);
 auto originalPolicy=meshRadio.channelList[0].policy;meshRadio.channelList[0].policy.priority=0;meshRadio.channelList[0].policy.deviceLimit=8;
 meshRadio.historyCount=0;for(unsigned i=0;i<20;i++)meshRadio.sendMessage("retention "+String(i));
 must(meshRadio.historyCount==8&&String(meshRadio.history[0].text)=="retention 12","per-channel cap removes oldest rows");
 meshRadio.historyCount=64;for(unsigned i=0;i<64;i++){meshRadio.history[i]={};meshRadio.history[i].destination=peerId(0);meshRadio.history[i].id=i;}
 for(unsigned i=0;i<20;i++)meshRadio.sendMessage("low priority "+String(i));
 unsigned low=0;for(unsigned i=0;i<64;i++)if(meshRadio.history[i].destination==meshmesh::Broadcast)low++;
 must(meshRadio.historyCount==64&&low==8&&meshRadio.history[0].id==8&&String(meshRadio.history[63].text)=="low priority 19","full history grants the bounded channel its eight rows");
 meshRadio.sendMessage("ordinary chat",peerId(0));low=0;for(unsigned i=0;i<64;i++)if(meshRadio.history[i].destination==meshmesh::Broadcast)low++;
 must(low==7&&meshRadio.history[0].id==8,"ordinary traffic evicts the oldest low-priority row first");
 {HistoryReply reply;must(reply.begin(),"stream snapshot allocated");String expected=meshRadio.history[63].text;strcpy(meshRadio.history[63].text,"changed after snapshot");String output;
  while(!reply.finished()){size_t size=0;const uint8_t* bytes=reply.peek(size);must(size&&bytes&&reply.okay(),"stream prepared");size=std::min(size,size_t(7));for(size_t i=0;i<size;i++)output+=char(bytes[i]);reply.advance(size);}
  DynamicJsonDocument decoded(65536);must(!deserializeJson(decoded,output)&&decoded.as<JsonArray>().size()==64&&decoded[63]["text"]==expected,"partial writes keep a complete immutable snapshot");}
 // The same production compactor survives both a partial file and a failed rename.
 previewFs::files().erase("/meshmesh/history.jsonl");previewFs::files().erase("/meshmesh/history.previous.jsonl");previewFs::files().erase("/meshmesh/history.next.jsonl");
 meshRadio.historyCount=0;nextId=0;for(unsigned i=0;i<20;i++)meshRadio.sendMessage("storage check");
 must(meshRadio.historyCount==8,"storage retains cap");meshRadio.trimHistory();for(unsigned i=0;i<4;i++)meshRadio.tick();meshRadio.historyCount=0;meshRadio.restoreHistory();must(meshRadio.historyCount==8&&meshRadio.history[0].id==13,"partial snapshot recovers old complete log");
 for(unsigned i=0;i<8;i++)meshRadio.tick();previewFs::renameFailures=1;meshRadio.tick();
 meshRadio.sendMessage("after rename failure");for(unsigned i=0;i<70;i++)meshRadio.tick();meshRadio.historyCount=0;meshRadio.restoreHistory();must(meshRadio.historyCount==8&&meshRadio.history[0].id==14&&meshRadio.history[7].id==21,"failed commit retries without losing a new message");
 for(const char* path:{"/meshmesh/history.jsonl","/meshmesh/history.previous.jsonl","/meshmesh/history.next.jsonl"})previewFs::files().erase(path);
 meshRadio.channelList[0].policy=originalPolicy;meshRadio.historyCount=savedCount;std::copy(history.begin(),history.end(),meshRadio.history);
 auto quick=quickSend::state;
 must(runQuick("set",0,"Принято\nНа месте").startsWith("OK"),"multiline template saved");quickSend::begin();must(String(quickSend::state.text[0])=="Принято\nНа месте","template survives reload");
 must(runQuick("target",-1,nullptr,"1122334455667788").startsWith("ERR"),"missing target rejected");
 unsigned before=meshRadio.historyCount;must(quickSend::send(0,0x1122334455667788ULL).startsWith("ERR")&&meshRadio.historyCount==before,"missing target never broadcasts");
 std::string longText(161,'x');must(runQuick("set",0,longText.c_str()).startsWith("ERR"),"template UTF-8 byte cap");
 must(runQuick("set",0,std::string(160,'x').c_str()).startsWith("OK")&&quickSend::send(0).startsWith("ERR"),"recipient limit enforced at send");
 quickSend::state=quick;
 auto gps=hardware.gps;bool trust=hardware.clockTrusted,conflict=hardware.clockConflict;String source=hardware.clockSource;
 hardware.gps.location.valid=true;hardware.gps.date.valid=true;hardware.gps.time.valid=true;hardware.gps.satellites.v=10;
 must(hardware.setUtc(uint32_t(time(nullptr)),"phone",true)&&hardware.clockConflict,"phone time does not trust a conflicting GPS fix");
 hardware.gps.date.valid=false;must(hardware.setUtc(uint32_t(time(nullptr)),"phone",true)&&!hardware.clockConflict,"phone time with no GPS conflict");
 hardware.gps=gps;hardware.clockTrusted=trust;hardware.clockConflict=conflict;hardware.clockSource=source;
 auto peopleSettings=people::settings;people::settings.ble=true;people::settings.rssi=-85;people::settings.window=30;people::resetWindow();
 people::hear(false,1,-60);people::hear(false,1,-55);people::hear(false,2,-99);must(people::count(false)==1,"BLE duplicates and RSSI threshold");
 for(unsigned i=2;i<140;i++)people::hear(false,i,-60);must(people::count(false)==128&&people::saturated(false)&&people::dropped(false)>0,"bounded device table reports saturation");
 fakeMillis+=30001;must(people::count(false)==0,"device window expires");
 must(runPeople("settings","window",-1).startsWith("ERR"),"window validation");
 people::settings.manual=0;must(!people::change(-1)&&people::change(1)&&people::settings.manual==1,"manual counter independent and bounded");people::flush();people::begin();must(people::settings.manual==1,"manual counter survives reload");
 people::settings=peopleSettings;people::resetWindow();hardware.fsOk=fs;
 notifications::Event event;while(notifications::take(event)){}
 ChatMessage sent;sent.destination=meshmesh::Broadcast;sent.packet=12345;notifications::emit(notifications::Transmitted,sent);notifications::repeatPacket(12345);fakeMillis+=30001;notifications::tick();while(notifications::take(event))must(event.kind!=notifications::Unconfirmed,"repeat outside retained history cancels timeout");
 printf("wishlist module checks passed\n");
}
