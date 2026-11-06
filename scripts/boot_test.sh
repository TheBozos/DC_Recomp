#!/usr/bin/env bash
# Capture a bounded native run; additional arguments go to dc.
# scripts/boot_test.sh <name> [seconds=60] [capture interval=5] [runner arguments...]
source "$(dirname "$0")/common.sh"
NAME="${1:?name}"; SECS="${2:-60}"; INTERVAL="${3:-5}"
[[ "$NAME" =~ ^[a-zA-Z0-9_-]+$ && "$SECS" =~ ^[1-9][0-9]*$ ]] || { echo "invalid name or duration"; exit 2; }
shift "$(( $# < 3 ? $# : 3 ))"
OUT="$ROOT/build/boots/$NAME"
mkdir -p "$OUT"
native() { command -v cygpath > /dev/null && cygpath -m "$1" || echo "$1"; }
export PS2X_CAPTURE_DIR="$(native "$OUT")" PS2X_CAPTURE_INTERVAL="$INTERVAL"
"$ROOT/scripts/run.sh" --frames "$(( SECS * 60 ))" "$@" > "$OUT/run.log" 2>&1
printf 'Native capture: %s\n' "$OUT"
