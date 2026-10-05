#pragma once
// Board identity and features. Each PlatformIO target (platformio.ini) sets one board flag;
// MM_COMPACT selects the 128x64 one-button interface (UiHeltec.cpp), otherwise the
// 320x240 keyboard interface (Ui.cpp) is used. Pins are in BoardPins.h.
//   MM_BOARD_ID    status "board" for the web page, the app and the checks
//   MM_BOARD_NAME  shown on the device page and in the USB boot line
//   MM_NODE_NAME   default node name
//   MM_MAX_POWER   highest LoRa power the board's front end takes, dBm
//   MM_ABSENT      diagnostic modules the board does not have, each followed by a comma
//                  (web page indices: 1 keyboard, 2 SD, 4 RTC, 5 GPS, 6 compass, 7 IMU)
//   MM_BUTTON      label of the single button on one-button boards
#if defined(MM_HELTEC_V4)
#define MM_BOARD_ID "heltec_v4"
#define MM_BOARD_NAME "Heltec V4"
#define MM_NODE_NAME "Heltec V4"
#define MM_ABSENT 1,2,4,6,7,
#elif defined(MM_BOARD_HELTEC_V3)
#define MM_BOARD_ID "heltec_v3"
#define MM_BOARD_NAME "Heltec V3"
#define MM_NODE_NAME "Heltec V3"
#define MM_ABSENT 1,2,4,5,6,7,
#elif defined(MM_BOARD_HELTEC_TRACKER)
#define MM_BOARD_ID "heltec_tracker"
#define MM_BOARD_NAME "Heltec Wireless Tracker"
#define MM_NODE_NAME "Tracker"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 1,2,4,6,7,
#elif defined(MM_BOARD_TDECK)
#define MM_BOARD_ID "tdeck"
#define MM_BOARD_NAME "LilyGO T-Deck"
#define MM_NODE_NAME "T-Deck"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 4,6,7,
#elif defined(MM_BOARD_TBEAM)
#define MM_BOARD_ID "tbeam"
#define MM_BOARD_NAME "LilyGO T-Beam"
#define MM_BUTTON "IO38"
#define MM_NODE_NAME "T-Beam"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 1,2,4,6,7,
#elif defined(MM_BOARD_TBEAM_SUPREME)
#define MM_BOARD_ID "tbeam_supreme"
#define MM_BOARD_NAME "LilyGO T-Beam Supreme"
#define MM_BUTTON "BOOT"
#define MM_NODE_NAME "T-Beam S3"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 1,2,4,6,7,
#elif defined(MM_BOARD_T3S3)
#define MM_BOARD_ID "t3s3"
#define MM_BOARD_NAME "LilyGO T3-S3"
#define MM_BUTTON "BOOT"
#define MM_NODE_NAME "T3-S3"
#define MM_ABSENT 1,2,4,5,6,7,
#elif defined(MM_BOARD_TLORA)
#define MM_BOARD_ID "tlora_v2_1_6"
#define MM_BOARD_NAME "LilyGO T-LoRa V2.1-1.6"
#define MM_BUTTON "BOOT"
#define MM_NODE_NAME "T-LoRa"
#define MM_MAX_POWER 20
#define MM_ABSENT 1,2,4,5,6,7,
#elif defined(MM_BOARD_XIAO_S3)
#define MM_BOARD_ID "xiao_s3_wio"
#define MM_BOARD_NAME "Seeed XIAO ESP32S3 + Wio-SX1262"
#define MM_BUTTON "BOOT"
#define MM_NODE_NAME "XIAO S3"
#define MM_ABSENT 1,2,4,5,6,7,
#elif defined(MM_BOARD_STATION_G2)
#define MM_BOARD_ID "station_g2"
#define MM_BOARD_NAME "Station G2"
#define MM_BUTTON "USER"
#define MM_NODE_NAME "Station G2"
// The power amplifier adds about 8 dB; more than 19 dBm into it damages it.
#define MM_MAX_POWER 19
#define MM_ABSENT 1,2,4,6,7, // GPS: optional module on the expansion header
#elif defined(MM_BOARD_THINKNODE_M2)
#define MM_BOARD_ID "thinknode_m2"
#define MM_BOARD_NAME "Elecrow ThinkNode M2"
#define MM_BUTTON "USER"
#define MM_NODE_NAME "M2"
#define MM_ABSENT 1,2,4,5,6,7,
#elif defined(MM_BOARD_GAT562)
// nRF52840: no Wi-Fi. The SX1262 drives a power amplifier (the seller states 30 dBm at the 22 dBm setting).
#define MM_BOARD_ID "gat562_30s"
#define MM_BOARD_NAME "GAT562 30S Mesh Kit"
#define MM_BUTTON "joystick"
#define MM_NODE_NAME "GAT562"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 1,2,4,6,7,
#elif defined(MM_BOARD_T114)
// nRF52840 + SX1262, 1.14" 240x135 colour TFT, one button; the L76K GPS is an option of the kit.
#define MM_BOARD_ID "heltec_t114"
#define MM_BOARD_NAME "Heltec Mesh Node T114"
#define MM_BUTTON "USER"
#define MM_NODE_NAME "T114"
#define MM_GPS_DEFAULT true
#define MM_ABSENT 1,2,4,6,7,
#else
#define MM_BOARD_ID "m9"
#define MM_BOARD_NAME "ThinkNode M9"
#define MM_NODE_NAME "M9"
#define MM_GPS_DEFAULT true
#define MM_ABSENT
#endif
#ifndef MM_BUTTON
#define MM_BUTTON "PRG"
#endif
#ifndef MM_GPS_DEFAULT
#define MM_GPS_DEFAULT false
#endif
#ifndef MM_MAX_POWER
#define MM_MAX_POWER 22
#endif
// LoRa transceiver.
#if defined(MM_BOARD_TLORA)
#define MM_RADIO_SX1276 1
#elif defined(MM_COMPACT) || defined(MM_BOARD_TDECK)
#define MM_RADIO_SX1262 1
#else
#define MM_RADIO_LR1110 1
#endif
// Five-way joystick and a back button instead of the single button of the compact boards.
#if defined(MM_BOARD_GAT562)
#define MM_JOYSTICK 1
#endif
// Colour TFT at its own resolution under the 128x64 compact layout (HiresCanvas.h, Palette.h).
#if defined(MM_BOARD_T114)
#define MM_HIRES 1
#endif
// No Wi-Fi radio (nRF52): no access point, Wi-Fi radar or CSI; the app reaches it over BLE or USB.
#if defined(MM_NRF52)
#define MM_NO_WIFI 1
#endif
// Native USB (ESP32-S3 USB Serial/JTAG) instead of a USB-UART bridge.
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#define MM_NATIVE_USB 1
#endif
