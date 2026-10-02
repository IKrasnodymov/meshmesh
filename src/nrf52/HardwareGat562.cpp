// GAT562 30S Mesh Kit (nRF52840 + SX1262): 1.3" 128x64 OLED on I2C, five-way joystick, back
// button, L76K GPS on Serial1, buzzer, green (TX) and blue LEDs. The compact interface
// (UiHeltec.cpp) draws a 128x64 canvas; this file puts it on the panel and reads the keys.
#include "Hardware.h"
#include "Config.h"
#include <LittleFS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>
Hardware hardware;
namespace {
Adafruit_SSD1306 oled(128,64,&Wire,pins::oledReset);
bool probe(uint8_t address) {Wire.beginTransmission(address);return Wire.endTransmission()==0;}
// Joystick: a press gives its arrow at once, then repeats while held (lists, the keyboard).
// Centre: click = Enter (13), hold 0.8 s = 0xa3. Back: click = 0x86, hold 0.8 s = 0x82 (home).
struct Key {int pin,code,hold;bool down=false,longSent=false;uint32_t at=0,repeat=0;};
Key keys[]={{pins::keyUp,0xb5,0},{pins::keyDown,0xb6,0},{pins::keyLeft,0xb4,0},{pins::keyRight,0xb7,0},{pins::keyPress,13,0xa3},{pins::keyBack,0x86,0x82}};
}
void Hardware::brightness(uint8_t level) {
  if(!panel)return;if(!level){oled.ssd1306_command(SSD1306_DISPLAYOFF);return;}
  oled.ssd1306_command(SSD1306_DISPLAYON);oled.ssd1306_command(SSD1306_SETCONTRAST);oled.ssd1306_command(level);
}
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;
  pinMode(pins::gpsPower,OUTPUT);digitalWrite(pins::gpsPower,enabled?HIGH:LOW); // IO2: the 3V3_GPS switch
  // The core's Uart::end() waits for stop events that a UART never started does not raise: with GPS
  // off in the settings, the boot hung right after the splash. Start and stop the port only once.
  static bool uartOn=false;
  if(enabled&&!uartOn){Serial1.setPins(pins::gpsRx,pins::gpsTx);Serial1.begin(9600);uartOn=true;}
  else if(!enabled&&uartOn){Serial1.end();uartOn=false;}
}
void Hardware::begin() {
  pinMode(pins::radioPower,OUTPUT);digitalWrite(pins::radioPower,HIGH);delay(10); // SX1262 supply
  SPI.setPins(pins::spiMiso,pins::spiClock,pins::spiMosi);SPI.begin();
  Wire.setPins(pins::sda,pins::scl);Wire.begin();Wire.setClock(400000);
  uint8_t address=probe(0x3c)?0x3c:probe(0x3d)?0x3d:0;
  if(address&&oled.begin(SSD1306_SWITCHCAPVCC,address,true,false))panel=&oled;
  if(panel){oled.clearDisplay();oled.display();}
  brightness(config.brightness);
  canvas=new GFXcanvas16(128,64);if(!canvas||!canvas->getBuffer()){Serial.println("FATAL framebuffer");while(true)delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);canvas->fillScreen(0);text(0,13,"MeshMesh");flush();
  for(auto& k:keys)pinMode(k.pin,INPUT_PULLUP);
  pinMode(pins::led,OUTPUT);digitalWrite(pins::led,!pins::ledOn);pinMode(pins::txLed,OUTPUT);digitalWrite(pins::txLed,!pins::ledOn);
  pinMode(pins::battery,INPUT);pinMode(pins::buzzer,OUTPUT);digitalWrite(pins::buzzer,LOW);
  setGps(config.gps);
  fsOk=LittleFS.mounted(); // mounted by main.cpp before the settings were read
  Serial.printf("HW %s screen=%d fs=%d heap=%u\n",MM_BOARD_NAME,panel!=nullptr,fsOk,ESP.getFreeHeap());
}
void Hardware::tick() {
  if(gpsEnabled){unsigned budget=512;while(Serial1.available()&&budget--){gps.encode(Serial1.read());gpsBytes++;}}
  if(millis()-lastSample<1000)return;lastSample=millis();
  // Battery divider on AIN3 (P0.05), as in MeshCore's board file: 12 bits against the 3.6 V reference.
  analogReadResolution(12);uint32_t raw=0;for(int i=0;i<8;i++)raw+=analogRead(pins::battery);
  batteryMv=uint16_t((3*1.75f*1.187f*1000)*(raw/8)/4096);
  if(gps.date.isValid()&&gps.time.isValid()&&gps.time.age()<10000&&gps.date.age()<10000&&gps.location.isValid()&&gps.location.age()<10000&&gps.date.year()>=2025) {
    DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());setUtc(now.unixtime(),"GPS");
  }
}
int Hardware::readKey() {
  uint32_t now=millis();
  for(auto& k:keys){
    bool pressed=digitalRead(k.pin)==LOW;
    if(pressed&&!k.down){
      if(now-k.at<30)continue; // contact bounce after a release
      k.down=true;k.longSent=false;k.at=now;k.repeat=now+450;
      if(!k.hold){keyCount++;lastKey=k.code;return k.code;}
    } else if(pressed&&k.down){
      if(k.hold&&!k.longSent&&now-k.at>=800){k.longSent=true;keyCount++;lastKey=k.hold;return k.hold;}
      if(!k.hold&&int32_t(now-k.repeat)>=0){k.repeat=now+110;keyCount++;lastKey=k.code;return k.code;}
    } else if(!pressed&&k.down){
      k.down=false;bool click=k.hold&&!k.longSent&&now-k.at>30;k.at=now;
      if(click){keyCount++;lastKey=k.code;return k.code;}
    }
  }
  return 0;
}
void Hardware::beep() {if(config.sound)tone(pins::buzzer,2200,60);}
void Hardware::ping(uint16_t hz,uint16_t ms) {if(config.sound)tone(pins::buzzer,hz,ms);}
void Hardware::flush() {
  if(!panel)return;oled.clearDisplay();for(int y=0;y<64;y++)for(int x=0;x<128;x++)if(canvas->getPixel(x,y))oled.drawPixel(x,y,1);oled.display();
}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(0,y,value,color);}
