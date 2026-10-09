#pragma once
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include <esp_flash.h>
#include <spi_flash_chip_driver.h>
#include <esp_ota_ops.h>
#include <Preferences.h>
#if CONFIG_IDF_TARGET_ESP32S3
#include <esp32s3/rom/spi_flash.h>
#else
#include <esp32/rom/spi_flash.h>
#endif
// Storage outside the partition table's LittleFS region. A user's Heltec V3 erases that region (0x610000)
// but does not keep a write there, with no protection bits set. "fsformat" then puts LittleFS into the OTA
// slot that is not running (MeshMesh has no OTA update, the slot stays unused) as far as that slot keeps
// writes, and records the place in NVS. The partition record in RAM is pointed there before mounting, so
// esp_littlefs and esp_partition_* use it as the LittleFS partition; the partition table is not rewritten.
namespace storage {
struct Region{uint32_t at=0,size=0;};
inline esp_partition_t* partition(){return const_cast<esp_partition_t*>(esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"littlefs"));}
// The table's LittleFS region, captured before the record is pointed elsewhere.
inline Region home(){static Region r;static bool got=false;if(!got){if(auto* p=partition())r={p->address,p->size};got=true;}return r;}
inline void point(Region r){home();if(auto* p=partition()){p->address=r.at;p->size=r.size;}}
inline const esp_partition_t* idleSlot(){const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);return next&&next!=esp_ota_get_running_partition()?next:nullptr;}
inline Region moved(){Region r;Preferences p;if(p.begin("meshmesh-fs",true)){r.at=p.getUInt("at",0);r.size=p.getUInt("size",0);p.end();}return r;}
inline bool remember(Region r){
  Preferences p;if(!p.begin("meshmesh-fs",false))return false;
  bool saved=r.size?p.putUInt("at",r.at)==4&&p.putUInt("size",r.size)==4:p.clear();p.end();return saved;
}
inline bool inside(const esp_partition_t* slot,Region r){return slot&&r.size&&r.at>=slot->address&&r.at+r.size<=slot->address+slot->size;}
}
// Community boards: mounts LittleFS and creates it only on a blank partition (all 0xFF), as
// left by the factory image or a full erase. Data of another firmware is never formatted
// here; the USB command "fsformat" does that on request.
inline bool mountStorage() {
  storage::Region place=storage::moved();
  if(place.size){
    if(!storage::inside(storage::idleSlot(),place)){Serial.println("LittleFS: the storage recorded in the OTA slot does not fit this partition table");return false;}
    storage::point(place);Serial.printf("LittleFS: storage in the free OTA slot at 0x%x, %u KB\n",unsigned(place.at),unsigned(place.size/1024));
  }
  if(LittleFS.begin(false,"/littlefs",10,"littlefs"))return true;
  const esp_partition_t* part=storage::partition();
  if(!part)return false;
  static uint32_t block[256];
  for(size_t at=0;at<part->size;at+=sizeof(block)){
    if(esp_partition_read(part,at,block,sizeof(block)))return false;
    for(uint32_t word:block)if(word!=0xffffffff){Serial.println("LittleFS: partition holds other data; USB command fsformat creates storage");return false;}
  }
  Serial.println("LittleFS: blank partition, creating storage");
  return LittleFS.format()&&LittleFS.begin(false,"/littlefs",10,"littlefs");
}
// Flash status register (SR1 | SR2<<8 | SR3<<16 as the ROM reads them); unlock clears the block-protect bits
// as ESP-IDF 4.x did before writing (spi_flash_unlock). SR3 (command 0x15) holds the Winbond-style WPS bit
// (bit 2): with it set, per-block locks protect the flash while SR1 shows no protection bits.
// Runs from IRAM with the flash cache off.
static IRAM_ATTR uint32_t flashStatus(bool unlock) {
  uint32_t low=0,high=0,third=0;const spi_flash_guard_funcs_t* guard=spi_flash_guard_get();guard->start();
  esp_rom_spiflash_read_status(&g_rom_flashchip,&low);esp_rom_spiflash_read_statushigh(&g_rom_flashchip,&high);
  esp_rom_spiflash_read_user_cmd(&third,0x15);
  if(unlock)esp_rom_spiflash_unlock();
  guard->end();return (low&0xff)|((high&0xff)<<8)|((third&0xff)<<16);
}
// USB "flashstatus": the chip's JEDEC ID, its size by that ID and by the image header, the status registers
// and where LittleFS lives.
inline String flashInfo() {
  uint32_t id=0,physical=0;esp_flash_read_id(nullptr,&id);esp_flash_get_physical_size(nullptr,&physical);
  const esp_partition_t* part=storage::partition();storage::Region home=storage::home();
  // The esp_flash driver's mode for erase, write and direct reads (SPI1); the cache reads use the image header's mode.
  static const char* const modes[]={"slowrd","fastrd","dout","dio","qout","qio"};const esp_flash_t* chip=esp_flash_default_chip;
  const char* mode=chip&&chip->read_mode<6?modes[chip->read_mode]:"?";const char* driver=chip&&chip->chip_drv?chip->chip_drv->name:"?";
  char s[256];snprintf(s,sizeof(s),"OK flash id %06x, size %u KB (image header %u KB), driver %s %s, status %06x (SR3<<16 | SR2<<8 | SR1; SR1 bits 2-6 and SR3 bit 2 protect blocks), storage at 0x%x, %u KB%s",
    unsigned(id),unsigned(physical/1024),unsigned(spi_flash_get_chip_size()/1024),driver,mode,unsigned(flashStatus(false)),
    part?unsigned(part->address):0,part?unsigned(part->size/1024):0,part&&part->address!=home.at?" (moved to the free OTA slot)":"");
  return s;
}
// Erases one sector, writes a pattern and reads it back: "ok", "... failed" or the first word read back.
inline String flashSectorTest(uint32_t address) {
  const uint32_t probe[4]={0x4d657368,0x4d657368,0x5a5aa5a5,0x0f1e2d3c};uint32_t back[4]={};char word[12];
  if(esp_flash_erase_region(nullptr,address,SPI_FLASH_SEC_SIZE))return "erase failed";
  if(esp_flash_read(nullptr,back,address,sizeof(back)))return "read failed";
  for(uint32_t w:back)if(w!=0xffffffff){snprintf(word,sizeof(word),"%08x",unsigned(back[0]));return String("not erased, reads ")+word;}
  if(esp_flash_write(nullptr,probe,address,sizeof(probe))||esp_flash_read(nullptr,back,address,sizeof(back)))return "write failed";
  esp_flash_erase_region(nullptr,address,SPI_FLASH_SEC_SIZE);
  if(!memcmp(probe,back,sizeof(probe)))return "ok";
  snprintf(word,sizeof(word),"%08x",unsigned(back[0]));return String("lost, reads ")+word;
}
// The same test through the ROM functions, as esptool writes: the mode the bootloader set (the image
// header's), not the esp_flash driver's. Runs from IRAM with the flash cache off, so the data is in DRAM.
// ESP32-S3 only (the Heltec V3 case): the classic ESP32 builds have no IRAM to spare.
#if CONFIG_IDF_TARGET_ESP32S3
static IRAM_ATTR __attribute__((noinline)) int romSectorTest(uint32_t address,uint32_t* back) {
  static DRAM_ATTR const uint32_t probe[4]={0x4d657368,0x524f4d21,0x5a5aa5a5,0x0f1e2d3c};
  const spi_flash_guard_funcs_t* guard=spi_flash_guard_get();guard->start();
  int r=esp_rom_spiflash_erase_sector(address/SPI_FLASH_SEC_SIZE);
  if(!r)r=esp_rom_spiflash_write(address,probe,sizeof(probe));
  if(!r)r=esp_rom_spiflash_read(address,back,16);
  int erased=esp_rom_spiflash_erase_sector(address/SPI_FLASH_SEC_SIZE);
  guard->end();
  if(r)return 1;if(erased)return 2;
  for(int i=0;i<4;i++)if(back[i]!=probe[i])return 3;
  return 0;
}
inline String romSectorReport(uint32_t address) {
  uint32_t back[4]={};char s[48];int r=romSectorTest(address,back);
  if(r==1)return "rom failed";if(r==2)return "rom erase-back failed";if(!r)return "rom ok";
  snprintf(s,sizeof(s),"rom lost, reads %08x",unsigned(back[0]));return s;
}
#endif
// USB "flashprobe": the erase and write test at the first and last sector and every megabyte boundary of
// the free OTA slot and the table's LittleFS region, each only while nothing lives there; at the first
// sector also through the ROM functions (ESP32-S3). Shows where the flash stops keeping writes; destroys
// only data nobody uses.
inline String flashProbe(bool fsMounted) {
  String out="OK flash probe:";
  auto region=[&](uint32_t start,uint32_t size){
    auto test=[&](uint32_t address){char at[16];snprintf(at,sizeof(at)," 0x%x ",unsigned(address));out+=at;out+=flashSectorTest(address);out+=';';};
    test(start);
#if CONFIG_IDF_TARGET_ESP32S3
    {char at[16];snprintf(at,sizeof(at)," 0x%x ",unsigned(start));out+=at;out+=romSectorReport(start);out+=';';}
#endif
    for(uint32_t at=(start+0xfffff)&~0xfffffu;at<start+size;at+=0x100000)if(at>start)test(at);
    test(start+size-SPI_FLASH_SEC_SIZE);
  };
  bool moved=storage::partition()&&storage::partition()->address!=storage::home().at;
  const esp_partition_t* idle=storage::idleSlot();
  if(!idle)out+=" no free OTA slot;";else if(moved)out+=" the free OTA slot holds the storage, not tested;";else region(idle->address,idle->size);
  if(fsMounted&&!moved)out+=" littlefs mounted, not tested;";else if(storage::home().size)region(storage::home().at,storage::home().size);
  return out;
}
// USB "fsformat" on ESP32: frees the superblock pair (the first two blocks) of the table's LittleFS region and
// checks that the flash takes an erase and a write there before littlefs formats it, so a failure names the
// step. When the flash does not keep the write, the storage goes to the free OTA slot instead (see storage).
// Formatting over another firmware's bytes works in QEMU.
inline String homeCheck(String& note) {
  const esp_partition_t* part=storage::partition();
  const size_t span=2*SPI_FLASH_SEC_SIZE;uint32_t block[64];char at[16];snprintf(at,sizeof(at),"0x%x",unsigned(part->address));
  auto blank=[&]()->bool{for(size_t o=0;o<span;o+=sizeof(block)){if(esp_partition_read(part,o,block,sizeof(block)))return false;for(uint32_t w:block)if(w!=0xffffffff)return false;}return true;};
  esp_err_t e=esp_partition_erase_range(part,0,span);
  if(e)return String("flash erase at ")+at+": "+esp_err_to_name(e);
  if(!blank())return String("flash at ")+at+" keeps data after erase";
  const uint32_t probe[4]={0x4d657368,0x4d657368,0x5a5aa5a5,0x0f1e2d3c};uint32_t back[4]={};
  auto written=[&]()->esp_err_t{esp_err_t r=esp_partition_write(part,0,probe,sizeof(probe));if(!r)r=esp_partition_read(part,0,back,sizeof(back));return r;};
  e=written();if(e)return String("flash write at ")+at+": "+esp_err_to_name(e);
#if defined(MM_TEST_STORAGE_MOVE)
  back[0]=~back[0]; // emulator check of the move: the region acts as if it lost the write
#endif
  if(memcmp(probe,back,sizeof(probe))){
    // Block-protect bits (SR1 bits 2-6) could guard the top of the flash: cleared only when set, then the
    // erase and write are tried again.
    uint32_t sr=flashStatus(false);char status[64];
    if(!(sr&0x7c)){snprintf(status,sizeof(status)," (reads %08x; status %06x, no SR1 protection bits)",unsigned(back[0]),unsigned(sr));return String("flash at ")+at+" does not keep a write"+status;}
    uint32_t after=(flashStatus(true),flashStatus(false));snprintf(status,sizeof(status)," (status %06x, after unlock %06x)",unsigned(sr),unsigned(after));
    if(esp_partition_erase_range(part,0,span)||!blank()||written()||memcmp(probe,back,sizeof(probe)))return String("flash at ")+at+" is write-protected"+status;
    note=String("; flash protection cleared")+status;
  }
  e=esp_partition_erase_range(part,0,span);if(e||!blank())return String("flash erase at ")+at+" after the write test";
  return "";
}
inline String formatStorage() {
  if(!storage::partition())return "ERR no littlefs partition";
  storage::point(storage::home());String note,failed=homeCheck(note);
  if(failed.isEmpty()){
    if(storage::moved().size&&!storage::remember({}))return "ERR NVS: the storage place not cleared"; // nothing recorded: NVS not needed
    if(!LittleFS.format())return "ERR littlefs format failed on a flash that passed the erase and write test";
    if(!LittleFS.begin(false,"/littlefs",10,"littlefs"))return "ERR littlefs mount after format";
    return "OK MeshMesh filesystem initialized"+note;
  }
  // The free OTA slot, 64 KB steps from its start while each step keeps a write, up to the table's size.
  const esp_partition_t* slot=storage::idleSlot();
  if(!slot)return "ERR "+failed+"; no free OTA slot for the storage";
  uint32_t limit=min(slot->size,storage::home().size),good=0;String stop;
  for(uint32_t o=0;o<limit;o+=0x10000){String r=flashSectorTest(slot->address+o);if(r!="ok"){char at[16];snprintf(at,sizeof(at),"0x%x ",unsigned(slot->address+o));stop=at+r;break;}good=o+0x10000;}
  if(good<0x40000)return "ERR "+failed+"; the free OTA slot does not keep writes either ("+stop+")";
  storage::Region place{slot->address,good};storage::point(place);
  if(!LittleFS.format()||!LittleFS.begin(false,"/littlefs",10,"littlefs")){storage::point(storage::home());return "ERR "+failed+"; littlefs format in the free OTA slot failed";}
  if(!storage::remember(place)){LittleFS.end();storage::point(storage::home());return "ERR "+failed+"; NVS: the storage place not saved";}
  char s[96];snprintf(s,sizeof(s),"OK MeshMesh filesystem initialized in the free OTA slot at 0x%x, %u KB: ",unsigned(place.at),unsigned(good/1024));
  return s+failed;
}
