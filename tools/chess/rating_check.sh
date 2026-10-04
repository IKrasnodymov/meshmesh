#!/bin/sh
# Host check of rated chess results: tools/chess/rating_check.sh (needs .pio/libdeps/m9: pio run -e m9 first)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd);LIB=$ROOT/.pio/libdeps/m9;ED=$ROOT/lib/MeshCore/src/ed25519;B=$(mktemp -d)
for f in "$ED"/*.c;do cc -c -O2 -w -DED25519_NO_SEED "$f" -o "$B/$(basename "$f" .c).o";done
for f in SHA256 Hash Crypto;do c++ -std=gnu++17 -O1 -w -DMM_HOST_CHECK -I"$ROOT/tools/ui_preview/shim" -c "$LIB/Crypto/$f.cpp" -o "$B/crypto-$f.o";done
c++ -std=gnu++17 -O2 -Wall -Wextra -DMM_HOST_CHECK -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_STD_STRING=0 -DARDUINOJSON_ENABLE_STD_STREAM=0 \
 -I"$ROOT/tools/ui_preview/shim" -I"$ROOT/include" -I"$ED" -I"$LIB/Crypto" -I"$LIB/ArduinoJson/src" "$ROOT/src/ChessRating.cpp" "$ROOT/tools/chess/rating_check.cpp" "$B"/*.o -o "$B/rating"
"$B/rating";rc=$?;rm -rf "$B";exit $rc
