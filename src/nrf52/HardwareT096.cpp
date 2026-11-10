// Heltec Mesh Node T096 (nRF52840 + SX1262 + KCT8103L front end): 0.96" 160x80 ST7735 colour TFT, one user
// button, UC6580 GPS on Serial1, green LED. The compact interface (UiHeltec.cpp) draws a 128x64 canvas; this
// file centres it on the TFT, as on the Wireless Tracker (HardwareCompact.cpp). Pins, panel set-up and the
// front end follow MeshCore's heltec_t096 variant (T096Board, LoRaFEMControl, ST7735Display).
#if defined(MM_BOARD_T096)
#pragma GCC optimize("Os") // 1 MB flash (AGENTS.md, "Языки")
#include "Hardware.h"
#include "Config.h"
#include "Palette.h"
#include <LittleFS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ST7735.h>
#include <esp_system.h>
Hardware hardware;
namespace {
// SPI1 is the 32 MHz SPIM3 (SPI_32MHZ_INTERFACE=1 in platformio.ini); LoRa stays on SPI.
Adafruit_ST7735 tft(&SPI1,pins::tftCs,pins::tftDc,pins::tftReset);
bool tftOn=false,uartOn=false;
void output(int pin,int level){if(pin<0)return;pinMode(pin,OUTPUT);digitalWrite(pin,level);}
void light(uint8_t level){if(level>=255)output(pins::tftLight,LOW);else analogWrite(pins::tftLight,255-level);} // LEDA: LOW = on
}
void Hardware::brightness(uint8_t level) {
  if(!panel)return;
  if(!level){output(pins::tftLight,HIGH);if(tftOn){tft.enableDisplay(false);tft.enableSleep(true);tftOn=false;}return;}
  if(!tftOn){tft.enableSleep(false);delay(5);tft.enableDisplay(true);tftOn=true;}
  light(level);
}
// Vext also feeds the TFT, so it stays on; the GPS has its own enable (active LOW) and reset.
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;
  output(pins::gpsReset,HIGH);output(pins::gpsEnable,enabled?pins::gpsOn:!pins::gpsOn);
  // As on the GAT562: Uart::end() of a port never started waits forever, so start and stop it once.
  if(enabled&&!uartOn){Serial1.setPins(pins::gpsRx,pins::gpsTx);Serial1.begin(115200);uartOn=true;}
  else if(!enabled&&uartOn){Serial1.end();uartOn=false;}
}
void Hardware::begin() {
  // Front end: its LDO, then CSD on; CTX LOW = receive with the LNA (MeshRadio switches it for transmissions).
  output(pins::femPower,HIGH);delay(1);output(pins::femEnable,HIGH);output(pins::femTx,LOW);
  output(pins::radioCs,HIGH);
  SPI.setPins(pins::spiMiso,pins::spiClock,pins::spiMosi);SPI.begin();
  Wire.setPins(pins::sda,pins::scl);Wire.begin();Wire.setClock(400000);
  output(pins::tftLight,HIGH);output(pins::vext,!pins::vextOn);delay(20);digitalWrite(pins::vext,pins::vextOn);delay(120);
  SPI1.setPins(0,pins::tftClock,pins::tftData); // pin 0 maps to "not connected": the panel has no MISO
  tft.initR(INITR_MINI160x80);tft.setRotation(1);
  uint8_t madctl=ST77XX_MADCTL_MY|ST77XX_MADCTL_MV|ST7735_MADCTL_BGR;tft.sendCommand(ST77XX_MADCTL,&madctl,1); // BGR, as MeshCore
  tft.setSPISpeed(32000000);tft.fillScreen(0);panel=&tft;tftOn=true;
  canvas=new GFXcanvas16(128,64);if(!canvas||!canvas->getBuffer()){Serial.println("FATAL framebuffer");while(true)delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);canvas->fillScreen(0);text(0,13,"MeshMesh");flush();
  brightness(config.brightness);
  pinMode(pins::button,INPUT_PULLUP);
  output(pins::led,!pins::ledOn);output(pins::adcEnable,LOW);pinMode(pins::battery,INPUT);
  setGps(config.gps);
  fsOk=LittleFS.mounted(); // mounted by main.cpp before the settings were read
  Serial.printf("HW %s screen=%d fs=%d heap=%u\n",MM_BOARD_NAME,panel!=nullptr,fsOk,ESP.getFreeHeap());
}
static uint16_t batteryRead();
void Hardware::tick() {
  if(gpsEnabled){unsigned budget=512;while(Serial1.available()&&budget--){gps.encode(Serial1.read());gpsBytes++;}}
  static uint32_t dividerOn=0; // the divider settles 10 ms before a reading, without stopping the loop
  if(dividerOn&&millis()-dividerOn>=10){batteryMv=batteryRead();dividerOn=0;}
  if(millis()-lastSample<1000)return;lastSample=millis();
  output(pins::adcEnable,HIGH);dividerOn=millis()|1;
  if(gpsTime()) {
    DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());setUtc(now.unixtime(),"GPS");
  }
}
// Divider on AIN1 (P0.03), switched on by P1.15 for the reading, as in MeshCore's T096Board.
uint16_t Hardware::batteryNow() {output(pins::adcEnable,HIGH);delay(10);return batteryRead();}
// Read 10 ms after the divider is connected, as MeshCore and Meshtastic do.
static uint16_t batteryRead() {
  analogReference(AR_INTERNAL_3_0);analogReadResolution(12);uint32_t raw=0;for(int i=0;i<8;i++)raw+=analogRead(pins::battery);
  digitalWrite(pins::adcEnable,LOW);
  return uint16_t(3000.f*pins::batteryScale*(raw/8)/4096);
}
int Hardware::readKey() {
  static bool held=false,longSent=false,veryLongSent=false;static uint32_t down=0;
  bool pressed=digitalRead(pins::button)==LOW;
  if(pressed && !held) {held=true;longSent=veryLongSent=false;down=millis();}
  if(pressed && held && !longSent && millis()-down>=800) {longSent=true;keyCount++;lastKey=0xa3;return 0xa3;}
  if(pressed && held && !veryLongSent && millis()-down>=5000) {veryLongSent=true;keyCount++;lastKey=0xa4;return 0xa4;} // still held at 5 s (after 0xa3)
  if(!pressed && held) {held=false;if(!longSent && millis()-down>30){keyCount++;lastKey=13;return 13;}}
  return 0;
}
// Off: backlight, Vext (TFT, GPS), GPS enable, LED and the front end off, the SX1262 deselected; System OFF
// keeps the levels. The USER button wakes it.
void Hardware::powerDown(bool started) {
  if(started){brightness(0);setGps(false);}
  output(pins::tftLight,HIGH);output(pins::vext,!pins::vextOn);output(pins::gpsEnable,!pins::gpsOn);output(pins::led,!pins::ledOn);
  output(pins::femEnable,LOW);output(pins::femTx,LOW);output(pins::femPower,LOW);output(pins::adcEnable,LOW);output(pins::radioCs,HIGH);
}
int Hardware::wakePin() const {return pins::button;}
void Hardware::beep() {}
void Hardware::ping(uint16_t,uint16_t) {}
void Hardware::flush() {
  if(!panel)return;
  // The interface draws with palette indices (Palette.h); the 128x64 canvas sits in the middle of 160x80.
  static uint16_t row[128];const uint16_t* px=canvas->getBuffer();tft.startWrite();tft.setAddrWindow(16,8,128,64);
  for(int y=0;y<64;y++,px+=128){for(int x=0;x<128;x++)row[x]=px[x]<ColCount?palette565[px[x]]:px[x];tft.writePixels(row,128);}
  tft.endWrite();
}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(0,y,value,color);}
#endif
