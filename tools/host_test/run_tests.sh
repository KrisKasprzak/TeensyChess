#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds and runs the host tests for the engine with the portable MinGW GCC.
#   tools/host_test/run_tests.sh          debug build: asserts + UB traps, shallow perft
#   tools/host_test/run_tests.sh release  optimized build, deep perft
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ENGINE="$HERE/../../TeensyChess/src/engine"
GXX="$HERE/../w64devkit/bin/g++"
export PATH="$HERE/../w64devkit/bin:$PATH"

if [ "$1" = "release" ]; then
  FLAGS="-O2 -DNDEBUG"
  ARGS="deep"
else
  FLAGS="-O1 -g -DSF_DEBUG -fsanitize=undefined -fsanitize-trap=all"
  ARGS=""
fi

"$GXX" -std=gnu++17 -Wall -Wextra $FLAGS -I"$ENGINE" "$HERE/test.cpp" "$ENGINE"/*.cpp -o "$HERE/test.exe"
"$HERE/test.exe" $ARGS
