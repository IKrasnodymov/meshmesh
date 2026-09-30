// Screens rendered by the preview; the key codes are the production M9 codes.
static void scenario(){
#if defined(MM_HELTEC_V4)
 for(const char* n:{"home","messages","nodes","gps","wifi","ble","settings","modules"}){shot((std::string("heltec-")+n).c_str());if(!strcmp(n,"messages")){key(0xa3);shot("heltec-messages-menu");key(13);key(13);key(0xa3);shot("heltec-messages-older");key(13);key(13);key(0xa3);}if(!strcmp(n,"wifi")){key(0xa3);shot("heltec-wifi-on");}key(13);}
 {ChatMessage m;m.source=0xA1B2C3D40000ULL;m.destination=meshRadio.nodeId;strcpy(m.name,"Heltec V4");strcpy(m.text,"Как слышно? Жду на точке через 5 минут");m.timestamp=time(nullptr);m.id=77;meshRadio.history[meshRadio.historyCount++]=m;}shot("heltec-popup");
#else
 shot("home");key(0x81);shot("threads");key(13);shot("chat-public");key(0x86);key(0xb6);key(13);shot("chat");
 for(char ch:std::string("Privet! Kak dela? 5to novogo @ 5"))key(ch);key(0xb7);shot("compose");key(0xa3);shot("layout");key(0x86);key(0x86);key(0x86);
 key(0x85);shot("map");key(0xb7);key(0xb6);shot("map-browse");key('L');shot("library");key(0x86);
 key(0x82);key(0xb7);key(0xb7);key(13);shot("nodes");key(13);shot("node");key(0xb7);key(0xb7);key(0xb7);key(13);shot("node-forget");key(0x86);key(0x86);
 key(0x82);key(0xb6);key(13);shot("sensors");key(0x90);shot("settings");key(13);key(0xb7);shot("radio");key(0x86);key(0xb6);key(13);for(int i=0;i<8;i++)key(0xb6);key(0xb7);shot("display-battery");for(int i=0;i<8;i++)key(0xb5);shot("display");key(13);shot("display-name");key(0x86);key(0x86);
 key(0x82);key(0xb6);key(0xb7);key(13);key(13);key(0xb6);key(13);shot("network");key(13);key(0xb6);key(13);key(0x83);shot("diagnostics");key(0x90);for(int i=0;i<5;i++)key(0xb6);key(13);shot("help");key(13);shot("help-layout");key(0x86);key(13);shot("help-layout");key(0x86);
 key(0x88);shot("locked");key(0xa3);key(0x82);
#endif
}
