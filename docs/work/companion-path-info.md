# Companion path info (MeshCore app)

**Goal:** Fix "No Path Info" for official MeshCore app on ZauggerOS T114.

**Scope (this pass only):**
1. `advert_paths[16]` + store on `onDiscoveredContact`
2. `CMD_GET_ADVERT_PATH` (42) → `RESP_ADVERT_PATH` (22)
3. `logRxRaw()` → push `0x88` when BLE/USB linked

**Reference:** `meshcore-dev/MeshCore` `examples/companion_radio/MyMesh.cpp` (cached in prior fetch).

**Files touched:** `src/Companion.inc`, `src/MeshRadio.cpp`

**Out of scope:** full companion v14+, message frame changes, persistence of advert paths.

**Done:** `Companion.inc` + `MeshRadio.cpp` — advert path table, cmd 42, push 0x88, `logRxRaw` override.

**Flash:** `c:\Users\Root\Cursor\Lora\firmware\ZauggerOS-T114-0.33.0.uf2` (double-tap RST → copy to `HT-n5262` drive).