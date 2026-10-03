# MeshCore channels

> Translated from the Russian original [docs/channels.md](../channels.md); when they differ, the original is current.

Channels are MeshCore group chats: all members share one 16-byte key (AES-128), messages flood through
the mesh without delivery acknowledgements, and the sender's name is not verified. MeshMesh keeps up to
8 channels, Public included; the list lives in NVS (`meshmesh-mc/channels`, the name and key of each
channel) and survives firmware updates. Keys and links use the formats of the MeshCore app and WadaMesh,
so channels and QR codes are interchangeable with them.

## Adding a channel

| Way | M9 and T-Deck | Web page | Android app | Heltec, GAT562 etc. (OLED) |
|---|---|---|---|---|
| Hashtag (`#name`) | Chats → “Add a channel” | yes | yes | via the page or the app |
| Link `meshcore://channel/add?...` | — (no need to type a long link: see invitations) | paste | paste, open from another app | via the page or the app |
| QR code | shows the QR of its channel | shows the QR | scans with the camera and shows | — |
| Name and key (32 hex or base64) | yes | yes | yes | via the page or the app |
| Create a private channel (random key) | yes, then the card with its QR | yes | yes | via the page or the app |
| Hashtag heard on air | list in “Add a channel” | list | list | “Join: #name” in the Messages menu |
| Invitation in a direct message | ← in the chat: “Join” | “Join” button | “Join” button | “Join: name” menu item |

**Hashtag.** The key is the first 16 bytes of SHA-256 of the string `#name`; leading `#` and spaces are
removed first and Latin letters lowered (as WadaMesh and the MeshCore web client do). `#Test`, `test` and
`# test` give the same channel `#test` with the key `9cd8fcf22a47333b591d96a2b848b73f`. A hashtag channel
is open: anyone who knows or guesses the name can read it.

**Link and QR code.** The MeshCore format (`docs.meshcore.io/qr_codes`):
`meshcore://channel/add?name=<URL-encoded name>&secret=<32 hex>`. The `region_scope` parameter is accepted
and not used yet. The link is found anywhere in a text, so a pasted piece of a conversation works too.
QR code: byte mode, error correction M, versions 1–10; the encoder is our own (`src/Channels.cpp`, its
JavaScript copy is in `web/index.html`), checked bit for bit against python-qrcode and read by zxing.

**Name and key.** 16 bytes: 32 hexadecimal digits or base64 (24 characters with `==`, like the Public key
`izOH6cXN6mrJ5e26oRXNcg==`). MeshCore does not support 256-bit keys, neither does MeshMesh.

**New private channel.** The device's random number generator makes the key; then open the channel card
and show its QR code or send an invitation. The key of a private channel is shown on the M9 card (QR and
32 hex), on the page over the device access point and in the app; the page over the home network (plain
HTTP) does not get the links of private channels, just as it does not get the node key.

**Heard on air.** Packets of channels this node has not joined cannot be decrypted; only the channel's hash
byte is visible. MeshMesh counts such packets per hash (relayed copies once), keeps the last six and tries
common hashtags on them (`#test`, `#ping`, `#mesh`, `#ru`, `#moscow`, `#kazan` and others, the list is
`channels::commonTags`). When a name fits, the channel is shown as “#name — join”. While a hashtag is being
typed, the screen and the page show how many stored packets it opens. The key of a private channel cannot
be found this way.

**Invitation.** Channel card → “Invite” → a chat contact: a direct message with the channel link is sent
(encrypted with the node keys, with a delivery acknowledgement). MeshMesh shows it as “Channel invitation …”
and offers to join; stock MeshCore shows the link text. A Cyrillic name longer than 13 letters does not fit
a message (151 bytes) URL-encoded; then its letters go as they are, without `%D0%..`; MeshMesh reads both.

## Controls

- **M9, T-Deck.** Chats: Public, the channels, “Add a channel”, then direct conversations. On a channel row
  ←/→ open the channel card, in a channel chat the ← key. Card: QR code, hash, key of a private channel,
  “Invite” (←/→ choose the contact), “Leave channel” (OK twice).
- **Heltec, GAT562 and other OLED boards.** Messages name their channel (`Marat #kazan`); the “OK”/“Got it”
  replies and the GAT562 on-screen keyboard write to the same channel.
- **Web page and app.** Chat list with the channels, “+ Channel”, channel card with the QR code,
  invitations, removal.
- **USB and BLE.** `channels` lists them (with keys); `channel do {JSON}`:
  `{"action":"add","hashtag":"test"}`, `{"action":"add","link":"meshcore://..."}`,
  `{"action":"add","name":"Friends","key":"<32 hex>"}`, `{"action":"add","create":"Friends"}`,
  `{"action":"remove","channel":"<ID>"}`, `{"action":"invite","channel":"<ID>","to":"<node>"}`,
  `{"action":"probe","hashtag":"ru"}`. Sending: `send <channel ID> text`. HTTP has the same actions:
  `GET/POST /api/channels`.

A channel's ID in the history is `FF` followed by the first 7 bytes of SHA-256 of its key (Public: `ALL`).
A channel that was left disappears from the list; its messages stay in the history and come back when it
is joined again. Public cannot be removed.

## Limits

- One transceiver: channels work on the current radio profile; a channel on another frequency is not heard.
- Channels have no delivery acknowledgements (as in MeshCore); the sender's name is part of the text and is not verified.
- Channels are not added in the repeater and room server modes.
- What has been checked on devices: [verification.md](verification.md).
