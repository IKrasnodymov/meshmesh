#!/bin/sh
# The interactive UI simulator, rebuilt and restarted whenever src/, include/ or i18n/ change:
#   tools/ui_preview/sim.sh [m9|tdeck|heltec|gat562|t114] [LANG]
# Without fswatch (brew install fswatch) it builds and runs once. Window keys: see tools/ui_preview/live.inc.
# Needs the board libraries in .pio/libdeps/m9 (pio pkg install -e m9) and SDL2 (brew install sdl2).
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BOARD=${1:-m9};LANG_CODE=${2:-en};BIN=$ROOT/.pio/ui-sim-$BOARD
mkdir -p "$ROOT/.pio"
build(){ SIM=1 NORUN=1 KEEP="$BIN" sh "$ROOT/tools/ui_preview/build.sh" "$BOARD" - "$LANG_CODE"; }
if ! command -v fswatch >/dev/null;then build&&exec "$BIN" --live "$LANG_CODE";fi
PID=
start(){ if build;then "$BIN" --live "$LANG_CODE" & PID=$!;else echo "build failed: fix and save again";fi; }
trap '[ -n "$PID" ]&&kill $PID 2>/dev/null;exit 0' INT TERM
start
fswatch -o -l 0.5 "$ROOT/src" "$ROOT/include" "$ROOT/i18n" "$ROOT/tools/ui_preview" | while read -r _;do
 [ -n "$PID" ]&&kill $PID 2>/dev/null;wait $PID 2>/dev/null;PID=;echo "-- rebuilding";start
done
