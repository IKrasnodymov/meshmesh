#pragma once
// Heltec T114: the compact interface lays out a 128x64 screen; this canvas takes those coordinates
// and draws at the 240x135 TFT's own resolution (x 1.875, y 2.11). Rectangles and dots become
// blocks, lines and circles stay one pixel wide; UiHeltec.cpp draws text in larger fonts straight
// on `screen`. Pixels are palette indices (Palette.h): 32 KB instead of 64 for RGB565.
#include <Adafruit_GFX.h>
class HiresCanvas:public Adafruit_GFX {
 public:
  static constexpr int Width=240,Height=135;
  GFXcanvas8 screen{Width,Height};
  bool blocks=false; // the 128x64 fonts draw runs of lines: whole blocks keep their glyphs' shape
  HiresCanvas():Adafruit_GFX(128,64) {}
  static int X(int x){return (x*15)>>3;}  // arithmetic shifts floor negative coordinates too
  static int Y(int y){return (y*135)>>6;}
  static int cx(int x){return (X(x)+X(x+1))>>1;}
  static int cy(int y){return (Y(y)+Y(y+1))>>1;}
  uint8_t* getBuffer(){return screen.getBuffer();}
  void drawPixel(int16_t x,int16_t y,uint16_t c) override {block(x,y,1,1,c);}
  void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c) override {block(x,y,w,h,c);}
  void fillScreen(uint16_t c) override {screen.fillScreen(c);}
  void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c) override {if(blocks)block(x,y,w,1,c);else if(w>0)screen.drawFastHLine(X(x),cy(y),X(x+w)-X(x),c);}
  void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c) override {if(blocks)block(x,y,1,h,c);else if(h>0)screen.drawFastVLine(cx(x),Y(y),Y(y+h)-Y(y),c);}
  void drawLine(int16_t x0,int16_t y0,int16_t x1,int16_t y1,uint16_t c) override {
    if(x0==x1)drawFastVLine(x0,min(y0,y1),abs(y1-y0)+1,c);else if(y0==y1)drawFastHLine(min(x0,x1),y0,abs(x1-x0)+1,c);
    else screen.drawLine(cx(x0),cy(y0),cx(x1),cy(y1),c);
  }
  // Not virtual in Adafruit_GFX: these hide the base versions for calls on a HiresCanvas.
  void drawCircle(int16_t x,int16_t y,int16_t r,uint16_t c) {screen.drawCircle(cx(x),cy(y),r*2,c);}
  void fillCircle(int16_t x,int16_t y,int16_t r,uint16_t c) {screen.fillCircle(cx(x),cy(y),r*2,c);}
  void fillTriangle(int16_t x0,int16_t y0,int16_t x1,int16_t y1,int16_t x2,int16_t y2,uint16_t c) {screen.fillTriangle(cx(x0),cy(y0),cx(x1),cy(y1),cx(x2),cy(y2),c);}
 private:
  void block(int x,int y,int w,int h,uint16_t c){if(w>0&&h>0)screen.fillRect(X(x),Y(y),X(x+w)-X(x),Y(y+h)-Y(y),c);}
};
