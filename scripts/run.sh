#!/usr/bin/env bash
# The Bozo build has one native path; no emulation switches or fallback.
source "$(dirname "$0")/common.sh"
cd "$ROOT"
BIN="$GAME_BUILD/dc$EXE"
[ -x "$BIN" ] || { echo "missing $BIN; run scripts/build_runtime.sh first"; exit 1; }
exec "$BIN" "$@"
