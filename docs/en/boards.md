# Community boards

> Translated from the Russian original [docs/boards.md](../boards.md); when they differ, the original is current.

Besides the ThinkNode M9 and Heltec V4, which are tested on hardware, MeshMesh builds
for ten popular MeshCore/Meshtastic boards. We do not have devices of these models:
**the firmware is verified only on a computer** (build, emulator, web page).
Operation on the board is confirmed by owners; reports are added to this file.

In addition, there are nRF52840 ports tested on the devices: the GAT562 30S Mesh Kit
([gat562.md](gat562.md)) and the Heltec Mesh Node T114 with a colour screen ([t114.md](t114.md)). The nRF52 has no Wi-Fi, so it has no web page over Wi-Fi, Wi-Fi radar,
CSI sensor or internet client; the phone connects through the app over BLE or USB.
The shared nRF52 layer (`src/nrf52/`: storage, `Preferences`, BLE, BLE/LoRa radar) also suits
other boards with a RAK4631-like layout (RAK4631, ThinkNode M1, etc.),
but their targets have not been added.

## Boards

| PlatformIO target | Board | Chip, flash / PSRAM | LoRa | Screen, input | Interface |
|---|---|---|---|---|---|
| `heltec_v3` | Heltec WiFi LoRa 32 V3 | ESP32-S3, 8 MB / — | SX1262 | OLED SSD1306 128×64, PRG | like Heltec V4 |
| `heltec_tracker` | Heltec Wireless Tracker V1.1 | ESP32-S3, 8 MB / — | SX1262 | TFT ST7735 160×80, PRG; GPS | like Heltec V4 (128×64 centred) |
| `tdeck` | LilyGO T-Deck / T-Deck Plus | ESP32-S3, 16 MB / 8 MB | SX1262 | TFT ST7789 320×240, keyboard, trackball, SD; GPS on the Plus | like M9 |
| `tbeam` | LilyGO T-Beam v1.x (SX1262) | ESP32, 4 MB / 4 MB* | SX1262 | OLED SSD1306 (if fitted), IO38; GPS; PMU AXP192/AXP2101 | like Heltec V4 |
| `tbeam_supreme` | LilyGO T-Beam Supreme (SX1262) | ESP32-S3, 8 MB / 8 MB | SX1262 | OLED SH1106, BOOT; GPS; PMU AXP2101 | like Heltec V4 |
| `t3s3` | LilyGO T3-S3 (SX1262) | ESP32-S3, 4 MB / 2 MB | SX1262 | OLED SSD1306, BOOT | like Heltec V4 |
| `tlora_v2_1_6` | LilyGO T-LoRa V2.1-1.6 | ESP32, 4 MB / — | SX1276 | OLED SSD1306, BOOT | like Heltec V4 |
| `xiao_s3_wio` | Seeed XIAO ESP32S3 + Wio-SX1262 | ESP32-S3, 8 MB / 8 MB | SX1262 | no screen, BOOT | web, BLE, USB |
| `station_g2` | B&Q Station G2 | ESP32-S3, 16 MB / 8 MB | SX1262 + amplifier | OLED SH1106, USER; GPS — external module | like Heltec V4 |
| `thinknode_m2` | Elecrow ThinkNode M2 | ESP32-S3, 4 MB / — | SX1262 | OLED SH1106, button, buzzer | like Heltec V4 |

\* the classic ESP32 addresses 4 MB of the 8 MB PSRAM; the firmware does not require it.

Single-button boards use the Heltec V4 interface (`src/UiHeltec.cpp`): a short
press — next screen, a hold — action or menu. The hardware layer is
`src/HardwareCompact.cpp`, the pins are in `include/BoardPins.h`, the name, missing modules
and power limit are in `include/Board.h`. As on the Heltec V4, these boards have no maps and
no internet client (they need SD and PSRAM). The T-Deck uses the M9 interface (`src/Ui.cpp`)
with maps on SD and the internet client; its hardware layer is `src/HardwareTDeck.cpp`.

