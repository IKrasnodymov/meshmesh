#pragma once
// ThinkNode M9 V1.0: Elecrow schematic GPIO names (not package pin numbers).
namespace pins {
#if defined(MM_HELTEC_V4)
constexpr int spiClock=9, spiMiso=11, spiMosi=10;
constexpr int radioCs=8, radioIrq=14, radioReset=12, radioBusy=13;
constexpr float radioTcxo=1.8f;
constexpr int sda=17, scl=18, oledReset=21, battery=1, button=0;
constexpr int femPower=7,femEnable=2;
#if defined(MM_HELTEC_R8)
constexpr int peripheralPower=40,gpsEnable=42,adcEnable=-1,led=46;
#else
constexpr int peripheralPower=36,gpsEnable=34,adcEnable=37,led=35;
#endif
// ESP-side names: the GNSS connector module transmits on GPIO39 and listens on GPIO38.
constexpr int gpsRx=39,gpsTx=38;
#else
constexpr float radioTcxo=3.3f;
constexpr int spiClock=40, spiMiso=38, spiMosi=47;
constexpr int radioCs=39, radioIrq=42, radioReset=45, radioBusy=41;
constexpr int lcdCs=16, lcdDc=15, lcdReset=14, backlight=17, peripheralPower=18;
constexpr int sdCs=48, sda=7, scl=6, keyboardSda=20, keyboardScl=21;
constexpr int gpsRx=2, gpsTx=3, gpsReset=5, gpsEnable=11;
constexpr int battery=13, buzzer=9;
constexpr unsigned keyboardAddress=0x6c;
#endif
}
