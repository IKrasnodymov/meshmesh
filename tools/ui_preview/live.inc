// Interactive simulator: the production UI in an SDL window, the host keyboard as the M9 keyboard.
// Built by tools/ui_preview/build.sh with SIM=1; run as "preview --live LANG". Included by preview.cpp.
// It is a layout and flow tool, not a hardware check: radio, Wi-Fi, BLE and SD are the preview stubs.
#include <SDL.h>
#include <mutex>
#include <thread>
#include <deque>
// Host key -> M9 keyboard code (src/Ui.cpp, enum Key). Printable characters come through SDL_TEXTINPUT.
static int liveKey(const SDL_Keysym& k){
 bool shift=k.mod&KMOD_SHIFT;
 switch(k.sym){
  case SDLK_LEFT:return 0xb4;case SDLK_UP:return 0xb5;case SDLK_DOWN:return 0xb6;case SDLK_RIGHT:return 0xb7;
  case SDLK_RETURN:case SDLK_KP_ENTER:return shift?0xa3:13; // Shift+Return: a held OK
  case SDLK_BACKSPACE:case SDLK_DELETE:return 8;
  case SDLK_ESCAPE:return 0x86;  // BACK
  case SDLK_F1:return 0x82;      // HOME
  case SDLK_F2:return 0x81;      // MSG
  case SDLK_F3:return 0x85;      // MAP
  case SDLK_F4:return 0x90;      // SET
  case SDLK_F5:return 0x84;      // ADV
  case SDLK_F6:return 0x87;      // GPS
  case SDLK_F7:return 0x83;      // @ (keyboard language)
  case SDLK_F8:return 0x88;      // MIC (lock)
  case SDLK_TAB:return shift?0x83:0; // Shift+Tab: @ as well
  default:return 0;}
}
// Lines typed on stdin while the window runs: "recv N TEXT" (a direct message from demo peer N, 0-5),
// "all N TEXT" (the same on Public), "key CODE" (any key code, e.g. key 0xa3), "status" (uiStatus JSON).
static std::mutex liveLock;static std::deque<std::string> liveLines;
// MeshRadio::addMessage is private; RemoteAccess is one of its friends (include/MeshRadio.h). Its firmware definition
// (src/Remote.inc, through src/MeshRadio.cpp) is not linked into the preview, so the simulator gives it this one.
struct RemoteAccess {static void add(const ChatMessage& m){meshRadio.addMessage(m,false);}};
static void liveCommand(const std::string& line){
 auto incoming=[](int peer,uint64_t to,const std::string& text){
  if(peer<0||peer>=int(meshRadio.peerCount)){printf("no peer %d (0-%u)\n",peer,meshRadio.peerCount-1);return;}
  ChatMessage m;m.source=meshRadio.peers[peer].id;m.destination=to;m.outgoing=false;m.status=ChatMessage::Received;
  m.timestamp=time(nullptr);m.id=++nextId;strncpy(m.name,meshRadio.peers[peer].name,sizeof m.name-1);strncpy(m.text,text.c_str(),sizeof m.text-1);
  RemoteAccess::add(m);meshRadio.rxCount++;printf("OK from %s\n",m.name);};
 char word[16]={};int n=0,used=0;
 if(sscanf(line.c_str(),"%15s %n",word,&used)<1)return;
 std::string cmd=word,rest=line.substr(used);
 if(cmd=="recv"||cmd=="all"){if(sscanf(rest.c_str(),"%d %n",&n,&used)<1){printf("usage: %s PEER TEXT\n",cmd.c_str());return;}
  incoming(n,cmd=="all"?meshmesh::Broadcast:meshRadio.nodeId,rest.substr(used));}
 else if(cmd=="key"){uiKey(int(strtol(rest.c_str(),nullptr,0)));}
 else if(cmd=="status"){printf("%s\n",uiStatus().c_str());}
 else printf("commands: recv PEER TEXT | all PEER TEXT | key CODE | status\n");
 fflush(stdout);
}
// The canvas as RGB565: as it is on the colour boards, through the palette on the T114, white on black on the OLEDs.
static const uint16_t* liveFrame(int W,int H){
#if defined(MM_HIRES)
 static std::vector<uint16_t> f;f.resize(W*H);const uint8_t* px=hardware.canvas->getBuffer();
 for(int i=0;i<W*H;i++)f[i]=palette565[px[i]<ColCount?px[i]:ColInk];return f.data();
#elif defined(MM_HELTEC_V4)
 static std::vector<uint16_t> f;f.resize(W*H);const uint16_t* px=hardware.canvas->getBuffer();
 for(int i=0;i<W*H;i++)f[i]=px[i]?0xffff:0;return f.data();
#else
 (void)W;(void)H;return hardware.canvas->getBuffer();
#endif
}
static int live(){
#if defined(MM_HIRES)
 const int W=HiresCanvas::Width,H=HiresCanvas::Height;
#else
 const int W=hardware.canvas->width(),H=hardware.canvas->height();
#endif
 int scale=max(1,960/W);if(const char* s=getenv("SIM_SCALE"))scale=max(1,atoi(s)); // about 960 px wide by default
 if(SDL_Init(SDL_INIT_VIDEO)!=0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0"); // sharp pixels
 SDL_Window* win=SDL_CreateWindow("MeshMesh M9",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W*scale,H*scale,SDL_WINDOW_ALLOW_HIGHDPI);
 SDL_Renderer* ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
 if(!win||!ren){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 SDL_RenderSetLogicalSize(ren,W,H);
 SDL_Texture* tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,W,H);
 SDL_StartTextInput();
 printf("MeshMesh simulator %dx%d\n"
  " arrows: move  Return: OK  Shift+Return: hold OK  Esc: BACK  Backspace: DEL\n"
  " F1 HOME  F2 MSG  F3 MAP  F4 SET  F5 ADV  F6 GPS  F7 @  F8 MIC/lock  F12 screenshot\n"
  " stdin: recv PEER TEXT | all PEER TEXT | key CODE | status\n",W,H);
 fflush(stdout);
 std::thread([]{std::string l;while(std::getline(std::cin,l)){std::lock_guard<std::mutex> g(liveLock);liveLines.push_back(l);}}).detach();
 const uint32_t base=fakeMillis-SDL_GetTicks();int shots=0;std::string title;uint32_t lastTitle=0;
 for(bool run=true;run;){
  fakeMillis=base+SDL_GetTicks();
  for(SDL_Event e;SDL_PollEvent(&e);){
   if(e.type==SDL_QUIT)run=false;
   else if(e.type==SDL_KEYDOWN){
    if(e.key.keysym.sym==SDLK_q&&(e.key.keysym.mod&KMOD_GUI)){run=false;continue;} // Cmd+Q
    if(e.key.keysym.sym==SDLK_F12){char n[32];snprintf(n,sizeof n,"sim-%d.ppm",++shots);save(n);printf("saved %s\n",n);fflush(stdout);continue;}
    int k=liveKey(e.key.keysym);if(k)uiKey(k);}
   else if(e.type==SDL_TEXTINPUT){for(const char* c=e.text.text;*c;c++)if(*c>=32&&*c<127)uiKey(*c);}}
  {std::deque<std::string> lines;{std::lock_guard<std::mutex> g(liveLock);lines.swap(liveLines);}for(auto& l:lines)liveCommand(l);}
  radar.tick();meshRadio.tick();uiTick();
  SDL_UpdateTexture(tex,nullptr,liveFrame(W,H),W*2);
  SDL_RenderClear(ren);SDL_RenderCopy(ren,tex,nullptr,nullptr);SDL_RenderPresent(ren);
  if(fakeMillis-lastTitle>500){lastTitle=fakeMillis;StaticJsonDocument<1024> d;deserializeJson(d,uiStatus().c_str());
   std::string t=std::string("MeshMesh M9 — ")+(const char*)(d["page"]|"?");if(t!=title){title=t;SDL_SetWindowTitle(win,t.c_str());}}
  SDL_Delay(16);}
 SDL_DestroyTexture(tex);SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
