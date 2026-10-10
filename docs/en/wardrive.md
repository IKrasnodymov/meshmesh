# Wardriving: a coverage map of the mesh

Added in firmware 0.20.0 and Android 0.13.0. Build and hardware results are in [verification.md](verification.md).

Wardriving is a drive or walk with a node that logs what it hears and where. In MeshMesh it is first of all a
MeshCore coverage map: where the mesh can be heard, through which repeater, and where the dead spots are. Logging
Wi-Fi access points and Bluetooth devices with positions, the classic kind, is an extra part on ESP32 boards.

## What is logged

Logging needs a trusted position:

- a GPS fix of the board accepted by `gpsFix` (a date not before the build, no conflict with the phone/NTP clock,
  HDOP 5 or less);
- or the phone's position from the app, at most 30 s old (accuracy 50 m or better).

A spoofed GPS (a fix dated 8 July 2026, see `AGENTS.md`) is rejected, so such points never reach the log. Without a
position, received packets are only counted (`no_fix`).

Three parts are switched on independently:

| Part | What it does | On air |
|---|---|---|
| Listening (`passive`) | Every packet received: type, hops, SNR/RSSI and the last repeater of its path (for an advert heard directly, the first bytes of the node's key). The same repeater is logged again after moving 1/4 of the ping distance (at least 25 m) or after 2 min. | Sends nothing |
| Pings (`ping`) | A short `wardrive` message to a channel every `distance` metres (200 by default), at most once per `interval` seconds (60). The copies repeaters pass on within 30 s are its echoes: who heard us and at what SNR. No echo means no repeater heard the node at that spot. | One message per ping |
| Wi-Fi and BLE (`nets`, ESP32 only) | Access points (BSSID, SSID, channel, security, RSSI) and Bluetooth devices from the radar's sweep. The same address is logged again after 100 m or 5 min. | Sends nothing; the radar takes Wi-Fi |

Pings go to `#wardrive` by default; the board joins it on the first ping. Any joined channel other than Public can
be chosen. A ping does not enter the chat history. With "Coordinates in the ping text" (`coords`) the message carries
latitude and longitude with five decimals: the channel's members learn where the node is. Coordinates are not sent
by default.

Echoes are the copies of our packet the board heard itself (the same MeshCore tables as the packet path of messages).
A repeater that received the ping but cannot be heard by the board does not appear. No echo does not prove nobody
received the ping.

While Wi-Fi and BLE are logged the radar stays open: the access point, the Wi-Fi probe and the internet client come
first and pause the sweep. The power saving that waits for the LoRa IRQ does not apply while the radar is open.

## Storage

The log lives in MeshMesh storage (LittleFS): `wd-mesh.bin` and `wd-mesh.old` hold the mesh points, `wd-nets.bin`
and `wd-nets.old` the Wi-Fi and BLE records. A full file becomes the previous one and the old previous one is
deleted, so the last 4096–8192 mesh points and 2048–4096 Wi-Fi/BLE records are kept on ESP32, 256–512 points on
nRF52. Records wait in RAM for up to 16 or 30 s; power off (`poweroff`) and stopping wardriving write them at once.
A file cut in the middle of a write becomes the previous one: its whole records stay readable.

The log never goes on air and is not uploaded anywhere. Export it from the web page or the app. Files with Wi-Fi
addresses, network names and your track are personal data: do not publish them needlessly.

## Screens

- **M9 and T-Deck.** The "Wardrive" tile: mode, position, point count, last ping, repeaters (echoes and packets
  received, best SNR). OK starts (listening + pings) or stops, P pings now, N switches Wi-Fi/BLE. On the map points
  are coloured by SNR: green 5 dB and over, yellow from −5 dB, orange weaker, red a ping without echoes; pings are
  larger.
- **One-button boards and GAT562.** The "Wardrive" screen in the app list; a hold opens the menu: start/stop, ping
  now, repeaters heard, Wi-Fi/BLE (ESP32). The T114 draws the same screen in colour.

## Web page and app

The "Wardrive" section: state, position, counters, repeaters, settings, export to CSV, GeoJSON and KML (mesh points)
and a CSV for WiGLE (Wi-Fi and BLE, WigleWifi-1.4 format). "On the map" opens the map with the points; without an SD
card they are drawn on a blank background.

The Android app can send the phone's position: "Send the phone's position" asks for precise location and sends a
fresh fix to the board every 5 s (`fix`). While it is on, the app's service also runs as a location service, so the
position keeps coming with the screen off.

## Commands

USB, BLE and HTTP (`/api/wardrive`, `/api/wardrive/log`; the app turns them into the same commands):

```text
wardrive                                   state and settings (JSON)
wardrive do {"action":"settings","passive":true,"ping":true,"coords":false,"nets":false,"distance":200,"interval":60,"channel":"FF…"}
wardrive do {"action":"ping"}              ping now (at most once per 10 s)
wardrive do {"action":"fix","lat":55.79,"lon":49.10,"accuracy":8}   the phone's position
wardrive do {"action":"stop"}              switch everything off, the log stays
wardrive do {"action":"clear"}             delete the log
wardrive log mesh FROM                     mesh points from number FROM: [time,lat,lon,kind,type,hops,snr,rssi,hash,count,flags]
wardrive log nets FROM                     Wi-Fi/BLE: [time,lat,lon,kind,mac,channel,rssi,auth,name,flags]
```

Point `kind`: 0 a packet received, 1 a ping (`count` repeaters with echoes, `hash` and `snr` the best one), 2 an
echo (one repeater, at the ping's position). `lat`/`lon` are in millionths of a degree, `snr` in dB, `time` UTC (0
before the clock is set). `flags`: 1 the phone's position, 2 `hash` from the sender's key, 4 direct route. A part
holds 32 points or 16 Wi-Fi/BLE records; `next` is the number of the next one, `total` the count. If `total` went
down, the log was cleared or shifted: read again from zero.

On nRF52 wardriving is an optional module (`tools/nrf52.py package ENV --without wardrive`, `MM_NO_WARDRIVE`); the
site and the app install the full set.

## Limits

- One LoRa transceiver: wardriving listens and pings on the current radio profile.
- Pings use airtime and are seen by the channel's members. The distance and interval limits keep them rare; your
  country's airtime rules come first.
- RSSI and SNR give no distance to a repeater; the map shows where the mesh can be heard, not where repeaters stand.
- A repeater hash is the first 1–3 bytes of its key, as in the packet path: different repeaters may share one byte.
