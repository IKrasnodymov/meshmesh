#include "Version.h"
#include <Arduino.h>
#include "App.h"
#include "Config.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "BleDiagnostics.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "WifiDiagnostics.h"
#include "Internet.h"
#include <esp_system.h>
#if defined(MM_HELTEC_V4)
#include <hal/usb_serial_jtag_ll.h>
#endif
namespace {
uint8_t* usbBytes=nullptr;
size_t usbSize=0,usbOffset=0;
unsigned usbBaud=115200,pendingBaud=0;uint32_t baudExpires=0;
void usbLine(const String& value) {
  usbSize=value.length()+1;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);
  if(!usbBytes) {usbSize=0;Serial.println("ERR USB output allocation");return;}
  memcpy(usbBytes,value.c_str(),value.length());usbBytes[usbSize-1]='\n';
}
void usbScreenshot() {
  unsigned width=hardware.canvas->width(),height=hardware.canvas->height(),size=width*height*2;
  char header[64];unsigned length=snprintf(header,sizeof(header),"RGB565 %u %u %u\n",width,height,size);
  usbSize=length+size+1;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);
  if(!usbBytes) {usbSize=0;Serial.println("ERR screenshot allocation");return;}
  memcpy(usbBytes,header,length);memcpy(usbBytes+length,hardware.canvas->getBuffer(),size);usbBytes[usbSize-1]='\n';
}
void usbTick() {
#if defined(MM_HELTEC_V4)
  // The Arduino HWCDC driver can leave bytes queued when its SOF connection
  // check briefly reports disconnection. Kick the FIFO without clearing it.
  if(HWCDC::isPlugged()&&Serial.availableForWrite()<2048){usb_serial_jtag_ll_txfifo_flush();usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);}
#endif
  if(!usbBytes)return;
  int available=Serial.availableForWrite();if(available<=0)return;
  size_t count=min(size_t(available),min(size_t(256),usbSize-usbOffset));
  usbOffset+=Serial.write(usbBytes+usbOffset,count);
  if(usbOffset==usbSize) {free(usbBytes);usbBytes=nullptr;usbSize=usbOffset=0;}
}
}
void setup() {
  Serial.setRxBufferSize(2048);Serial.setTxBufferSize(2048);Serial.begin(115200);delay(300);
#if defined(MM_HELTEC_V4)
  Serial.printf("\n" MESHMM_FIRMWARE " / Heltec V4 / reset=%d\n",esp_reset_reason());
#else
  Serial.printf("\n" MESHMM_FIRMWARE " / ThinkNode M9 / reset=%d\n",esp_reset_reason());
#endif
  config.load();hardware.beginClock();hardware.begin();meshRadio.begin();maps.begin();
#if !defined(MM_HELTEC_V4)
  internet.begin();
#endif
  navigation.begin();portalBegin();uiBegin();
  Serial.println(meshRadio.selfTest()?"SELFTEST crypto/UTF-8/tamper PASS":"SELFTEST FAIL");
  Serial.println("READY: USB commands are available; type help");
}
void loop() {
  hardware.tick();meshRadio.tick();
#if !defined(MM_HELTEC_V4)
  internet.tick();
#endif
  maps.tick();navigation.tick();radar.tick();
  int key=hardware.readKey();if(key)uiKey(key);
  static String command;
  unsigned budget=256;
  while(!usbBytes && Serial.available() && budget--) {
    char c=Serial.read();baudExpires=millis()+10000;
    if(c=='\n') {
      if(command.startsWith("baud ")) {
#if defined(MM_HELTEC_V4)
        usbLine("ERR native USB does not need baud switching");
#else
        unsigned rate=command.substring(5).toInt();if(rate==115200||rate==460800||rate==921600){pendingBaud=rate;usbLine("OK USB baud switching");}else usbLine("ERR baud 115200/460800/921600");
#endif
      }
      else if(command=="screenshot")usbScreenshot();
      // Research: raw CSI lines ("CSI us rssi iqhex") while on; beacon frames per second.
      else if(command=="csistream on"||command=="csistream off"){radar.csiStream=command.endsWith("on");usbLine(radar.csiStream?"OK CSI stream on":"OK CSI stream off");}
      else if(command.startsWith("csirate ")){radar.setBeaconHz(command.substring(8).toInt());usbLine("OK beacon "+String(radar.beaconHz)+" Hz");}
      else if(command=="connections")usbLine(connectionCredentials());
      else if(command=="bleprobe")usbLine(bleProbeResult());
      else if(command=="wifiprobe")usbLine(wifiProbeResult());
      else if(command.startsWith("wifiprobe ")){StaticJsonDocument<512>d;if(deserializeJson(d,command.substring(10)))usbLine("ERR Wi-Fi probe JSON");else usbLine(startWifiProbe(d.as<JsonObjectConst>()));}
      else if(command.startsWith("bleprobe ")) {
        StaticJsonDocument<512> options;
        if(deserializeJson(options,command.substring(9)) || !options.is<JsonObject>())usbLine("ERR bleprobe JSON");
        else usbLine(startBleProbe(options.as<JsonObjectConst>()));
      }
      else if(command.startsWith("uikey ")) {uiKey(strtol(command.substring(6).c_str(),nullptr,0));usbLine("OK UI key");}
      else usbLine(executeCommand(command));command="";
    }
    else if(c!='\r' && command.length()<1024)command+=c;
    else if(command.length()>=1024) {command="";usbLine("ERR command too long");}
  }
  portalTick();uiTick();usbTick();
  if(radar.csiStream&&!usbBytes){String line;for(int i=0;i<8&&Serial.availableForWrite()>=240&&radar.streamLine(line);i++)Serial.println(line);}
#if !defined(MM_HELTEC_V4)
  if(!usbBytes&&(pendingBaud||(usbBaud!=115200&&int32_t(millis()-baudExpires)>=0))){Serial.flush();usbBaud=pendingBaud?pendingBaud:115200;pendingBaud=0;Serial.updateBaudRate(usbBaud);baudExpires=millis()+10000;}
#endif
  delay(2);
}
