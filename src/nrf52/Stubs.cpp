// nRF52 boards have no Wi-Fi and no SD card: offline maps, the Wi-Fi client and the USB
// Wi-Fi/BLE probes answer that they are not available.
#include "Maps.h"
#include "Internet.h"
#include "WifiDiagnostics.h"
#include "BleDiagnostics.h"
Maps maps;
Internet internet;
// The same fields as a board without an SD card answers (src/Maps.cpp), so the page shows "no SD card".
String Maps::info(){return "{\"available\":false,\"tiles\":0,\"name\":\"\",\"latitude\":0,\"longitude\":0,\"center\":false,\"zoom\":15,\"follow\":true,\"uploading\":false,\"received\":0,\"expected\":0,\"error\":\"\",\"compressed_upload\":false,\"online\":false,\"fetching\":0,\"located\":false,\"cached\":0}";}
String Maps::areas(){return "[]";}
String Maps::command(const String& line){if(line=="map info")return info();if(line=="map areas")return areas();return "ERR offline maps need a board with an SD card";}
uint32_t mapCrc(uint32_t crc,const uint8_t* bytes,size_t size){
  for(size_t i=0;i<size;i++){crc^=bytes[i];for(int k=0;k<8;k++)crc=crc&1?(crc>>1)^0xedb88320u:crc>>1;}return crc;
}
String startWifiProbe(JsonObjectConst){return "ERR no Wi-Fi on this board";}
String wifiProbeResult(){return "{\"active\":false,\"error\":\"no Wi-Fi on this board\"}";}
bool wifiProbeActive(){return false;}
String startBleProbe(JsonObjectConst){return "ERR BLE probe needs an ESP32 board";}
String bleProbeResult(){return "{\"active\":false,\"error\":\"BLE probe needs an ESP32 board\"}";}
bool bleProbeActive(){return false;}
