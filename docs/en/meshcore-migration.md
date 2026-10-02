# MeshMesh 0.3.0: MeshCore

> Translated from the Russian original [docs/meshcore-migration.md](../meshcore-migration.md); when they differ, the original is current.

The radio layer of the M9 and Heltec V4 has been moved from MM/1 to MeshCore. Our own screen,
web interface, maps, GPS, compass, Wi-Fi and diagnostic BLE are kept.
Compatibility applies to the radio. Our BLE service does not yet implement the companion API
of the official MeshCore app: to control the M9 itself, use our
web interface. A third-party MeshCore companion can see the M9 and message it over the radio.

## Settings and discovery

On both devices: 868.731 MHz, BW 62.5 kHz, SF8, CR 4/6, chip power
10 dBm. MeshCore preamble: 32 symbols at SF7–8, 16 at SF9–12; sync word 0x12.
The radio parameters must match on the third-party device. Matching the
frequency alone is not enough. Meshtastic and the old MM/1 are not supported by this radio layer.

An advert is sent after boot, then every five minutes. On the M9 the ADV button
sends an advert manually. Send an advert from the third-party device as well;
after a signed advert is received, the node appears in “Nodes”.
A chat node can be sent a direct message. Repeaters, room servers
and sensors are also shown with their role; ordinary direct chat is meant
for the chat role. In the node card (0.3.1) you can reset the discovered route — the next
message goes by flood — or delete the contact; its next advert adds the
node again. Room login, server commands and telemetry have not been added yet
([comparison](feature-parity.md)).

The contact table holds 24 nodes. When it is full, an advert from a new node takes
the place of the contact that has not been heard from for the longest time (the MeshCore rule
“overwrite when full”). Chat nodes and nodes with a conversation in the history
or a chess game are not evicted; in practice it is long-silent
repeaters that go. A repeater does not need an entry to relay our messages;
its next advert returns it to the list. The counter is `contacts_replaced` in `status`.

“Our relay limit” = 3 limits relaying by our device of packets
that already carry an accumulated path. MeshCore has no global TTL as in MM/1; this is not a promise
that other repeaters will limit the whole route to three hops.
The Heltec's amplifying radio front end affects the actual output power:
the setting value applies to the SX1262; antenna power has not been measured.

## Keys, chats and delivery receipts

Ed25519 keys are created for each device and stored in a separate
NVS namespace `meshmesh-mc`. Signed adverts are verified by the MeshCore core.
The full public key is the contact identifier; the short addresses of our UI/API
are its first eight bytes. ECDH and encryption use the full key.
Contacts and discovered return paths are saved in NVS with a checksum.
Message/advert timestamps stay monotonic across restarts.

Direct messages use the standard MeshCore exchange and ACK. “Delivered”
appears only after the expected ACK matches. Three attempts are bounded by a
timeout; retries of one send do not create multiple history rows.
The first attempt goes along the known path, the second and third by flood (the reply to a flood
brings a new path). The “direct” path is used only if the node was heard in the last 30 minutes
without repeaters and with an SNR margin of 5 dB above the SF threshold; otherwise the first attempt is also
flood. After three failures the path is reset. The route, the number of repeaters and the number of
the successful attempt are shown at the message in the chat.
On restart, unfinished direct sends become unconfirmed.

The public chat is the standard Public channel with the well-known MeshCore key.
It is not private, sender names in it are not authenticated, and there is no delivery ACK.
Outgoing direct messages are limited to 151 bytes of UTF-8. The Public limit depends
on the name length: up to 160 bytes including the “name: ” prefix (no more than 151 bytes of text).
The interfaces show the actual limit; text is not truncated when sending.

## Preservation and rollback

The old MM/1 key, history and settings are kept. Old messages are marked
as the MM/1 archive; old MAC addresses are not automatically turned into new contact
keys. SD maps and calibration are not formatted. The UI/API show the last
64 messages; bounded history logs remain on the storage.
Snapshots of data and sources before the migration are in `backups/before-meshcore-0.3`;
known working 0.2.0 packages are kept separately in `artifacts`.

## Sources and verification

Vendored core: MeshCore fork `ALLFATHER-BV/meshcomod`, tag `core-v1.17.4`, commit
`edd7ed47f42acf6c64c9fcac551050fdbd531a09`. The MIT license and original headers are kept.
Two local bounds checks are listed in `lib/MeshCore/PATCHES.md`.
The hardware adapter, persistent identity and UI integration are written in MeshMesh.

Independent reference firmware: the official `meshcore-dev/MeshCore`,
`companion-v1.17.1`. The local test build changes the board/flash/USB description
for the connected Heltec; its radio protocol sources are unchanged.
The result of the real radio exchange is `artifacts/meshcore-stock-check.json`.
The final installed build is identified by the device's `build_sha256`
and the package's `app_elf_sha256`, not by the version string alone.

The user tests their separate MeshCore device on their own. Outdoor range,
operation through a third repeater, a fresh outdoor GPS fix and compass axis accuracy
remain separate checks. Street maps work without internet after they are loaded;
satellite imagery, routes, voice and OTA are not available yet.
