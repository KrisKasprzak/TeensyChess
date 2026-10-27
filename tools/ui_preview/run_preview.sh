#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Renders the Teensy Chess screens to PNG files in tools/ui_preview/out
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SKETCH="$HERE/../../TeensyChess"
LIB="${ARDUINO_LIBS:-/c/Arduino_Sketches/libraries}/ILI9341_t3"  # override with ARDUINO_LIBS=...
export PATH="$HERE/../w64devkit/bin:$PATH"
BUILD="$HERE/build"
mkdir -p "$BUILD" "$HERE/out"

# The font files include "ILI9341_t3.h" with quotes, so they are copied next to
# the mock header to pick it up instead of the real driver.
cp "$HERE"/mock/* "$BUILD"/
cp "$LIB"/font_Arial.c "$LIB"/font_Arial.h "$LIB"/font_ArialBold.c "$LIB"/font_ArialBold.h "$BUILD"/

gcc -c -O1 -I"$BUILD" "$BUILD/font_Arial.c" -o "$BUILD/font_Arial.o"
gcc -c -O1 -I"$BUILD" "$BUILD/font_ArialBold.c" -o "$BUILD/font_ArialBold.o"
g++ -std=gnu++17 -O1 -Wall -I"$BUILD" -I"$SKETCH" \
  "$HERE/preview.cpp" "$BUILD/ILI9341_t3.cpp" \
  "$SKETCH/ui.cpp" "$SKETCH/game.cpp" "$SKETCH/touch_input.cpp" "$SKETCH/storage.cpp" \
  "$SKETCH"/src/engine/*.cpp "$BUILD/font_Arial.o" "$BUILD/font_ArialBold.o" -o "$BUILD/preview.exe"
"$BUILD/preview.exe" "$HERE/out"
