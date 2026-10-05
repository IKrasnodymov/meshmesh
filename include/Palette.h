#pragma once
#include <stdint.h>
// Colours of the compact interface as canvas values. Monochrome panels light any non-zero value,
// so UiHeltec.cpp uses the colour entries only on colour screens (MM_HIRES, the T114 TFT);
// colour panels look values up in palette565 (RGB565).
enum UiColor:uint8_t {ColBack,ColInk,ColAccent,ColDim,ColGood,ColWarn,ColBad,ColBar,
  ColBoardLight,ColBoardDark,ColTarget,ColCursor,ColPieceDark,ColLastMove,ColCount};
constexpr uint16_t palette565[ColCount]={
  0x0000, // background
  0xFFFF, // text and lines
  0x185C, // accent: menu focus, header rule (#1E88E5)
  0x8C51, // secondary text and dots (#8A8A8A)
  0x4508, // good: battery, signal, GPS fix (#43A047)
  0xFD80, // warning, unread (#FFB300)
  0xE1C6, // error, low battery (#E53935)
  0x1127, // header bar (#12263A)
  0xEED6, // light squares (#EED9B6)
  0xB44C, // dark squares (#B58863)
  0xF7AD, // reachable squares, picked piece (#F6F669)
  0x073F, // board cursor (#00E5FF)
  0x2104, // black pieces, white piece outline (#202020)
  0xCE8D, // last move (#CDD26A)
};
