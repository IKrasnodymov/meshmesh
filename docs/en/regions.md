# MeshCore regions

A region (flood scope) limits where flood packets spread. A node marks a packet with the transport code of a
region, and repeaters with regions set up forward it only if they serve that region. A packet without a region is
common: repeaters that allow `*` (`region allowf *`) forward it. Regions exist since MeshCore 1.10; the
`region def` syntax since 1.16.

The code is the first 2 bytes of HMAC-SHA256 keyed with the region key over the packet's type byte and payload.
The key of a public region is the first 16 bytes of SHA-256 of the string `#name` (`msk` → SHA-256(`#msk`)).
Names are case-sensitive, as on repeaters: `msk` and `MSK` are different regions. A region starting with `$` is
private: its key does not come from the name, and only the MeshCore app sets it.

Code: `include/Regions.h`, `src/Regions.cpp`, the search in `src/RegionSearch.inc`, sending in
`MeshCoreBackend::floodScoped` and `MeshRadio::scopeKey` of `src/MeshRadio.cpp`.

## Normal mode (chats)

Which region a flood packet of the node carries (messages, ACKs, path returns, requests to repeaters and rooms,
adverts):

1. While a MeshCore app is connected and has chosen a session region (`CMD_SET_FLOOD_SCOPE_KEY`): that region or
   "no region". The choice is dropped when the app disconnects.
2. A channel message: the channel's region — its own, "no region" (`*`) or "default".
3. Otherwise the default region of the radio settings; when it is empty, the packet goes without a region.

A message's region is fixed when it is queued; retries keep it. Direct and zero-hop packets carry no region, as
in the stock firmware. Reception does not depend on regions: the board hears all traffic. The board's own relay
does not check regions, like the stock companion.

While not every repeater of a route has its regions set up, messages with a region may not arrive. Then give the
channel "no region" (`*`) or clear the default region.

| Where | Default region | Channel region | Search |
|---|---|---|---|
| M9, T-Deck screen | "Settings → Radio → Region": OK types a name, ← → the regions found | the channel card, "Region" row: ← → default, no region, the regions found; OK saves or types a name | "Radio → Find regions" |
| Heltec, GAT562, T114 and other one-button boards | "Radio → Region": "none" and the regions found | — (web page or app) | "Radio → Find regions" (hold) |
| MeshMesh web page and app | "Settings → Radio → Region" with the regions found suggested | the channel card, "Region" section | "Radio → Regions → Find regions" |
| MeshCore app | "Default scope" (`CMD_SET_DEFAULT_FLOOD_SCOPE` / `GET`) | a chat's region scope (the session region before sending) | "Discover regions" (control data and anonymous requests) |
| USB / BLE | `set {"region":"msk"}`, `""` none | `channel do {"action":"region","channel":ID,"region":"msk"}`, `""` default, `"*"` no region | `regions find`, `regions` |

A channel link carries its region in the `region_scope` parameter (MeshCore App 1.47+ format): joining by such a
link or QR code sets the channel's region at once. "No region" is not put into links.

Storage: the default region in the NVS keys `region` and `region_key` (`meshmesh`); channel regions in a record of
their own, `meshmesh-mc/ch_regions` (channel ID and region); the `channels` list keeps its format, so the channels
survive a return to a firmware before 0.15.0.

### Region search

The board sends a zero-hop repeater discovery (control packet `0x80`, filter "repeater"), collects answers for
8 s (up to 8 repeaters; when none answers, it sends the discovery once more), then sends each in turn the anonymous "regions" request (`ANON_REQ_TYPE_REGIONS`, a
zero-hop reply); a silent repeater is asked once more. A repeater answers with the regions it floods; `*` is left out of the choice. Up to 12 names in
all. A search takes from 35 s (one repeater) to a minute; late answers count for 30 s more: a busy repeater queues its answer.

Only repeaters that hear the board directly answer. A stock repeater answers at most 4 anonymous requests per
3 minutes from all nodes together, so frequent repeated searches return an empty list. Repeaters are not added to the
contacts: an answer is opened with the key from the discovery answer, so the search works with a full contact table.

## Repeater and room modes

In a server mode MeshMesh runs the MeshCore 1.17 CLI (`src/MeshServer.cpp`, `docs/en/repeater.md`), including every
`region` command. Commands go over USB as `server cli <command>`, in the CLI console of the web page, from the
MeshCore app or from another MeshMesh board after an admin login.

An example for the Moscow region network (the community scheme: `ru` → `mow` → `msk` or `mosobl` → district or
sector). A repeater inside the MKAD ring, Northern district:

```
region allowf *
region def ru mow msk sao
region
region save
```

`region` afterwards prints:

```
*^ F
 ru F
  mow F
   msk F
    sao F
```

The same step by step (1.15 and older syntax):

```
region allowf *
region put ru
region put mow ru
region put msk mow
region put sao msk
region allowf ru
region allowf mow
region allowf msk
region allowf sao
region save
```

Moscow districts: `cao`, `sao`, `svao`, `vao`, `uvao`, `uao`, `uzao`, `zao`, `szao`. Sectors of the oblast (after
`mosobl`): `sever`, `severo-vostok`, `vostok`, `yug`, `zapad`. A wrong region is removed with `region remove name`,
children first. Give a repeater only the regions where it stands: a node on a border may have two, a node with all
regions makes one big network again.

A MeshMesh repeater answers the region search and forwards by its regions as the stock one does.

## Checked (9 October 2026, Heltec V4, firmware 0.15.0, boots 211–215)

`tools/region_check.py --port PORT` over USB, frames from `txframe`, the region code recomputed independently in
Python:

- an advert with the default region: `TRANSPORT_FLOOD`, the code matched; an invalid name is refused;
- a channel with its own region, "no region" (a plain flood) and "default": the codes matched;
- the app: reading and writing the default region (63/64, a private key too), the session region (54) and the
  "unscoped" flag, a control packet (55);
- the search on a real network (Kazan): a third-party MeshCore repeater sent the regions `ru`, `ru-ta`,
  `ru-ta-kazan` three times, also with a full contact table (24 of 24; the contacts did not change); in another run
  3 repeaters answered, one without regions set up (only `*`). In about half of ~12 searches the repeaters did not
  answer the discovery or the regions request (the stock repeater's limits — 4 discoveries per 2 minutes and
  4 anonymous requests per 3 minutes from everyone — and the air); a repeated discovery and a second request to a
  silent repeater were added afterwards, their success rate was not measured separately;
- a channel's region (Public too) survives a restart; after a return to an older firmware the channel list is
  still read, as the `channels` format did not change;
- the web page through `tools/web_usb_bridge.py` in a browser: the "Region" field of the radio settings (saving and
  clearing), the search with its progress, the channel card's region (`sao`, `*`, reset; an invalid name is
  refused) — checked through the DOM and the board's replies, no screenshot of the page taken;
- QEMU: the T-LoRa image (classic ESP32, the search buffers on the heap) boots, `regions`, `config`, `channels`
  answer.

Screens: the `tools/ui_preview` preview (M9, T-Deck, Heltec, GAT562, T114).

The M9 was not connected, and the T114 hung before the install (USB present, the firmware did not answer and did
not enter the bootloader on the 1200-baud touch): 0.15.0 was not installed on them. Not checked: a repeater forwarding or refusing a scoped packet (a second node in the repeater role is needed; on
the desk every board hears the others directly, so only the repeater's own reaction shows), choosing regions in the
MeshCore app's interface (the board reports protocol version 10; the version the app needs for its region menu is
not checked), the physical buttons and screens of the boards, a region from a link scanned as a QR code in the app.
