#include "Version.h"
#include <Arduino.h>
#include "App.h"
#include "Config.h"
#include "Hardware.h"
#include "Palette.h"
#include "MeshRadio.h"
#include "BleDiagnostics.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "WifiDiagnostics.h"
#include "Internet.h"
#include "ChessNet.h"
#include "ChessTour.h"
#include "ChessSync.h"
#include "Pet.h"
#include "Dice.h"
#include "Board.h"
#include "Power.h"
#include "Companion.h"
#include "QuickSend.h"
#include "Notifications.h"
#include "People.h"
#include "Wardrive.h"
#include <Wire.h>
#include <esp_system.h>
#include <memory>
#include <new>
#include <utility>
#if defined(MM_NATIVE_USB)
#include <hal/usb_serial_jtag_ll.h>
#endif
#if defined(MM_NRF52)
#include <LittleFS.h>
#include "HistoryReply.h"
bool mountStorage();
#endif
namespace {
uint8_t* usbBytes=nullptr;
std::unique_ptr<String> usbText;
#if defined(MM_NRF52)
std::unique_ptr<HistoryReply> usbHistory;
#endif
bool usbBusy(){return usbBytes||usbText
#if defined(MM_NRF52)
 ||usbHistory
#endif
;}
size_t usbSize=0,usbOffset=0;
unsigned usbBaud=115200,pendingBaud=0;uint32_t baudExpires=0;
// MeshCore companion apps over USB: '<', a 16-bit length and the frame in; '>', the length and the frame out.
// A text command on the port ends that link, so the frames never mix with text replies.
uint8_t appFrame[companion::MaxFrame];int appState=-1;size_t appLength=0,appGot=0; // -1: text; 0, 1: length; 2: frame
void usbFrame(const uint8_t* frame,size_t length) {
  usbSize=length+3;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);if(!usbBytes){usbSize=0;return;}
  usbBytes[0]='>';usbBytes[1]=length&255;usbBytes[2]=length>>8;memcpy(usbBytes+3,frame,length);
}
void usbLine(String value) {
  // Own the command's existing string instead of allocating a second full JSON reply.
  usbText.reset(new(std::nothrow) String(std::move(value)));usbOffset=0;
  if(!usbText){usbSize=0;Serial.println("ERR USB output allocation");return;}
  usbSize=usbText->length()+1; // newline is sent separately, without growing the buffer
}
#if defined(MM_HIRES)
// Screenshot of the TFT: usbBytes holds the header and the palette indices (32 KB; RGB565 would
// take 64 KB, more than one free block), usbTick() sends them as RGB565.
size_t usbHead=0;
#endif
void usbScreenshot() {
#if defined(MM_HIRES)
  // The TFT canvas holds palette indices: the screenshot is the screen in RGB565.
  unsigned width=HiresCanvas::Width,height=HiresCanvas::Height,size=width*height*2;
#else
  unsigned width=hardware.canvas->width(),height=hardware.canvas->height(),size=width*height*2;
#endif
  char header[64];unsigned length=snprintf(header,sizeof(header),"RGB565 %u %u %u\n",width,height,size);
#if defined(MM_HIRES)
  usbSize=length+size+1;usbOffset=0;usbBytes=(uint8_t*)malloc(length+width*height); // a copy of the palette indices
#else
  usbSize=length+size+1;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);
#endif
  if(!usbBytes) {usbSize=0;Serial.println("ERR screenshot allocation");return;}
  memcpy(usbBytes,header,length);
#if defined(MM_HIRES)
  memcpy(usbBytes+length,hardware.canvas->getBuffer(),width*height);usbHead=length; // usbTick() converts and ends the line
#else
  memcpy(usbBytes+length,hardware.canvas->getBuffer(),size);usbBytes[usbSize-1]='\n';
#endif
}
void usbTick() {
#if defined(MM_NATIVE_USB)
  // The Arduino HWCDC driver can leave bytes queued when its SOF connection
  // check briefly reports disconnection. Kick the FIFO without clearing it.
  if(HWCDC::isPlugged()&&Serial.availableForWrite()<2048){usb_serial_jtag_ll_txfifo_flush();usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);}
#endif
  if(!usbBusy())return;
  int available=Serial.availableForWrite();if(available<=0)return;
  size_t count=usbSize>=usbOffset?min(size_t(available),min(size_t(256),usbSize-usbOffset)):0;