The MeshCore repeater and room server modes ([repeater.md](repeater.md)) are available on all boards:
- **single-button boards with a screen:** the mode is chosen in the first 5 seconds after boot
  (click — next, hold — select) and from the “Settings” menu;
- **T-Deck:** as on the M9;
- **boards without a screen** (XIAO, T-Beam without OLED): there is no selection at boot, so that a button
  press does not change the mode unnoticed; the mode is changed on the web page, in the app (BLE)
  or over USB with the command `role repeater|room|normal`.

On boards without PSRAM (Heltec V3, Wireless Tracker, T-LoRa, ThinkNode M2) the server takes
about 9 KB (repeater) or 14 KB (room server) of internal memory. On these boards the modes
are verified only by building.

The default radio parameters are shared (868.731 MHz, 62.5 kHz, SF8, CR4/6, 10 dBm).
Power limit: Station G2 — 19 dBm (amplifier output about 27 dBm; more at its input
damages it), T-LoRa (SX1276) — 20 dBm, the rest — 22 dBm.

## Installation

Packages: `artifacts/meshmesh-<board>-<version>/` (`tools/package.py <target>`), each with
`INSTALL.txt` containing the `esptool` commands.

- **First install** — `meshmesh-<board>-<version>-factory.bin` at address `0x0`.
  The image covers the whole flash: it erases the previous firmware, its settings and data.
  On the first boot MeshMesh creates the MeshCore key (NVS) and the LittleFS file system
  — only on a completely clean partition.
- **MeshMesh update** — four components at their offsets (bootloader `0x0` on the ESP32-S3
  and `0x1000` on the ESP32, `0x8000`, `0xe000`, `0x10000`); key, settings, contacts and
  history are kept.

If the LittleFS partition is left over from other firmware, MeshMesh does not format it: the USB
log prints `LittleFS: partition holds other data` and the Modules page shows `FS ERR`. The key
and settings are stored in NVS and are kept regardless; contacts, read marks, the message history,
chess games and room posts are not saved. The storage is created (the old data of the partition is
erased and the board restarts) with the USB command `fsformat` or on the screen: on one-button
boards with “Create storage...” in the menu of the home or Modules page (hold, then hold again within 5 s), on the
T-Deck with OK on the “Module health” page and OK again.
Before formatting, `fsformat` erases the first two blocks of the partition and checks that the flash
takes an erase and a write; on failure the reply names the step (erase, test write, format or
mount). On a user's Heltec V3 the erase went through but the write did not stay
(`ERR flash at 0x610000 does not keep a write`): the LittleFS partition lies in the top quarter of the
flash, which the block-protect bits of the chip's status register can lock. Since 0.3.9 `fsformat` then
reads the register and, if protection bits are set, clears them (as ESP-IDF 4.x did before writing) and
tests again; the reply shows the register values. The same user's next reply (0.3.9 or later): register `0000`, no protection bits, and the write still
does not stay; writes to NVS (`0x9000`) work. The cause is not found.

Since 0.4.3 `fsformat` (and the “Create storage…” item) then puts the storage into the free OTA slot: on
8 MB that is `0x310000`–`0x610000`, which the firmware does not use (there is no over-the-air update). The
slot is tested with a one-sector write every 64 KB from its start; the storage takes the tested part, no
more than the usual partition (1984 KB on 8 MB) and no less than 256 KB. The place is kept in NVS
(`meshmesh-fs`); at boot the partition record in memory is pointed there, the partition table is not
changed. The `fsformat` reply names the address, the size and the reason. If the partition works, the
storage stays there and the remembered place is cleared. A USB update with the four files leaves the slot
alone; the factory image erases the whole flash with the storage, then `fsformat` is needed again. Boards
with 4 MB have no second slot and no move.

