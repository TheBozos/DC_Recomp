#!/usr/bin/env bash
# Read-only instruction census; retained retail symbols define the function map.
source "$(dirname "$0")/common.sh"
"$ROOT/scripts/recomp.sh" "$@"
"$GAME_BUILD/BozoEngine/recompiler/bozo-recomp$EXE" "$GAME_BUILD/dc_gen/dc.elf" --census
