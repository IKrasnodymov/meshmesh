#!/bin/sh
# Build and render the screen UI on the host: tools/ui_preview/build.sh m9|tdeck|heltec|gat562|t114 OUTDIR [LANG]
# tdeck: the M9 interface with the T-Deck keys and the touch checks (it stops at the first failure).
# LANG is a code of include/I18n.h (ru by default); a list with spaces ("ru en de") renders each into OUTDIR/<code>/.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$ROOT/.pio/libdeps/m9
BOARD=${1:-m9};OUT=${2:-$ROOT/artifacts/ui-preview};mkdir -p "$OUT"
DEF="-DARDUINO=10819 -DMM_UI_PREVIEW=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_STD_STRING=0 -DARDUINOJSON_ENABLE_STD_STREAM=0"
SRC=$ROOT/src/Ui.cpp
if [ "$BOARD" = tdeck ];then DEF="$DEF -DMM_BOARD_TDECK=1";fi
if [ "$BOARD" = heltec ];then DEF="$DEF -DMM_HELTEC_V4=1 -DMM_COMPACT=1";SRC=$ROOT/src/UiHeltec.cpp;fi
# GAT562: the 128x64 interface with the joystick, without Wi-Fi.
if [ "$BOARD" = gat562 ];then DEF="$DEF -DMM_HELTEC_V4=1 -DMM_COMPACT=1 -DMM_JOYSTICK=1 -DMM_NO_WIFI=1";SRC=$ROOT/src/UiHeltec.cpp;fi
# T114: the one-button 128x64 interface drawn at 240x135 on the colour TFT (HiresCanvas), without Wi-Fi.
if [ "$BOARD" = t114 ];then DEF="$DEF -DMM_HELTEC_V4=1 -DMM_COMPACT=1 -DMM_HIRES=1 -DMM_NO_WIFI=1";SRC=$ROOT/src/UiHeltec.cpp;fi
INC="-I$ROOT/tools/ui_preview/shim -I$ROOT/include -I$ROOT/lib/MeshProtocol -I$LIB/ArduinoJson/src -I$LIB/Adafruit\ GFX\ Library -I$LIB/U8g2_for_Adafruit_GFX/src"
B=$(mktemp -d)
cc -c -O1 -w "$LIB/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c" -o "$B/fonts.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "\"$LIB/Adafruit GFX Library/Adafruit_GFX.cpp\"" -o "$B/gfx.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "$LIB/U8g2_for_Adafruit_GFX/src/U8g2_for_Adafruit_GFX.cpp" -o "$B/u8g2.o"
eval c++ -std=gnu++17 -O1 -Wall -Wno-unused-function $DEF $INC -c "$SRC" -o "$B/ui.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "$ROOT/tools/ui_preview/preview.cpp" -o "$B/preview.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/I18n.cpp" -o "$B/i18n.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/UiIcons.cpp" -o "$B/icons.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Apps.cpp" -o "$B/apps.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/RadarModel.cpp" -o "$B/radar.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Solitaire.cpp" -o "$B/solitaire.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Chess.cpp" -o "$B/chess.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ChessNet.cpp" -o "$B/chessnet.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -I$LIB/Crypto -c "$ROOT/src/ChessRating.cpp" -o "$B/chessrating.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ChessTour.cpp" -o "$B/chesstour.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ChessSync.cpp" -o "$B/chesssync.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Pet.cpp" -o "$B/pet.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Dice.cpp" -o "$B/dice.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -I$LIB/Crypto -c "$ROOT/src/Channels.cpp" -o "$B/channels.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -I$LIB/Crypto -c "$ROOT/src/Regions.cpp" -o "$B/regions.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Notifications.cpp" -o "$B/notifications.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ChannelPolicy.cpp" -o "$B/policy.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/QuickSend.cpp" -o "$B/quick.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/People.cpp" -o "$B/people.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Wardrive.cpp" -o "$B/wardrive.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/History.cpp" -o "$B/history.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/HistoryStorage.cpp" -o "$B/history-storage.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/HistoryReply.cpp" -o "$B/history-reply.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Clock.cpp" -o "$B/clock.o"
for f in SHA256 Hash Crypto;do eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "$LIB/Crypto/$f.cpp" -o "$B/crypto-$f.o";done
c++ "$B"/*.o -o "$B/preview"
# Several languages ("ru en de"): one build, a folder per language.
case "${3:-ru}" in
*" "*)for L in $3;do mkdir -p "$OUT/$L";"$B/preview" "$OUT/$L" $L >/dev/null;done;;
*)"$B/preview" "$OUT" ${3:-ru};;
esac
rm -rf "$B"
