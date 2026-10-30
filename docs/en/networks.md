# Network of your city

The radio profile of a local MeshCore network (frequency, bandwidth, SF, CR) can be picked from a catalogue of
cities instead of typing it: on the website, on the device page and in the Android app. The catalogue is kept by
the community: [VoxMesh/meshcore-local-networks](https://github.com/VoxMesh/meshcore-local-networks). Report
mistakes and new cities there through Issues.

## What changes

Only the four parameters a network shares: frequency, bandwidth, SF and CR. Power, relaying, the hop limit, the
region, keys and contacts stay. Installing firmware never changes radio settings. A profile is written to a board
only when the user has chosen a network and confirmed the write.

MeshMesh works in 863–870 MHz with a bandwidth of 62.5, 125, 250 or 500 kHz. Networks outside that range (433 MHz,
for example) are listed but cannot be written to a MeshMesh board.

## Catalogue

- A copy is kept in `site/networks.json`. The site loads it next to the page; the app carries it in the APK; ESP32 firmware
  serves it to its page over Wi-Fi (`/networks.json`, `tools/pio_web.py`).
- "Update the catalogue" downloads the current `catalog.json` from GitHub and keeps it in the browser (or the app's
  WebView). The newer of the two copies by `updated` is used. Nothing is downloaded on every start.
- `tools/networks.py update` refreshes the copy, `tools/networks.py check` validates it (part of the Pages checks).
- The list is sorted by distance when the position is known, otherwise in catalogue order. Each place shows its
  networks and those of its parent territory (`parent_id`). The search matches the city and region names.
- The position comes from the board's GPS (a trusted fix only), from the phone in the app (an approximate one is
  enough) or from the browser. The board's page opened over Wi-Fi (http) cannot get the browser's position: find
  the city by name.

## Website

The "Network of your city" box is in the install section (`site/netpick.js`) and appears only on the Russian version
of the site (the catalogue covers networks in Russia for now). It is folded: the heading shows the chosen network, and
opening it shows two steps.

1. Choose a network: one row per network — city, name, settings, distance after "Near me", a link to the community.
   Five rows come first; searching a city or region also lists the networks of the territory around it. The choice
   stays in the browser (`localStorage`) across page reloads, the next board, a repeated installation or another board
   type; pressing it again clears it. If the catalogue changes the network's settings, the
   site shows the new ones and offers to use them.
2. Close the install dialog (the port must be free) and press "By USB" or "By Bluetooth". The site
   talks the MeshCore companion protocol, so it works with MeshMesh 0.9.0+ and the stock MeshCore companion alike.
3. The site shows the board's name, model and firmware, its current and the new settings. When the network is in another
   band than the board's current frequency (433 / 868 / 915 MHz), writing needs the box "The board and antenna are
   made for … MHz". A MeshMesh board cannot get a frequency outside 863–870 MHz.
4. "Write to this board" confirms each board. The site checks what was written, keeps the board's previous settings
   (by its key) and disconnects, so the next board can be connected. Connecting the same board again offers "Return
   the previous ones".

The stock MeshCore client repeat flag (firmware 9+) is sent back as it was.

## Device page and app

"Settings → Radio" has "Choose a network by city" (`renderNet` in `web/index.html`). A chosen network shows what will
change; "Apply" takes two presses, as other risky actions of the page.

With firmware 0.27.0 there is "Test": the settings are applied but not saved.

- **Keep** saves them.
- **Revert** returns the saved ones.
- **Check** asks the repeaters that hear the board directly (the request of the region search) and shows how many
  answered. A person decides: there is no automatic rollback on silence.

A restart during a test brings the saved settings back.

The board remembers the profile before the last change of frequency, bandwidth, SF or CR. "Return the previous" goes
back to it, and the current one becomes the previous. Changes over the companion protocol (MeshCore apps, the site)
count too.

## Board

- USB/BLE/HTTP commands (`/api/command`): `radio try {"frequency":868.731,"bandwidth":62.5,"sf":7,"cr":7}`,
  `radio keep`, `radio revert`, `radio previous` (`src/App.cpp`, `radioCommand`).
- `config` gives `radio_prev` (the previous profile) and `radio_trial` (the saved profile during a test).
- A test lives in memory only: `Config::save` writes the saved profile to NVS until the test is kept. The previous
  profile is kept in NVS under `radio_prev`.
