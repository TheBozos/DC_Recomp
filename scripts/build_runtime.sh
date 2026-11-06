#!/usr/bin/env bash
# Build the native game, headless runner, sound libraries and converted textures.
source "$(dirname "$0")/common.sh"
BUILD_TYPE="${1:-Release}"
[ $# -eq 0 ] || shift
configure_game "$@"
cmake --build "$GAME_BUILD" -j "$JOBS"
