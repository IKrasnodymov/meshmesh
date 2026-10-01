#include "Maps.h"
#include "Hardware.h"
#include "Internet.h"
#include <Preferences.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <math.h>
#if CONFIG_IDF_TARGET_ESP32S3
#include <esp32s3/rom/miniz.h>
#else
#include <esp32/rom/miniz.h>
#endif
#include <Mm1Packet.h>
Maps maps;
uint32_t mapCrc(uint32_t crc,const uint8_t* bytes,size_t size) {
  while(size--) {crc^=*bytes++;for(int b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320:0);}return crc;
}
namespace {
const uint32_t Pixels=256*256;
uint32_t u32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put32(uint8_t* p,uint32_t v) {for(int i=0;i<4;i++)p[i]=v>>(i*8);}
String path(int z,int x,int y) {return "/meshmesh/maps/"+String(z)+"/"+String(x)+"_"+String(y)+".mmt";}
double px(double lon,int z) {return (lon+180)/360*(256UL<<z);}
double py(double lat,int z) {double r=constrain(lat,-85.05112878,85.05112878)*M_PI/180;return (1-log(tan(r)+1/cos(r))/M_PI)/2*(256UL<<z);}
}
void Maps::begin() {
#if !defined(MM_COMPACT)
  available=hardware.sdOk;
  // The last view the user chose (or the IP location) opens the map without any preloaded area.
  Preferences p;if(p.begin("mm-map",true)){double lat=p.getDouble("lat",NAN),lon=p.getDouble("lon",NAN);int z=p.getUChar("z",0);p.end();
    if(isfinite(lat)&&isfinite(lon)&&fabs(lat)<=85.05112878&&fabs(lon)<=180&&z>=MinZoom&&z<=MaxZoom){latitude=lat;longitude=lon;zoom=z;haveCenter=true;}}
#endif
  if(!available)return;
  SD.mkdir("/meshmesh");SD.mkdir("/meshmesh/maps");
  SD.mkdir("/meshmesh/maps/areas");
  if(!SD.exists("/meshmesh/maps/area.json")&&SD.exists("/meshmesh/maps/area.json.old"))SD.rename("/meshmesh/maps/area.json.old","/meshmesh/maps/area.json");
  File f=SD.open("/meshmesh/maps/area.json");
  if(f) {DynamicJsonDocument d(2048);if(!deserializeJson(d,f)) {double lat=d["latitude"]|0.0,lon=d["longitude"]|0.0;int z=d["default_zoom"]|15;if(isfinite(lat)&&isfinite(lon)&&fabs(lat)<=85.05112878&&fabs(lon)<=180&&z>=10&&z<=17){if(!haveCenter){latitude=lat;longitude=lon;haveCenter=true;zoom=z;}title=d["name"]|"Offline map";tileCount=d["tiles"]|0;readBounds(d.as<JsonVariantConst>());}}f.close();}
}
void Maps::center(double lat,double lon) {if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>85.05112878||fabs(lon)>180)return;latitude=lat;longitude=lon;haveCenter=true;dirty=true;}
void Maps::pan(int dx,int dy) {
  if(!haveCenter)return;follow=false;double scale=256UL<<zoom;
  double x=fmod(px(longitude,zoom)+dx+scale,scale),y=constrain(py(latitude,zoom)+dy,0.0,scale);
  center(atan(sinh(M_PI*(1-2*y/scale)))*180/M_PI,x/scale*360-180);markView();
}
void Maps::changeZoom(int delta) {zoom=constrain(int(zoom)+delta,MinZoom,MaxZoom);dirty=true;markView();}
void Maps::readBounds(JsonVariantConst d){JsonArrayConst b=d["bounds"];haveBounds=b.size()==4;for(int i=0;i<4&&haveBounds;i++){bounds[i]=b[i]|NAN;haveBounds=isfinite(bounds[i]);}}
bool Maps::inArea() const{return title.length()&&(!haveBounds||(latitude>=bounds[0]&&latitude<=bounds[2]&&longitude>=bounds[1]&&longitude<=bounds[3]));}
void Maps::markView(){viewChanged=true;viewChangedAt=millis();}
// Written only after the view settles: NVS wear stays low while panning.
void Maps::saveView(){viewChanged=false;Preferences p;if(p.begin("mm-map",false)){p.putDouble("lat",latitude);p.putDouble("lon",longitude);p.putUChar("z",zoom);p.end();}}
bool Maps::recentlyFailed(int z,int x,int y){for(auto& f:failed)if(f.z==z&&f.x==x&&f.y==y&&millis()-f.at<60000)return true;return false;}
void Maps::invalidate() {if(input)input.close();loading=-1;for(auto& t:tiles) {t.z=-1;t.ready=t.missing=t.fetch=false;}dirty=true;}
Maps::Tile* Maps::request(int z,int x,int y) {
  int count=1<<z;if(x<0||y<0||x>=count||y>=count)return nullptr;
  for(auto& t:tiles)if(t.z==z&&t.x==x&&t.y==y) {t.stamp=millis();return &t;}
  if(loading>=0 || output)return nullptr;
  int chosen=-1;for(int i=0;i<6;i++)if(tiles[i].z<0){chosen=i;break;}
  if(chosen<0)for(int i=0;i<6;i++)if(chosen<0||int32_t(tiles[i].stamp-tiles[chosen].stamp)<0)chosen=i;
  Tile& t=tiles[chosen];t.z=z;t.x=x;t.y=y;t.stamp=millis();t.ready=t.missing=t.fetch=false;
  if(!t.pixels)t.pixels=(uint16_t*)heap_caps_malloc(Pixels*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!t.pixels) {t.missing=true;error="Map RAM allocation";return &t;}
  uint8_t h[24];
  if(available){String target=path(z,x,y);if(!SD.exists(target)&&SD.exists(target+".old"))SD.rename(target+".old",target);input=SD.open(target);}
  if(!input || input.read(h,24)!=24 || memcmp(h,"MMT1",4) || h[4]!=z || u32(h+8)!=uint32_t(x) || u32(h+12)!=uint32_t(y) || !u32(h+16) || u32(h+16)>Pixels*4 || u32(h+16)%4 || input.size()!=24+u32(h+16)) {
    if(input)input.close();
    if(internet.online()&&!recentlyFailed(z,x,y))t.fetch=true;else t.missing=true;
    return &t;
  }
  fileSize=u32(h+16);expectedCrc=u32(h+20);readSize=loadedPixels=0;readCrc=0xffffffff;loading=chosen;return &t;
}
void Maps::tick() {
  if(output && millis()-writeAt>30000) {uploadCancel();error="Map upload timed out";}
  static uint32_t gpsAt=0;
  if(follow && hardware.gpsFix()&&millis()-gpsAt>2000) {gpsAt=millis();center(hardware.gps.location.lat(),hardware.gps.location.lng());}
  if(viewChanged&&millis()-viewChangedAt>10000)saveView();
  cacheTick();netTick();
  if(!available||loading<0)return;
  // Bound every storage read and expansion; radio gets a turn between batches.
  uint8_t data[512];unsigned size=min(uint32_t(sizeof(data)),fileSize-readSize);
  if(!size)return;
  int got=input.read(data,size);bool bad=got!=int(size);
  if(!bad) {
    readCrc=mapCrc(readCrc,data,size);readSize+=size;
    for(unsigned i=0;i<size;i+=4) {unsigned n=data[i]|unsigned(data[i+1])<<8;uint16_t color=data[i+2]|uint16_t(data[i+3])<<8;
      if(!n||loadedPixels+n>Pixels){bad=true;break;}
      for(unsigned j=0;j<n;j++)tiles[loading].pixels[loadedPixels++]=color;
    }
  }
  if(bad || readSize==fileSize) {
    Tile& t=tiles[loading];t.ready=!bad&&loadedPixels==Pixels&&(readCrc^0xffffffff)==expectedCrc;t.missing=!t.ready;
    if(!t.ready)error="Damaged map tile";input.close();loading=-1;dirty=true;
  }
}
void Maps::draw(int x,int y,int w,int h) {
  visibleTiles=fetching=0;waiting=false;
  if(!haveCenter || (!available&&!internet.online()))return;
  int left=floor(px(longitude,zoom)-w/2),top=floor(py(latitude,zoom)-h/2);
  for(int ty=int(floor(double(top)/256));ty<=int(floor(double(top+h-1)/256));ty++)for(int tx=int(floor(double(left)/256));tx<=int(floor(double(left+w-1)/256));tx++) {
    Tile* t=request(zoom,tx,ty);int sx=max(left,tx*256),sy=max(top,ty*256),ex=min(left+w,(tx+1)*256),ey=min(top+h,(ty+1)*256);
    if(t&&t->ready){visibleTiles++;for(int row=sy;row<ey;row++)memcpy(hardware.canvas->getBuffer()+(y+row-top)*hardware.canvas->width()+x+sx-left,t->pixels+(row-ty*256)*256+sx-tx*256,(ex-sx)*2);}
    else {hardware.canvas->fillRect(x+sx-left,y+sy-top,ex-sx,ey-sy,0xe73c);hardware.canvas->drawRect(x+sx-left,y+sy-top,ex-sx,ey-sy,0xc618);}
    if(!t||(!t->ready&&!t->missing))waiting=true;
    if(t&&t->fetch)fetching++;
  }
}
String Maps::info() {
  StaticJsonDocument<1024> d;d["available"]=available;d["tiles"]=tileCount;d["name"]=title;d["latitude"]=latitude;d["longitude"]=longitude;d["center"]=haveCenter;d["zoom"]=zoom;d["follow"]=follow;d["uploading"]=bool(output);d["received"]=writeSize;d["expected"]=uploadSize;d["error"]=error;d["compressed_upload"]=true;d["online"]=internet.online();d["fetching"]=fetching;d["located"]=located;d["min_zoom"]=MinZoom;d["max_zoom"]=MaxZoom;
  unsigned ready=0;for(auto& t:tiles)if(t.ready)ready++;d["cached"]=ready;
  if(available){d["sd_total"]=double(SD.totalBytes());d["sd_used"]=double(SD.usedBytes());}String s;serializeJson(d,s);return s;
}
File Maps::openTile(unsigned z,uint32_t x,uint32_t y){if(!available||z<MinZoom||z>MaxZoom||x>=1UL<<z||y>=1UL<<z)return File();return SD.open(path(z,x,y));}
bool Maps::uploadBegin(unsigned z,uint32_t x,uint32_t y,uint32_t size,uint32_t crc) {
  if(!available||z<10||z>17||x>=(1UL<<z)||y>=(1UL<<z)||!size||size>Pixels*4||size%4) {error="Invalid map tile or SD unavailable";return false;}
  if(output){error="Map upload already active";return false;}
  if(SD.totalBytes()-SD.usedBytes()<size+8192){error="SD full";return false;}
  if(input){input.close();loading=-1;invalidate();}
  SD.mkdir("/meshmesh/maps/"+String(z));destination=path(z,x,y);SD.remove(destination+".part");output=SD.open(destination+".part",FILE_WRITE);
  if(!output){error="Cannot create map tile";return false;}
  uint8_t h[24]={};memcpy(h,"MMT1",4);h[4]=z;put32(h+8,x);put32(h+12,y);put32(h+16,size);put32(h+20,crc);
  if(output.write(h,24)!=24){uploadCancel();error="SD write error";return false;}
  uploadBuffer=(uint8_t*)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!uploadBuffer){uploadCancel();error="Map upload RAM allocation";return false;}
  compressed=false;uploadSize=size;writeSize=writePixels=0;writeExpected=size;writeExpectedCrc=crc;writeCrc=0xffffffff;writeAt=millis();error="";return true;
}
bool Maps::uploadCompressedBegin(unsigned z,uint32_t x,uint32_t y,uint32_t size,uint32_t crc,uint32_t packedSize){if(!packedSize||packedSize>262400){error="Invalid compressed tile size";return false;}if(!uploadBegin(z,x,y,size,crc))return false;free(uploadBuffer);uploadBuffer=(uint8_t*)heap_caps_malloc(packedSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!uploadBuffer){uploadCancel();error="Compressed upload RAM allocation";return false;}compressed=true;uploadSize=packedSize;return true;}
bool Maps::uploadChunk(const uint8_t* bytes,size_t size) {
  if(!output || !uploadBuffer || !size || (!compressed&&size%4) || size>2048 || writeSize+size>uploadSize){error="Invalid map chunk";return false;}
  if(!compressed){uint32_t pixels=writePixels;for(unsigned i=0;i<size;i+=4){unsigned n=bytes[i]|unsigned(bytes[i+1])<<8;if(!n||pixels+n>Pixels){uploadCancel();error="Invalid RLE pixels";return false;}pixels+=n;}writePixels=pixels;writeCrc=mapCrc(writeCrc,bytes,size);}
  memcpy(uploadBuffer+writeSize,bytes,size);writeSize+=size;writeAt=millis();return true;
}
bool Maps::uploadFinish() {
  if(!output){error="No map upload";return false;}
  if(writeSize!=uploadSize){uploadCancel();error="Incomplete map transfer";return false;}
  if(compressed){uint8_t* raw=(uint8_t*)heap_caps_malloc(writeExpected,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);auto* codec=(tinfl_decompressor*)malloc(sizeof(tinfl_decompressor));if(!raw||!codec){free(raw);free(codec);uploadCancel();error="Map decompression RAM allocation";return false;}
    // The miniz state exceeds the Arduino loop stack; keep it on the heap.
    tinfl_init(codec);size_t inSize=uploadSize,n=writeExpected;tinfl_status result=tinfl_decompress(codec,uploadBuffer,&inSize,raw,raw,&n,TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);free(codec);free(uploadBuffer);uploadBuffer=raw;compressed=false;writePixels=0;
    if(result!=TINFL_STATUS_DONE||inSize!=uploadSize||n!=writeExpected){uploadCancel();error="Invalid compressed map";return false;}for(unsigned i=0;i<n;i+=4){unsigned run=raw[i]|unsigned(raw[i+1])<<8;if(!run||writePixels+run>Pixels){uploadCancel();error="Invalid decompressed RLE";return false;}writePixels+=run;}writeCrc=mapCrc(0xffffffff,raw,n);writeSize=n;uploadSize=n;}
  if(writePixels!=Pixels || writeSize!=writeExpected || (writeCrc^0xffffffff)!=writeExpectedCrc){uploadCancel();error="Incomplete map tile or CRC mismatch";return false;}
  if(output.write(uploadBuffer,writeExpected)!=writeExpected){uploadCancel();error="SD map write error";return false;}free(uploadBuffer);uploadBuffer=nullptr;
  output.flush();output.close();
  // FAT rename cannot replace. Keep the previous complete tile until the new one is verified.
  String old=destination+".old";SD.remove(old);bool previous=SD.exists(destination);
  if(previous&&!SD.rename(destination,old)){SD.remove(destination+".part");error="Map replace failed";return false;}
  if(!SD.rename(destination+".part",destination)){if(previous)SD.rename(old,destination);error="Map commit failed";return false;}
  SD.remove(old);invalidate();error="";return true;
}
void Maps::uploadCancel() {if(output){output.close();SD.remove(destination+".part");}if(uploadBuffer)free(uploadBuffer);uploadBuffer=nullptr;compressed=false;writeSize=0;}
bool Maps::saveManifest(const String& value) {
  DynamicJsonDocument d(2048);if(!available||output||deserializeJson(d,value)||!d.is<JsonObject>()||!d["latitude"].is<double>()||!d["longitude"].is<double>()||!d["tiles"].is<unsigned>()||!d["name"].is<const char*>()){error="Invalid map manifest";return false;}
  double lat=d["latitude"],lon=d["longitude"];unsigned z=d["default_zoom"]|15;
  const char* name=d["name"];if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>85.05112878||fabs(lon)>180||z<10||z>17||!strlen(name)||strlen(name)>96||!meshmesh::validUtf8((const uint8_t*)name,strlen(name))){error="Invalid map bounds or name";return false;}
  String p="/meshmesh/maps/area.json";SD.remove(p+".part");File f=SD.open(p+".part",FILE_WRITE);if(!f){error="Cannot save map manifest";return false;}
  bool ok=f.print(value)==value.length();f.flush();f.close();if(!ok){SD.remove(p+".part");return false;}
  SD.remove(p+".old");bool previous=SD.exists(p);if(previous&&!SD.rename(p,p+".old"))return false;
  if(!SD.rename(p+".part",p)){if(previous)SD.rename(p+".old",p);return false;}SD.remove(p+".old");
  String identity=d["name"].as<String>()+String(lat,5)+String(lon,5)+String(z);char id[9];snprintf(id,sizeof(id),"%08x",mapCrc(0xffffffff,(const uint8_t*)identity.c_str(),identity.length())^0xffffffff);
  String catalog="/meshmesh/maps/areas/"+String(id)+".json";SD.remove(catalog+".part");File entry=SD.open(catalog+".part",FILE_WRITE);if(entry){entry.print(value);entry.close();SD.remove(catalog);SD.rename(catalog+".part",catalog);}
  title=d["name"].as<String>();tileCount=d["tiles"];readBounds(d.as<JsonVariantConst>());zoom=z;center(lat,lon);follow=false;markView();error="";return true;
}
String Maps::areas(){DynamicJsonDocument d(8192);JsonArray list=d.to<JsonArray>();if(available){File dir=SD.open("/meshmesh/maps/areas");for(File f=dir.openNextFile();f&&list.size()<24;f=dir.openNextFile()){String name=f.name();if(name.endsWith(".json")){StaticJsonDocument<2048> area;if(!deserializeJson(area,f)){JsonObject j=list.createNestedObject();j["id"]=name.substring(name.lastIndexOf('/')+1,name.length()-5);j["name"]=area["name"];j["latitude"]=area["latitude"];j["longitude"]=area["longitude"];j["zoom"]=area["default_zoom"];j["tiles"]=area["tiles"];}}f.close();}dir.close();}String s;serializeJson(d,s);return s;}
bool Maps::selectArea(const String& id){if(id.length()!=8)return false;for(char c:id)if(!isxdigit(c))return false;File f=SD.open("/meshmesh/maps/areas/"+id+".json");if(!f)return false;String value=f.readString();f.close();return saveManifest(value);}
String Maps::command(const String& line) {
  if(line=="map info")return info();
  if(line=="map areas")return areas();
  // Forget the view: GPS or, online, the IP location centres the map again.
  if(line=="map locate"){haveCenter=false;located=false;locateAt=0;follow=true;dirty=true;return internet.online()?"OK locating by IP":"OK waiting for GPS or internet";}
  if(line.startsWith("map forget ")){String id=line.substring(11);if(!available||id.length()!=8||output)return "ERR map catalog busy or invalid ID";for(char c:id)if(!isxdigit(c))return "ERR map ID";return SD.remove("/meshmesh/maps/areas/"+id+".json")?"OK map catalog entry removed":"ERR map entry not found";}
  if(line.startsWith("map has ")){unsigned z,x,y,size,crc;if(sscanf(line.c_str(),"map has %u %u %u %u %u",&z,&x,&y,&size,&crc)!=5)return "ERR map has fields";File f=openTile(z,x,y);uint8_t h[24];bool ok=f&&f.read(h,24)==24&&!memcmp(h,"MMT1",4)&&h[4]==z&&u32(h+8)==x&&u32(h+12)==y&&u32(h+16)==size&&u32(h+20)==crc&&f.size()==24+size;if(f)f.close();return ok?"OK map tile exists":"OK map tile missing";}
  if(line.startsWith("map select "))return selectArea(line.substring(11))?"OK map area selected":"ERR map area not found";
  if(line=="map cancel"){uploadCancel();return "OK map upload cancelled";}
  if(line=="map finish")return uploadFinish()?"OK map tile saved":"ERR "+error;
  if(line.startsWith("map zbegin ")){StaticJsonDocument<384>d;if(deserializeJson(d,line.substring(11))||!d["z"].is<unsigned>()||!d["x"].is<uint32_t>()||!d["y"].is<uint32_t>()||!d["size"].is<uint32_t>()||!d["crc"].is<uint32_t>()||!d["packed_size"].is<uint32_t>())return "ERR compressed map JSON";return uploadCompressedBegin(d["z"],d["x"],d["y"],d["size"],d["crc"],d["packed_size"])?"OK compressed map started":"ERR "+error;}
  if(line.startsWith("map begin ")) {StaticJsonDocument<256>d;if(deserializeJson(d,line.substring(10))||!d["z"].is<unsigned>()||!d["x"].is<uint32_t>()||!d["y"].is<uint32_t>()||!d["size"].is<uint32_t>()||!d["crc"].is<uint32_t>())return "ERR map begin JSON";return uploadBegin(d["z"],d["x"],d["y"],d["size"],d["crc"])?"OK map upload started":"ERR "+error;}
  if(line.startsWith("map chunk ")) {String text=line.substring(10);uint8_t bytes[640];size_t n=0;if(mbedtls_base64_decode(bytes,sizeof(bytes),&n,(const uint8_t*)text.c_str(),text.length()))return "ERR map base64";return uploadChunk(bytes,n)?"OK map chunk":"ERR "+error;}
  if(line.startsWith("map manifest "))return saveManifest(line.substring(13))?"OK map area saved":"ERR "+error;
  if(line.startsWith("map view ")){StaticJsonDocument<256>d;if(deserializeJson(d,line.substring(9))||!d.is<JsonObject>())return "ERR map view JSON";
    for(JsonPairConst pair:d.as<JsonObjectConst>()){String k=pair.key().c_str();if(k!="latitude"&&k!="longitude"&&k!="zoom"&&k!="follow")return "ERR unknown map view setting";}
    bool location=d.containsKey("latitude")||d.containsKey("longitude");double lat=latitude,lon=longitude;int z=zoom;bool track=follow;
    if(location){if(!d["latitude"].is<double>()||!d["longitude"].is<double>())return "ERR map coordinates";lat=d["latitude"];lon=d["longitude"];if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>85.05112878||fabs(lon)>180)return "ERR map coordinates";}
    if(d.containsKey("zoom")){if(!d["zoom"].is<int>())return "ERR map zoom integer";z=d["zoom"];if(z<MinZoom||z>MaxZoom)return "ERR map zoom 3..18";}
    if(d.containsKey("follow")){if(!d["follow"].is<bool>())return "ERR map follow boolean";track=d["follow"];}
    if(location)center(lat,lon);zoom=z;follow=track;dirty=true;markView();return "OK map view";}
  return "ERR unknown map command";
}
// Web tiles: the worker downloads and decodes; the loop shows the tile at once and
// writes the same MMT1 file an upload would, in 4 KB steps so LoRa keeps its turn.
void Maps::netTick(){
#if !defined(MM_COMPACT)
  bool on=internet.online();
  if(on!=wasOnline){wasOnline=on;for(auto& t:tiles){if(on&&t.missing)t.z=-1;if(!on&&t.fetch){t.fetch=false;t.missing=true;}}dirty=true;}
  if(!cacheOut){
    int z,x,y;const uint16_t* pixels;double lat,lon;
    switch(internet.result(z,x,y,pixels,lat,lon)){
    case Internet::TileOk:
      for(auto& t:tiles)if(t.z==z&&t.x==x&&t.y==y&&t.pixels){memcpy(t.pixels,pixels,Pixels*2);t.ready=true;t.fetch=t.missing=false;dirty=true;}
      if(available)cacheBegin(z,x,y,pixels);internet.release();break;
    case Internet::TileFailed:
      failed[failedNext]={z,x,y,millis()};failedNext=(failedNext+1)%16;
      for(auto& t:tiles)if(t.z==z&&t.x==x&&t.y==y&&t.fetch){t.fetch=false;t.missing=true;dirty=true;}
      internet.release();break;
    case Internet::Located:
      // City-level only: GPS replaces it as soon as there is a fix.
      if(!haveCenter){zoom=12;center(lat,lon);located=true;markView();}internet.release();break;
    case Internet::LocateFailed:internet.release();break;
    default:break;
    }
  }
  if(!on||!internet.idle())return;
  if(!haveCenter){if(!locateAt||millis()-locateAt>300000){locateAt=millis();internet.locate();}return;}
  Tile* next=nullptr;
  for(auto& t:tiles)if(t.fetch&&t.z==zoom&&millis()-t.stamp<3000&&(!next||int32_t(t.stamp-next->stamp)>0))next=&t;
  if(next)internet.fetchTile(next->z,next->x,next->y);
#endif
}
void Maps::cacheBegin(int z,int x,int y,const uint16_t* pixels){
  if(output)return;
  if(!cacheBuffer)cacheBuffer=(uint8_t*)heap_caps_malloc(Pixels*4,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!cacheBuffer)return;
  uint32_t n=0;for(uint32_t i=0;i<Pixels;){uint16_t c=pixels[i];uint32_t run=1;while(i+run<Pixels&&run<65535&&pixels[i+run]==c)run++;
    cacheBuffer[n]=run;cacheBuffer[n+1]=run>>8;cacheBuffer[n+2]=c;cacheBuffer[n+3]=c>>8;n+=4;i+=run;}
  // Free space is slow to compute on a large FAT volume: refresh it once a minute.
  if(!freeAt||millis()-freeAt>60000){freeAt=millis();freeBytes=SD.totalBytes()-SD.usedBytes();}
  if(freeBytes<n+65536){error="SD full: web tiles not cached";return;}
  SD.mkdir("/meshmesh/maps/"+String(z));cacheTarget=path(z,x,y);SD.remove(cacheTarget+".part");cacheOut=SD.open(cacheTarget+".part",FILE_WRITE);if(!cacheOut)return;
  uint8_t h[24]={};memcpy(h,"MMT1",4);h[4]=z;put32(h+8,x);put32(h+12,y);put32(h+16,n);put32(h+20,mapCrc(0xffffffff,cacheBuffer,n)^0xffffffff);
  if(cacheOut.write(h,24)!=24){cacheOut.close();SD.remove(cacheTarget+".part");return;}
  cacheSize=n;cacheAt=0;freeBytes-=n+24;
}
void Maps::cacheTick(){
  if(!cacheOut)return;
  uint32_t n=min<uint32_t>(4096,cacheSize-cacheAt);
  if(n&&cacheOut.write(cacheBuffer+cacheAt,n)!=n){cacheOut.close();SD.remove(cacheTarget+".part");error="SD cache write error";return;}
  cacheAt+=n;if(cacheAt<cacheSize)return;
  cacheOut.flush();cacheOut.close();
  // Same replacement order as uploads: the previous complete tile survives a power cut.
  String old=cacheTarget+".old";SD.remove(old);bool previous=SD.exists(cacheTarget);
  if(previous&&!SD.rename(cacheTarget,old)){SD.remove(cacheTarget+".part");return;}
  if(!SD.rename(cacheTarget+".part",cacheTarget)){if(previous)SD.rename(old,cacheTarget);return;}
  SD.remove(old);
}