- On the screen: M9 and T-Deck have "Previous radio" / "Radio test" in the radio settings (OK returns or keeps it).
  One-button boards, GAT562 and T114 have "Previous" / "Test" in the radio editor (hold).

## Network search on air (0.29.0)

The board finds by itself which catalogue profile can be heard nearby (`src/NetFind.inc`, `include/NetFind.h`). The
firmware carries an extract of the catalogue: the places and the profiles of the 863–870 MHz band
(`include/NetCatalog.h`). `tools/networks.py update` writes it together with `site/networks.json`, and
`tools/networks.py check` compares it with the copy.

How the search goes:

1. The current profile is heard first, then the candidates. With a known position these are the networks of the places
   within 150 km, nearest first (with the networks of the parent territory). Without a position, or with "All profiles",
   the board goes through every profile of the catalogue, the most common first. Profiles with the same frequency,
   bandwidth and SF are checked once: CR does not stop reception.
2. On each profile the board retunes the transceiver without writing the settings (`MeshRadio::tune`), sends a zero-hop
   discovery request for repeaters (the one of the region search) and listens for 8 s. It counts the repeaters that
   answered (with the best SNR) and the other packets received.
3. After the last profile the saved one comes back. The best profile is the one with more repeaters, then with more
   packets. If it is better than the current one, the board offers it.
4. Only a person applies the network found, and only as a test (`radio try`): "Keep" saves it, a restart brings back
   the previous one.

While the search runs (10 s to 4–5 minutes) our messages wait in the queue, relaying is off and the network of the
saved profile is not heard. Packets received on another profile are handled as usual: node adverts add the nodes to
the contacts. The search works in the normal mode only, not together with the region search. A change of the radio
settings meanwhile ends it.

A stock repeater answers at most 4 discovery requests per 2 minutes from all nodes together. Only repeaters in direct
reach answer. A network of companions without repeaters shows only by the packets received.

**By itself** (`net_auto`, on by default). Every 30 s the board checks its position: a fresh GPS fix (dated not before
the build) or the phone's position from the app. If the nearest catalogue place is within 150 km and the current profile
is not one of its networks, the board searches the networks of the nearest places once per boot for that place. The
screen tells about a network found: "Network nearby: Moscow… SF7". No automatic search starts during a radio test.

Where to find it:

- M9 and T-Deck: "Settings → Radio → Find network". OK starts the search (or stops it), and after a find starts the
  test of the network found; ← → searches all profiles.
- One-button boards, GAT562 and T114: the "Find network" row of the radio editor (hold does what OK does).
- Device page and app: the "Network search on air" box in "Settings → Radio": "Search nearby" (with the position found
  by "Nearby"), "All profiles", the table of answers with "Test" at each profile, the switch "Search by itself near
  another city".
- USB/BLE/HTTP: `netfind` (state and results in JSON), `netfind start`, `netfind start all`, `netfind start LAT LON`,
  `netfind stop`, `netfind use N` (test result N); `set {"net_auto":false}`.

The firmware does not apply the catalogue field `path_hash_bytes` (the path hash size of a network) yet: the path hash
is set separately, 2 bytes by default.

## Checked

- Firmware built for M9, Heltec V4, Heltec V3, T-Deck, GAT562 and T114; the radio screens checked in `tools/ui_preview`.
- Device page: choosing, testing, Keep, Revert and Return the previous checked in a browser with `tools/web_demo.py`.
- Website: the choice survives a page reload.
- M9: the `radio …` commands, the test rollback on a restart, the screen row, writing over the companion protocol by USB
  (as the site does) and the page with the real board (through `tools/web_usb_bridge.py`). Details: `docs/verification.md`.
- 0.29.0, network search: firmware built for every board; screens in `tools/ui_preview` (M9, T-Deck, Heltec, GAT562,
  T114), the page with `tools/web_demo.py`. On the T114: the search of all 25 profiles (a real repeater answered on the
  current profile and on the one next to it in frequency), the search by coordinates, stopping, the profile coming back,
  a test of a profile found and its rollback. Not checked: finding another network, the automatic search by GPS or the
  phone, the screens on boards, the other boards.
- Not checked: other boards, writing from a real browser over Web Serial/Bluetooth, stock MeshCore, repeaters answering
  "Check" and the phone position in the app.
