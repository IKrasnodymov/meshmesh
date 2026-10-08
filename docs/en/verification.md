# MeshMesh verification

> **0.9.0: MeshCore apps (companion protocol)** (8 October 2026, branch `feature/companion`; no boards were connected — not
> checked on hardware or with a phone, no full run). All 15 boards build; the `tools/ui_preview` scenarios (M9, Heltec,
> T114) pass, including the "Bluetooth for MeshCore" switch. The Chinese T114 image has 9 KB free: `MeshRadio.cpp` is now
> built with `-Os` on the nRF52 (−10 KB), so LoRa exchange on the T114 and GAT562 must be checked again. QEMU emulator
> (`tlora_v2_1_6` with `MM_EMULATOR`, no radio): the third-party `meshcore` 2.3.15 Python library gets device and node
> information, contacts, channels, time, battery, the contact card and statistics; adds, finds and removes a contact,
> joins a channel; an unsupported command answers with an error; text commands work after frames. The "Connections"
> web page in headless Chromium through `web_usb_bridge.py` to the emulator switches the mode both ways without
> JavaScript errors. Messaging and ACKs through the app — `tools/companion_check.py` on two boards — not run yet.

> **0.8.0: power off** (7 October 2026, branch `power-off`, no full run). All 15 boards build; the `tools/ui_preview`
> scenarios pass (the menu, two OKs, the "Turning off..." and "Device is off" frames). **Heltec V4** (`f0caee3`, image
> `CE80DADF…`): `poweroff` switches it off after ~4.5 s, USB goes and does not come back (watched for 20 s); the owner
> turned it on by holding PRG twice — `reset_reason` 8 (deep sleep wake), history (64) and contacts kept. The farewell
> frame on the screen and the current while off were not checked. **T114:** the first variant with System OFF switched
> off, but after holding USER the board did not start: its bootloader takes USER held at reset for the Bluetooth OTA
> update mode (seen as `HT-n5262-OTA`, no USB). After a RESET in one such case LittleFS did not mount: two 4 KB pages
> (0xD4000 with the superblock and 0xDC000) were erased and not written back — the trace of an interrupted write; the
> exact moment is not established. A copy of the region is in `backups/t114/` (0600); at the owner's choice the storage
> was created again (`fsformat`): the T114 has a new node key, history, contacts and the pet are lost, the radio
> profile matches the network. The nRF52 now has a soft off (the CPU waits for the button in System ON; after a ~1 s
> hold the LED lights, the restart follows the release): on the T114 (image `D3425988…`) the owner turned it on by
> holding USER — `reset_reason` 3, storage and radio fine, BLE back. The GAT562, M9, T-Deck and community boards were
> not checked on hardware; the T114 Chinese image has 2 KB left.

> **nRF52 modules** (7 October 2026, branch `feature/dice`; the T114 and GAT562 were not connected — not checked on
> hardware). Chess, the pet and the dice are left out with `MM_NO_CHESS|PET|DICE` (`include/Modules.h`); a custom package
> is built by `tools/nrf52.py package ENV --without … [--lang CODE]`. The T114 and GAT562 were built without each module,
> without chess and dice, without all three (T114: 686 → 581, 629, 641, 566, 567 KB); a T114 package without chess and the
> pet with the Chinese image was made, `manifest.modules` = `dice`. Full set: the 15 boards build, the preview scenarios
> pass. The web page in headless Chromium hides tiles by `status.modules`.

> **Dice on the M9** (7 October 2026): the `feature/dice` build (before the commit, revision `21405ff-dirty`, image
> `CA4D4883…`) installed with `tools/flash.py`, boot 218, history (64) and settings kept, `dice` answers; the owner checked
> the screen with the keys — "everything works".

> **Dice (the DIC3R roller)** (7 October 2026, branch `feature/dice`; no board was connected — not checked on
> hardware, no full run). RPG, Warhammer and counter modes, characters, the `dice` command, the web page and app section
> ([dice.md](dice.md)). The `tools/ui_preview` scenario (`dice checks passed`) checks formula parsing (`2d6+1`, `4к6-2`,
> d%, d66 and bad ones), the range of 400 rolls of 3d6+2, the ends of d%, the digits of d66, subtraction, the grid's
> order, the success count, the commands and the JSON size; the M9, T-Deck (taps on a tab, a grid die, the roll card and a
> counter's +5), Heltec, GAT562 (joystick) and T114 screens were rendered in Russian and English. The web section was
> checked in headless Chromium (1100 and 390 px wide) with a mock API that runs the same `Dice.cpp`: pool, formulas,
> saved rolls, grid, threshold, counters, adding/renaming/deleting a character; no page errors. Found and fixed: quick
> taps on the modifier's "+" used a stale state (commands are now built in turn); `prompt()` became a field on the page
> (WebView has none). The Android unit tests passed (new test `diceAreReadFreshAndTheirActionsGoAsCommands`). Size: on
> the nRF52 the dice and the compact boards' interface (`UiHeltec.cpp`) are built with `-Os` — without it the T114
> Chinese image overflowed by 4.7 KB; now T114 Chinese has 4 KB free, Japanese 7 KB, GAT562 Chinese 34 KB.

> **0.5.1: the Heltec V3 flash driver mode** (7 October 2026). The V3 owner's reply on 0.4.3: flash `c84017`,
> the write is lost at every address from `0x310000` to `0x7ff000`, the storage move failed; other firmware on
> the same board writes. The `heltec_v3` target moved from the `qio_qspi` driver libraries to `dio_qspi`
> (confirmed in the build's map file); `flashstatus` shows the driver and mode, `flashprobe` also writes through
> the ROM functions. Checked only in QEMU (ESP32 T-LoRa, 8 MB): `driver gd dio`, `rom ok`, 85 s without restarts
> after the probe; in the release the ROM test is on ESP32-S3 only (the T-Beam has no IRAM to spare) (watchdog resets in the emulator also happen without commands). The Heltec V4 was not
> connected and we have no V3; not verified on a board.

> **0.5.0: mesh pet** (7 October 2026; firmware 0.5.0, Android 0.5.0). The pet (15 species, optional, every
> board's screen, the web page and the app) and nRF52 language images without the Russian strings (translation
> keys at compile time): T114 Chinese 12 KB free (it would be 3), GAT562 Chinese 39 KB. 15 boards built; the
> screen emulator (m9, tdeck, heltec, gat562, t114) and the Android unit tests passed; Chinese translations
> are found by the new keys. On hardware (7 October): packages `0c31aa7` installed on the M9 (`tools/flash.py`, ELF
> `43D98FA1…`, boot 217) and the T114 (`tools/nrf52.py`, ELF `2C7A2937…`, boot 63, repeater role kept); settings
> and history (64 and 44 records) kept, the pets stayed (their species number is now a new drawing: Lora is Boo,
> Mote Octomesh). The M9's advert was received and relayed by the T114 (`rx` 1, `relayed` 1), its pet's packets
> and relays grew. The app in the Android emulator through `tools/usb_tcp_bridge.py` to the M9 showed the "Pet"
> section and ran "Pet" (the answer "Мурр"). A repeater's pet gets no "friends": in the server role the MeshCore
> server keeps the contacts, not the normal role's node list. Bluetooth, the Wi-Fi page, a phone, physical
> buttons and the GAT562 on 0.5.0 unchecked.

> **Optional pet, web page and app** (7 October 2026, branch `feature/pet`; the full run was not done, no
> boards were plugged in). The app (debug build, unit tests passed, a test for `pet` added) in the Android
> emulator was connected "USB through a computer" to the T-LoRa firmware in QEMU (no radio): without a pet,
> the tile "No pet" and "Start a pet"; an egg with spots of the species' colour; a hatched Buzz with bars,
> experience, "Pet" (Purr, joy +10 %) and "Feed" (Yum, snacks 2 → 1); the name in a field on the page; "Let it
> go" with a second press; the grave with the memory after 7 days without food (died after 2 d 8 h of hunger).
> Found and fixed: the app did not take the `pet` JSON answer (the JSON command list in `Lines.kt`); the
> standard `confirm()`/`prompt()` in the WebView were replaced by a second press and a field on the page; the
> owner's decisions are written to flash at once (QEMU restarts by its watchdog and lost an action saved
> 5 s later). The section was not checked over Wi-Fi or BLE with a real board or on a phone.

> **Mesh pet, branch `feature/pet`** (6 October 2026, commit `9ffbbbf`, version number unchanged — 0.4.3;
> the full run was not done). All 15 boards build; the nRF52 language images fit (the largest, Chinese:
> GAT562 35 KB free, T114 8 KB). Screen emulator (m9, tdeck, heltec, gat562, t114): shots of the egg, a happy,
> a hungry and an ill pet and the grave; death at zero health, a new egg with the old pet remembered, hatching
> after 15 min, no death with death off. This commit was installed with `tools/flash.py` on the M9 (boot 216,
> ELF `002F361C…`) and the Heltec V4 (boot 165, ELF `BCD57A4F…`): settings and 64 history records kept. Over
> LoRa, direct messages M9 → Heltec and Heltec → M9 were delivered with ACK: the sender's `acks` and snacks
> grow, the receiver's `messages`, both `packets`; "friends" are the two nodes each heard (the other board and
> a third-party repeater nearby), not counted again after a restart and an advert. The pet's name and progress
> survived a Heltec restart and reflashing both boards. The pet screen and "Pet" were checked with USB keys and
> screenshots (M9 page with a speech bubble, Heltec hold menu); physical buttons, the T114, the GAT562 and the
> community boards were not checked on hardware; growing up to adult and death only with time skipped
> (`pet skip`), not real days.
> T114 (7 October, repeater): package `3d367a3` (the same code, ELF `1A2D10CA…`) installed with `tools/nrf52.py`
> over 0.4.2, boot 58; the repeater role and 44 history records kept. The colour pet screen, the hold menu
> and "Pet" were checked with USB keys and a screenshot. Radio reception after the install was not checked:
> the M9 and the Heltec were unplugged at the time, `rx` stayed 0.

