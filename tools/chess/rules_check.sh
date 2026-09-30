#!/bin/sh
# Host check of the chess rules (perft, notation, endings): tools/chess/rules_check.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd);B=$(mktemp -d)
c++ -std=gnu++17 -O2 -Wall -Wextra -I"$ROOT/include" "$ROOT/src/Chess.cpp" "$ROOT/tools/chess/rules_check.cpp" -o "$B/rules"
"$B/rules";rc=$?;rm -rf "$B";exit $rc
