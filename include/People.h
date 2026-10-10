#pragma once
#include "Board.h"
#include <ArduinoJson.h>
namespace people {
struct Settings {int32_t manual=0;uint16_t window=60;int8_t rssi=-85;bool ble=false,wifi=false,personal=false;};
extern Settings settings;
void begin();void tick();void flush();bool change(int delta);void resetWindow();
void hear(bool wifi,uint64_t address,int rssi,bool personal=true);
void noteDropped(bool wifi,unsigned count);unsigned count(bool wifi);bool saturated(bool wifi);uint32_t dropped(bool wifi);
String json();String command(JsonObjectConst values);
void wifiTick();void releaseWifi();bool wifiRunning();const char* wifiState();
}
