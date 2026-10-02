# MeshMesh for Android

> Translated from the Russian original [docs/android.md](../android.md); when they differ, the original is current.

The app shows the device's own web interface (`web/index.html` — the same file from
which `include/PortalPage.h` is built) and connects to the board in three ways:
Wi-Fi, Bluetooth LE and USB. All sections of the page — chats, nodes, map, navigation, connections,
radar and motion sensor, modules, settings, Klondike solitaire and chess — work over any of them.

Sources — `android/`, package `org.meshmesh.app`, Android 10 and newer (minSdk 29, targetSdk 35).

## Build and install

```sh
cd android
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
./gradlew testDebugUnitTest assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
```

The build copies `web/index.html` into the app's resources, so the app's interface always
matches the firmware web page of the same revision. The app version is the firmware version from
`include/Version.h` with a suffix (`0.3.2-app1`). The local `release` package is signed with the debug
key and installs directly. The APK on the website is built by GitHub Actions and signed with the
project key (`MM_KEYSTORE`, `MM_KEYSTORE_PASSWORD` from the secrets), so that new versions install over old ones. In the debug build
WebView debugging is enabled (`chrome://inspect`).

## How it works

- `MainActivity` — a WebView with the page and `assets/host.js`, `assets/host.css`: a connection
  screen instead of the password form, and interception of `fetch('/api/…')`. Other requests (OpenStreetMap
  when preparing a map) go to the phone's internet as usual. Uploading a map picks the file
  with the system dialog; “download” saves a file through the Android dialog.
- `MeshService` — a foreground service: keeps the connection in the background; with the app minimized
  it reads `status` every 8 s and, when RX grows, the history; new incoming messages become
  notifications (tapping opens the chat); every 30 s it checks chess (“your move”).
- `api/HttpApi` — Wi-Fi: the device's HTTP server (`src/Portal.cpp`) with Basic authentication.
- `api/CommandApi` — USB and BLE: every page request becomes the command that
  the device's HTTP handler executes (`executeCommand`), with the same response codes.
- `link/` — channels: `UsbLink` (usb-serial-for-android), `BleLink` (GATT), `TcpLink` (a bridge
  on a computer); `LineTransport` — one command at a time, one response line.

### Wi-Fi

The device's access point (`MM-XXXXXX`, WPA2, 192.168.4.1) is joined from the app through
`WifiNetworkSpecifier`: Android shows its own prompt, the network is used only by the app,
and mobile internet remains for everything else. The password is on the device screen; if you previously
connected over USB or BLE while the access point was on, the app already knows the password of this boot
(it changes at every device start). “Other address” — a device or a bridge on a network
the phone is already connected to.

### Bluetooth LE

Service `7a9e0001-…`: a command is written to RX with acknowledgement (up to 255 bytes), the response arrives
as TX notifications and ends with a newline. Both characteristics require
authenticated pairing: on the first connection Android asks for the PIN from the device
screen. If the phone remembers a pairing that the board no longer has (its flash was replaced),
the app removes the old bond and asks for the PIN again. BLE is slower than the other channels, so
lists (history, nodes, chess, settings) are re-read only when `status` changes or
every 20 s, and the page refreshes every 4 s.

### USB

M9 (CH340 bridge, 1A86:7522) and Heltec V4 (built-in ESP32-S3 USB, 303A:1001) over OTG.
Control lines as in `tools/device.py`: on the M9 DTR and RTS are off, on the Heltec DTR is on —
connecting does not restart the board. The M9 is switched to 921600 baud (like `tools/maps.py`); every
3 s of idle time the app sends `\r` so that the board does not fall back to 115200; on a baud-rate failure it falls back
to 115200 and repeats the command. When a board is plugged in with a cable, Android offers to open the app,
and it connects by itself.

## Firmware commands for the app

Added to `executeCommand`, so they are available over USB, BLE and `/api/command`:

| Command | Response | Purpose |
|---|---|---|
| `connections` | JSON: SSID, Wi-Fi password, BLE PIN | previously USB only; now also over paired BLE |
| `radar web` | JSON `/api/radar?open=1` | opens and holds the radar (10 s), with network names, like the web page |
| `radar do {JSON}` | `OK …` / `ERR …` | `/api/radar` actions: track, untrack, peak, csi, calibrate, close |
| `map tile Z X Y OFFSET [LEN]` | `OK tile SIZE OFFSET BASE64` | a tile file from the SD card in parts of up to 6144 bytes |
| `sendjson {"to":…,"text":…}` | same as `send` | text with a newline that does not fit on the command line |

The diagnostic command `radar` still does not print network and device names; `radar web`
prints them, like the page over Wi-Fi. The BLE response is now sent in notifications the size of
the negotiated MTU (up to 244 bytes instead of 20) and only when NimBLE buffers are free
(`os_msys_num_free`); otherwise the notification is deferred: a lost fragment would corrupt the response.

Map tiles that have been read are cached on the phone (up to 200 MB); the copy is used as long as
the tile header on the SD card (size and CRC) has not changed, so viewing again over BLE is fast.

## Testing on a computer

```sh
.venv/bin/python tools/usb_tcp_bridge.py --port /dev/cu.wchusbserial110   # USB protocol on :8771
.venv/bin/python tools/web_usb_bridge.py --port /dev/cu.usbmodem101 --http 8093  # HTTP API
```

In the emulator the computer is visible as 10.0.2.2: “USB → USB via computer” tests the command
layer (the same code as for USB and BLE on the phone) with a real board, “Wi-Fi → Other address”
tests the HTTP client. The USB bridge stays at 115200 baud and refuses to change the baud rate.

Unit tests (`./gradlew testDebugUnitTest`) cover what breaks silently:
UTF-8 split across notifications, log lines among responses, a JSON response broken by a
Wi-Fi driver log line, a late command response after a timeout, the 255-byte BLE limit when
uploading a map, a newline in a message, response codes, tile assembly and the BLE list cache.

## Limitations

- An nRF52 board (GAT562) that the phone was paired with under the previous firmware comes with
  a stale Android service cache; the app clears it and discovers the services again
  (`docs/gat562.md`).

- Packet detection, compatibility and radio parameters — as in the firmware (`docs/verification.md`).
- Uploading a map over BLE runs at about 2 KB/s (180 bytes per write); for a large package the app
  suggests uploading over USB or Wi-Fi.
- The ESP32 driver line `wifi:timeout when WiFi un-init` sometimes ends up inside a USB response
  (after closing the radar); the app detects the truncated JSON and repeats the command.
- One board at a time; to switch — “Connections → Disconnect”.
