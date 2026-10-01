# MeshMesh

**English** · [Русский](README.ru.md)

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
- **Offline maps** — OpenStreetMap tiles on the SD card, GPS position and nodes on the map.
- **Nearby nodes** — signal, hops, distance and bearing; contacts, repeaters and rooms.
- **Signal radar** — Wi-Fi, Bluetooth and LoRa around you, with “warmer / colder” homing.
- **Motion sensor** — two boards detect a person walking between them (Wi-Fi CSI).
- **Chess** with your contacts over the mesh, and Klondike solitaire on the M9.
- **Repeater and room server modes** — chosen at boot (M9 screen, Heltec button) or in Settings,
  on the web page, over USB: the device becomes a stock-compatible MeshCore repeater or room server
  with admin login and remote CLI from the MeshCore app; key, contacts and history stay.
  [docs/repeater.md](docs/repeater.md)
- **GPS and compass**, position sharing, lock screen, phonetic Cyrillic keyboard input.
- **Web interface** over the device's own Wi-Fi access point, and an **Android app**
  (Wi-Fi, Bluetooth LE or USB) with message notifications.
- Device UI in English or Russian.

## Boards

| Board | Status |
|---|---|
| Elecrow ThinkNode M9 (keyboard, 320×240 screen) | tested on hardware |
| Heltec WiFi LoRa 32 V4 (OLED, one button) | tested on hardware |
| GAT562 30S Mesh Kit (nRF52840, OLED, joystick; no Wi-Fi) | tested on hardware, GPS not working yet — [docs/gat562.md](docs/gat562.md) |
| Heltec V4-R8, Heltec V3, Heltec Wireless Tracker, LilyGO T-Deck, T-Beam, T-Beam Supreme, T3-S3, T-LoRa V2.1-1.6, Seeed XIAO ESP32S3 + Wio-SX1262, B&Q Station G2, Elecrow ThinkNode M2 | builds only, not yet run on hardware — reports welcome |

ESP32 boards have every feature. The GAT562 (nRF52840) has no Wi-Fi: no access point, Wi-Fi radar,
motion sensor or internet client; the phone connects through the Android app over Bluetooth or USB.
It adds an on-screen keyboard and chess on the OLED. Pins and details: [docs/boards.md](docs/boards.md).

## Install

**Browser:** open the [website](https://ikrasnodymov.github.io/meshmesh/#install) in Chrome or
Edge on a computer, pick your board, press **Install**. Updating MeshMesh keeps your key,
contacts, settings and history; tick “Erase device” only when coming from other firmware.

**esptool:** download the board's zip from the website and run

```sh
python -m esptool --chip esp32s3 --port PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

(ESP32 boards: `--chip esp32`, bootloader at `0x1000`, `--flash-freq 40m`; flash size per board.)

**GAT562 (nRF52840):** press RESET twice — a `GAT562-BOOT` drive appears — and copy `firmware.uf2`
from the website onto it. Later updates: `python tools/nrf52.py flash PACKAGE` ([docs/gat562.md](docs/gat562.md)).

Default radio: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6 — change it in Settings → Radio; all nodes
of a network must match. Follow your country's radio regulations.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9            # or heltec_v4, tdeck, tbeam, ...
.venv/bin/python tools/package.py m9
```

Android app: `cd android && ./gradlew testDebugUnitTest assembleRelease` ([docs/android.md](docs/android.md)).
The screen UI can be rendered on a computer without a board: `tools/ui_preview/build.sh m9 OUTDIR en`.

## Documentation

Detailed documentation is in Russian: [README.ru.md](README.ru.md) (controls and features),
[MeshCore compatibility](docs/meshcore-migration.md), [boards](docs/boards.md),
[GAT562](docs/gat562.md), [Android](docs/android.md), [repeater and room modes](docs/repeater.md), [chess protocol](docs/chess.md), [map format](docs/maps-format.md),
[comparison with MeshCore](docs/feature-parity.md), [hardware verification](docs/verification.md).

Not verified yet: LoRa range, GPS accuracy under open sky, compass accuracy, relaying through a
third repeater. Not implemented: voice, route planning, OTA updates.

## License

[MIT](LICENSE). Bundled MeshCore (`lib/MeshCore`) keeps its own MIT license. Map data ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) contributors. The radar idea follows
the RSSI tracker and radar HUDs by [Stevee87](https://github.com/Stevee87).
