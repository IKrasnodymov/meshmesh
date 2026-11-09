# Backup

Since 0.30.0 (app 0.21.0) a board's settings can be saved to one JSON file and written back to the same board
or moved to another one: Settings → Backup on the device page and in the Android app (Wi-Fi, Bluetooth or USB).
The page reads and writes the copy with the board's ordinary commands, so the same is available over USB and BLE
(`tools/device.py`).

## What it holds

- **Settings** (`config`): name, radio profile, power, relaying, path hash, region, screen language and
  brightness, lock, notifications, time zone, app order, network search (`net_auto`). The previous and test radio
  profiles (`radio_prev`, `radio_trial`) are not carried over.
- **Node key** (optional, ticked by default): the MeshCore private key in MeshCore's export format — 64 bytes as
  hexadecimal digits. With it a restored or new board stays the same node for your contacts.
- **Channels** with their links and keys, region and notification settings; for Public, its region and notifications.
- **Contacts**: public key, name, type, position. The path to a node is not kept: after a restore the first message
  goes through the whole mesh (flood) and the path is found again.
- **Quick Send**: templates, recipient, position in the message.
- **Quick counter**: cards (name, value, colour), the open card, the device counting settings.
- **Character sheets** (dice module): the `MMS1~…` text of each character.

Not included: messages (the app keeps their archive), maps, the wardrive log (it has its own export), the pet,
chess and rating, the Wi-Fi access point password (new at every start) and the BLE PIN, notification settings the
browser or the app keeps itself. In the repeater or room mode the copy holds the settings and the node key, without
channels and contacts.

The file holds the channel keys and, if ticked, the node key as plain text: with them one can read your direct
messages and private channels. Keep it like a password.

## Restore

Once a file is chosen the page compares it with the board and shows only what will change:

- every changed setting with its old and new value, each with its own tick; for a copy of another board (`board`
  in the file) power, brightness, app order, GPS and the voltage display are left unticked; a change of the radio
  profile gets its own warning; settings this firmware does not know are listed and skipped;
- new channels and channels whose region or notifications differ;
- new contacts (existing ones are not overwritten);
- changed Quick Send templates, the counter cards;
- character sheets whose names the board does not have (the general set is not restored);
- the node key, if it differs from the current one.

Only what is ticked is written, in this order: settings (the radio profile in one command, the rest in pieces of up
to 200 bytes — a BLE write holds 255; if a piece fails, its settings are retried one by one), channels, contacts,
Quick Send, counters, sheets and, last, the node key. The result is shown line by line. A new key is stored and
takes effect after a restart the board makes by itself 1.5 s later; contact changes still pending are saved first.
With the new key the board announces itself once.

Do not run two boards with the same key at once: the mesh cannot tell them apart, and replies and acknowledgements
reach both.

## Commands

New firmware commands (USB, BLE, `/api/command`):

| Command | Answer |
|---|---|
| `identity` | `{"private_key":"<128 hex>","public_key":"<64 hex>"}` |
| `identity set <128 hex>` | `OK identity saved; restarting`, `OK identity unchanged`, `ERR identity: …` |
| `contact add {"key":"<64 hex>","name":"…","type":1,"lat":55.79,"lon":49.12}` | `OK contact saved` (a contact already known stays as it is) |

Over Wi-Fi, `identity` and `identity set` work only through the board's access point: on a home network the page
travels unencrypted, and the board answers `ERR the key is given out on the device access point only`. A key is
accepted only if it gives a public key MeshCore allows (not starting with 00 or FF).

Everything else is read and written with existing commands: `config`/`set`, `channels`/`channel do`, `nodes`,
`quick`/`quick do`, `people`/`people do`, `dice char use`, `dice sheet export`, `dice sheet put`.

## File format

```json
{"format":"meshmesh-backup","version":1,"created":"2026-11-09T07:09:28Z","firmware":"0.30.0",
 "board":"m9","board_name":"Elecrow ThinkNode M9","role":"normal","node":{"id":"…","name":"…","public_key":"…"},
 "settings":{…},"identity":{"private_key":"…","public_key":"…"},
 "channels":[{"id":"…","name":"…","kind":"private","link":"meshcore://channel/add?…","region":"","policy":{…}}],
 "contacts":[{"key":"…","name":"…","type":1,"lat":0,"lon":0}],
 "quick":{"to":"ALL","gps":true,"presets":["…"]},
 "counters":{"current":0,"counters":[{"name":"","value":0,"color":0}],"settings":{…}},
 "sheets":[{"name":"…","general":false,"text":"MMS1~…"}]}
```

If the node key could not be read (home network, older firmware), `identity_error` is written instead of
`identity`. A section is missing when the board does not have that feature.

## Limits

- Older firmware (before 0.30.0) saves a copy without the node key and does not take contacts; the rest works.
- The contact table holds 24 nodes: when it is full a new contact replaces one not heard for a long time.
- At most 8 channels including Public, and 6 characters.

## Verification

`tools/web_demo.py` answers the commands of the backup: saving and restoring were checked in a browser (the file's
content, the preview of changes, the ticks for another board, the order and content of the commands). The firmware
commands were built for all boards; the backup has not been checked on hardware yet (see `docs/verification.md`).
