#pragma once
// Icons drawn with lines and shapes (not font glyphs), so they scale and take state colours. Shared by the
// M9 interface (Ui.cpp) and the T114 colour screen (UiHires.inc). hole: the colour cut into an icon
// (its background); shade: the second colour of the compass needle. IcHelp draws only its circle.
#include <Adafruit_GFX.h>
enum Icon {IcChat,IcPin,IcMesh,IcCompass,IcWifi,IcBle,IcGear,IcTower,IcRoom,IcSensor,IcPerson,IcLock,IcMail,IcHash,IcPulse,IcHelp,IcRadio,IcScreen,IcKey,IcRadar,IcCards,IcChess,IcPaw,IcDice,IcPower};
void drawIcon(Adafruit_GFX& d,Icon id,int cx,int cy,int s,uint16_t c,uint16_t hole,uint16_t shade);
// A die of the dice page by its dice::Type: d4 a triangle, d6 a square (pips 1-6 draws its pips in face), d8 a
// diamond, d10 a kite, d12 a pentagon, d20 a hexagon with a facet in edge, d66, d% and other dice a tile for
// two digits. s: half the size. The value is written by the caller (each screen has its fonts).
void drawDieShape(Adafruit_GFX& d,uint8_t shape,int cx,int cy,int s,uint16_t fill,uint16_t edge,uint16_t face,unsigned pips);
