#!/usr/bin/env bash
# Both overlays are part of the same dependency-tracked native generation target.
exec "$(dirname "$0")/recomp.sh" "$@"