> **0.4.3: storage in the free OTA slot** (6 October 2026). A user's Heltec V3 erases the LittleFS partition
> (`0x610000`) but does not keep a write, status register `0000`. `fsformat` now puts LittleFS into the free
> OTA slot in that case and keeps the place in NVS; `flashstatus` (JEDEC ID, size, SR1–SR3, storage place)
> and `flashprobe` were added. Checked: the diagnostic build on the Heltec V4 (boot 159) — ID `684018`,
> 16 MB, every slot address `ok`, settings and 64 history records kept; in QEMU (ESP32 T-LoRa with the 8 MB
> layout, a build that fakes the lost write, `MM_TEST_STORAGE_MOVE`) the storage was created at `0x310000`
> (1984 KB) and mounted from there after a restart, `flashprobe` skips the occupied slot. In QEMU an image
> without these changes also restarts by watchdog ~33 s after the second boot — an emulator limit, not
> the move. Not verified on the V3; the full run was not done.

> **0.4.2: message times and a clock without a source** (6 October 2026, local Heltec V4 image `B33C9817…` built before the version bump,
> boot 157; the full run was not done). The app shows the reception time in bubbles and the chat list,
> but some V4 stamps were false: the V4's GPS module reports a fix of 10 satellites dated 8 July 2026
> (spoofed signals; the M9 saw the same) and set the clock from it, and the ESP32 time survived a
> restart marked "not set". Now a GPS or RTC time before the firmware's build date is rejected (the
> position of such a receiver is not used either), GPS does not overwrite a phone or NTP time by more
> than 5 minutes and needs at least 3 satellites; a time kept over a restart stays only with a known
> source. A message recorded before the clock is set keeps 0 and its uptime, and gets the exact time
> when the clock is set (checked: 0 s off, kept after a restart; the M9 confirmed delivery). On the V4
> the spoofed fix was rejected (`gps_conflict`) and the phone time kept. Stamps already recorded
> wrong are not corrected. An incoming message before the clock is set and the unread count on the
> board's screen were not checked (the T114 was a repeater). Built m9, heltec_v4, heltec_v3, tdeck,
> gat562_30s, heltec_t114; Android unit tests and build passed, checked in the emulator with the V4
> over the bridge. Then the published 0.4.2 package was installed on the V4 (`119869e`, ELF
> `E883862A…`, boot 158): settings and 64 history records kept, the phone time survived the restart,
> a direct message to the M9 was delivered with an ACK, one copy, with the right time. The published
> 0.4.2 was installed on the M9 over 0.3.9 (ELF `6E1A4585…`, boot 213): settings and 64 history records
> kept, clock from the RTC. Two pairs of direct messages M9 ↔ V4 were delivered with ACKs, one copy
> each, RX/TX growing, the M9 without restarts. V4 boot 159 before these exchanges was the install of a flash
> diagnostic build from a parallel session (`flashstatus`/`flashprobe`), not a fault. The M9's RTC was 11 s fast; both clocks
> were set from the computer. An old history-restore bug was found: an `id` above 2³¹ reads back as 0
> after a restart (`d["id"]|0`); text, time and status are kept. Fixed in `MeshRadio::restoreHistory` (not released yet):
> a local build on the V4 (`D3ED79D3…`, boot 160) restored all 64 records with no id 0 (14 ids above
> 2³¹), other fields unchanged. The published 0.4.2 package (build
> `e11cfaf`, same code; `9A32B2E4…`, boot 57) was installed on the T114 with `tools/nrf52.py`: repeater
> role, settings and 44 history records kept; the T114 receives the V4's packets (RX grows), the V4 hears
> the T114's advert, no restarts.

> **0.4.1: bounded history-export memory** (6 October 2026).
> On published 0.4.0, switching T114 from repeater to normal mode reproduced an empty history
> response after sender ACK. A potential silent-failure path was removed: the additional 32 KB
> JSON pool. Records are now serialized individually, output length measured first, and memory
> or incomplete-output failures reported explicitly; `status.history_count` checks completeness.
> The role-change test now waits for a CHAT advert, not merely a cached repeater contact.
> **Local images**: T114 `BFD1E2A4…`, boot 46; V4 `AF2E53BD…`, boot 150. The first run passed
> six idle-reception messages with ACK and one copy, BLE enabled, complete history matching
> `history_count`, no resets/injection (`artifacts/power-041-pair.json`). Settings and roles were
> restored (T114 repeater). All 64 V4 history entries were checked. Three further T114 role-change
> cycles returned all 38 records each (`artifacts/history-041-role-cycles.json`), then restored
> the repeater role. All 15 targets built; Android
> unit tests and build passed. Independent versions: firmware 0.4.1, Android 0.4.0 with a new CI code.
> Current, phone, physical-button and other-board limitations remain. These hashes identify local
> images, not subsequent CI images.