#if defined(MM_NRF52)
  if(usbHistory){
    size_t length=0;const uint8_t* bytes=usbHistory->peek(length);
    if(length)usbHistory->advance(Serial.write(bytes,min(size_t(available),min(length,size_t(256)))));
    if(usbHistory->finished()){bool okay=usbHistory->okay();usbHistory.reset();if(!okay)usbLine("\nERR history stream memory");}return;
  }
#endif
  if(usbText){
    if(usbOffset<usbText->length())usbOffset+=Serial.write((const uint8_t*)usbText->c_str()+usbOffset,min(count,size_t(usbText->length()-usbOffset)));
    else usbOffset+=Serial.write(uint8_t('\n'));
    if(usbOffset==usbSize){usbText.reset();usbSize=usbOffset=0;}return;
  }
#if defined(MM_HIRES)
  if(usbHead) {
    uint8_t chunk[256];
    for(size_t n=0;n<count;n++){size_t at=usbOffset+n;
      if(at<usbHead)chunk[n]=usbBytes[at];else if(at==usbSize-1)chunk[n]='\n';
      else{uint8_t i=usbBytes[usbHead+(at-usbHead)/2];uint16_t c=palette565[i<ColCount?i:ColInk];chunk[n]=(at-usbHead)%2?c>>8:c;}}
    usbOffset+=Serial.write(chunk,count);
    if(usbOffset==usbSize){free(usbBytes);usbBytes=nullptr;usbSize=usbOffset=usbHead=0;}
    return;
  }
