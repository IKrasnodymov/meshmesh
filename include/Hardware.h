#pragma once
#include <Arduino.h>
#include "Board.h"
#include "BoardPins.h"
#if defined(MM_HELTEC_V4)
#include <Adafruit_SSD1306.h>
#elif defined(MM_COMPACT)
#include <Adafruit_GFX.h>
#else
#include <Adafruit_ST7789.h>
#if !defined(MM_UI_PREVIEW)
#include <esp_heap_caps.h>
#endif
#endif
#if defined(MM_HIRES)
#include "HiresCanvas.h"
#endif
#include <U8g2_for_Adafruit_GFX.h>
#include <TinyGPSPlus.h>
#include <RTClib.h>

class Hardware {
 public:
#if defined(MM_HELTEC_V4)
  Adafruit_SSD1306 display;
#elif defined(MM_COMPACT)
  Adafruit_GFX* panel=nullptr; // the board's screen; nullptr when it has none or it did not answer
#else
  Adafruit_ST7789 display;
#endif
#if defined(MM_HIRES)
  HiresCanvas* canvas=nullptr; // 128x64 layout coordinates on the 240x135 TFT
#else
  GFXcanvas16* canvas=nullptr;
#endif
#if !defined(MM_HELTEC_V4) && !defined(MM_COMPACT)
  uint16_t* shown=nullptr; // the frame last sent to the panel (PSRAM): flush() sends only the rows that differ
  bool shownValid=false;   // false: the next flush sends the whole frame
  uint32_t flushRows=0;    // rows sent by the last flush
#endif
  U8G2_FOR_ADAFRUIT_GFX font;
  TinyGPSPlus gps;
  RTC_PCF8563 rtc;
  bool keyboardOk=false, rtcOk=false, rtcValid=false, sdOk=false, fsOk=false;
  bool compassOk=false, imuOk=false, compassSample=false, imuSample=false;
  float mag[3]={}, accel[3]={};
  uint16_t batteryMv=0;
  int8_t usbPower=-1; // USB power (charging) from a sense pin: 1 yes, 0 battery, -1 no pin (the screens guess from the voltage)
#if defined(MM_NRF52)
  uint16_t batteryNow(); // a reading now (batteryMv: once a second in tick); also before begin()
#endif
  uint32_t gpsBytes=0, lastKey=0, keyCount=0, utc=0;
  uint8_t keyboardHw=0, keyboardFw=0;
  const char* boardName=MM_BOARD_NAME; // the M9 adds its revision once the keyboard answers (Hardware.cpp)
  bool micKey=true; // false on the M9 V1.1: its keyboard has no MIC key, hold OK (about 5 s) locks instead (Ui.cpp)
#if defined(MM_HELTEC_V4)
  Hardware():display(128,64,&Wire,pins::oledReset) {}
#elif defined(MM_COMPACT)
  Hardware() {}
#else
  Hardware():display(&SPI,pins::lcdCs,pins::lcdDc,pins::lcdReset) {}
#endif
  bool clockTrusted=false,clockConflict=false;
  String clockSource="unset";
  void beginClock();
  bool setUtc(uint32_t epoch,const char* source,bool persist=false);
  String clockInfo();
  bool gpsFix();
  uint32_t clockSyncAt=0;uint32_t clockAge() const {return clockSyncAt?uint32_t(millis()-clockSyncAt):UINT32_MAX;}
  bool gpsTime(); // the receiver's date and time may set the clock
  unsigned gpsInView(String* systems=nullptr); // satellites in view (GSV); systems: "GP:9 GB:6"
  void begin();
  void tick();
  int readKey();
#if defined(MM_BOARD_TDECK)
  char readTouch(int& x,int& y);
#endif
  void brightness(uint8_t level);
  void setGps(bool enabled);
  void beep();
  void ping(uint16_t hz,uint16_t ms);
  void flush();
  // Power off (Power.cpp): the screen, GPS and peripheral supplies off; the levels are held through
  // deep sleep (ESP32) or System OFF (nRF52). started: begin() ran; false on a wake that goes back to
  // sleep, when only the pin levels are set again. A board with a PMU (T-Beam) is switched off by it here.
  void powerDown(bool started);
  int wakePin() const; // the button that turns it on again, pressed = LOW; -1: only RESET or the power switch
  void text(int x,int y,const String& value,uint16_t color=0xffff);
  void line(int y,const String& value,uint16_t color=0xffff);
 private:
  uint32_t lastSample=0, lastProbe=0, lastGpsBaud=0;
  bool gpsEnabled=false;
  bool read(TwoWire& bus,uint8_t addr,uint8_t reg,uint8_t* bytes,uint8_t size,bool stop=false);
  bool write(uint8_t addr,uint8_t reg,uint8_t value);
  bool probeKeyboard(); // M9: the keyboard controller at either of its addresses
};
extern Hardware hardware;
#if !defined(MM_HELTEC_V4) && !defined(MM_COMPACT) && !defined(MM_UI_PREVIEW)
// The 320x240 ST7789 boards (M9, T-Deck): the canvas goes out in bands of changed rows. A moved cursor,
// the status bar clock or a toast sends a few rows instead of all 240 (150 KB, over 15 ms at 80 MHz).
// Without the copy (no PSRAM left) every flush sends the whole frame, as before.
inline void flushChangedRows(Hardware& h) {
  const int W=h.canvas->width(),H=h.canvas->height();uint16_t* buf=h.canvas->getBuffer();const size_t row=W*sizeof(uint16_t);
  if(!h.shown)h.shown=(uint16_t*)heap_caps_malloc(row*H,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!h.shown){h.display.drawRGBBitmap(0,0,buf,W,H);h.flushRows=H;return;}
  bool all=!h.shownValid;h.flushRows=0;
  h.display.startWrite();
  for(int y=0;y<H;){
    if(!all&&!memcmp(buf+y*W,h.shown+y*W,row)){y++;continue;}
    int end=y+1;while(end<H&&(all||memcmp(buf+end*W,h.shown+end*W,row)))end++;
    h.display.setAddrWindow(0,y,W,end-y);h.display.writePixels(buf+y*W,uint32_t(end-y)*W);
    memcpy(h.shown+y*W,buf+y*W,row*(end-y));h.flushRows+=end-y;y=end;
  }
  h.display.endWrite();h.shownValid=true;
}
#endif
// Charge of a Li-ion cell from its voltage, rounded: the same curve as batteryPercent() of web/index.html,
// so the screen, the page and the app agree.
inline unsigned batteryPercent(uint16_t v){
  static const uint16_t mv[]={3300,3500,3600,3700,3800,3900,4000,4100,4200};static const uint8_t pc[]={0,5,15,35,55,70,82,93,100};
  if(v<=mv[0])return 0;for(int i=1;i<9;i++)if(v<=mv[i]){unsigned span=mv[i]-mv[i-1];return pc[i-1]+(2u*(pc[i]-pc[i-1])*(v-mv[i-1])+span)/(2*span);}return 100;}
