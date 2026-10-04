#pragma once
#include <Arduino.h>
// Power saving on ESP32 boards (nRF52: the idle task already sleeps; these do nothing there).
// CPU clock: 240 MHz while someone uses the device (screen lit, Wi-Fi, radar, recent USB or BLE
// commands) or the Wi-Fi driver or Bluetooth controller has been started, 80 MHz otherwise. Repeater and room modes also enter light sleep between packets:
// the LoRa IRQ line, the button, USB-UART bytes and a 0.5 s timer wake the board. A USB-UART host
// sends a few CR first (the board ignores CR) and waits ~50 ms: the bytes that wake it are lost.
void powerWake();             // input arrived: full clock now, no light sleep for a while
void powerTick(bool usbIdle); // end of each loop pass; usbIdle: no USB output or baud change pending
uint8_t powerMhz();
uint32_t powerSleeps();uint32_t powerSleptMs();uint32_t powerSlowMs(); // light sleeps, time asleep, time at 80 MHz
