# Device mode: normal, MeshCore repeater and room server

> Translated from the Russian original [docs/repeater.md](../repeater.md); when they differ, the original is current.

Since version 0.3.3.

There is one firmware, and the mode is stored in NVS (`meshmesh/role`). The mode is chosen at boot and
changed by a restart, without reflashing and without erasing data.

- **Normal mode.** Screen, chats, maps, radar, games. A MeshCore chat node (`ADV_TYPE_CHAT`);
  relaying is set in “Radio”.
- **MeshCore repeater** (`ADV_TYPE_REPEATER`). Behaves like the stock `simple_repeater`:
  - relays packets by its rules (flood.max, regions, loop protection);
  - accepts admin and guest login;
  - answers status, telemetry, neighbour and owner requests;
  - runs admin CLI commands over LoRa.
- **MeshCore room server** (room server, `ADV_TYPE_ROOM`). Behaves like the stock `simple_room_server`:
  - members log in with the room password and write posts (each post is confirmed with an ACK);
  - the room delivers new posts in turn to the other logged-in members and waits for their ACKs;
  - read-only login is allowed by a setting;
  - the admin gets status, telemetry, the access list and the CLI.

  Difference from the stock room server: the last 32 posts are also stored on LittleFS (`/room_posts`), so
  they survive a restart.

There are no chats, chess, internet over Wi-Fi or radar in the server modes.

The node key is shared by all modes (`meshmesh-mc/identity`). Contacts, history, games, maps and
calibration are not touched in the server modes, but they are not updated either. For example, if another node changed
its type while the device was a room server, this becomes visible after that node's next advert. After returning
to normal mode the node advertises itself as a chat node on its own: the last advert was of a different type
(`meshmesh-mc/adv_type`).

## Choosing the mode

- **M9 and T-Deck (screen).**
  - For the first 5 seconds after boot, “Device mode” is shown with three cards and a countdown.
    If nothing is pressed, the previous mode is loaded.
  - Choosing a different mode is saved, and the device restarts.
  - The same screen opens from “Settings → Device mode” and from a row on the server page.
- **Heltec and single-button boards.**
  - For the first 5 seconds after boot — a list of modes: a click scrolls, a hold selects.
  - The same list opens from the “Settings” menu and from the menu of the server's main page.
- **Web page and app.** “Settings → Device mode”. After the restart the access point has to be
  turned on again: its password changes on every boot.
- **USB or BLE.**
  - `role` prints the current mode.
  - `role normal`, `role repeater`, `role room` switch it. The restart happens 1.5 s
    after the reply.

## Settings

Name, frequency, bandwidth, SF, CR and power are shared with MeshMesh (`Config`). Everything else is MeshCore settings
in `/prefs.json` on LittleFS. File formats and names are stock: `/prefs.json`, `/s_contacts`,
`/regions2`, `/packet_log`.

The settings file is shared by the repeater and the room server: passwords and admins are kept when the mode changes.
On switching to another server mode, relaying gets that mode's default value (`srv_role` in NVS):
on for the repeater, off for the room server.

| Where | What |
|---|---|
| M9 screen: “Repeater” / “Room” page | relaying, admin password, guest password (for the room server — the room password), read without password (room server), “Write to room”, local advert (60–240 min), flood advert (3–168 h), “Advertise now”, device mode; radio and name — “Settings” |
| Heltec screen | role, frequency, counters, passwords; menu: advertise, relaying, device mode |
| Web page: “Repeater” / “Room” | statistics, the same settings, neighbouring repeaters or room posts with a post form, CLI console |
| MeshCore app (over LoRa) | password login, status, telemetry, neighbours or posts, admin CLI commands |
| USB / BLE | `server` (status without passwords), `server secrets` (passwords), `server cli <command>`, `server post <text>` (room server) |

On the first start of a server mode, a random 8-digit admin password is created instead of the
stock `password`. It is shown on the screen and on the web page. The guest password (room password) is
empty by default, as in the stock firmware. On the repeater, login with an empty password gives read-only guest
access. On the room server, an empty password lets in members who can write.

Changes through the CLI are checked against the board's limits. This applies to `set radio`, `set freq`, `set tx` and
`set name`. Limits: 863–870 MHz, BW 62.5/125/250/500, SF7–12, CR5–8, 0–MAX dBm, name 1–24 bytes.
Out-of-range values are rejected with an `Error: …` message. Radio settings, as in the stock firmware, take effect
after a restart; power — immediately.

Differences from the stock `simple_repeater` and `simple_room_server`:

- The advert is sent without coordinates until they are set: `0,0` is not transmitted.
- A local advert every 2 hours instead of every 2 minutes before the first configuration.
- `erase` deletes only the server files, not the whole file system: it holds MeshMesh data.
- `set prv.key` changes the shared node key, i.e. the normal-mode key too.
- Room posts are saved on LittleFS.
- No bridges (RS232/ESP-NOW), OTA (`start ota`), power saving or manual receive gain:
  `set radio.rxgain` replies “unsupported”; gain is controlled by MeshMesh.

Code:
- `src/MeshServer.cpp`: shared `ServerMesh`, `RepeaterMesh`, `RoomMesh` — a port of the MeshCore
  1.17.1 examples (MIT);
- `include/MeshServer.h`;
- `src/UiServer.inc` (M9 screen), `src/UiHeltec.cpp` (Heltec);
- the server section in `web/index.html`;
- MeshCore helper modules in `lib/MeshCore/src/helpers/` (see `lib/MeshCore/UPSTREAM.md`,
  `PATCHES.md`);
