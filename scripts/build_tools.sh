#!/usr/bin/env bash
# Build BozoEngine's static recompiler and native-library binder.
source "$(dirname "$0")/common.sh"
cmake -S "$ROOT/BozoEngine" -B "$TOOLS_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBOZO_BUILD_TOOLS=OFF "$@"
cmake --build "$TOOLS_BUILD" --target bozo-recomp bozo-externals -j "$JOBS"