USB commands help to narrow it down. `flashstatus` gives the chip's JEDEC ID, its size by the ID and by the
image header, registers SR1–SR3 (on Winbond chips the WPS bit in SR3 turns on per-block locks that SR1 does
not show) and where the storage is. `flashprobe` erases and writes one sector at the start, at the end and
at every megabyte boundary of the free OTA slot and of the usual LittleFS partition — each only while it
holds no data — which shows from which address the flash stops keeping writes. On the Heltec V4 (ID
`684018`, 16 MB) every address of the slot is `ok`. Formatting over another firmware's data and the move were checked in QEMU (ESP32, 8 MB, a partition
made to lose the write): the storage was created at `0x310000` and mounted from there after a restart. Not
verified on the V3.

The reply on 0.4.3 (7 October): flash `c84017` (GigaDevice GD25Q64, 8 MB), registers `200000` (SR3 bit 5 on
GigaDevice appears to be output drive strength, not protection). `flashprobe`: at all eight addresses from
`0x310000` to `0x7ff000` the erase works and the write is lost (`lost, reads ffffffff`); the move failed. Other
firmware on the same board (MeshCore-Low-Power by dt267 among them) keeps messages, so the chip can write. Our
builds differ: the image header says DIO while the flash driver libraries (`libspi_flash` and others) come from
`qio_qspi`, built for QIO; stock V3 firmware uses QIO for both. Since 0.5.1 the `heltec_v3` target takes the
`dio_qspi` libraries (`memory_type` in `boards/meshmesh_heltec_v3.json`), so the driver mode matches the header.
`flashstatus` shows the driver and its mode (`driver gd dio`); `flashprobe` also writes the first sector of each
region through the ROM functions (`rom ok`/`rom lost`) in the bootloader's mode, as esptool does. If the V3 says
`rom ok` while the driver's write is lost, the driver is the cause. Checked in QEMU (ESP32, `driver gd dio`,
`rom ok`); in the release the ROM test is on ESP32-S3 only — the classic ESP32 builds have no IRAM to spare. Not
verified on the V3.

The ESPFlash (Android) error `Firmware overlap: boot_app0.bin` means a wrong address: `boot_app0.bin` goes
to `0xe000`, not `0xe0000` — with the extra zero it lands inside `firmware.bin` (`0x10000`, about 2 MB).
It is simpler to write the single `…-factory.bin` at `0x0` (first install; erases the previous firmware's data).

## What has been verified (1 October 2026)

- Build of all 13 targets (`m9`, `heltec_v4`, `heltec_v4_r8` and the ten new ones) and packaging
  of the ten new ones.
- The shared code was moved from “M9 or Heltec V4” to a board description. The M9 code after that
  matched the previous build byte for byte (`.flash.text`, `.flash.rodata`, `.iram0.text`;
  only the build timestamp differs); on the Heltec V4 one function differs
  (`uiTick`) — with equivalent forms of the same instructions. Board fields were then added
  to `status` and to the web page, which changes both previous builds intentionally.
- Espressif QEMU 9.2.2 (ESP32): the T-LoRa and T-Beam boot to `READY`, LittleFS
  is created on a clean partition and mounted on the next boot, the USB commands
  `status`, `config`, `ui`, `set`, `send`, `screenshot` (128×64) respond; the T-Beam without
  PMU and screen boots, with no restarts or panics in a minute of operation. For the emulator,
  a `-D MM_EMULATOR=1` variant without battery reading was built: QEMU has no ADC.
  The emulator does not test the radio (there is no transceiver in QEMU: `radio_error -2`), Wi-Fi and BLE (no RF part: a panic
  in `register_chipv7_phy`). ESP32-S3 images do not
  start in this QEMU: the Arduino console on the S3 requires USB Serial/JTAG, which the emulator lacks.
- Web page (in Node, without a browser): for the statuses of all boards and the previous Heltec
  firmware — board name, power limit, modules “not on this board”, the GPS switch,
  help for the button or the T-Deck keys.
- The Android app shows the board name from `status`; unit tests and the
  release build pass.

Nothing has been verified on the boards themselves: startup, screen, buttons, radio exchange with MeshCore,
Wi-Fi, BLE, GPS, battery, PMU power. The exception is a T-Deck owner's report (3 October 2026,
version 0.3.7 installed through a launcher): the firmware starts and the trackball works, the screen was
upside down and touch did not work; the next version adds the rotation and touch, not yet checked on the board.

