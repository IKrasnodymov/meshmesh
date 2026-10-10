// nRF52 counterparts of the ESP32 services the shared code uses: storage, preferences, random
// numbers, reset reason, the clock behind time() and the build hash.
#include <Arduino.h>
#include <SHA256.h>
#include <sys/time.h>
#include <time.h>
#include <malloc.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <flash/flash_nrf5x.h>
#include "compat/LittleFS.h"
#include "compat/Preferences.h"
#include "compat/esp_system.h"
#include "compat/esp_ota_ops.h"

// ---- storage ----
namespace {
constexpr uint32_t BlockSize=128;
uint32_t blockAddress(lfs_block_t block){return MeshFS::Start+block*BlockSize;}
int flashRead(const lfs_config*,lfs_block_t block,lfs_off_t off,void* buffer,lfs_size_t size){return flash_nrf5x_read(buffer,blockAddress(block)+off,size)>0?0:-1;}
int flashProg(const lfs_config*,lfs_block_t block,lfs_off_t off,const void* buffer,lfs_size_t size){return flash_nrf5x_write(blockAddress(block)+off,buffer,size)>0?0:-1;}
int flashErase(const lfs_config*,lfs_block_t block){uint32_t at=blockAddress(block);for(uint32_t i=0;i<BlockSize;i++)flash_nrf5x_write8(at+i,0xff);return 0;}
int flashSync(const lfs_config*){flash_nrf5x_flush();return 0;}
lfs_config fsConfig={
  .context=nullptr,.read=flashRead,.prog=flashProg,.erase=flashErase,.sync=flashSync,
  .read_size=BlockSize,.prog_size=BlockSize,.block_size=BlockSize,.block_count=MeshFS::Size/BlockSize,.lookahead=128,
  .read_buffer=nullptr,.prog_buffer=nullptr,.lookahead_buffer=nullptr,.file_buffer=nullptr};
}
MeshFS LittleFS;
MeshFS::MeshFS():Adafruit_LittleFS(&fsConfig){}
bool MeshFS::blank() const{
  uint32_t words[64];
  for(uint32_t at=Start;at<Start+Size;at+=sizeof(words)){flash_nrf5x_read(words,at,sizeof(words));for(uint32_t w:words)if(w!=0xffffffff)return false;}
  return true;
}
bool MeshFS::begin(bool formatOnFail,const char*,uint8_t,const char*){
  if(_mounted||Adafruit_LittleFS::begin())return true;
  return formatOnFail&&format()&&Adafruit_LittleFS::begin();
}
// Erases the whole region first: littlefs v1 formats on top of old bytes otherwise.
bool MeshFS::format(){
  if(_mounted)end();
  for(uint32_t at=Start;at<Start+Size;at+=FLASH_NRF52_PAGE_SIZE)if(!flash_nrf5x_erase(at))return false;
  flash_nrf5x_flush();if(!Adafruit_LittleFS::format()||!Adafruit_LittleFS::begin())return false;
  File f=Adafruit_LittleFS::open(marker,FILE_O_WRITE);if(!f)return false;f.write((const uint8_t*)"MeshMesh\n",9);f.close();return true;
}
// Creates storage only on a blank region (all 0xFF); bytes of an earlier firmware are never
// formatted here: the USB command "fsformat" does that on request.
// A littlefs of another firmware in the same region (MeshCore companion keeps its ExtraFS at
// 0xD4000) mounts as well: without the MeshMesh marker and with files in it, it stays foreign.
bool mountStorage(){
  if(LittleFS.begin()){
    if(LittleFS.exists(MeshFS::marker))return true;
    File root=LittleFS.open("/",FILE_O_READ);File first=root?root.openNextFile():File(LittleFS);bool empty=!first;if(first)first.close();if(root)root.close();
    if(empty){File f=LittleFS.open(MeshFS::marker,FILE_O_WRITE);if(f){f.write((const uint8_t*)"MeshMesh\n",9);f.close();return true;}}
    LittleFS.end();Serial.println("LittleFS: region holds another firmware's files; USB command fsformat creates storage");return false;
  }
  if(!LittleFS.blank()){Serial.println("LittleFS: region holds other data; USB command fsformat creates storage");return false;}
  Serial.println("LittleFS: blank region, creating storage");return LittleFS.format()&&LittleFS.begin();
}
File MeshFS::open(const char* path,const char* mode,bool){
  if(mode[0]=='w'){if(exists(path))remove(path);return Adafruit_LittleFS::open(path,FILE_O_WRITE);}
  if(mode[0]=='a')return Adafruit_LittleFS::open(path,FILE_O_WRITE); // the core seeks to the end
  return Adafruit_LittleFS::open(path,FILE_O_READ);
}

