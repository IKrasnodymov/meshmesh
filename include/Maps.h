#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
class Maps {
 public:
  bool available=false,dirty=false,follow=true,haveCenter=false;
  uint8_t zoom=15;
  double latitude=0,longitude=0;
  String title="",error="";
  uint32_t tileCount=0;
  unsigned visibleTiles=0;bool waiting=false;
  void begin();void tick();void draw(int x,int y,int w,int h);
  void center(double lat,double lon);void pan(int dx,int dy);void changeZoom(int delta);
  String info();String command(const String& line);
  File openTile(unsigned z,uint32_t x,uint32_t y);
  String areas();bool selectArea(const String& id);
  bool uploadBegin(unsigned z,uint32_t x,uint32_t y,uint32_t size,uint32_t crc);
  bool uploadCompressedBegin(unsigned z,uint32_t x,uint32_t y,uint32_t size,uint32_t crc,uint32_t packedSize);
  bool uploadChunk(const uint8_t* bytes,size_t size);
  bool uploadFinish();void uploadCancel();bool saveManifest(const String& value);
 private:
  struct Tile {int z=-1,x=-1,y=-1;uint16_t* pixels=nullptr;uint32_t stamp=0;bool ready=false,missing=false;} tiles[6];
  File input,output;String destination;
  int loading=-1;uint32_t loadedPixels=0,readCrc=0xffffffff,expectedCrc=0,readSize=0,fileSize=0;
  uint32_t writeSize=0,writeExpected=0,writeCrc=0xffffffff,writeExpectedCrc=0,writeAt=0;
  uint32_t writePixels=0;
  uint8_t* uploadBuffer=nullptr;uint32_t uploadSize=0;bool compressed=false;
  Tile* request(int z,int x,int y);
  void invalidate();
};
extern Maps maps;
uint32_t mapCrc(uint32_t crc,const uint8_t* bytes,size_t size);
