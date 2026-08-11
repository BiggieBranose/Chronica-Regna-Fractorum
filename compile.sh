#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf "$SCRIPT_DIR/build"

cmake -S "$SCRIPT_DIR/Code" -B "$SCRIPT_DIR/build" -G Ninja
cmake --build "$SCRIPT_DIR/build"

cd "$SCRIPT_DIR/build"
./crf_game "$@"
