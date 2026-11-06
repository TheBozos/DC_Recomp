#!/usr/bin/env bash
source "$(dirname "$0")/common.sh"
mkdir -p "$ROOT/build"
extra=()
[ ! -f "$GAME_BUILD/dc_gen/dc.elf" ] || extra+=("-DBOZO_TEST_ELF=$GAME_BUILD/dc_gen/dc.elf")
cmake -S "$ROOT/BozoEngine" -B "$TOOLS_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON -DBOZO_BUILD_TOOLS=OFF "${extra[@]}" "$@"
cmake --build "$TOOLS_BUILD" -j "$JOBS"
ctest --test-dir "$TOOLS_BUILD" --output-on-failure