## What to check on the board first

The USB log at boot: the lines `MeshMesh … / <board>`, `HW <board> screen=… pmu=… fs=…`,
`SELFTEST … PASS` and `READY`. Then `status` (field `radio: true`, `radio_error: 0`),
an advert and a message with a stock MeshCore node in both directions with ACK.

Places where sources disagree or behaviour was chosen without a board:

- **T-LoRa V2.1-1.6:** SX1276 reset on GPIO23 (per LilyGO and Meshtastic; MeshCore
  specifies GPIO14). The SX1276 also works without a reset.
- **Heltec Wireless Tracker:** the GPS pins in MeshCore and Meshtastic are swapped;
  the screen offset and colour inversion follow MeshCore (26/1, inversion).
- **T-Beam, T-Beam Supreme, Wireless Tracker, Station G2, T-Deck Plus:** GPS
  is detected automatically — the firmware listens on both pins in turn at 9600, 38400 and
  115200 baud until NMEA sentences with a correct checksum arrive (log:
  `GPS NMEA on GPIOn`). The firmware sends nothing to the GPS module.
- **T-Deck:** the SX1262 DIO2 is left as the antenna switch (as in Meshtastic; MeshCore
  disables it). Trackball directions follow Meshtastic. The screen uses Adafruit_ST7789 rotation 3
  (keyboard below; rotation 1 showed the image upside down, reported by an owner).
  There is no BACK key: DEL on the keyboard with no text is “back”; undo in Solitaire is the U key.
  Backlight — 16 driver steps; no sound.
- **T-Deck touch screen** (GT911, I2C 0x5D/0x14 on the keyboard bus; coordinates mapped as in
  LilyGO's and WadaMesh's code): tapping the title bar is “back”; the hints in the bottom row are
  buttons for their keys (OK, BACK, DEL, MSG, MAP, HOME, ADV, MIC, CTRL, letters), so DEL from the
  bottom row deletes a saved Wi-Fi network and a finished chess game; tapping a tile or a list row
  opens it (in settings, the mode choice and the opponent choice the first tap selects); tapping a
  board square moves the cursor there and presses OK; a swipe is the arrow in its direction, like the
  ball; holding 0.8 s is “hold OK”. A tap made before the screen has redrawn after the previous action
  is ignored. Solitaire cards cannot be picked by touch (the bottom row and swipes work).
  Checked in the screen emulator (`tools/ui_preview/build.sh tdeck`, all 15 languages) and with the
  USB command `uitouch`; not on the board itself.
- **Station G2:** the screen is driven as an SH1106 (as in MeshCore); receive gain
  boost (boosted gain) is off, as in MeshCore for this board.
- **ThinkNode M2:** TCXO 3.3 V, screen power GPIO46, button GPIO47, buzzer GPIO5 —
  per MeshCore.
- **XIAO ESP32S3 + Wio-SX1262:** the button is BOOT (GPIO0); antenna receive control —
  GPIO38.
- **Heltec V3:** the enable level of the battery divider (GPIO37) is detected at
  boot — it differs between revisions.
- **Classic ESP32 (T-Beam, T-LoRa):** after boot in QEMU, 126–133 KB of
  internal memory is free (the T-LoRa has no PSRAM). Whether it is enough for the Wi-Fi access point and BLE
  at the same time only the board will show: check `status` → `heap` with Wi-Fi and BLE on.

## Reports

| Date | Board | Revision | Result |
|---|---|---|---|
| — | — | — | no reports yet |
