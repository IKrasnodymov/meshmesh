#pragma once
#include <Arduino.h>
#include "BoardPins.h"
#if defined(MM_HELTEC_V4)
#include <Adafruit_SSD1306.h>
#else
#include <Adafruit_ST7789.h>
#endif
#include <U8g2_for_Adafruit_GFX.h>
#include <TinyGPSPlus.h>
#include <RTClib.h>

class Hardware {
 public:
#if defined(MM_HELTEC_V4)
  Adafruit_SSD1306 display;
#else
  Adafruit_ST7789 display;
#endif
  GFXcanvas16* canvas=nullptr;
  U8G2_FOR_ADAFRUIT_GFX font;
  TinyGPSPlus gps;
  RTC_PCF8563 rtc;
  bool keyboardOk=false, rtcOk=false, rtcValid=false, sdOk=false, fsOk=false;
  bool compassOk=false, imuOk=false, compassSample=false, imuSample=false;
  float mag[3]={}, accel[3]={};
  uint16_t batteryMv=0;
  uint32_t gpsBytes=0, lastKey=0, keyCount=0, utc=0;
  uint8_t keyboardHw=0, keyboardFw=0;
#if defined(MM_HELTEC_V4)
  Hardware():display(128,64,&Wire,pins::oledReset) {}
#else
  Hardware():display(&SPI,pins::lcdCs,pins::lcdDc,pins::lcdReset) {}
#endif
  bool clockTrusted=false,clockConflict=false;
  String clockSource="unset";
  void beginClock();
  bool setUtc(uint32_t epoch,const char* source,bool persist=false);
  String clockInfo();
  bool gpsFix();
  void begin();
  void tick();
  int readKey();
  void brightness(uint8_t level);
  void setGps(bool enabled);
  void beep();
  void ping(uint16_t hz,uint16_t ms);
  void flush();
  void text(int x,int y,const String& value,uint16_t color=0xffff);
  void line(int y,const String& value,uint16_t color=0xffff);
 private:
  uint32_t lastSample=0, lastProbe=0, lastGpsBaud=0;
  bool gpsEnabled=false;
  bool read(TwoWire& bus,uint8_t addr,uint8_t reg,uint8_t* bytes,uint8_t size,bool stop=false);
  bool write(uint8_t addr,uint8_t reg,uint8_t value);
};
extern Hardware hardware;
