#!/usr/bin/env bash
# Prepare and statically recompile the resident ELF and both guarded overlays.
source "$(dirname "$0")/common.sh"
configure_game "$@"
cmake --build "$GAME_BUILD" --target dc_generate -j "$JOBS"
