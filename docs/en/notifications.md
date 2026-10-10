# Notifications, quick send and counting

Introduced in firmware 0.19.0 and Android 0.12.0. Build and hardware results are recorded in [verification.md](verification.md).

## Notifications

LED, screen wake and incoming-message pop-up are independent: off, all messages or mentions. Each channel may override or inherit them. Configure them in the app's device settings; compact boards also have a Notifications settings page. M9 screen settings expose wake and pop-up controls.

Mentions use `@[node name]`, case-insensitive for Latin and Cyrillic. Optional comma-separated words also trigger mentions. A plain unmarked name does not. Sound uses the existing sound setting. Events are independent of history retention; queue overflow is reported by `status.notification_dropped`.

Most supported boards have one controllable LED, so ordinary messages, mentions and failures use different blink rates. They do not have separate green and red outputs. `status.notification_led` reports LED availability.

Optional failure alerts distinguish radio TX failure, missing direct-message ACK and no channel repeat heard within 30 seconds. Missing a repeat does not prove failed delivery. Delivered still requires the appropriate ACK. Compact incoming-message screens have no outer frame. Dice and manual counting do not let pop-ups consume the next button press.

Web and Android notification settings separately control incoming banners (all, mentions, off) and action feedback (all, errors, off). Operation errors remain visible in the interface. Android system notifications have separate settings.

## Quick send

Ten shared presets, a recipient and optional GPS attachment are stored on the board. Public is the initial recipient. A selected joined channel, chat contact or room is saved by ID; a removed recipient fails instead of silently falling back to Public. Edit and save presets in the app and use them on the device. The ↗ chat button opens quick reply to that conversation.

Since 0.19.1, all supported boards keep recipient selection in a separate card above the presets, visible while scrolling. It shows the current channel or contact: colour on M9, T-Deck and T114, an outline and selected inversion on compact screens. Normal entry selects the card; confirming a recipient saves its ID and selects the first preset without sending. Cancelling the recipient list returns to the card.

M9/T-Deck: arrows select, OK on the card opens recipients, OK on a preset sends, back exits. T-Deck also supports taps on the card, recipient and preset. Right with an empty chat composer opens quick reply. One-button devices: click selects; hold opens recipients, confirms the chosen recipient or sends a preset. The exit row remains at the end and advances to the next page. GAT562: up/down select, OK runs the selected action, left/right change page, back cancels recipient selection or goes home.

Presets allow at most 160 UTF-8 bytes; sending also checks the recipient's smaller limit. BLE commands have a shared 255-byte encoded limit including JSON and escaping; longer quoted/multiline templates can be saved through USB or Wi-Fi. GPS attachment defaults off and requires a fresh trusted fix, trusted clock and no GPS conflict. Stale or conflicting coordinates produce an error.

USB/BLE/HTTP: `quick` returns JSON. `quick do {"action":"set","index":0,"text":"OK"}` edits; `target` takes `to`; `send` takes `index` and optional `to`; `gps` takes `enabled`; `reset` restores defaults. HTTP GET/POST `/api/quick` has the same USB/BLE mappings in Android.

The preset and channel-selection idea comes from the [dt267 guide](https://github.com/dt267/MeshCore-Low-Power-Firmware/blob/main/Companion_Display_Guide.md#quick-send-channel-screen). MeshMesh uses its own UI and network implementation.

## Channel priority and retention

Channels have low, normal or high priority. Device limits: automatic, 8 or 16; Android archive limits: automatic, 8, 16 or 32. Automatic low priority retains 16 device messages and 32 Android messages. The board has a global 64-message history and evicts older lower-priority rows first; a channel with a bounded depth receives space even when the global history is full, then replaces its own oldest rows. The shared limit remains 64 when channels compete. Active sends are protected until complete. Reducing a cap removes that channel's oldest rows.

Compaction writes one row per main-loop iteration and retains the previous complete file until the new snapshot is committed. Presets, channel policies and manual counts use two verified LittleFS copies, keeping large records out of NVS.

Android archives foreground and background reads across all transports, separately by full node public key. Atomic storage is bounded to 32 nodes, 4096 messages per node and 1024 per ordinary conversation. Repeated polls, clock stamping and ACK updates do not duplicate messages. Only messages actually fetched by the app are archived; the standalone web page shows device history.

## Periodic Wi-Fi time

ESP32 boards, including compact ones, can save a network and enable periodic NTP without the persistent Wi-Fi client. With that client off and the clock not refreshed for an hour, a temporary connection lasts at most 45 seconds. Success switches Wi-Fi off until the next hour. Failures retry after 10, 20, 40 and 60 minutes. Access point, radar, CSI and Wi-Fi probes take priority. Fresh phone time postpones NTP regardless of the phone transport; web and Android refresh time every 30 minutes.

Configure this in Connection / Internet. Commands: `internet save {"ssid":"...","password":"..."}`, `internet ntp on|off`, `internet` for status. `internet add` still enables the persistent client. Periodic NTP defaults off. nRF52 has no Wi-Fi. NTP success requires an available network and hardware verification.

## People counter

This module is independent of Dice. Manual count: 0–999999, +1/−1, reset, save after a short pause. One-button boards: click +1, hold opens a menu with −1, reset, BLE, Wi-Fi, window and exit. GAT562: up +1, down −1, OK menu. M9: OK/up +1, down −1, hold toggles BLE.

Automatic results are BLE devices and Wi-Fi devices, kept separate from manual counting and from each other. RSSI threshold −100…−30 dBm, window 30/60/300 seconds, optional BLE phone/watch filter. Each table has 128 temporary identifiers and indicates saturation and dropped observations. No names, addresses or payloads are saved by the counter; identifiers are salted per boot and tables stay in RAM.

Background BLE scanning is passive and shares the existing controller with advertising and connections. Passive Wi-Fi listens to probe requests and client frames, hopping permitted channels; it yields to access point, radar, CSI, probes and Internet. nRF52 supports BLE only. Both automatic modes default off.

People may carry multiple devices, devices may not advertise, and addresses may change. Automatic device counts are estimates rather than headcounts and are not added to the manual result. The page explains this beside the indicators.

Commands: `people` returns JSON; `people do {"action":"change","delta":1}`, `reset`, `window_reset`, `settings` with `ble`, `wifi`, `personal_only`, `window`, `rssi`. HTTP GET/POST `/api/people` has matching Android mappings.
