# ZAUGGER OS (tzaugg private fork)

A rebranded [MeshMesh](https://github.com/IKrasnodymov/meshmesh) build for US MeshCore (910.525 MHz) on Heltec T114.

| Setting | Value |
|--------|--------|
| Boot splash / home title | **ZAUGGER OS** |
| App / companion firmware string | **ZauggerOS** + semver from `versions.properties` |
| Default node name | **Zaugger** |
| Default radio (T114) | 910.525 MHz, BW 62.5, SF7, CR5 |
| Default time zone offset | US Eastern (−300 min) |

Edit branding in `versions.properties` (`distro`, `splash`, `default_node`). Regenerate headers:

```sh
python tools/version.py --generate
```

Build T114: see upstream README / `pio run -e heltec_t114` and `tools/nrf52.py package heltec_t114`.

Upstream sync: `git fetch origin` on `IKrasnodymov/meshmesh`, merge or rebase your `granby-us-915` branch.