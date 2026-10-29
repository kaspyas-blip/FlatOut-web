#!/bin/bash
# Makes the FlatOutWeb folder that goes INTO your FlatOut install folder
# (next to FlatOut.exe): start.bat starts the local server and opens the game.
set -e
cd "$(dirname "$0")/.."
source ./fo_env.sh
OUT="${1:-$FO_WORK/FlatOutWeb}"
[ -f "$FO_WORK/flatout.wasm.gz" ] && [ -d web/dist ] || { echo "run ./flatout/build.sh first"; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT"
cp -r web/dist "$OUT/dist"; rm -rf "$OUT/dist/test"
cp web/fo-server.mjs "$OUT/server.mjs"
cp "$FO_WORK/flatout.wasm.gz" "$OUT/"
cp flatout/windows/*.bat flatout/windows/CTI-ME.txt "$OUT/"
echo "packed: $OUT  (copy it into your FlatOut folder, run start.bat)"
