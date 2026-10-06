#pragma once
#include <stdint.h>
// Colours of the compact interface as canvas values. Monochrome panels light any non-zero value,
// so the colour entries are used only on colour screens: the T114 TFT (MM_HIRES, UiHires.inc), where
// they are the M9 interface's colours (Ui.cpp), and colour 1 on the Wireless Tracker's TFT.
// Colour panels look values up in palette565 (RGB565).
enum UiColor:uint8_t {ColBack,ColInk,ColAccent,ColDim,ColGood,ColWarn,ColBad,ColBar,
  ColBoardLight,ColBoardDark,ColTarget,ColCursor,ColPieceDark,ColLastMove,
  ColCard,ColCardHi,ColLine,ColFaint,ColInfo,ColViolet,ColPink,ColOutBubble,ColInBubble,ColHashBack,ColLockBack,
  ColHue0,ColHue1,ColHue2,ColHue3,ColHue4,ColHue5,ColHue6,ColHue7,
  ColPet0,ColPet1,ColPet2,ColPet3,ColPet4,ColPet5,ColPet6,ColPet7,ColPetLight,ColPetEye,ColPetSick,ColPetStone,ColPetStoneLight,ColCount};
constexpr uint16_t rgb565(uint32_t v){return ((v>>8)&0xf800)|((v>>5)&0x07e0)|((v>>3)&0x1f);}
constexpr uint16_t palette565[ColCount]={
  rgb565(0x080c11), // background
  rgb565(0xe4e9ee), // text and lines
  rgb565(0x1fc2ae), // accent: the chosen row, home title
  rgb565(0x8d99a6), // secondary text
  rgb565(0x4cc26b), // good: battery, signal, GPS fix
  rgb565(0xe9b13b), // warning, unread
  rgb565(0xe5564d), // error, low battery
  rgb565(0x000000), // status bar and footer
  rgb565(0xeed9b6), // light squares
  rgb565(0xb58863), // dark squares
  rgb565(0xf6f669), // reachable squares, picked piece
  rgb565(0x00e5ff), // board cursor
  rgb565(0x202020), // black pieces, white piece outline
  rgb565(0xcdd26a), // last move
  rgb565(0x131a22), // card
  rgb565(0x1c2835), // chosen card
  rgb565(0x2a3542), // lines, key chips
  rgb565(0x56626e), // faint text, empty states
  rgb565(0x4f9df7), // info: Bluetooth, sensors
  rgb565(0xa78bfa), // repeaters, LoRa
  rgb565(0xf472b6), // phones and watches on the radar
  rgb565(0x103a35), // own message
  rgb565(0x1a232d), // received message
  rgb565(0x14524b), // channel avatar
  rgb565(0x2e2450), // private channel avatar
  rgb565(0x2f7d6f),rgb565(0x3f6fb5),rgb565(0x8a5cc2),rgb565(0xb5693f), // node avatars, by ID
  rgb565(0x4f8a3a),rgb565(0xa8466a),rgb565(0x3a8aa0),rgb565(0x8f7a2e),
  rgb565(0x5fd3b0),rgb565(0x6fb2ff),rgb565(0xb48cff),rgb565(0xffa36c), // the pet's body, by species (Pet.h hues)
  rgb565(0x9ad35a),rgb565(0xff7aa8),rgb565(0x52c8e0),rgb565(0xf2c94c),
  rgb565(0xfff4dc), // the pet's belly and spots
  rgb565(0x101418), // its eyes and mouth
  rgb565(0x9fb08a), // its body when ill
  rgb565(0x8d99a6),rgb565(0xc5ccd3), // the grave stone and its cross
};
