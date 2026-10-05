// Heltec Mesh Node T114 V2 (nRF52840 + SX1262): 1.14" 240x135 ST7789 colour TFT, one user button,
// optional L76K GPS on Serial1, green LED. The compact interface (UiHeltec.cpp) lays out 128x64 and
// draws at the TFT's resolution through HiresCanvas; this file sends its palette pixels to the panel.
#include "Hardware.h"
#include "Config.h"
#include "Palette.h"
#include <LittleFS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ST7789.h>
#include <esp_system.h>
Hardware hardware;
namespace {
// SPI1 is the 32 MHz SPIM3 (SPI_32MHZ_INTERFACE=1 in platformio.ini); LoRa stays on SPI.
Adafruit_ST7789 tft(&SPI1,pins::tftCs,pins::tftDc,pins::tftReset);
constexpr int W=HiresCanvas::Width,H=HiresCanvas::Height;
uint32_t rowHash[H];    // rows as last sent: flush() sends only the rows that changed
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
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;
  output(pins::vext,enabled?pins::vextOn:!pins::vextOn); // GPS and the I2C connector supply
  output(pins::gpsReset,HIGH);output(pins::gpsStandby,HIGH);
  // As on the GAT562: Uart::end() of a port never started waits forever, so start and stop it once.
  if(enabled&&!uartOn){Serial1.setPins(pins::gpsRx,pins::gpsTx);Serial1.begin(9600);uartOn=true;}
  else if(!enabled&&uartOn){Serial1.end();uartOn=false;}
}
void Hardware::begin() {
  SPI.setPins(pins::spiMiso,pins::spiClock,pins::spiMosi);SPI.begin();
  Wire.setPins(pins::sda,pins::scl);Wire.begin();Wire.setClock(400000);
  output(pins::tftLight,HIGH);output(pins::tftPower,LOW);delay(20); // panel supply on, backlight off until drawn
  SPI1.setPins(pins::tftMiso,pins::tftClock,pins::tftData);
  tft.init(135,240);tft.setRotation(3);tft.setSPISpeed(32000000);tft.fillScreen(0);panel=&tft;tftOn=true;
  canvas=new HiresCanvas();if(!canvas||!canvas->getBuffer()){Serial.println("FATAL framebuffer");while(true)delay(1000);}
  memset(rowHash,0xff,sizeof rowHash);
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);canvas->fillScreen(0);text(0,13,"MeshMesh");flush();
  brightness(config.brightness);
  pinMode(pins::button,INPUT_PULLUP);
  output(pins::led,!pins::ledOn);output(pins::adcEnable,LOW);pinMode(pins::battery,INPUT);
  setGps(config.gps);
  fsOk=LittleFS.mounted(); // mounted by main.cpp before the settings were read
  Serial.printf("HW %s screen=%d fs=%d heap=%u\n",MM_BOARD_NAME,panel!=nullptr,fsOk,ESP.getFreeHeap());
}
void Hardware::tick() {
  if(gpsEnabled){unsigned budget=512;while(Serial1.available()&&budget--){gps.encode(Serial1.read());gpsBytes++;}}
  if(millis()-lastSample<1000)return;lastSample=millis();
  // Divider on AIN2 (P0.04), switched on by P0.06 for the reading, as in MeshCore's T114Board.
  digitalWrite(pins::adcEnable,HIGH);delay(2);
  analogReference(AR_INTERNAL_3_0);analogReadResolution(12);uint32_t raw=0;for(int i=0;i<8;i++)raw+=analogRead(pins::battery);
  digitalWrite(pins::adcEnable,LOW);
  batteryMv=uint16_t(3000.f*pins::batteryScale*(raw/8)/4096);
  if(gps.date.isValid()&&gps.time.isValid()&&gps.time.age()<10000&&gps.date.age()<10000&&gps.location.isValid()&&gps.location.age()<10000&&gps.date.year()>=2025) {
    DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());setUtc(now.unixtime(),"GPS");
  }
}
int Hardware::readKey() {
  static bool held=false,longSent=false;static uint32_t down=0;
  bool pressed=digitalRead(pins::button)==LOW;
  if(pressed && !held) {held=true;longSent=false;down=millis();}
  if(pressed && held && !longSent && millis()-down>=1200) {longSent=true;keyCount++;lastKey=0xa3;return 0xa3;}
  if(!pressed && held) {held=false;if(!longSent && millis()-down>30){keyCount++;lastKey=13;return 13;}}
  return 0;
}
void Hardware::beep() {}
void Hardware::ping(uint16_t,uint16_t) {}
void Hardware::flush() {
  if(!panel)return;
  static uint16_t line[W];const uint8_t* px=canvas->getBuffer();bool open=false;
  for(int y=0;y<H;y++,px+=W) {
    uint32_t h=2166136261u;for(int x=0;x<W;x++)h=(h^px[x])*16777619u;
    if(h==rowHash[y])continue;rowHash[y]=h;
    for(int x=0;x<W;x++)line[x]=palette565[px[x]<ColCount?px[x]:ColInk];
    if(!open){tft.startWrite();open=true;}
    tft.setAddrWindow(0,y,W,1);tft.writePixels(line,W);
  }
  if(open)tft.endWrite();
}
// Boot splash only (the interface draws its text itself): the 128x64 font in blocks.
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.begin(*canvas);canvas->blocks=true;font.setForegroundColor(color);font.setCursor(x,y);font.print(value);canvas->blocks=false;}
void Hardware::line(int y,const String& value,uint16_t color) {text(0,y,value,color);}
