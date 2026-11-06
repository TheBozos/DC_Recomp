#!/usr/bin/env bash
# Compatibility command for existing contributors; tests the current engine.
exec "$(dirname "$0")/engine_tests.sh" "$@"