> **0.4.0: event waits on every board and a consistent release** (6 October 2026).
> All 15 targets built and packaged, including every language variant for both nRF52 boards.
> Android `0.4.0+local` and the site built; Android unit tests, version/package checks and translation
> checks passed; 975 UI frames generated. Versions come from `versions.properties`; see `docs/en/releases.md`.
> **Local images on hardware**: T114 `2A555B2E…` (boot 36), Heltec V4 `33823E84…` (boot 148),
> BLE enabled on both. `tools/power_pair_check.py` passed six messages: three each way after
> screen-off, with no receiver USB polling until ACK; one copy, ACK, increasing RX/TX and LoRa
> notifications, no resets or USB injection. Report: `artifacts/power-040-pair.json`. Original
> settings and identities preserved; existing T114 history records compared. The original
> T114 repeater (boot 37) and V4 normal roles were restored. Repeater `power_check.py` passed:
> advert reception from V4 during idle, USB and simulated UI wake-up
> (`artifacts/power-040-t114-repeater.json`).
> **Preliminary runs did not pass**: the first returned empty T114 history despite sender ACK;
> records became readable again after a restart. In the second, a packet arrived but the sender
> received no ACK within 100 seconds. Neither cause is established. Separate reports:
> `power-040-pair-first-attempt.json` and `power-040-pair-second-attempt.json`. The subsequent full
> run on the same images passed without those failures; this does not establish long-term stability.
> CI images have their own hashes and revision: these local results do not verify future published
> files. Current, battery life, phone BLE connections and physical buttons were not measured/tested
> for this release. M9, GAT562 and community boards are build-verified, without new hardware tests.
> The full hardware suite was not run.

> Translated from the Russian original [docs/verification.md](../verification.md); when they differ, the original is current.

> **T114: event waits in the nRF52 application loop** (6 October 2026, package `EE52F8C1…`).
> Installed with `tools/nrf52.py flash` after two matching reads of flash 0x1000–0x100000;
> settings, role and node identity matched afterwards. With the screen off, the task waits for
> a LoRa IRQ for up to 20 ms instead of polling every 2 ms, in every role; reception is continuous.
> **Repeater, boot 26**: `tools/nrf52_power_check.py` confirmed idle waits, USB responses and UI wake
> through a simulated USB key. Once Heltec V4 (`D60369E2…`, boot 145) was connected, its adverts
> and LoRa notifications during idle were checked (`artifacts/nrf52-power-peer-check.json`).
> **Normal mode, boot 31**: T114 → V4 delivered with ACK, followed by three consecutive V4 → T114
> messages received from idle, each with a LoRa notification, one copy and ACK. Until ACK, only
> the sender was polled; no USB commands reached T114. RX/TX increased, with no unexpected resets
> or USB injection. T114 was restored to repeater mode (boot 32); settings, node identities, roles
> and BLE states of both boards matched their original snapshots.
> Final report: `artifacts/nrf52-power-pair-check.json`. Two preliminary runs were not accepted:
> a 30 s timeout and requiring a lit screen after it could have timed out. Their reports remain as
> `nrf52-power-pair-first-attempt.json` and `nrf52-power-pair-second-attempt.json`.
> BLE was enabled during testing. Host BLE scanning was cancelled because CoreBluetooth did not
> become ready; a phone BLE connection is not confirmed on this package. M9, Heltec V4, GAT562
> and T114 built; both nRF52 packages include all language variants. Heltec V4 served as the peer
> and was not reflashed: the idle-loop change applies to nRF52.
> **Not checked**: the physical button on this package, current, battery life or long-term stability.
> Task-wait counters do not measure CPU sleep/current. RXPS and scheduled GPS power cycling are
> not implemented yet. The full hardware suite was not run.

> **Heltec T114: the M9-style interface** (5–6 October 2026, build `606703b`, package `EDBC33C5…` through
> `tools/nrf52.py flash`, settings kept). The compact interface's one-button logic is drawn on the T114 by its own
> layer, `src/UiHires.inc`, at 240×135: M9 colours and icons (`include/UiIcons.h`, code shared with the M9), cards,
> avatars, a status bar with clock and battery, key chips, a full-screen chess board; CJK and Arabic at their own
> size. **On hardware**: every page captured over USB (`device.py --screenshot`), ~78 KB RAM free, no restarts; the
> owner looked at the screen and confirmed it. The largest image (Chinese) is 674 KB of 712. **On the computer**:
> the one-button scenario with chess in `tools/ui_preview` (t114) passed; M9, Heltec and GAT562 frames match the
> previous ones except clock digits; M9, Heltec V4, Tracker, GAT562 and T114 build. Not checked: one-button chess on
> the T114 screen in a radio game, phone pairing.

> **0.3.8 — MeshCore channels, contacts in LittleFS** (3 October 2026; `docs/channels.md`). **On hardware**
> (`flash.py`): M9 0.3.8 — boots 137–139, ELF hash `A75EEFEC…`; Heltec 0.3.8 — `E92F48CE…`, boot 92. Both
> boards' settings matched the snapshots taken before, 64 messages kept. Radio check MeshMesh ↔ MeshMesh
> (M9 ↔ Heltec): a hashtag channel with the same ID on both, messages both ways, one copy each; a private
> channel created on the M9 joined by the Heltec from a link inside a text; an invitation as a direct
> message with an ACK, joined and answered; a channel the M9 had not joined counted by its hash, its hashtag
> opened the stored packet, messages arrive after joining; 5 channels kept after an M9 restart; leaving
> keeps the history, joining again gives the same ID, Public cannot be removed. Compatibility with the
> official MeshCore companion v1.17.1 (temporarily on the Heltec, full flash copy verified before and after,
> `tools/channel_check.py`, `artifacts/channel-check.json`): hashtag channel joined by name on both sides,
> a private channel with the key from the MeshMesh link — messages both ways; the invitation received with
> its key, ACK; an unknown channel counted and opened by its hashtag; leave and rejoin. The first install
> showed the M9 NVS (20 KB) nearly full: 482 of 504 entries, the contact list (3.5 KB) and the channel block
> could no longer be rewritten. Fixed: channels are stored compactly (joined ones only), contacts in
> `/meshmesh/contacts.bin`, M9 read marks in `/meshmesh/read.bin` (LittleFS, written through a temporary
> file); the old contact block is moved out of NVS at the first boot and removed. Afterwards the M9 NVS has
> 280 used entries (350 free), the Heltec 228 (402); `status` reports `nvs_used`, `nvs_free`, `contacts_saved`.
> The contacts matched one by one after an M9 restart. On the computer: 14 boards build (GAT562 692 of
> 713 KB, 97%), the QR encoder matches python-qrcode and its JavaScript copy bit for bit, zxing reads the
> codes from screenshots, host rendering and T-Deck touch checks, the page in headless Chrome with a mock
> API, Android `testDebugUnitTest` and `assembleRelease`, 67 new strings in 13 languages. **Not checked**:
> GAT562 and the community boards on hardware (moving the contacts on nRF52 is only built), the page and
> the app with a real board, the QR scanner on a phone. The full `finish_on_hardware.py` run was not done.