- `lib/CayenneLPP` (telemetry encoder).

## Power saving

On ESP32 boards (`src/Power.cpp`, in every mode) the CPU runs at 80 MHz while the screen is dark and
Wi-Fi, the radar and the CSI sensor are off; a key, a USB or a BLE command brings 240 MHz back at once.
The clock does not change while the Wi-Fi driver or the Bluetooth controller runs (once BLE has been on,
the controller runs until reboot): a change after a CSI check with the Wi-Fi driver still up ended in an
M9 panic. So with BLE on there is no clock saving and no sleep. BLE is on by default in the normal mode
and off in repeater and room modes until the user turns it on.
In repeater and room modes the board also enters light sleep between packets (as stock MeshCore does):
60 s after boot, with the screen dark, BLE, Wi-Fi and the radar off, an empty transmit queue and 30 s
after the last command or key. The transceiver IRQ line (a received packet), the button, bytes over
the USB-UART and a 0.5 s timer wake it (the M9 keyboard is polled after each wake). The bytes that
wake a board over a USB-UART bridge (M9, Heltec V3 and others) are lost, so `tools/device.py` and the
app first send a few CR (the board skips them) and wait 50 ms. With native USB (Heltec V4 and others)
the board does not sleep while USB is connected to a computer. Bluetooth turned on prevents sleep.
`status` shows `cpu_mhz`, `slow_ms` (time at 80 MHz), `sleeps` and `sleep_ms`.

Deaf receiver guard (in every mode): after 10 minutes without a received packet the transceiver is set
up again as at boot (reset, calibration, settings) and the message queue is kept; at most once in
10 minutes and never in the middle of a packet. In server modes the stock `agc.reset.interval`
(seconds, a multiple of 4; 0 is off) does the same. The USB command `recalibrate` does it at once,
and `status` counts it in `radio_recal`.

## Testing against stock MeshCore

`tools/repeater_check.py` and `tools/room_check.py` test the M9 in the corresponding mode from the side of
the official MeshCore companion_radio_usb v1.17.1 firmware, temporarily installed on the Heltec. This is
the `upstream/meshcore-stock` build for the MeshMesh flash layout.

Procedure:
1. Full copy of the Heltec flash and `verify-flash`.
2. Writing the stock `firmware.bin` at 0x10000 and `boot_app0.bin` at 0xe000. After writing, the Heltec
   may stay in the bootloader: exit with `esptool --after watchdog-reset`. The first start
   formats the data partition in about 40 s.
3. Checks.
4. Writing the copy back and `verify-flash`.

The scripts set the companion's clock, as the app does: the login is signed with the companion's clock, and
messages with the computer's time.

## Verified (1 October 2026, M9 + Heltec V4)

- **Mode selection.**
  - M9: the boot screen (countdown, fallback to the previous mode), “Settings → Device mode” with keys
    (USB UI events), `role …` over USB.
  - Heltec: the boot list, the “Settings → Device mode” menu (click/hold via USB events).
    Software restart (`reset_reason=3`).
- **M9 repeater and the official companion** (`artifacts/repeater-check.json`, M9 ELF `73F44F7F…`,
  13 of 13):
  - `type 2` advert with name and key;
  - rejection of a wrong password, guest login with an empty password, admin login (in one run
    on the second attempt: the reply was lost over the air);
  - status, telemetry (voltage, temperature), neighbours, access list;
  - CLI `get name`; `set advert.interval` visible over USB and reverted; `set tx 30` rejected;
  - the M9 did not restart.
- **M9 room server and the official companion** (`artifacts/room-check.json`, the same ELF, 9 of 9):
  - `type 3` advert, rejection of a wrong password;
  - login with the room password, a member's post with ACK — one copy with the author;
  - a room post delivered to a member (`txt_type` 2) and acknowledged;
  - admin login, status, CLI, read-only login with `allow.read.only on`.
  - Posts (8) survived a restart. The room server starts with relaying off.
- **Relaying between our boards.**
  - The M9 repeater relayed a flood advert of the Heltec.
  - The Heltec repeater relayed an advert of the M9 (`relayed` 1 → 2).
  - The M9 saw the Heltec as `type 2` and `type 3` in the corresponding modes.
- **Return to normal mode.**
  - The same key; 64 messages, 24 nodes, the map and settings (M9 14 dBm, relay off; Heltec 10 dBm)
    in place.
  - Direct messages M9 ↔ Heltec with ACKs both ways, one copy each.
  - After the temporary stock firmware, the Heltec was restored from the full copy, verified against it and updated:
    history, nodes, settings and games matched those saved before the test.
- **Final installs:** M9 — ELF `A6C14E7D…`, boot 118; Heltec — ELF `429B6B51…`, boot 78 (build from `cd75575`: for the V4 it differs from the verified `2A9CDCA2…` only by the check for a present screen),
  both in normal mode. The final M9 differs from the verified LoRa build only by a caption in the post
  window.

**Not verified:**
- the MeshCore app on a phone (the official companion firmware was tested through `meshcore` for
  Python);
- relaying of direct packets through the repeater (needs a third node);
- long-term operation next to the repeaters of a city network;
- the modes on community boards;
- a full run of `finish_on_hardware.py` on these builds.

Test posts remain in the room on both boards; `erase` in the CLI deletes them together with the server
settings.
