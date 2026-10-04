#pragma once
#include <LittleFS.h>
#include <esp_partition.h>
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
inline String formatStorage() {
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"littlefs");
  if(!part)return "ERR no littlefs partition";
  const size_t span=2*SPI_FLASH_SEC_SIZE;uint32_t block[64];char at[16];snprintf(at,sizeof(at),"0x%x",unsigned(part->address));
  auto blank=[&]()->bool{for(size_t o=0;o<span;o+=sizeof(block)){if(esp_partition_read(part,o,block,sizeof(block)))return false;for(uint32_t w:block)if(w!=0xffffffff)return false;}return true;};
  esp_err_t e=esp_partition_erase_range(part,0,span);
  if(e)return String("ERR flash erase at ")+at+": "+esp_err_to_name(e);
  if(!blank())return String("ERR flash at ")+at+" keeps data after erase (write-protected or faulty flash)";
  const uint32_t probe[4]={0x4d657368,0x4d657368,0x5a5aa5a5,0x0f1e2d3c};uint32_t back[4]={};
  e=esp_partition_write(part,0,probe,sizeof(probe));if(!e)e=esp_partition_read(part,0,back,sizeof(back));
  if(e)return String("ERR flash write at ")+at+": "+esp_err_to_name(e);
  if(memcmp(probe,back,sizeof(probe)))return String("ERR flash at ")+at+" does not keep a write (write-protected or faulty flash)";
  e=esp_partition_erase_range(part,0,span);if(e||!blank())return String("ERR flash erase at ")+at+" after the write test";
  if(!LittleFS.format())return "ERR littlefs format failed on a flash that passed the erase and write test";
  if(!LittleFS.begin(false,"/littlefs",10,"littlefs"))return "ERR littlefs mount after format";
  return "OK MeshMesh filesystem initialized";
}
