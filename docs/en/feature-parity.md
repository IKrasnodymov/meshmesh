# Comparison with MeshCore and WadaMesh

> Translated from the Russian original [docs/feature-parity.md](../feature-parity.md); when they differ, the original is current.

State of MeshMesh 0.3.2 from the sources as of 30 September 2026. Sources:
`upstream/meshcore-stock` (companion, repeater, room server, secure chat) and
`upstream/wadamesh` (firmware with the MeshCore core and an LVGL interface for the M9).
“Yes” means implemented in code. What is confirmed on hardware is listed in
[verification.md](verification.md).

## MeshCore protocol

| Feature | MeshMesh | Where |
| --- | --- | --- |
| Ed25519 key, signed adverts, receiving other nodes' adverts | Yes | `src/MeshRadio.cpp` |
| GPS position in the advert and the ADV button; as in the stock companion, no periodic adverts: the node advertises itself only with a new key or a new name after boot | Yes | `MeshCoreBackend::advertise` |
| Direct messages TXT_MSG, ACK, 3 attempts, direct/flood along the known path | Yes | `startMessage`, `processAck` |
| Public channel (GRP_TXT) | Yes | `onChannelMessageRecv` |
| Relaying with our relay limit, duplicate suppression | Yes | `allowPacketForward` |
| Contacts: auto-add, node type, path, saving in NVS | Yes, up to 24 | `saveContacts` |
| Path reset and contact deletion | Yes since 0.3.1 (node card) | `MeshRadio::resetPath/removeContact` |
| Hashtag and private channels (`#name`, own key) | No: `MAX_GROUP_CHANNELS=1` | — |
| Server modes: repeater and room server, like the stock `simple_repeater` / `simple_room_server` (login, status, telemetry, neighbours, ACL, CLI, posts, regions) | Yes since 0.3.3: separate modes, chosen at boot ([repeater.md](repeater.md)) | `src/MeshServer.cpp` |
| Login to another node's room server/repeater from normal mode (ANON_REQ), room posts (SIGNED_PLAIN) | No; signed messages are ignored | `onSignedMessageRecv` is empty |
| Commands to another node's repeater from normal mode (CLI, TXT_TYPE_CLI_DATA) | No; incoming ones are ignored | `onCommandDataRecv` is empty |
| Status, telemetry (CayenneLPP) and neighbour requests; replies to other nodes' requests | No; `onContactRequest` returns 0 | — |
| Trace path, path discovery, discover neighbours (CONTROL) | No | — |
| Share contact (zero-hop), export/import, `meshcore://` | No | — |
| Favourite contacts, manual add, auto-add settings | No | — |
| Multi-ACK, flood scope/regions, path hash size, GRP_DATA, raw | No | — |
| Companion API (the official app over BLE/USB/TCP) | Yes over BLE and USB; since 0.10.0 repeater and room logins, CLI, status, telemetry, trace and path discovery; no TCP ([companion.md](companion.md)) | `src/Companion.inc`, `src/Portal.cpp` |

Consequences for exchange with third-party nodes: in normal mode MeshMesh sees
repeaters, room servers and sensors as contacts, but cannot log in to a room server, read
its posts, request telemetry or answer such requests. In repeater
or room server mode, the official MeshCore app logs in to MeshMesh with a password and controls
it through the CLI; in normal mode the app can only exchange messages with it over the radio.

## WadaMesh interface on the M9

After the 0.3.1 rework MeshMesh has: a status bar with unread messages,
GPS, Wi-Fi/BLE, signal level from the SNR of the last packet, clock and battery;
avatars with initials and unread badges; ✓ / ✓✓ / error marks on
direct messages; a byte counter; a node card (write, on the map, path reset,
delete); distance and bearing to the node with a fresh GPS fix; a map with nodes,
compass heading and a scale bar; a lock screen with clock, date and
an unread summary; key help; RU/EN input; since 0.3.2 — a Wi-Fi/LoRa signal
radar with RSSI-based bearing (WadaMesh has none).

Missing compared with WadaMesh:

- Messages: message menu (resend, copy, delete, hop/SNR
  details), @-mentions, quick replies, emoji, links/QR.
- Contacts: search, sorting and filters, favourites, blocking, a list of
  discovered nodes separate from contacts, bulk deletion.
- Channels: creating/joining hashtag and private channels, mute, share via QR.
- Room servers and repeaters: login, admin console, telemetry with charts,
  trace SNR, range test.
- Radio: 21 regional presets, airtime factor, duty cycle indicator, spectrum
  analyser, signal and traffic page, regions.
- Device: first-run wizard, day/high-contrast theme, interface size,
  keyboard backlight, “do not disturb”, control centre, battery graph and sleep,
  OTA, settings backup, 14 languages (we have RU/EN).
- Other: Lua apps and a store, terminal, file manager, web browser,
  remote control/VNC, MQTT, console mode, online and topographic maps.

## What to add first

1. Hashtag and private channels: the most common feature of MeshCore networks; needs
   a channel identifier in the history, web interface and storage.
2. Room servers: password login and receiving SIGNED_PLAIN; then status and telemetry
   of repeaters and replies to telemetry requests.
3. Message actions: resend an unconfirmed message, path details, delete.
4. Contacts: favourites, manual add by key, contact sharing,
   raising the 24 limit using PSRAM.
5. Trace path and a link check with a selected node.
6. Regional radio presets after checking the values against the official list.
7. Repeater and room logins, room posts and trace on the screen and the web page (in the MeshCore app since 0.10.0).

Each network item is tested with a third-party MeshCore node: adverts in both
directions, addresses, keys, messages and delivery receipts. Receiving a packet does not prove
that the exchange works.
