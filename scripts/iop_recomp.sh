#!/usr/bin/env bash
# Statically compile the five sound libraries; pad/card are native SDK services.
source "$(dirname "$0")/common.sh"
configure_game "$@"
cmake --build "$GAME_BUILD" --target dc_native_audio -j "$JOBS"
