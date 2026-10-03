#pragma once
#include <stdint.h>
// Brightness of an SSD1306/SH1106 OLED, level 1..255. Contrast (0x81) only sets the segment
// current: from 255 down to 10 the panel lost about half of its light. A lower VCOMH (0xDB)
// dims the low end; level 255 gives the 0x40 that Adafruit's begin() sets. The bottom is
// 0.65 Vcc on SSD1306 (0x00) and the same level on SH1106 (0x22: 0.430 + 0.006415 x code).
// The precharge stays as begin() sets it: a 1-clock precharge made a Heltec V4 flicker.
template<class Send> void oledLevel(uint8_t level,bool sh1106,Send send) {
  uint8_t bottom=sh1106?0x22:0x00;
  send(0x81);send(level);
  send(0xDB);send(uint8_t(bottom+level*(0x40-bottom)/255));
}