> **15 interface languages — 0.3.7** (2 October 2026; `i18n/README.md`). The M9/T-Deck screens and the OLED
> boards (Heltec, GAT562, one-button boards) in English, Russian, Ukrainian, Spanish, Portuguese, French, German,
> Italian, Polish, Turkish, Chinese, Japanese, Korean, Arabic and Indonesian: 861 strings in `i18n/firmware`,
> `tools/i18n.py check` without errors. Glyphs are subsets of Fusion Pixel 8/12 px (Chinese, Japanese, Korean),
> ClearlyU 12 (Arabic: joined letters, right-to-left order within a line while the screen layout stays
> left-to-right) and misc-fixed (Polish and Turkish letters). Checked on the computer: all 14 boards build
> (GAT562 — 683 of 713 KB flash, about 96%); the screens rendered on the host in 15 languages
> (`tools/site_shots.py`), the M9 in Arabic, Chinese, German, Japanese and Korean and the OLED in Chinese looked
> through; the Russian and English M9 screens match 0.3.6 pixel for pixel. In QEMU (T-Beam, ESP32): the language
> from `partitions-de.bin` applies on the first start, a partition table without the mark does not change it, a new
> mark (`ja`) applies once; changing the language with `set` was not checked in QEMU (without a LoRa chip `set`
> hangs on the radio setup). The site is in 15 languages and asks for the device language before installing; for
> nRF52 the page writes the language into the image and recomputes the DFU packet CRC (Node, GAT562 package: 2 bytes
> change, the UF2 matches the image); the German and the Arabic (right-to-left) versions were looked through in
> Chrome with a local build. **On hardware** (3 October, `flash.py`): M9 0.3.6 → 0.3.7, boot 127 (build hash
> `F70A0D6E…`); Heltec 0.3.3 → 0.3.7, boot 84 (`E058B815…`, with the OLED hint clipped by width). Settings and
> key matched the snapshots taken before, the old `russian` became `lang` (M9 `ru`, Heltec `en`), 64 messages,
> 24 nodes and the chess games are in place. `set {"lang":…}` works; on the M9 the home screen and settings were
> looked through via `screenshot` and `uikey` in de, zh, ja, ko, ar, uk, pl, on the Heltec the home screen in zh,
> ja, ko, ar, de, uk; the language survives a restart (M9, boot 128). A message M9 → Heltec was delivered with ACK
> as one copy. GAT562 (`nrf52.py flash`, serial DFU): 0.3.6 → 0.3.7, then the final build — boot 52, hash
> `E1ADA783…`, repeater mode, settings, key, 12 messages and 2 games in place; the OLED looked through in zh, ja, ko,
> ar, pl, tr; after the Turkish title fix (“Röle” instead of the long “Tekrarlayıcı”) the final build is on the
> Heltec too (boot 85, `49AE43CC…`), adverts GAT562 ↔ Heltec received both ways. The M9 runs the previous 0.3.7
> build (`F70A0D6E…`), which differs only in that Turkish string. All boards are back on their languages.
> Installing from the site in Chrome and native-speaker review are not checked, the full
> `finish_on_hardware.py` run was not performed. Limits: 12 px Arabic on the OLED (and in the M9's small rows)
> overlaps the next rows, 8 px CJK on the OLED is dense; incoming messages in these scripts show only the glyphs
> of the subset; the device web page and the Android app are in Russian.

> **Chess with a stock MeshCore companion — the site page `chess/`** (2 October 2026; `docs/chess.md`).
> On the computer: `node tools/chess/companion_check.cjs` — 174 checks, including 300 random games
> (44,692 plies) that matched `src/Chess.cpp` ply by ply. Over the radio: the GAT562 temporarily ran the stock
> USB companion v1.17.1 (build `GAT562_30S_Mesh_Kit_companion_radio_usb` from `upstream/meshcore-stock`,
> serial DFU) against the M9 on MeshMesh 0.3.3 (boot 125 before and after). `chess_companion_check.py`: the page
> code in Node via `usb_tcp_bridge.py --raw` — a challenge from the page and mate in 7 plies, a repeated and an
> illegal move sent as text were ignored, a challenge from the M9 and a draw by agreement, an ACK for every command,
> the positions match, an ordinary message arrived as one copy in “Other messages”, no commands in the M9 chat. In Chrome
> over Web Serial (port chosen by the owner): a challenge, moves by clicking the board with ACK, a draw offer by button; after
> the window was closed, the page itself reopened the permitted port, restored the game and picked up from the companion queue
> the M9's acceptance of the draw. Then MeshMesh 0.3.6 was put back on the GAT562 (build hash matched, boot 50); the key,
> mode, games and history were preserved: the GAT562 USB companion does not use the 0xD4000 region. Bluetooth
> (Web Bluetooth with a BLE companion), Chrome on Android and two pages playing each other were not checked.
> After the check the M9 was updated from 0.3.3 to 0.3.6 via `flash.py` (a build from `main` with this page, hash
> `99282E58…`, boot 126): the settings matched the pre-install snapshot, history, contacts and the game are in place,
> the advert went out and LoRa packets are received. The full `finish_on_hardware.py` run was not performed.

> **Device modes: MeshCore repeater and room server** (1 October 2026; details in `docs/repeater.md`).
> Final installs via `flash.py`: M9 — ELF `A6C14E7DEB5225B1751DF1BC7AE7E4B26F1732781928031EC9CF012E382FE78F`,
> boot 118; Heltec — ELF `429B6B51C8C0C8D67F14589368FD721FA1A39D3E7F160B56AAC5BC6BD7925606`, boot 78 (targeted checks were on `2A9CDCA2…`, boot 77; the new build differs by a check for the presence of a display, which is always true on the V4);
> both in normal mode. The full `finish_on_hardware.py` run was not performed.
> From the side of the official MeshCore companion v1.17.1 (temporarily on the Heltec, then the full flash copy
> written back and verified) against M9 ELF `73F44F7F…`: repeater — 13 of 13 (`repeater-check.json`:
> login, status, telemetry, neighbours, ACL, remote CLI), room server — 9 of 9 (`room-check.json`: login
> with the room password, posts with ACK in both directions, admin login, read-only). The final M9
> differs from it only by a caption in the post window. Forwarding between the boards in both directions, return to
> normal mode without data loss, M9 ↔ Heltec messages with ACK, one copy each.

> **Android app and the commands for it** (1 October 2026): `android/`, `docs/android.md`.
> Firmware: `radar web`, `radar do`, `map tile`, `sendjson`, `connections` in `executeCommand`
> (and therefore over BLE too); BLE replies as MTU-sized notifications (up to 244 bytes) waiting for free
> NimBLE buffers. `bleprobe`, `radio_check.py` and `release_check.py` compare between boards
> only the radio profile (frequency, BW, SF, CR): power and hops are each node's own choice (currently M9 —
> 14 dBm, hops 3, relay off; Heltec — 10 dBm, hops 3). `persistence_check.py` and
> `release_check.py` compare the M9 settings with the snapshot taken before installation
> (`artifacts/m9-config-before-finish.json`), not with the pre-migration snapshot.
> Full `finish_on_hardware.py` run on the final package: M9 — #97, ELF
> `C731B7E9431E4C29BE6DB788D00D3F51A3443A4697F94D9F7E3AA27C4382A30D`; Heltec — #64, ELF
> `D4F0434B402EC580661113BCCB41F7F0DFD11D50ECB5C3429E3BD733861A1585`. All 16 hardware
> stages passed (clock, preservation of history/key/maps/calibration, map, LoRa with ACK in both directions,
> one copy each, chess, UI, parallel load, radar, CSI, Wi-Fi on both boards, BLE on both boards:
> authenticated pairing, history of 14.4 and 14.2 KB as MTU notifications without loss, a command over
> BLE delivered over LoRa). `release_check.py` did not pass: it requires the compatibility check against
> stock MeshCore (`meshcore_stock_check.py`) for this M9 ELF, which was last run
> for 0.3.0 and requires official MeshCore firmware on a second device — not performed.
> Separately, before the run: `bleprobe` M9 → Heltec 4 of 5 complete (one failure — the known “BLE connection
> failed” at the connection stage); `map tile` over USB — a 72 KB tile in parts, CRC correct; `radar web`
> over USB — 31–37 signals with names, actions are executed. After the radar is closed, the driver line
> `wifi:timeout when WiFi un-init` ends up inside the JSON reply over USB (this happened before too) —
> the app recognises the truncated reply and repeats the command.
> Then both boards were updated from the current tree (app changes + `e4eeda6` + community board support
> from another session; before that they ran other sessions' builds, M9 `B551C1…` #99 and Heltec `F99DFD…`
> #70): M9 — #100, ELF `410FE09BCEEAE64CA22340E30874A6224E11F3FEB2583F0B538ED5677461CB94`; Heltec —
> #71, ELF `044F4D49DF82689C12DDF77B079412153B294762473D79C1D64B540F178B6DE6`. Targeted: settings,
> history 64, 24 nodes, 5 games, the “Kazan” map with 1022 tiles and calibration preserved; the app
> commands respond; `radio_check.py` — delivery with ACK in both directions. A full run on this
> tree was not performed. The packages verified at 14:31 are kept as `artifacts/*-0.3.2-verified-1431`.
> On a phone (Nubia NX729J, Android 15) the app connected to the M9 over BLE with a PIN (MTU 255),
> the home screen and data were received; this was on build `B551C1…` without the app commands.
> The app (APK `0.3.2-app1`) was checked in an Android 16 emulator with real boards through
> the computer: the command layer (the same as for USB and BLE) with the M9 via `usb_tcp_bridge.py` — all
> sections with data, the map from SD tiles, radar, chess, saving settings, a direct message
> with a line break M9 → Heltec delivered (✓✓, one copy, text intact); the HTTP client with the Heltec
> via `web_usb_bridge.py`; a notification of a new message in the background and opening the chat from it;
> loss of connection returns to the connection screen. **Not checked on a phone:** USB OTG (CH340
> and CDC drivers, 921600 baud and holding the rate), Android BLE pairing with a PIN and joining the access
> point via `WifiNetworkSpecifier` — the emulator cannot do this. Visual checking on a
> real phone screen was not performed.

> **Direct message route** (1 October 2026): only the first attempt goes over the known
> path, the second and third go by flood; the “direct” path is used only if the node was
> heard within the last 30 min without repeaters and with an SNR margin of 5 dB over the SF threshold
> (otherwise flood straight away); after three failures the path is reset, as before. Each message
> stores the route, the number of repeaters and the attempt number (`route`, `hops`, `tries` in
> `messages` and history); they are shown in the M9 chat, on the Heltec OLED and in the web chat.
> Installed with targeted checks: M9 — #91, ELF `43ACA724AB2E1FECF4ED87B588ADEB9DD085D74732AF066315514FFB2118C050`;
> Heltec — #62, ELF `B8770D97EE56E0F58579BCA84F6E257F972A80F75AF341A2723A0A783FB29512`;
> a full run was not performed. Over the radio: (1) M9 → Heltec — direct, on the first attempt;
> (2) Heltec temporarily on 869.1 MHz — exactly three transmissions: direct, flood, flood, then
> “not confirmed”, path reset (255); (3) after returning to 868.731 — flood, the first reply
> did not reach the M9 (the Heltec transmitted it), delivered on the 2nd attempt, the “direct” path learned
> again; (4) Heltec → M9 and the next M9 → Heltec — direct, on the first attempt. The M9 chat screen
> and the Heltec OLED were reviewed in screenshots; on the web the captions were checked via the DOM in headless Chromium,
> no visual screenshot was obtained. Multi-hop delivery through a repeater was not checked.

> **Fallback flood for direct messages** (1 October 2026): with a known path the first
> two attempts go over it, the third by flood; if that one also gets no ACK, the path is reset
> and the next message goes by flood, and the reply brings a new route. Applies to chat
> and chess. Installed with targeted checks: M9 — #89, ELF `DF8E2DE52F4E4F9E41A578AA5ECF0073C52F15E672A7C41719A7E661E7B7B2A7`;
> Heltec — #60, ELF `5EF0D4B10D48EC14E9CA506FDF979BCA98E5003E3CD1C3628539D521A89E9FC5`.
> Checked (Heltec temporarily on 869.1 MHz): challenge frames — direct, direct, flood; the path
> M9 → Heltec reset (255); after returning, the automatic retry went by flood, the challenge was delivered, the path
> is “direct” again; `chess_check.py` on #89/#60 passed. Multi-hop delivery through a
> repeater (GAT) was not checked: the boards hear each other directly. Both contact
> tables are full (24 of 24, mostly repeaters): new nodes are not added.
> During the checks the M9 USB disappeared twice on the computer side without the board restarting.

> **0.3.2 + automatic retry of chess commands** (1 October 2026): an undelivered move
> is retried by itself — after 2, 5, 10, then every 15 min, immediately on receiving a packet
> from the opponent (no more often than once per 2 min), for 24 h; the move is tracked separately from the last
> command; the flag is saved (format `MMC1` v2, v1 is readable — checked on game 1AC0).
> Installed with targeted checks: M9 — ELF `553D59A02FD77A9B7DB13E4F6E16091E5216669A2A1F54FBD120CA74236BCC71`,
> Heltec — ELF `C2ED97FA6113EBCE71559B983FB9B1B058E89439DB5F6260332D6C1548BF28E0`; a full
> run was not performed. Over the radio (Heltec temporarily on 869.1 MHz, then returned to
> 868.731): (1) a move and a draw offer without ACK — the move was delivered on schedule
> 121 s after the frequency was restored, the offer 2 s after it (the first check
> found and confirmed a bug: the move retry overwrote the draw offer; fixed);
> (2) a Heltec advert — the move was delivered after 2 s instead of 92 s on schedule;
> (3) M9 reset (#87 → #88) with an undelivered move — delivered 67 s after the reset;
> (4) the Heltec was unplugged from USB and plugged in again (#56) — the pending M9 move got through by itself.
> The positions matched. `chess_check.py` on #88/#56 passed. One M9 flashing was interrupted
> by a USB disconnect (3 of 4 components written); after reconnecting the package was written
> completely and verified, settings, keys and games preserved. The text “retry in N min”
> on the web was not checked visually.

> **0.3.2 + chess in the web interface and notifications** (1 October 2026): a
> “Chess” section on the web page of both boards (list, challenging a contact, board by touch,
> promotion, draw, resignation, rematch, retry, deletion), `GET /api/chess`; notifications of
> the opponent's move: M9 — “…: Nf3, your move”, Heltec — a window on the OLED and the LED, web —
> a message, a counter in the tab and on the tile, optional sound, vibration. Installed
> with targeted checks: M9 — boot #85, ELF `658E6A23169616CF81D0F7CFD8F133283DFFFBEE1BC51079B022ECC38DF4A869`;
> Heltec — #53, ELF `5AECD5C7A586DA8B06276B8F3510A8D198A93B314DE1BD9205198E16029A4396`;
> a full run was not performed. Checked: the page in headless Chromium (390 and 1100 px)
> via `tools/web_usb_bridge.py` to the Heltec — over the radio with the M9, a challenge as white, notifications
> “MeshMesh M9 accepted the challenge — your move” and “MeshMesh M9: e5 — your move”, move e2–e4
> by touch, promotion bxa8=Q through the choice window, the M9's draw offer and acceptance
> by button, deletion by double press (on build #84/#52; afterwards only the CSS of
> the promotion window and the colour circle was fixed, captured from disk). The toast on the M9 and the window on the Heltec OLED
> were captured from the devices. `wifi_probe.py` (Heltec — access point, M9 — client, #84/#52):
> the page, refusal without a password and all APIs, including `/api/chess`. `chess_check.py` on
> #85/#53 passed. Game 1AC0 (a challenge from the M9 to the Heltec, created by the user) was preserved
> across reflashes. Not checked: the page on a phone over the device's Wi-Fi, vibration
> and sound on a phone.

> **0.3.2 + chess with contacts** (1 October 2026, after the CSI check commit):
> `src/Chess.cpp` (rules), `src/ChessNet.cpp` (games, `♟…` commands in MeshCore direct
> messages, LittleFS `/meshmesh/chess.bin`), `src/UiChess.inc` (M9 screen),
> USB `chess ...` on both boards; protocol — `docs/chess.md`. Installed with targeted checks via
> `flash.py`, a full run was not performed: M9 — boot #83, ELF
> `B4CA2A937B03CFF0B84848ECBF2D5C91C8586440D6B8B3AB8B84C38AFE9B8271`; Heltec — boot
> #51, ELF `620727E6092083D15065E2866C978CEA4EEEE0AFEA36A8469335DE3683FBE2A4`
> (radio settings and NVS preserved). Host: perft of six reference positions matched
> (up to 674,624 nodes), move notation, mate, stalemate, repetition, insufficient material;
> screens in `tools/ui_preview`. Over the radio M9 ↔ Heltec: (1) the Heltec on the previous firmware
> as a “text” client — the challenge from the M9 was delivered with ACK and shown as text, replies
> typed as ordinary messages: acceptance, moves; an illegal move and a wrong ply
> number were rejected, a repeated move ignored; the move Ng1–f3 was made with the M9 keys;
> (2) both boards on the new firmware — a challenge from the Heltec accepted on the M9 screen (board
> rotated to black), a game to mate (Qxf7#) with the same position on both boards;
> after an M9 reset (#82) the games were restored unchanged; a challenge from the M9 screen
> (choosing a contact and colour), a draw on the Heltec's offer, accepted with the D key;
> a rematch with the N key; a move while the Heltec was on another frequency — 3 attempts without ACK,
> “not delivered” after ~90 s, after the frequency was restored R delivered the move; resignation X×2;
> deleting finished games with DEL×2 on the board and in the list. Chess commands did not end up in the M9
> chat history. `tools/chess_check.py` (boots #83/#51): challenge, acceptance on
> screen, 7 plies to mate, a repeated and an illegal move sent as text ignored,
> no restarts. Test games were deleted from both boards; 8 chess texts in the
> Heltec chat history remain from the stage with the old firmware. With physical keys
> and against the stock MeshCore app it was not checked; the Heltec has no chess screen.

> **0.3.2 + online maps over Wi-Fi on the M9** (30 September 2026, after the web portal commit):
> Wi-Fi client (`src/Internet.cpp`: up to 5 networks in NVS `mm-wifi`, scanning, connecting,
> NTP), OpenStreetMap tiles over HTTPS with certificate verification (`include/RootCerts.h`),
> our own PNG decoder (`src/MapPng.cpp`, ROM miniz), a cache of downloaded tiles on SD
> in MMT1 format, zoom 3–18, the last view in NVS `mm-map`, position by IP
> (get.geojs.io, fallback ipapi.co) if there is no GPS and no saved view. The screen “Connect →
> Internet over Wi-Fi”: list of networks, password entry. Installed on the M9 with targeted checks via
> `flash.py` (without a full run): boot #78, ELF
> `F9EBE5B87BE4AA44DE745D14AE3319149CB0483C7D39EB3D0CFFD7E6322CD9CA`. The Heltec was built
> (the shared radar, portal and Wi-Fi probe were touched) but not reflashed; the client is not part of its
> build. Checked on the M9: the user chose a network and entered the password on the
> M9 keyboard; connection, NTP (`clock_source: NTP`); z14 tiles of central Moscow,
> which were not on SD, were downloaded and shown; after the client was turned off the same tiles
> are read from SD; after a reboot (#78) the M9 connected by itself and restored the view;
> `map locate` — centre by IP (Kazan, z12). Load: 24 pans at z17 in 100 s — 8 new
> tiles, 0 errors, internal heap 140–142 KB (222 KB without the client), no restarts,
> LoRa RX 5 → 10. The access point and the radar take Wi-Fi away from the client (pause); after
> they are turned off the client reconnects (radar: 3 s). The PNG decoder on the host matched
> Pillow pixel for pixel: 1/4/8-bit palette with tRNS, 8/16-bit grey, grey+alpha, RGB,
> RGBA, 256 and 512 px tiles, a real OSM tile; truncated and 300×300 PNGs are rejected.
> Full `finish_on_hardware.py` run on these packages (M9 — boot #80, ELF
> `F9EBE5B8…`; Heltec — boot #50, ELF `C539C748FD19E359…`): all 15
> hardware stages passed, from `flash-m9` to `heltec-ble`, including the Wi-Fi probe of the M9 as a client
> of the Heltec access point with the internet client's network saved. The `csi` stage in the first
> attempt failed because of a bug in the check: `heltec-ui` leaves the Heltec access point
> on, and `csi_check.py` turned it off only on the M9. After the fix the check was
> continued with `--resume` on the same boots: CSI in both directions, 48 frames/s.
> `release_check.py` did not pass for the same reason as before: there is no compatibility check against
> stock MeshCore on the new M9 ELF (it needs temporary flashing of the Heltec with official MeshCore).
> Not checked: long-term operation, network loss in the middle of a download, a full SD card, physical
> keys when entering a password with capitals and symbols (the user checked only their own
> network), coexistence with BLE on under load.

> **Experiment: heart rate and breathing via CSI in the “reflection” setup** (30 September 2026): M9 — beacon
> at 100 frames/s, Heltec — receiver next to it (30–50 cm), a raw IQ stream over USB
> (`csistream`, recordings `logs/csi-vitals-1.txt`, `logs/csi-vitals-empty.txt`, 0600);
> a person sat still 70 cm in front of the boards, not between them. Spectra of the principal
> component of the amplitudes of 52 subcarriers: peaks in the heart-rate range (60–96/min) and the “breathing” range
> (13–14/min) are present in an empty room too — heart rate and breathing frequencies cannot be
> determined in this setup. Only the power of the 0.1–0.5 Hz oscillations differs (normalised
> amplitudes): with a person present it is about 20 times higher than in an empty room. Heart rate and
> breathing by reflection need a 60 GHz radar (for example MR60BHA2).

> **0.3.2 + Solitaire** (30 September 2026, after the CSI commit): Klondike solitaire on the M9 —
> `src/Solitaire.cpp` (rules, undo, hint, saving), `src/UiSolitaire.inc`
> (screen), a menu tile. Installed on the M9 with targeted checks via `flash.py`: boot #71,
> ELF `816269D0F867B12337155CE597236FFFFB1752CBBE76213AF03A741EBDB51D6F`; the Heltec was not
> reflashed (the game is excluded from its build, its sources did not change).
> Checked on the M9 with USB keys: opening from the menu, dealing, taking and returning, 40 moves
> by hint with auto-move, undo, help, exit with saving; after a
> hardware reset (boot #72) the game was fully restored. The radio received packets during
> the game (RX 0 → 12), free memory about 240 KB. Device screenshots
> were reviewed. Host: 3000 games with random and hinted moves —
> 52 unique cards, save/load and undo exact, corrupted records rejected;
> `tools/ui_preview` — game screens in Russian and English. The user checked the game with the M9's physical keys:
> everything works. Note: a reset via `esptool --after hard-reset` on the CH340 once
> left the M9 unresponsive until an RTS pulse (the firmware then booted normally).

> **0.3.2 + Wi-Fi CSI** (30 September 2026, after the radar commit): the CSI motion sensor
> and a permanent Bluetooth controller. Installed with targeted checks via `flash.py` (without a full
> run): M9 — boot #66, ELF `53EF267106B42ED9191562AFF43A47EF91E505C9FFD11D4B1AC15EF08C5E2AD0`;
> Heltec — boot #44, ELF `060FCCD9791E283AE77F855CBFA0EEF277A955D55611313A3F0A84AA6106CFAA`.
> Checked: `csi_check.py` (`artifacts/csi-check.json`) — Heltec beacon → M9 receiver and
> M9 beacon → Heltec receiver, 49–50 frames/s on channel 1, CSI data changes, without
> receiver restarts; `ble_probe.py` in both directions — authenticated commands and
> BLE → LoRa → ACK (reports in the scratchpad, not in `artifacts/`); the Heltec with BLE off
> disappears from the M9 BLE radar and comes back after BLE is turned on; 6 cycles of opening/closing
> the radar with BLE off — no panics or reboots, CSI after the cycles and after
> turning BLE on/off does not “freeze”. Motion — an experiment with a person, boards on a table
> (−40 dBm): still 0.005–0.011, walking between the boards 0.013–0.039, individual sharp
> movements up to 0.25; the threshold was chosen from these data, the calibration and threshold were not
> verified by another walk. Found and fixed: ESP-NOW frames with MCS0 did not
> go out on the air (now 6 Mbit/s OFDM); the fixed beacon rate “froze”
> CSI on subsequent reception (it is reset on stop); deinitialising NimBLE right
> after stopping a scan caused an M9 panic (`logs/radar-close-panic.log`) — this
> bug was also present in the radar commit build when closing the radar with BLE off; after
> deinitialising and restarting the Bluetooth controller, CSI did not change until
> a reboot — the controller is no longer deinitialised. The full `finish_on_hardware.py`
> (now with a `csi` stage) was not run on this build. Later (~19:21) a parallel
> session reflashed the M9 with its own build (a copy of this tree + solitaire, ELF `844FC0ED5A4B0061…`,
> boot #68), then once more (ELF `B50021EA…`, boot #70); the results above refer
> to boot #66, the Heltec did not change.

> **0.3.2** (30 September 2026): a signal radar (Wi-Fi, Bluetooth, LoRa) with homing by
> RSSI; the M9 main menu — tiles three per row with scrolling, new “Radar” and “Modules”;
> Heltec — 9 screens. Installed on both boards via `finish_on_hardware.py`:
> M9 — boot #47, ELF `8CBE03A7720C6F7C3E3B6A8774E0758F77E8A353C1E584A14FA1B5769DAFAF14`;
> Heltec — boot #34, ELF `5EB1081EFE2A917F0B3C757365D2E6A7A6BA63F1FD2B3D0AEA1C782D9F46632E`.
> All hardware stages passed, including the new `radar_check.py` (`artifacts/radar-check.json`,
> real radio reception): the M9 found 10 Wi-Fi networks, 13 BLE devices (3 phones/watches) and
> a LoRa node; homing on the Heltec access point — 17 beacons/s, −26 dBm; homing on the Heltec BLE service
> — 11 adverts/s, about −35 dBm; turning on the access point took Wi-Fi away from the radar;
> the Heltec saw the M9 over LoRa directly and got a homing reading from a new M9 advert.
> The M9 BLE service was on all the time (Wi-Fi/BLE coexistence). One-off failure:
> the first BLE attempt M9 → Heltec — “BLE connection failed” after a successful scan
> (`logs/finish-heltec-ble-radar-build-first-attempt.log`), the `--resume` retry on the same
> boots passed, as in 0.3.1. The final `release_check.py` reconciliation again did not pass
> only because of the compatibility check against official MeshCore on the new M9 ELF.
>
> Found and fixed along the way: `scanNetworks()` in Arduino 2.0.17 does not zero
> `home_chan_dwell_time`, and the M9 driver rejected the scan (status 1 after 3 ms) —
> the scan is now started directly through ESP-IDF. `map_ui_check.py` twice got an
> incomplete screenshot at 921600 baud (CH340, no flow control; in isolation
> 20/20 screenshots were intact) — now there is one retry recorded in `capture_retries`; in the final
> run there were no retries. Limits: RSSI gives neither direction nor distance; phone BLE addresses
> change, the device count is approximate; homing while moving outdoors,
> range and device classification accuracy were not measured; radar screenshots
> on the device were reviewed only partially (M9, list and sweep).

> **0.3.1** (30 September 2026): reworked on-screen interface for the M9 and Heltec,
> phonetic Russian layout on the M9 (double space switches RU/EN, → for a capital,
> holding OK in a chat shows a cheat sheet; Sym symbols are not replaced by letters), the setting
> “Battery shows: percent/volts”, path reset and contact deletion,
> `reset_reason` in the status, fixed GPS reception on the Heltec V4 (the module transmits
> on GPIO39). Heltec: 8 screens, a menu on holding PRG, a new message window
> with the LED, screen blanking.
>
> Installed on both boards via `finish_on_hardware.py`: M9 — boot #34,
> ELF `CDA2CA78A537ED248E38153D89F00B5F133C81556CFF2504923EE0A6FA4948C4`; Heltec — boot #30, ELF `44C8DA471944B394590C599EABD55D785F763F1EDCA81391C520E999C6B2BB7F`.
> All hardware stages passed: flashing, clock, data preservation, M9 map and UI
> (layout, double space, cheat sheet, drafts, lock, LoRa while
> locked), radio exchange with ACK, M9 UI with sending and ACK, parallel operation of
> USB and radio, Heltec OLED (8 screens, an “OK” reply from the menu delivered over LoRa with ACK,
> the settings menu closed without changes, Wi-Fi/BLE, encryption test; screenshots from
> the screen reviewed), Wi-Fi and BLE on both boards. One-off failures with a successful
> `--resume` continuation on the same boots: no Heltec response over native USB
> after `heltec-ui` (without a reboot) and the first BLE attempt M9 → Heltec
> (`logs/finish-heltec-ble-first-attempt.log`).
>
> The final `release_check.py` reconciliation did not pass only because of the missing
> compatibility check against official MeshCore on the new M9 ELF
> (`meshcore-stock-check.json` refers to 0.3.0); it requires temporarily
> flashing the Heltec with official MeshCore.
>
> Heltec GPS (stock GNSS connector): NMEA is received with a correct
> checksum; no position was obtained — the module either sees no satellites or reports a date
> that disagrees with the trusted time, and such a position is not used. While
> the module was being connected, the Heltec on 0.3.0 once stopped responding over USB and
> rebooted. The layout was also checked with the `tools/ui_preview` host renderer.
> The results below refer to 0.3.0.

On 30 September 2026 the final packages were installed on the ThinkNode M9 and the
connected Heltec V4 with 2 MB PSRAM. The overall hardware verification is complete:
`artifacts/hardware-finish.json` — `passed`,
`artifacts/release-check.json` — `passed_hardware_suite`.
All checks of the installed pair refer to boot #23 on each board.
The `heltec_v4_r8` target was built, but the other hardware revision was not checked.

The ELF SHA-256 of the installed application were compared with the package manifests:

| Board | ELF SHA-256 |
| --- | --- |
| M9 | `6F78DBFF99181D1560EED15FBE76C9D32338C1CE56B0CF41AD989C6626E9C66C` |
| Heltec V4 | `AD74F6B254299F68EF92E287F3F626F2E3E7619D22EE3AB239EF1CC79FF0F747` |

## Confirmed on the devices

- Radio parameters: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6, power setting
  10 dBm, our forwarding limit 3. The persistent MeshCore keys survived
  reinstallation and restart.
- Signed adverts, contact discovery, direct messages in both directions
  with a real ACK and one incoming copy. Independent exchange with official
  MeshCore 1.17.1 was checked on the final M9 ELF: adverts, repeated direct
  messages and the Public channel in both directions (`meshcore-stock-check.json`).
  After the reference check the Heltec was returned to MeshMesh 0.3.0.
- Production M9 UI: Russian input, backspace, sending and ACK, map,
  panning/zoom, map library, waiting-for-fresh-GPS mode, lock,
  holding OK to unlock, dimming and waking. The checks use
  USB key events; the user confirmed the physical buttons earlier.
- Heltec OLED: six pages, quick reply with ACK, Wi-Fi/BLE control.
  Real screenshots were reviewed; the web styling in a browser was not checked.
- The radio keeps working during the map, the lock and a slow USB download:
  ACK received in 1.54 seconds during a download lasting 13.51 seconds.
- Wi-Fi on both boards: real HTTP requests, the main page, refusal without
  authorisation, protected APIs. On the M9 — transfer of a binary tile and exact read
  back. BLE stayed on during the Wi-Fi STA check.
- BLE in both directions: pairing with encryption and authentication, selftest,
  status/config/history with fragmentation, a send command and the subsequent
  real LoRa ACK. The first attempt of the reverse connection to the Heltec was not
  established; a retry on the same boots, with Wi-Fi and BLE on both boards,
  passed. The initial failure was saved separately and noted in the final report.
- The original MM/1 keys, history, radio settings, compass calibration
  and SD were preserved. Kazan: 1022 tiles z12–14; detailed area: 218 tiles z15–16.
  Corrupted and interrupted downloads do not replace intact tiles; invalid settings
  are not applied partially. The UI/API show the last 64 messages.
- The M9 RTC keeps trusted time after a restart. The old GPS date of
  8 July does not replace it and is not treated as a fresh position. The GPS outputs NMEA;
  the IMU and magnetometer output samples. Both boards pass the
  cryptography, UTF-8 and corrupted-packet rejection checks.

Two bugs in the verification tools were fixed: the Heltec quick-reply check
takes into account the own identifiers of incoming MeshCore messages, and the BLE check
reads the real advertised name from the API. The verified stages were continued without
restarting the devices; the causes and original logs are kept in the receipt.

## Web interface in the style of the M9 screen (after 0.3.2)

The Wi-Fi page (`web/index.html`) was redone to match the palette, icons and sections
of the M9 screen: a main menu of nine tiles, a status bar, chats with read
marks and delivery statuses, nodes with a card (write, on the map, path reset,
deletion), a map with node markers, own position, azimuth and scale, a list of
maps, navigation, connections, radar (signals, homing, CSI), modules, radio
and screen settings, key hints and Solitaire (the game runs in the browser).
New APIs: `/api/connections`, `/api/radar` (holds the radar while the page is
open; 10 s after it is closed the radar is released unless it is open on
the screen) and the commands `resetpath`/`forget NODE_ID`.

- Headless Chromium on an API mock-up with real device data: screenshots of all
  sections at widths 390 and 1280 px, M9 and Heltec variants, no JavaScript errors.
- `tools/check_web_chat.cjs` and `tools/check_web_codec.cjs` pass.
- Installed on both boards via `tools/flash.py` (M9 boot 76, Heltec 49).
  `tools/wifi_probe.py`: the Heltec received the whole page over Wi-Fi (121,760 bytes),
  checked refusal without a password, all APIs including the new ones, and tile write/read.
  Reverse run (Heltec — access point, M9 — client): the page and all APIs
  passed; there are no maps on the Heltec, as it has no SD. The Heltec on-screen radar opens
  and closes as before.
- The M9 on-screen radar opens and closes as before; invalid IDs in the new
  commands are rejected.

Not checked: the page on a real phone, controlling the radar and turning off
Wi-Fi from the browser on the device (done only on the mock-up), the homing sound in
the browser. While the access point is open, the radar does not scan Wi-Fi networks, and the CSI
receiver does not work (the beacon does). The web shows names of networks and BLE devices, like
the screen; addresses are not transmitted. The full `finish_on_hardware.py` run was not performed.

## Community boards (1 October 2026)

Targets were added for ten boards we have no access to ([boards.md](boards.md)). Checked
on the computer: building all 13 targets and packaging the new ones; identical M9 machine code
(and equivalence for the Heltec V4) after moving shared code to the board description; booting
the ESP32 images of the T-Beam and T-LoRa in Espressif QEMU to `READY` with responses to USB commands and
creation of LittleFS; the web page for the statuses of all boards (Node); Android — tests and build.
Nothing was run on the community boards. The M9 and Heltec V4 with these changes
were updated by the Android app session (M9 #100, Heltec #71, targeted checks);
a full hardware run on them was not performed.

## GAT562 30S, nRF52840 (1 October 2026)

The nRF52 port was installed and checked on the connected board — details in
[gat562.md](gat562.md#verified-october-1-2026). In short: UF2 installation and updates via DFU
with a matching build hash and the storage preserved; exchange with the M9 (adverts, direct messages
in both directions with ACK and one copy); BLE pairing with a Mac and long commands/replies; BLE and
LoRa radar; the display, joystick, buzzer and a chess game with the M9 confirmed by the owner; the Android
app in the emulator with the board through the USB bridge (sections, sending with ✓✓). The board has no GPS module;
the app on a real phone with this board and the server modes from the side of the official
MeshCore app were not checked. The M9, Heltec V4, Heltec V3 and T-Deck builds
with the shared changes passed; 0.3.4 was not installed on the M9 and Heltec.

GAT562, app 0.3.6 (2 October): the owner connected to the board over Bluetooth from a phone;
the cause of the first error “no MeshMesh BLE service” was the Android service cache from the earlier pairing
with MeshCore; app 0.3.6-app2 clears it by itself.

GAT562, 0.3.6 (2 October): the GPS module is not fitted on our board — the manufacturer's schematic,
a check of the lines with pull-ups and an inspection of the case by the owner ([gat562.md](gat562.md)).

GAT562, 0.3.5 (2 October): fixed the hang on the splash screen with GPS off (details in
[gat562.md](gat562.md#fixed-in-035-hang-on-the-splash-screen)); restarts in
both GPS states and a mode change were checked on the board, data were preserved.

## Limits of the result

The user checks the separate MeshCore device on their own.
A fresh GPS position under open sky, compass accuracy and orientation, LoRa
range and forwarding through a third repeater were not checked. The web interface
was reviewed in headless Chromium, but not on a phone.
Our BLE does not implement the official MeshCore companion API; a macOS connection
with an old BLE keychain was not rechecked. Satellite maps, address
search, routing, voice and OTA are not available yet.

Migration, discovery and protocol limitations:
[meshcore-migration.md](meshcore-migration.md).
Archive of results for the previous version: [verification-0.2.md](../verification-0.2.md).