// ---- preferences ----
bool Preferences::begin(const char* name,bool readOnly,const char*){
  end();if(!LittleFS.mounted()||strlen(name)>24)return false;
  snprintf(path,sizeof(path),"/prefs/%s",name);writable=!readOnly;size=0;
  File f=LittleFS.open(path,FILE_O_READ);
  if(f){size_t n=f.size();if(n&&n<16384){data=(uint8_t*)malloc(n);if(data&&f.read(data,n)==int(n))size=n;else{free(data);data=nullptr;}}f.close();}
  // A damaged record table is dropped from the first record that runs past the end.
  size_t at=0;while(at<size){size_t k=data[at];if(at+1+k+2>size)break;size_t v=data[at+1+k]|data[at+2+k]<<8;if(at+3+k+v>size)break;at+=3+k+v;}size=at;
  open=true;return true;
}
void Preferences::end(){free(data);data=nullptr;size=0;open=false;batched=changed=failed=false;}
bool Preferences::beginBatch(){if(!open||!writable||batched)return false;batched=true;changed=failed=false;return true;}
bool Preferences::commitBatch(){if(!batched)return false;batched=false;if(failed)return false;bool dirty=changed;changed=false;return !dirty||writeback();}
int Preferences::find(const char* key) const{
  size_t length=strlen(key);
  for(size_t at=0;at<size;){size_t k=data[at],v=data[at+1+k]|data[at+2+k]<<8;if(k==length&&!memcmp(data+at+1,key,k))return at;at+=3+k+v;}
  return -1;
}
size_t Preferences::entryLength(int at) const{size_t k=data[at];return data[at+1+k]|data[at+2+k]<<8;}
bool Preferences::get(const char* key,void* out,size_t n){int at=find(key);if(at<0||entryLength(at)!=n)return false;memcpy(out,data+at+3+data[at],n);return true;}
size_t Preferences::getBytes(const char* key,void* out,size_t max){int at=find(key);if(at<0)return 0;size_t n=entryLength(at);if(n>max)return 0;memcpy(out,data+at+3+data[at],n);return n;}
size_t Preferences::getString(const char* key,char* out,size_t max){int at=find(key);if(at<0||!max)return 0;size_t n=entryLength(at);if(n+1>max)return 0;memcpy(out,data+at+3+data[at],n);out[n]=0;return n+1;}
String Preferences::getString(const char* key,const String& d){int at=find(key);if(at<0)return d;size_t n=entryLength(at);String s;s.reserve(n);for(size_t i=0;i<n;i++)s+=char(data[at+3+data[at]+i]);return s;}
bool Preferences::isKey(const char* key){return find(key)>=0;}
bool Preferences::remove(const char* key){
  if(!open||!writable)return fail();int at=find(key);if(at<0)return true;
  size_t length=3+data[at]+entryLength(at);memmove(data+at,data+at+length,size-at-length);size-=length;return flush();
}
bool Preferences::clear(){if(!open||!writable)return fail();size=0;return flush();}
bool Preferences::put(const char* key,const void* value,size_t n){
  size_t k=strlen(key);if(!open||!writable||!k||k>15||n>8192)return fail();
  int at=find(key);
  if(at>=0&&entryLength(at)==n){if(!memcmp(data+at+3+k,value,n))return true;memcpy(data+at+3+k,value,n);return flush();}
  size_t keep=size;if(at>=0)keep-=3+k+entryLength(at);
  uint8_t* next=(uint8_t*)malloc(keep+3+k+n);if(!next)return fail();
  size_t used=0;
  for(size_t i=0;i<size;){size_t kk=data[i],vv=data[i+1+kk]|data[i+2+kk]<<8;if(int(i)!=at){memcpy(next+used,data+i,3+kk+vv);used+=3+kk+vv;}i+=3+kk+vv;}
  next[used]=k;memcpy(next+used+1,key,k);next[used+1+k]=n&0xff;next[used+2+k]=n>>8;memcpy(next+used+3+k,value,n);used+=3+k+n;
  free(data);data=next;size=used;return flush();
}
bool Preferences::flush(){
  if(batched){changed=true;return true;}return writeback();
}
bool Preferences::writeback(){
  char temporary[44];snprintf(temporary,sizeof(temporary),"%s.new",path);
  LittleFS.mkdir("/prefs");LittleFS.remove(temporary);
  File f=LittleFS.open(temporary,FILE_O_WRITE);if(!f)return false;
  bool ok=size==0||f.write(data,size)==size;f.close();if(!ok){LittleFS.remove(temporary);return false;}
  // littlefs rename replaces the destination atomically; keep it until that commit.
  return LittleFS.rename(temporary,path);
}

