#pragma once
// GPIO numbers of each board. ThinkNode M9 V1.0: Elecrow schematic GPIO names (not package pin numbers).
#include <Arduino.h>
namespace pins {
#if defined(MM_HELTEC_V4)
constexpr int spiClock=9, spiMiso=11, spiMosi=10;
constexpr int radioCs=8, radioIrq=14, radioReset=12, radioBusy=13;
constexpr float radioTcxo=1.8f;
constexpr int sda=17, scl=18, oledReset=21, battery=1, button=0;
constexpr float batteryScale=4.9f*1.045f; // 390k/100k divider; Meshtastic's ADC_MULTIPLIER (4.9 read low on a V3)
constexpr int femPower=7,femEnable=2;
#if defined(MM_HELTEC_R8)
constexpr int peripheralPower=40,gpsEnable=42,adcEnable=-1,led=46;
#else
constexpr int peripheralPower=36,gpsEnable=34,adcEnable=37,led=35;
#endif
constexpr int ledOn=HIGH;
// ESP-side names: the GNSS connector module transmits on GPIO39 and listens on GPIO38.
constexpr int gpsRx=39,gpsTx=38;
#elif defined(MM_COMPACT)
// Other one-button boards (HardwareCompact.cpp), from the boards' MeshCore and Meshtastic
// variants. -1: not on this board. GPS pins are ESP-side; HardwareCompact also tries them
// swapped, as the sources disagree for some boards. vextOn/gpsOn: level that powers it.
// SX1276: radioIrq is DIO0, radioBusy is DIO1.
#if defined(MM_BOARD_HELTEC_V3)
constexpr int spiClock=9, spiMiso=11, spiMosi=10, radioCs=8, radioIrq=14, radioReset=12, radioBusy=13, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=17, scl=18, oledReset=21, button=0, vext=36, vextOn=LOW;
constexpr int battery=1, adcEnable=37, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=4.9f*1.045f; // Meshtastic's ADC_MULTIPLIER; 4.9 read 3-6 % low against a voltmeter
constexpr int led=35, ledOn=HIGH;
#elif defined(MM_BOARD_HELTEC_TRACKER)
constexpr int spiClock=9, spiMiso=11, spiMosi=10, radioCs=8, radioIrq=14, radioReset=12, radioBusy=13, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=-1, scl=-1, oledReset=-1, button=0, vext=3, vextOn=HIGH;
constexpr int tftClock=41, tftData=42, tftDc=40, tftReset=39, tftCs=38, tftLight=21;
constexpr int battery=1, adcEnable=2, gpsRx=33, gpsTx=34, gpsEnable=35, gpsOn=HIGH, gpsReset=36, buzzer=-1;
constexpr float batteryScale=4.9f*1.045f; // Meshtastic's ADC_MULTIPLIER
constexpr int led=18, ledOn=HIGH;
#elif defined(MM_BOARD_TBEAM)
constexpr int spiClock=5, spiMiso=19, spiMosi=27, radioCs=18, radioIrq=33, radioReset=23, radioBusy=32, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=21, scl=22, oledReset=-1, button=38, vext=-1, vextOn=HIGH;
constexpr int pmuSda=21, pmuScl=22;
constexpr int battery=-1, adcEnable=-1, gpsRx=34, gpsTx=12, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=1.f;
constexpr int led=4, ledOn=LOW;
#elif defined(MM_BOARD_TBEAM_SUPREME)
constexpr int spiClock=12, spiMiso=13, spiMosi=11, radioCs=10, radioIrq=1, radioReset=5, radioBusy=4, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=17, scl=18, oledReset=-1, button=0, vext=-1, vextOn=HIGH;
constexpr int pmuSda=42, pmuScl=41;
constexpr int battery=-1, adcEnable=-1, gpsRx=9, gpsTx=8, gpsEnable=7, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=1.f;
constexpr int led=6, ledOn=HIGH;
#elif defined(MM_BOARD_T3S3)
constexpr int spiClock=5, spiMiso=3, spiMosi=6, radioCs=7, radioIrq=33, radioReset=8, radioBusy=34, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=18, scl=17, oledReset=21, button=0, vext=-1, vextOn=HIGH;
constexpr int battery=1, adcEnable=-1, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=2.f;
constexpr int led=37, ledOn=HIGH;
#elif defined(MM_BOARD_TLORA)
constexpr int spiClock=5, spiMiso=19, spiMosi=27, radioCs=18, radioIrq=26, radioReset=23, radioBusy=33, radioRxEn=-1;
constexpr float radioTcxo=0.f;
constexpr int sda=21, scl=22, oledReset=-1, button=0, vext=-1, vextOn=HIGH;
constexpr int battery=35, adcEnable=-1, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=2.f;
constexpr int led=25, ledOn=HIGH;
#elif defined(MM_BOARD_XIAO_S3)
// Wio-SX1262 for XIAO: DIO2 switches the antenna for TX, GPIO38 enables the receive path.
constexpr int spiClock=7, spiMiso=8, spiMosi=9, radioCs=41, radioIrq=39, radioReset=42, radioBusy=40, radioRxEn=38;
constexpr float radioTcxo=1.8f;
constexpr int sda=-1, scl=-1, oledReset=-1, button=0, vext=-1, vextOn=HIGH;
constexpr int battery=-1, adcEnable=-1, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=1.f;
constexpr int led=21, ledOn=LOW;
#elif defined(MM_BOARD_STATION_G2)
constexpr int spiClock=12, spiMiso=14, spiMosi=13, radioCs=11, radioIrq=48, radioReset=21, radioBusy=47, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=5, scl=6, oledReset=-1, button=38, vext=-1, vextOn=HIGH;
constexpr int battery=-1, adcEnable=-1, gpsRx=7, gpsTx=15, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=1.f;
constexpr int led=-1, ledOn=HIGH;
#elif defined(MM_BOARD_THINKNODE_M2)
constexpr int spiClock=12, spiMiso=13, spiMosi=11, radioCs=10, radioIrq=3, radioReset=21, radioBusy=14, radioRxEn=-1;
constexpr float radioTcxo=3.3f;
constexpr int sda=16, scl=15, oledReset=-1, button=47, vext=46, vextOn=HIGH;
constexpr int battery=17, adcEnable=-1, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=5;
constexpr float batteryScale=1.509f;
constexpr int led=6, ledOn=HIGH;
#elif defined(MM_BOARD_HELTEC_E213)
// Vision Master E213, from MeshCore's heltec_e213 and Meshtastic's heltec_vision_master_e213 variants. Vext
// powers the e-ink panel and the I2C connector; the panel has its own SPI bus (EinkE213.cpp).
constexpr int spiClock=9, spiMiso=11, spiMosi=10, radioCs=8, radioIrq=14, radioReset=12, radioBusy=13, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=39, scl=38, oledReset=-1, button=0, vext=18, vextOn=HIGH;
constexpr int einkClock=4, einkData=6, einkCs=5, einkDc=2, einkReset=3, einkBusy=1;
constexpr int battery=7, adcEnable=46, gpsRx=-1, gpsTx=-1, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=4.9f*1.03f; // Meshtastic's ADC_MULTIPLIER
constexpr int led=45, ledOn=HIGH;
#elif defined(MM_BOARD_GAT562)
// nRF52840 GPIO numbers (P1.xx = 32+xx), from MeshCore's gat562_30s_mesh_kit variant. LoRa has
// its own SPI pins; radioPower switches the SX1262 supply. Joystick and back button pull to GND.
constexpr int spiClock=43, spiMiso=45, spiMosi=44, radioCs=42, radioIrq=47, radioReset=38, radioBusy=46, radioRxEn=-1;
constexpr int radioPower=37;
constexpr float radioTcxo=1.8f;
constexpr int sda=13, scl=14, oledReset=-1, button=26, vext=-1, vextOn=HIGH;
constexpr int keyUp=28, keyDown=4, keyLeft=30, keyRight=31, keyPress=26, keyBack=9;
// GAT562 30S Mesh Kit V1.1 schematic (gat-iot/GAT562-family): L76K TXD -> P0.15, RXD <- P0.16 (diode),
// TIMEPULSE -> P0.17 (not fitted: R23 NC); IO2 = P1.02 (34) switches 3V3_GPS through a MOSFET, HIGH = on.
// BEE_EN = P1.01 (33) is the buzzer, although MeshCore's variant also names 33 PIN_GPS_EN.
constexpr int battery=5, adcEnable=-1, gpsRx=15, gpsTx=16, gpsEnable=-1, gpsOn=HIGH, gpsReset=-1, gpsPower=34, buzzer=33;
constexpr float batteryScale=1.f;
constexpr int led=36, ledOn=HIGH, txLed=35;
#elif defined(MM_BOARD_T114)
// Heltec Mesh Node T114 V2: nRF52840 GPIO numbers (P1.xx = 32+xx), from MeshCore's heltec_t114 and
// Meshtastic's heltec_mesh_node_t114 variants. LoRa on SPI, the ST7789 TFT on SPI1 (32 MHz SPIM3).
// tftPower (VTFT_CTRL) and tftLight (LEDA) are active LOW. vext (P0.21) powers the GPS and the
// I2C connector; L76K: TX -> P1.05, RX <- P1.07, RESET P1.06 active LOW, STANDBY P1.02 HIGH = awake.
constexpr int spiClock=19, spiMiso=23, spiMosi=22, radioCs=24, radioIrq=20, radioReset=25, radioBusy=17, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int sda=26, scl=27, oledReset=-1, button=42, vext=21, vextOn=HIGH;
constexpr int tftClock=40, tftData=41, tftMiso=43, tftDc=12, tftReset=2, tftCs=11, tftLight=15, tftPower=3;
constexpr int battery=4, adcEnable=6, gpsRx=37, gpsTx=39, gpsEnable=21, gpsOn=HIGH, gpsReset=38, gpsStandby=34, buzzer=-1;
constexpr float batteryScale=4.9f;
constexpr int led=35, ledOn=LOW;
#elif defined(MM_BOARD_T096)
// Heltec Mesh Node T096: nRF52840 GPIO numbers (P1.xx = 32+xx), from MeshCore's heltec_t096 and Meshtastic's
// heltec_mesh_node_t096 variants. LoRa on SPI, the ST7735 TFT on SPI1. The KCT8103L front end: femPower
// (its LDO) and femEnable (CSD) HIGH while running, femTx (CTX) HIGH to transmit, LOW = receive with the LNA.
// vext (P0.26) powers the TFT and the GPS; tftLight (LEDA) is active LOW. UC6580 GPS at 115200: TX -> P0.25,
// RX <- P0.23, EN P0.06 active LOW, RESET P1.14 active LOW. adcEnable (P1.15) switches the battery divider.
constexpr int spiClock=40, spiMiso=14, spiMosi=11, radioCs=5, radioIrq=21, radioReset=16, radioBusy=19, radioRxEn=-1;
constexpr float radioTcxo=1.8f;
constexpr int femPower=30, femEnable=12, femTx=41;
constexpr int sda=7, scl=8, oledReset=-1, button=42, vext=26, vextOn=HIGH;
constexpr int tftClock=20, tftData=17, tftDc=15, tftReset=13, tftCs=22, tftLight=44;
constexpr int battery=3, adcEnable=47, gpsRx=25, gpsTx=23, gpsEnable=6, gpsOn=LOW, gpsReset=46, buzzer=-1;
constexpr float batteryScale=4.9f;
constexpr int led=28, ledOn=HIGH;
#elif defined(MM_BOARD_FAKETEC)
// fakeTec / ProMicro nRF52840 DIY: nRF52840 GPIO numbers (P1.xx = 32+xx). MeshCore's promicro variant gives
// the same pins as Arduino numbers of its own pin map; here they are translated (variants/faketec). The
// SX1262 module (HT-RA62, E22) is powered through radioPower; radioRxEn opens its receive path (E22).
// OLED SSD1306 on I2C, button to GND, GPS on Serial1 with an enable pin: all optional.
constexpr int spiClock=43, spiMiso=2, spiMosi=47, radioCs=45, radioIrq=10, radioReset=9, radioBusy=29, radioRxEn=17;
constexpr int radioPower=13;
constexpr float radioTcxo=1.8f;
constexpr int sda=36, scl=11, oledReset=-1, button=32, vext=-1, vextOn=HIGH;
constexpr int battery=31, adcEnable=-1, gpsRx=20, gpsTx=22, gpsEnable=24, gpsOn=HIGH, gpsReset=-1, buzzer=-1;
constexpr float batteryScale=1.815f; // mV per count of the 12-bit reading (MeshCore's ADC_MULTIPLIER)
constexpr int led=15, ledOn=HIGH;
#endif
#elif defined(MM_BOARD_TDECK)
// LilyGO T-Deck / T-Deck Plus: display, LoRa and SD share one SPI bus, as on the M9.
constexpr float radioTcxo=1.8f;
constexpr int spiClock=40, spiMiso=38, spiMosi=41;
constexpr int radioCs=9, radioIrq=45, radioReset=17, radioBusy=13, radioRxEn=-1;
constexpr int lcdCs=12, lcdDc=11, lcdReset=-1, backlight=42, peripheralPower=10;
constexpr int sdCs=39, keyboardSda=18, keyboardScl=8;
constexpr int ballUp=3, ballDown=15, ballLeft=1, ballRight=2, ballClick=0, touchInt=16;
constexpr int gpsRx=44, gpsTx=43, battery=4;
constexpr unsigned keyboardAddress=0x55;
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
