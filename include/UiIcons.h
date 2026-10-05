#pragma once
// Icons drawn with lines and shapes (not font glyphs), so they scale and take state colours. Shared by the
// M9 interface (Ui.cpp) and the T114 colour screen (UiHires.inc). hole: the colour cut into an icon
// (its background); shade: the second colour of the compass needle. IcHelp draws only its circle.
#include <Adafruit_GFX.h>
enum Icon {IcChat,IcPin,IcMesh,IcCompass,IcWifi,IcBle,IcGear,IcTower,IcRoom,IcSensor,IcPerson,IcLock,IcMail,IcHash,IcPulse,IcHelp,IcRadio,IcScreen,IcKey,IcRadar,IcCards,IcChess};
void drawIcon(Adafruit_GFX& d,Icon id,int cx,int cy,int s,uint16_t c,uint16_t hole,uint16_t shade);
