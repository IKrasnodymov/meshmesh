# Offline map format, version 1

> A copy of [docs/maps-format.md](../maps-format.md), which is written in English; when they differ, the original is current.

Coordinates use the standard spherical Web Mercator tile grid. Tiles are
256×256 pixels; supported zooms 10–17. Data are RGB565, little endian.
Attribution must accompany every map: © OpenStreetMap contributors / ODbL.
The base data remain subject to ODbL; the application does not claim ownership.

Package (`.mmmap`): six bytes `MMAP1\n`, uint32 LE manifest byte length,
UTF-8 JSON manifest (max 2048 bytes), then consecutive tile records.
Manifest: version, name, latitude, longitude, bounds [south,west,north,east],
zooms, default_zoom, tiles, attribution, created (Unix UTC).
Tile record: uint8 zoom, uint32 LE x,y,payload length,CRC32, followed by payload.
CRC is IEEE CRC32 of the encoded payload, compatible with zlib.crc32.

Payload consists of uint16 LE run length and uint16 LE RGB565 colour pairs.
Every run must be nonzero. Expansion must total exactly 65536 pixels;
maximum payload length is 262144 bytes. All records are checked before upload.

Device files: `/meshmesh/maps/Z/X_Y.mmt`. Header is 24 bytes: magic MMT1
at 0, zoom at 4, bytes 5–7 zero, x,y,length,CRC32 at 8,12,16,20.
Complete tiles replace previous files only after full RLE and CRC validation;
interrupted uploads stay `.part` and do not replace the previous complete tile.
Replacement uses an `.old` file to recover a rename interrupted by power loss.
The active manifest is `area.json`; the saved-area catalog is `areas/*.json`.
Maps from multiple areas coexist. Shared tile coordinates use the newest tile.

Uploads use bounded chunks: 512 binary bytes encoded as base64 in USB commands,
up to 2048 decoded bytes via authenticated `/api/maps/chunk` JSON `{data: base64}`.
Using binary HTTP `arg("plain")` on Arduino 2.x would truncate at NUL, so the
API deliberately uses base64 and tests payloads containing NUL bytes.
The tile GET API returns the exact binary `.mmt` file.

Rendering caches six expanded tiles in PSRAM. Storage reads and RLE decoding
run in bounded batches between radio ticks. The UI only copies visible rows
from complete, validated tiles. Gray regions indicate unavailable tiles;
M9's own access point provides no internet.

Optional USB transfer compression: `map zbegin {z,x,y,size,crc,packed_size}`
announces a zlib stream containing the same RLE payload. `map chunk` receives
compressed bytes; `map finish` verifies exact decompressed size, complete zlib
stream, RLE pixel count and original CRC before writing. Decompression output
is bounded by the declared size (max 262144), and the miniz state is heap
allocated because it does not fit the Arduino loop stack. Stored files and
web packages remain unchanged. Upload data are buffered per tile in PSRAM;
only a complete validated tile is written to SD.

Web tiles (M9, Wi-Fi client with internet): a missing tile is requested from the
tile server (default `https://tile.openstreetmap.org/{z}/{x}/{y}.png`, USB
`internet tiles URL|default`), shown as soon as it arrives and written to SD as the
same MMT1 file an upload produces, in 4 KB steps with the `.part`/`.old` order.
Download and PNG decoding run on a core-0 worker task; the loop keeps SD, display
and LoRa (one SPI bus). PNG: non-interlaced, 256 or 512 px square (512 decimated
2:1), all colour types; transparency blends onto the missing-tile grey. Failed tiles
are not retried for 60 s; HTTP 403/418/429 pauses downloads for 2 minutes. The
User-Agent identifies MeshMesh as the OSM tile policy requires; only viewed tiles are
fetched, no bulk prefetch. Zoom 3-18 for viewing; uploads keep 10-17.
Without GPS or a saved view (NVS `mm-map`), the map centres on an approximate IP
location (get.geojs.io, then ipapi.co; this sends the public IP to that service).
