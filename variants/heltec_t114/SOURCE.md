# Heltec Mesh Node T114 variant

`variant.h` and `variant.cpp` are taken unchanged from MeshCore (`variants/heltec_t114`,
upstream/meshcore-stock), which derives them from the Adafruit nRF52 Arduino core (LGPL-2.1,
headers kept). The Adafruit core fork MeshCore pins has no T114 variant, so the board file
`boards/meshmesh_heltec_t114.json` points here.