#endif
  usbOffset+=Serial.write(usbBytes+usbOffset,count);
  if(usbOffset==usbSize) {free(usbBytes);usbBytes=nullptr;usbSize=usbOffset=0;}
}
}
#if defined(MM_BOOT_TRACE)
// Diagnostic builds only (-D MM_BOOT_TRACE): wait for the USB host, print each start step, and
// before storage is mounted serve "flashread ADDR LEN" until "go", so storage can be saved first.
#define BOOT(step) do{Serial.println("BOOT " step);Serial.flush();delay(30);}while(0)
static void bootWindow(){
  uint32_t t=millis();while(!Serial&&millis()-t<20000)delay(10);delay(200);
  Serial.println("BOOT trace: flashread ADDR LEN, go");String line;t=millis();
  while(millis()-t<60000){
    if(!Serial.available()){delay(2);continue;}char c=Serial.read();if(c!='\n'){if(c!='\r')line+=c;continue;}
    if(line=="go")break;
    if(line.startsWith("flashread ")){uint32_t at=strtoul(line.c_str()+10,nullptr,16),n=0;int sp=line.indexOf(' ',10);if(sp>0)n=strtoul(line.c_str()+sp+1,nullptr,16);
      if(n&&n<=2048&&at>=0x1000&&at+n<=0x100000){String h="FLASH "+String(at,HEX)+" ";for(uint32_t i=0;i<n;i++){uint8_t v=*(const uint8_t*)(at+i);h+="0123456789abcdef"[v>>4];h+="0123456789abcdef"[v&15];}Serial.println(h);}
      else Serial.println("ERR flashread");t=millis();}
    line="";
  }
}
#else
#define BOOT(step) do{}while(0)
#endif
void appSetup() {
  powerBootCheck(); // woken from power off by a short press: back to sleep
#if defined(MM_NRF52)
  Serial.begin(115200);delay(300);
#if defined(MM_BOOT_TRACE)
  bootWindow();
#endif
  BOOT("mount");mountStorage(); // the settings live in the same storage
#else
  Serial.setRxBufferSize(2048);Serial.setTxBufferSize(2048);Serial.begin(115200);delay(300);
#endif
  Serial.printf("\n" MESHMM_FIRMWARE " / " MM_BOARD_NAME " / reset=%d\n",esp_reset_reason());
  BOOT("config");config.load();BOOT("clock");hardware.beginClock();BOOT("hardware");hardware.begin();
  BOOT("chess");chessNet.begin();tour::net.begin();BOOT("radio");meshRadio.begin();BOOT("pet");creature.begin();BOOT("dice");dicer.begin();BOOT("maps");maps.begin();
#if !defined(MM_NO_WIFI)
  internet.begin(); // a server keeps Wi-Fi for the device page only
#endif
  quickSend::begin();people::begin();wardrive::begin();BOOT("navigation");navigation.begin();BOOT("portal");portalBegin();BOOT("ui");uiBegin();
  // Bluetooth left on comes back after a restart (power, RESET, auto-reset of a USB-UART bridge), unless the restart was a crash.
#if !defined(MM_EMULATOR) // QEMU has no radio
  {esp_reset_reason_t r=esp_reset_reason();if(config.bleOn&&r!=ESP_RST_PANIC&&r!=ESP_RST_INT_WDT&&r!=ESP_RST_TASK_WDT&&r!=ESP_RST_WDT){BOOT("ble");bleToggle();}}
#endif
  BOOT("selftest");
  Serial.println(meshRadio.selfTest()?"SELFTEST crypto/UTF-8/tamper PASS":"SELFTEST FAIL");
  Serial.println("READY: USB commands are available; type help");
}
void appLoop() {
  hardware.tick();meshRadio.tick();if(config.role==RoleNormal){chessNet.tick();tour::net.tick();ledger::exchange.tick();} // games wait for the normal mode
#if !defined(MM_NO_WIFI)
  internet.tick();
#endif
  maps.tick();navigation.tick();radar.tick();people::tick();wardrive::tick();notifications::tick();creature.tick();dicer.tick();
  int key=hardware.readKey();if(key){powerWake();uiKey(key);}
#if defined(MM_BOARD_TDECK)
  {int x=0,y=0;char touch=hardware.readTouch(x,y);if(touch){powerWake();uiTouch(touch,x,y);}}
#endif
  static String command;
  unsigned budget=256;
  if(!usbBusy()&&Serial.available())powerWake();
  while(!usbBusy() && Serial.available() && budget--) {
    char c=Serial.read();baudExpires=millis()+10000;
    if(appState>=0) {
      uint8_t b=c;
      if(appState==0){appLength=b;appState=1;}
      else if(appState==1){appLength|=size_t(b)<<8;appGot=0;appState=appLength?2:-1;}
      else{if(appGot<sizeof(appFrame))appFrame[appGot]=b;if(++appGot>=appLength){appState=-1;if(appLength<=sizeof(appFrame))companion::command(appFrame,appLength,companion::LinkUsb);}}
      continue;
    }
    if(c=='<'&&!command.length()){appState=0;continue;}
    if(c=='\n') {
      companion::disconnected(companion::LinkUsb);
      if(command.startsWith("baud ")) {
#if defined(MM_NATIVE_USB) || defined(MM_NRF52)
        usbLine("ERR native USB does not need baud switching");
#else
        unsigned rate=command.substring(5).toInt();if(rate==115200||rate==460800||rate==921600){pendingBaud=rate;usbLine("OK USB baud switching");}else usbLine("ERR baud 115200/460800/921600");
#endif
      }
      else if(command=="screenshot")usbScreenshot();
      // Research: raw CSI lines ("CSI us rssi iqhex") while on; beacon frames per second.
      else if(command=="csistream on"||command=="csistream off"){radar.csiStream=command.endsWith("on");usbLine(radar.csiStream?"OK CSI stream on":"OK CSI stream off");}
      else if(command.startsWith("csirate ")){radar.setBeaconHz(command.substring(8).toInt());usbLine("OK beacon "+String(radar.beaconHz)+" Hz");}
      else if(command=="bleprobe")usbLine(bleProbeResult());
      else if(command=="wifiprobe")usbLine(wifiProbeResult());
      else if(command.startsWith("wifiprobe ")){StaticJsonDocument<512>d;if(deserializeJson(d,command.substring(10)))usbLine("ERR Wi-Fi probe JSON");else usbLine(startWifiProbe(d.as<JsonObjectConst>()));}
      else if(command.startsWith("bleprobe ")) {
        StaticJsonDocument<512> options;
        if(deserializeJson(options,command.substring(9)) || !options.is<JsonObject>())usbLine("ERR bleprobe JSON");
        else usbLine(startBleProbe(options.as<JsonObjectConst>()));
      }
#if defined(MM_NRF52)
      // USB only, read-only: raw flash for backups (CURRENT.UF2 of the bootloader stops at 0xEA000).
      else if(command.startsWith("flashread ")){uint32_t at=strtoul(command.c_str()+10,nullptr,16),n=0;int sp=command.indexOf(' ',10);if(sp>0)n=strtoul(command.c_str()+sp+1,nullptr,16);
        if(sp<0||!n||n>2048||at<0x1000||at+n>0x100000)usbLine("ERR flashread ADDR LEN (hex, LEN<=800)");
        else{String hex;hex.reserve(n*2+12);hex="FLASH "+String(at,HEX)+" ";for(uint32_t i=0;i<n;i++){uint8_t v=*(const uint8_t*)(at+i);hex+="0123456789abcdef"[v>>4];hex+="0123456789abcdef"[v&15];}usbLine(hex);}}
      // USB only, hardware bring-up: "gpio N" reads a pin, "gpio N 0|1" drives it, "gpio N tone HZ" sounds it.
      else if(command.startsWith("gpio ")){int n=command.substring(5).toInt();int sp=command.indexOf(' ',5);String arg=sp>0?command.substring(sp+1):String();
        if(n<0||n>47)usbLine("ERR gpio 0..47");
        else if(!arg.length()){pinMode(n,INPUT);usbLine("GPIO "+String(n)+" "+String(digitalRead(n)));}
        else if(arg.startsWith("tone ")){tone(n,arg.substring(5).toInt(),300);usbLine("OK tone");}
        else if(arg=="pd"||arg=="pu"){pinMode(n,arg=="pd"?INPUT_PULLDOWN:INPUT_PULLUP);delay(5);usbLine("GPIO "+String(n)+" "+arg+" "+String(digitalRead(n)));}
        else if(arg=="watch"){pinMode(n,INPUT);int last=digitalRead(n),edges=0,lows=0;uint32_t start=millis();while(millis()-start<1500){int v=digitalRead(n);edges+=v!=last;lows+=!v;last=v;}usbLine("GPIO "+String(n)+" edges "+String(edges)+" lowsamples "+String(lows));}
        else{pinMode(n,OUTPUT);digitalWrite(n,arg.toInt()?HIGH:LOW);usbLine("OK gpio "+String(n)+"="+String(arg.toInt()?1:0));}}
      else if(command=="i2cscan"){String r="I2C";for(uint8_t a=1;a<127;a++){Wire.beginTransmission(a);if(!Wire.endTransmission())r+=" 0x"+String(a,HEX);}usbLine(r);}
#endif
      else if(command.startsWith("uikey ")) {uiKey(strtol(command.substring(6).c_str(),nullptr,0));usbLine("OK UI key");}
#if !defined(MM_COMPACT)
      // "uitouch t|h X Y" or "uitouch u|d|l|r": the T-Deck touch gestures, for checks without a finger.
      else if(command.startsWith("uitouch ")&&command.length()>8&&strchr("thudlr",command[8])) {int x=0,y=0;sscanf(command.c_str()+9,"%d %d",&x,&y);uiTouch(command[8],x,y);usbLine("OK UI touch");}
#endif
#if defined(MM_NRF52)
      else if(command=="messages"){usbHistory.reset(new(std::nothrow) HistoryReply);if(!usbHistory||!usbHistory->begin()){usbHistory.reset();usbLine("ERR history snapshot memory");}}
#endif
      else usbLine(executeCommand(command));command="";
    }
    else if(c!='\r' && command.length()<1024)command+=c;
    else if(command.length()>=1024) {command="";usbLine("ERR command too long");}
  }
  portalTick();uiTick();usbTick();
  if(!usbBusy()){uint8_t frame[companion::MaxFrame];size_t length=companion::next(frame,companion::LinkUsb);if(length)usbFrame(frame,length);}
  restartTick();powerOffTick();
  if(radar.csiStream&&!usbBusy()){String line;for(int i=0;i<8&&Serial.availableForWrite()>=240&&radar.streamLine(line);i++)Serial.println(line);}
#if !defined(MM_NATIVE_USB) && !defined(MM_NRF52)
  if(!usbBusy()&&(pendingBaud||(usbBaud!=115200&&int32_t(millis()-baudExpires)>=0))){Serial.flush();usbBaud=pendingBaud?pendingBaud:115200;pendingBaud=0;Serial.updateBaudRate(usbBaud);baudExpires=millis()+10000;}
#endif
#if defined(MM_NATIVE_USB) || defined(MM_NRF52)
  powerTick(!usbBusy());
#else
  powerTick(!usbBusy()&&!pendingBaud&&usbBaud==115200);
#endif
}
#if defined(MM_NRF52)
// The core's loop task has a 4 KB stack; the JSON replies need more. The application runs in
// its own task and the Arduino loop task stays suspended.
void setup() {xTaskCreate([](void*){appSetup();for(;;)appLoop();},"meshmesh",4096,nullptr,TASK_PRIO_LOW,nullptr);}
void loop() {suspendLoop();}
#else
void setup() {appSetup();}
void loop() {appLoop();}
#endif
