# GAT562 30S Mesh Kit variant

`variant.h` and `variant.cpp` are taken unchanged from MeshCore
(`variants/gat562_30s_mesh_kit`, upstream/meshcore-stock), which derives them from the Adafruit
nRF52 Arduino core (LGPL-2.1, headers kept). The Adafruit core fork MeshCore pins has no RAK4631
variant, so the board file `boards/meshmesh_gat562_30s.json` points here.
