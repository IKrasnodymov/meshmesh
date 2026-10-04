#pragma once
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#if CONFIG_IDF_TARGET_ESP32S3
#include <esp32s3/rom/spi_flash.h>
#else
#include <esp32/rom/spi_flash.h>
#endif
// Community boards: mounts LittleFS and creates it only on a blank partition (all 0xFF), as
// left by the factory image or a full erase. Data of another firmware is never formatted
// here; the USB command "fsformat" does that on request.
inline bool mountStorage() {
  if(LittleFS.begin(false,"/littlefs",10,"littlefs"))return true;
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"littlefs");
  if(!part)return false;
  static uint32_t block[256];
  for(size_t at=0;at<part->size;at+=sizeof(block)){
    if(esp_partition_read(part,at,block,sizeof(block)))return false;
    for(uint32_t word:block)if(word!=0xffffffff){Serial.println("LittleFS: partition holds other data; USB command fsformat creates storage");return false;}
  }
  Serial.println("LittleFS: blank partition, creating storage");
  return LittleFS.format()&&LittleFS.begin(false,"/littlefs",10,"littlefs");
}
// USB "fsformat" on ESP32: frees the superblock pair (the first two blocks) and checks that the flash
// takes an erase and a write before littlefs formats it, so a failure names the step. Formatting over
// another firmware's bytes works in QEMU; a board that still fails reports the flash fault here.
// Flash status register (SR1 | SR2<<8 as the ROM reads them); unlock clears the block-protect bits as
// ESP-IDF 4.x did before writing (spi_flash_unlock). Runs from IRAM with the flash cache off.
static IRAM_ATTR uint32_t flashStatus(bool unlock) {
  uint32_t low=0,high=0;const spi_flash_guard_funcs_t* guard=spi_flash_guard_get();guard->start();
  esp_rom_spiflash_read_status(&g_rom_flashchip,&low);esp_rom_spiflash_read_statushigh(&g_rom_flashchip,&high);
  if(unlock)esp_rom_spiflash_unlock();
  guard->end();return (low&0xff)|((high&0xff)<<8);
}
inline String formatStorage() {
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"littlefs");
  if(!part)return "ERR no littlefs partition";
  const size_t span=2*SPI_FLASH_SEC_SIZE;uint32_t block[64];char at[16];snprintf(at,sizeof(at),"0x%x",unsigned(part->address));
  auto blank=[&]()->bool{for(size_t o=0;o<span;o+=sizeof(block)){if(esp_partition_read(part,o,block,sizeof(block)))return false;for(uint32_t w:block)if(w!=0xffffffff)return false;}return true;};
  esp_err_t e=esp_partition_erase_range(part,0,span);
  if(e)return String("ERR flash erase at ")+at+": "+esp_err_to_name(e);
  if(!blank())return String("ERR flash at ")+at+" keeps data after erase (write-protected or faulty flash)";
  const uint32_t probe[4]={0x4d657368,0x4d657368,0x5a5aa5a5,0x0f1e2d3c};uint32_t back[4]={};
  String note;
  auto written=[&]()->esp_err_t{esp_err_t r=esp_partition_write(part,0,probe,sizeof(probe));if(!r)r=esp_partition_read(part,0,back,sizeof(back));return r;};
  e=written();if(e)return String("ERR flash write at ")+at+": "+esp_err_to_name(e);
  if(memcmp(probe,back,sizeof(probe))){
    // A Heltec V3 erased this region but did not keep a write: block-protect bits (SR1 bits 2-6)
    // guard the top of the flash. Cleared only when set, then the erase and write are tried again.
    uint32_t sr=flashStatus(false);char status[48];
    if(!(sr&0x7c)){snprintf(status,sizeof(status)," (status %04x, no protection bits)",unsigned(sr));return String("ERR flash at ")+at+" does not keep a write"+status;}
    uint32_t after=(flashStatus(true),flashStatus(false));snprintf(status,sizeof(status)," (status %04x, after unlock %04x)",unsigned(sr),unsigned(after));
    if(esp_partition_erase_range(part,0,span)||!blank()||written()||memcmp(probe,back,sizeof(probe)))return String("ERR flash at ")+at+" is write-protected"+status;
    note=String("; flash protection cleared")+status;
  }
  e=esp_partition_erase_range(part,0,span);if(e||!blank())return String("ERR flash erase at ")+at+" after the write test";
  if(!LittleFS.format())return "ERR littlefs format failed on a flash that passed the erase and write test";
  if(!LittleFS.begin(false,"/littlefs",10,"littlefs"))return "ERR littlefs mount after format";
  return "OK MeshMesh filesystem initialized"+note;
}
