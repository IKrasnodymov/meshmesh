#pragma once
// ESP-IDF calls used by the shared code, on nRF52 (src/nrf52/Platform.cpp).
#include <Arduino.h>
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
enum esp_reset_reason_t {ESP_RST_UNKNOWN=0,ESP_RST_POWERON=1,ESP_RST_EXT=2,ESP_RST_SW=3,ESP_RST_PANIC=4,ESP_RST_INT_WDT=5,ESP_RST_TASK_WDT=6,ESP_RST_WDT=7,ESP_RST_DEEPSLEEP=8,ESP_RST_BROWNOUT=9};
// Hardware random numbers: the SoftDevice pool once Bluetooth runs, the RNG peripheral before.
void esp_fill_random(void* buffer,size_t size);
uint32_t esp_random();
esp_reset_reason_t esp_reset_reason();
inline float temperatureRead(){return readCPUTemperature();}
class EspClass {
 public:
  uint64_t getEfuseMac(); // the factory device address in FICR
  uint32_t getFreeHeap();
  uint32_t getFreePsram(){return 0;}
  uint32_t getPsramSize(){return 0;}
  [[noreturn]] void restart();
};
extern EspClass ESP;
