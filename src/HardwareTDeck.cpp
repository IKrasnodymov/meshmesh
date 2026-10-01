// LilyGO T-Deck and T-Deck Plus with the M9 interface (Ui.cpp): 320x240 ST7789, the I2C
// keyboard (ESP32-C3 at 0x55, one ASCII byte per key), the trackball and the SD card.
// Display, LoRa and SD share one SPI bus, as on the M9. GPS: T-Deck Plus only.
#include "Version.h"
#include "Hardware.h"
#include "Config.h"
#include "GpsProbe.h"
#include "Storage.h"
#include <Wire.h>
#include <SD.h>
#include <LittleFS.h>
#include <driver/rtc_io.h>
Hardware hardware;
namespace {
GpsProbe gpsPort(pins::gpsRx,pins::gpsTx,38400);
volatile uint32_t ball[4]={}; // up, down, left, right: pulses from the trackball
void IRAM_ATTR ballUp(){ball[0]++;}
void IRAM_ATTR ballDown(){ball[1]++;}
void IRAM_ATTR ballLeft(){ball[2]++;}
void IRAM_ATTR ballRight(){ball[3]++;}
uint8_t backlightLevel=0; // 0 off, 1..16: steps of the backlight driver
}
bool Hardware::read(TwoWire& bus,uint8_t addr,uint8_t reg,uint8_t* bytes,uint8_t size,bool stop) {
  bus.beginTransmission(addr);bus.write(reg);if(bus.endTransmission(stop)) return false;
  if(bus.requestFrom(int(addr),int(size))!=size) return false;
  for(unsigned i=0;i<size;i++) bytes[i]=bus.read();return true;
}
bool Hardware::write(uint8_t addr,uint8_t reg,uint8_t value) {
  Wire.beginTransmission(addr);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;
}
// The backlight driver counts pulses: high = brightest step, each short low pulse is one step
// dimmer, low for over 2.5 ms switches it off (as in LilyGO's T-Deck examples).
void Hardware::brightness(uint8_t level) {
  uint8_t steps=level?max(1,(level*16+128)/256):0;if(steps>16)steps=16;
  if(steps==backlightLevel)return;
  if(!steps){digitalWrite(pins::backlight,LOW);delay(3);backlightLevel=0;return;}
  if(!backlightLevel||steps>backlightLevel){digitalWrite(pins::backlight,LOW);delay(3);digitalWrite(pins::backlight,HIGH);delayMicroseconds(30);backlightLevel=16;}
  for(;backlightLevel>steps;backlightLevel--){digitalWrite(pins::backlight,LOW);delayMicroseconds(2);digitalWrite(pins::backlight,HIGH);delayMicroseconds(2);}
}
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;if(enabled)gpsPort.open(gps);else gpsPort.close();
}
void Hardware::begin() {
  gpio_deep_sleep_hold_dis();
  for(int pin:{pins::peripheralPower,pins::backlight}){gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  for(int pin:{pins::radioCs,pins::lcdCs,pins::sdCs}){gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
  digitalWrite(pins::peripheralPower,HIGH);delay(100); // keyboard, LoRa and display power
  pinMode(pins::spiMiso,INPUT_PULLUP);
  SPI.begin(pins::spiClock,pins::spiMiso,pins::spiMosi);
  display.init(240,320,SPI_MODE0);display.setSPISpeed(40000000);display.setRotation(1);display.fillScreen(0x0862);
  canvas=new GFXcanvas16(320,240); // in PSRAM
  if(!canvas || !canvas->getBuffer()) {Serial.println("FATAL framebuffer allocation");while(true) delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);
  canvas->fillScreen(0x0862);text(14,38,MESHMM_FIRMWARE,0x07ff);text(14,65,"Starting T-Deck...");flush();brightness(config.brightness);
  Wire.begin(pins::keyboardSda,pins::keyboardScl,100000);Wire.setTimeOut(20);
  // The keyboard controller boots after the ESP32; give it time before the first probe.
  for(int i=0;i<10&&!keyboardOk;i++){Wire.beginTransmission(uint8_t(pins::keyboardAddress));keyboardOk=Wire.endTransmission()==0;if(!keyboardOk)delay(50);}
  for(int pin:{pins::ballUp,pins::ballDown,pins::ballLeft,pins::ballRight,pins::ballClick})pinMode(pin,INPUT_PULLUP);
  attachInterrupt(pins::ballUp,ballUp,FALLING);attachInterrupt(pins::ballDown,ballDown,FALLING);
  attachInterrupt(pins::ballLeft,ballLeft,FALLING);attachInterrupt(pins::ballRight,ballRight,FALLING);
  fsOk=mountStorage();
  sdOk=SD.begin(pins::sdCs,SPI,4000000);
  setGps(config.gps);
  Serial.printf("HW T-Deck keyboard=%d sd=%d fs=%d psram=%u\n",keyboardOk,sdOk,fsOk,ESP.getPsramSize());
}
void Hardware::tick() {
  if(gpsEnabled) {
    gpsBytes+=gpsPort.tick(gps);
    if(gps.date.isValid() && gps.time.isValid() && gps.time.isUpdated() && gps.time.age()<10000 && gps.date.age()<10000 && gps.location.isValid() && gps.location.age()<10000 && gps.date.year()>=2025) {
      DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());setUtc(now.unixtime(),"GPS");
    }
  }
  if(millis()-lastSample<1000) return;lastSample=millis();
  uint32_t sum=0;analogReadResolution(12);analogSetPinAttenuation(pins::battery,ADC_11db);
  for(int i=0;i<6;i++)sum+=analogReadMilliVolts(pins::battery);batteryMv=2*sum/6;
  if(!keyboardOk && millis()-lastProbe>2000) {lastProbe=millis();Wire.beginTransmission(uint8_t(pins::keyboardAddress));keyboardOk=Wire.endTransmission()==0;}
}
// Keys as on the M9 (Ui.cpp): trackball = arrows, click = OK, hold = hold OK.
// Keyboard: Enter = OK, Backspace = DEL, other keys as typed.
int Hardware::readKey() {
  static bool held=false,longSent=false;static uint32_t down=0,poll=0,rolled=0;
  bool pressed=digitalRead(pins::ballClick)==LOW;
  int key=0;
  if(pressed && !held) {held=true;longSent=false;down=millis();}
  if(pressed && held && !longSent && millis()-down>=1200) {longSent=true;key=0xa3;}
  if(!pressed && held) {held=false;if(!longSent && millis()-down>30)key=13;}
  // One arrow per roll step; the ball gives several pulses per step, extra ones are dropped.
  if(!key && millis()-rolled>=120) {
    static const int arrows[]={0xb5,0xb6,0xb4,0xb7};
    for(int i=0;i<4&&!key;i++)if(ball[i]){key=arrows[i];rolled=millis();}
    if(key)for(auto& n:ball)n=0;
  }
  if(!key && keyboardOk && millis()-poll>=15) {
    poll=millis();
    if(Wire.requestFrom(int(pins::keyboardAddress),1)==1){uint8_t c=Wire.read();if(c==0x0d)key=13;else if(c==0x08)key=8;else if(c>=0x20&&c<0x7f)key=c;}
  }
  if(!key)return 0;
  lastKey=key;keyCount++;brightness(config.brightness);return key;
}
void Hardware::beep() {}  // the T-Deck speaker is an I2S amplifier, not driven here
void Hardware::ping(uint16_t,uint16_t) {}
void Hardware::flush() {display.drawRGBBitmap(0,0,canvas->getBuffer(),320,240);}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(12,y,value,color);}
