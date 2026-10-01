MeshCore core-v1.17.4, https://github.com/ALLFATHER-BV/meshcomod
Commit: edd7ed47f42acf6c64c9fcac551050fdbd531a09
MIT license. Selected core and chat sources vendored with the bounds fixes listed in PATCHES.md.
Hardware, transports and UI are implemented by MeshMesh.

Repeater and room server roles (src/MeshServer.cpp): helpers/ClientACL, CommonCLI, ConfigSerializer, IdentityStore, RegionMap,
TransportKeyStore, SensorManager, sensors/LocationProvider and HttpOtaWifiSession.h come from the same
core-v1.17.4 snapshot. The server logic is ported from examples/simple_repeater and simple_room_server of
https://github.com/meshcore-dev/MeshCore (companion-v1.17.1, MIT). lib/CayenneLPP is MeshMesh's minimal
encoder with the ElectronicCats/CayenneLPP wire format.