// ---- random numbers, reset reason, chip identity ----
EspClass ESP;
void esp_fill_random(void* buffer,size_t size){
  uint8_t* out=(uint8_t*)buffer;uint8_t enabled=0;sd_softdevice_is_enabled(&enabled);
  while(size){
    if(enabled){uint8_t available=0;sd_rand_application_bytes_available_get(&available);if(!available){delay(1);continue;}
      uint8_t n=min<size_t>(available,size);if(sd_rand_application_vector_get(out,n)==NRF_SUCCESS){out+=n;size-=n;}continue;}
    NRF_RNG->CONFIG=RNG_CONFIG_DERCEN_Msk;NRF_RNG->EVENTS_VALRDY=0;NRF_RNG->TASKS_START=1;
    while(!NRF_RNG->EVENTS_VALRDY){}*out++=NRF_RNG->VALUE;size--;NRF_RNG->TASKS_STOP=1;
  }
}
uint32_t esp_random(){uint32_t v;esp_fill_random(&v,sizeof(v));return v;}
esp_reset_reason_t esp_reset_reason(){
  uint32_t r=readResetReason();
  if(r&POWER_RESETREAS_RESETPIN_Msk)return ESP_RST_EXT;if(r&POWER_RESETREAS_DOG_Msk)return ESP_RST_WDT;
  if(r&POWER_RESETREAS_SREQ_Msk)return ESP_RST_SW;if(r&POWER_RESETREAS_LOCKUP_Msk)return ESP_RST_PANIC;
  if(r&(POWER_RESETREAS_OFF_Msk|POWER_RESETREAS_LPCOMP_Msk|POWER_RESETREAS_VBUS_Msk))return ESP_RST_DEEPSLEEP;
  return r?ESP_RST_UNKNOWN:ESP_RST_POWERON;
}
uint64_t EspClass::getEfuseMac(){return (uint64_t(NRF_FICR->DEVICEADDR[1]&0xffff)<<32|NRF_FICR->DEVICEADDR[0])|0xc00000000000ULL;}
uint32_t EspClass::getFreeHeap(){return dbgHeapTotal()-dbgHeapUsed();}
void EspClass::restart(){flash_nrf5x_flush();NVIC_SystemReset();while(true){}}

// ---- clock: time() and settimeofday() over millis(), as the ESP32 keeps them ----
namespace {int64_t epochOffsetMs=0;uint32_t lastMs=0,wraps=0;
int64_t monotonicMs(){uint32_t now=millis();if(now<lastMs)wraps++;lastMs=now;return (int64_t(wraps)<<32)+now;}}
extern "C" int _gettimeofday(struct timeval* tv,void*){if(!tv)return 0;int64_t ms=monotonicMs()+epochOffsetMs;tv->tv_sec=ms/1000;tv->tv_usec=(ms%1000)*1000;return 0;}
extern "C" int settimeofday(const struct timeval* tv,const struct timezone*){if(!tv)return -1;epochOffsetMs=int64_t(tv->tv_sec)*1000+tv->tv_usec/1000-monotonicMs();return 0;}

// ---- build hash: SHA-256 of the image from the application start to the end of its initialised data ----
extern "C" uint32_t __etext,__data_start__,__data_end__;
const esp_app_desc_t* esp_ota_get_app_description(){
  static esp_app_desc_t desc;static bool done=false;if(done)return &desc;
  uint32_t start=0x26000,end=uint32_t(&__etext)+(uint32_t(&__data_end__)-uint32_t(&__data_start__));
  SHA256 h;h.update((const void*)start,end-start);h.finalize(desc.app_elf_sha256,32);done=true;return &desc;
}
