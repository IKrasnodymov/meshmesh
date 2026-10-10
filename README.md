# MeshMesh

**English** · [Русский](README.ru.md) · [Українська](README.uk.md) · [Español](README.es.md) · [Português](README.pt.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md) · [Polski](README.pl.md) · [Türkçe](README.tr.md) · [中文](README.zh.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md) · [Bahasa Indonesia](README.id.md)

**Website and one-click browser install: https://ikrasnodymov.github.io/meshmesh/**

Off-grid messaging firmware for ESP32 and nRF52 LoRa devices. Messages hop by radio from device to
device, with no internet, no cell network and no servers. The radio layer is
[MeshCore](https://github.com/meshcore-dev/MeshCore), so MeshMesh nodes talk to standard
MeshCore nodes.

<p>
<img src="site/img/m9-home-en.png" width="32%" alt="Home screen">
<img src="site/img/m9-chat-en.png" width="32%" alt="Direct chat">
<img src="site/img/m9-radar-en.png" width="32%" alt="Signal radar">
</p>

## Features

- **Chats** — direct and public messages, ✓✓ delivery receipts, retries, route shown per message.
- **Channels** — up to 8 MeshCore channels: join by #hashtag, `meshcore://` link or QR code, name and key;
  create a private one, invite contacts by direct message, find channels heard on air. [docs/en/channels.md](docs/en/channels.md)
- **Regions** — MeshCore flood scope: a default region and one per channel, a search of the regions nearby repeaters serve; repeaters set up with the `region` commands. [docs/en/regions.md](docs/en/regions.md)
- **Offline maps** — OpenStreetMap tiles on the SD card, GPS position and nodes on the map.
- **Nearby nodes** — signal, hops, distance and bearing; contacts, repeaters and rooms.
- **Signal radar** — Wi-Fi, Bluetooth and LoRa around you, with “warmer / colder” homing.
- **Motion sensor** — two boards detect a person walking between them (Wi-Fi CSI).
- **Chess** with your contacts over the mesh, and Klondike solitaire on the M9.
- **Mesh pet** — a Tamagotchi-like pixel creature on every board that feeds on radio traffic, grows up and can die of hunger or loneliness. [docs/en/pet.md](docs/en/pet.md)
- **Dice** — the DIC3R dice roller on every board: RPG pools and formulas (2d6+1, d20, d66, d%), a Warhammer grid with a success threshold, counters and characters, the same on the screen, the web page and the app. [docs/en/dice.md](docs/en/dice.md)
- **Chess without MeshMesh on your device**: [the chess page](https://ikrasnodymov.github.io/meshmesh/chess/) plays through a
  board running the official MeshCore Companion firmware, over USB (Web Serial) or Bluetooth (Web Bluetooth), in Chrome or Edge.
- **Repeater and room server modes** — chosen at boot (M9 screen, Heltec button) or in Settings,
  on the web page, over USB: the device becomes a stock-compatible MeshCore repeater or room server
  with admin login and remote CLI from the MeshCore app; key, contacts and history stay.
  [docs/en/repeater.md](docs/en/repeater.md)
- **MeshCore apps** — the stock MeshCore apps connect over Bluetooth or USB and work with the board's chats, contacts and channels (chess stays in the MeshMesh app). [docs/en/companion.md](docs/en/companion.md)
- **GPS and compass**, position sharing, lock screen, phonetic Cyrillic keyboard input.
- **Web interface** over the device's own Wi-Fi access point or the home Wi-Fi it joins (M9, T-Deck), and an **Android app**
  (Wi-Fi, Bluetooth LE or USB) with message notifications.
- **Device UI in 15 languages**: English, Russian, Ukrainian, Spanish, Portuguese, French, German, Italian,
  Polish, Turkish, Chinese, Japanese, Korean, Arabic and Indonesian. The website installs the firmware with
  the language you pick; Settings change it later.

## Boards

| Board | Status |
|---|---|
| Elecrow ThinkNode M9 (keyboard, 320×240 screen) | tested on hardware |
| Heltec WiFi LoRa 32 V4 (OLED, one button) | tested on hardware |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; no Wi-Fi) | tested on hardware (our unit has no GPS module fitted) — [docs/en/gat562.md](docs/en/gat562.md) |
| Heltec Mesh Node T114 (nRF52840, 240×135 colour TFT, one button; no Wi-Fi) | tested on hardware — [docs/en/t114.md](docs/en/t114.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | builds only, not yet run on hardware — reports welcome |

ESP32 boards have every feature. The GAT562 (nRF52840) has no Wi-Fi: no access point, Wi-Fi radar,
motion sensor or internet client; the phone connects through the Android app over Bluetooth or USB.
It adds an on-screen keyboard and chess on the OLED. Pins and details: [docs/en/boards.md](docs/en/boards.md).

## Install

**Browser:** open the [website](https://ikrasnodymov.github.io/meshmesh/#install) in Chrome or
Edge on a computer, pick your board and the device language, press **Install**. Updating MeshMesh
keeps your key, contacts, settings and history; tick “Erase device” only when coming from other firmware.

**esptool:** download the board's zip from the website and run

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(ESP32 boards: `--chip esp32`, bootloader at `0x1000`, `--flash-freq 40m`; flash size per board.)

**GAT562 (nRF52840):** the same **Install** button on the website (serial DFU over Web Serial); a board
with other firmware first needs RESET pressed twice. Or press RESET twice — a `GAT562-BOOT` drive
appears — and copy `firmware.uf2` from the website onto it. Later updates: `python tools/nrf52.py flash PACKAGE` ([docs/en/gat562.md](docs/en/gat562.md)).

Default radio: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — change it in Settings → Radio; all nodes
of a network must match. Follow your country's radio regulations.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android app: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/en/android.md](docs/en/android.md)).
The screen UI can be rendered on a computer without a board: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentation

Detailed documentation in English: [channels](docs/en/channels.md), [regions](docs/en/regions.md), [MeshCore compatibility](docs/en/meshcore-migration.md), [boards](docs/en/boards.md),
[GAT562](docs/en/gat562.md), [Android](docs/en/android.md), [repeater and room modes](docs/en/repeater.md),
[chess protocol](docs/en/chess.md), [mesh pet](docs/en/pet.md), [dice](docs/en/dice.md), [map format](docs/en/maps-format.md), [comparison with MeshCore](docs/en/feature-parity.md),
[hardware verification](docs/en/verification.md). The Russian originals are in [docs/](docs/), with the full
guide to controls and features in [README.ru.md](README.ru.md).

Not verified yet: LoRa range, GPS accuracy under open sky, compass accuracy, relaying through a
third repeater. Not implemented: voice, route planning, OTA updates.

## License

[MIT](LICENSE). Bundled MeshCore (`lib/MeshCore`) keeps its own MIT license. Map data ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) contributors. The radar idea follows
the RSSI tracker and radar HUDs by [Stevee87](https://github.com/Stevee87).

Release 0.4.0: [event waits on every board](docs/en/power.md) and [independent firmware/Android versions and publishing](docs/en/releases.md). Battery-life gains have not been measured.

Firmware 0.4.1 reduces history-export memory use and reports allocation failures explicitly; Android remains 0.4.0.

Firmware 0.8.1: switching off also writes the dice counters changed in the last two seconds.

Firmware and Android 0.8.0: switch the device off from the Settings menu on its screen, the web page, the app or the USB command `poweroff`; it saves its data, shows "Device is off" and how to turn it on, then enters deep sleep (ESP32), a soft off (nRF52: everything off, the CPU waits for the button) or is cut off by its PMU (T-Beam). Holding the button for about a second, RESET or the power switch turns it on, depending on the board ([docs/en/power.md](docs/en/power.md#power-off)).

Firmware and Android 0.7.0: [dice](docs/en/dice.md) — the DIC3R roller (RPG pools and formulas, a Warhammer grid with a success threshold, counters, characters) on every board, the web page and the app; on the T114 and GAT562 chess, the pet and the dice are [optional modules](docs/en/gat562.md#modules-chosen-before-installing) chosen when building (`tools/nrf52.py package ENV --without …`). Dice checked on the M9 by the owner; the module images not yet on a board.

Firmware and Android 0.6.0: reply to a chat message by swiping it from right to left, as in Telegram (double-click on a computer); a channel reply starts with the `@[Name]` mention, a direct one with a short `> …` quote. Checked in the Android emulator's Chrome with test data, not with a board.

Firmware 0.5.1: the Heltec V3 build uses the DIO flash driver, matching its image header (a user's V3 did not keep writes with the QIO driver libraries; not yet verified there); `flashstatus` names the driver mode and `flashprobe` also writes through the ROM functions, as esptool does.

Firmware 0.4.3: when the LittleFS partition does not keep writes (as on a user's Heltec V3), `fsformat` puts the storage into the free OTA slot; the USB commands `flashstatus` and `flashprobe` show the flash chip and where writes stop staying.

Firmware 0.4.2: messages recorded before the clock is set get their exact time once it is set; a spoofed GPS date earlier than the firmware build, or one contradicting the phone/NTP time, is rejected. Android 0.4.1 does not repeat message notifications after that.

[Notifications, quick send, channel retention, periodic NTP and people counter](docs/en/notifications.md).
