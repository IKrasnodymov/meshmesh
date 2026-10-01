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
