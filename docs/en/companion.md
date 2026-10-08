# MeshCore apps (companion protocol)

Starting with this version, the stock MeshCore apps can connect to a board running MeshMesh: MeshCore,
MeshCore Open and other clients of the companion protocol, for example the `meshcore` Python library.
The app works with the board's own chats, contacts and channels. Chess, the pet, the dice, maps and
the radar stay in the MeshMesh app, on the web page and on the screen.

## Connecting

There is nothing to switch. The board's Bluetooth advertises as stock firmware does: the Nordic UART
service (`6E400001-B5A3-F393-E0A9-E50E24DCCA9E`) and the name `MeshCore-<node name>` (the node name is cut
to 20 bytes). MeshCore apps find the board by this name. The MeshMesh app finds it by the `MM` mark in the
manufacturer data (company ID 0xFFFF, "no company") and connects to its own service `7a9e0001-…`, which
answers as before. The MeshMesh app before 0.9.0 looks for the `7a9e0001-…` service in the advert and does
not find a board with the new firmware: it has to be updated. One phone connects at a time.

Pairing is protected by the same PIN shown on the screen. The `MeshCore-…` name is shown in "Connections"
on the screen and on the web page. The radar counts Bluetooth devices named `MeshCore-` and `MeshMesh ` as
mesh nodes.

The same protocol works over USB, as on the stock firmware: the byte `<`, a 2-byte length (low byte
first), then the frame; replies come as `>`, the length, the frame. Text USB commands keep working.
The first text command closes the link with the app, so frames never mix with text.

## What works

Protocol version 10 (`examples/companion_radio` of MeshCore companion-v1.17.1), code in `src/Companion.inc`.

- Device and node information: name, key, radio parameters, power.
- Contacts: the list (also only those changed since a given mark), lookup by key, adding, editing,
  removing, path reset, exporting and importing a contact card, sending a contact's card to nearby nodes.
- Direct messages. The app retries on its own; the board makes one attempt per request. A retry of
  the same message does not add a new history row. The delivery acknowledgement (ACK) reaches the app
  as on the stock firmware, also after its timeout has passed.
- Channels: the list by number, joining by key or hashtag, leaving. Public stays first.
- Receiving: direct and channel messages wait for the app in a queue (12 frames, in RAM). When it
  overflows, old channel messages go first. Messages appear on the screen as usual.
- Node advert (flooded or zero-hop), time (read and set), name, radio parameters, power, relaying,
  battery and storage, core, radio and packet statistics, restart.
- Messages sent from the app are kept in the board's history and shown on the screen and the web page.

Input limits are those of the screen: radio parameters within MeshMesh settings (863–870 MHz,
BW 62.5/125/250/500 kHz), the name up to 24 bytes of UTF-8, a channel name by the rules of `docs/channels.md`.

## What is missing

The board answers these commands with "unsupported" (`ERR_CODE_UNSUPPORTED_CMD`):

- logging in to a repeater or a room, their CLI, status and telemetry requests; managing repeaters
  from the app is not available yet;
- path trace, path discovery, raw packets and channel data, data signing;
- coordinates for the advert set from the app (the position comes from the board's GPS), other
  parameters (telemetry mode, extra ACKs, manual contact adding), auto-add settings, receive delay,
  flood scopes, path hash mode, PIN change;
- exporting and importing the private key (answered "disabled", as on stock firmware without that option).

Chess moves arriving over the radio are not passed to the app: they are service messages.
Channels with 256-bit keys are not supported by MeshCore either.

In the repeater and room modes only the device information request answers: those modes have no chats.

## Verification

- `pio run` for all 15 boards.
- QEMU emulator (ESP32, board `tlora_v2_1_6`, built with `MM_EMULATOR`; no radio in the emulator):
  the third-party `meshcore` 2.3.15 Python library, over a TCP bridge to the UART, gets device and node
  information, the contact list, channels (reading, joining `#mmtest`), time (read and set), battery and
  storage, the exported card, statistics, and adds, finds, resets the path of and removes a contact;
  an unsupported command answers with an error. Text commands work after frames.
- On the boards over Bluetooth and USB, with the app on a phone, messaging with another node and ACKs
  have not been checked yet.
