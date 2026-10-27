#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Command-line build of the TeensyChess sketch for a Teensy 4.0, following the
# Teensyduino 1.62 platform recipes (Optimize: "Faster", USB: Serial, 600 MHz).
# The Arduino IDE builds the same thing; this script exists to check that the
# sketch compiles, links and fits in memory without opening the IDE.
#
# Usage: tools/build.sh            (output in tools/build/)

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
SKETCH="$HERE/../TeensyChess"
OUT="$HERE/build"
TEENSY="$HOME/AppData/Local/Arduino15/packages/teensy"
BIN="$TEENSY/tools/teensy-compile/15.2.1/arm/bin"
CORE="$TEENSY/hardware/avr/1.62.0/cores/teensy4"
PLATLIB="$TEENSY/hardware/avr/1.62.0/libraries"
# Arduino sketchbook libraries folder (holding ILI9341_t3 and XPT2046_Touchscreen);
# override with: ARDUINO_LIBS=/path/to/libraries tools/build.sh
USERLIB="${ARDUINO_LIBS:-/c/Arduino_Sketches/libraries}"

LIBS=("$PLATLIB/SPI" "$PLATLIB/EEPROM" "$USERLIB/ILI9341_t3" "$USERLIB/XPT2046_Touchscreen")

CPU="-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16"
DEFS="-D__IMXRT1062__ -DTEENSYDUINO=162 -DARDUINO=10819 -DARDUINO_TEENSY40 -DF_CPU=600000000 -DUSB_SERIAL -DLAYOUT_US_ENGLISH"
COMMON="-O2 -g -Wall -ffunction-sections -fdata-sections -nostdlib -MMD"
CXXFLAGS="$COMMON -std=gnu++17 -fno-exceptions -fpermissive -fno-rtti -fno-threadsafe-statics -felide-constructors -Wno-error=narrowing -Wno-psabi -Wno-maybe-uninitialized"
INCS="-I$CORE"
for l in "${LIBS[@]}"; do INCS="$INCS -I$l"; done

mkdir -p "$OUT/core" "$OUT/libs" "$OUT/sketch"

compile() {  # source object [extra flags]
  local src="$1" obj="$2"
  if [ "$obj" -nt "$src" ] && [ -z "$FORCE" ]; then return; fi
  case "$src" in
    *.c) "$BIN/arm-none-eabi-gcc" -c $COMMON $CPU $DEFS $INCS "$src" -o "$obj" ;;
    *.S) "$BIN/arm-none-eabi-gcc" -c $COMMON -x assembler-with-cpp $CPU $DEFS $INCS "$src" -o "$obj" ;;
    *) "$BIN/arm-none-eabi-g++" -c $CXXFLAGS $CPU $DEFS $INCS $3 "$src" -o "$obj" ;;
  esac
}

# Teensy core -> core.a
if [ ! -f "$OUT/core.a" ]; then
  echo "Compiling Teensy core..."
  for f in "$CORE"/*.c "$CORE"/*.cpp "$CORE"/*.S; do
    [ -f "$f" ] || continue
    compile "$f" "$OUT/core/$(basename "$f").o"
  done
  "$BIN/arm-none-eabi-gcc-ar" rcs "$OUT/core.a" "$OUT"/core/*.o
fi

# Libraries
echo "Compiling libraries..."
LIBOBJS=()
for l in "${LIBS[@]}"; do
  for f in "$l"/*.c "$l"/*.cpp; do
    [ -f "$f" ] || continue
    o="$OUT/libs/$(basename "$l")_$(basename "$f").o"
    compile "$f" "$o"
    LIBOBJS+=("$o")
  done
done

# Sketch (the .ino becomes a .cpp with Arduino.h included, as the IDE does)
echo "Compiling sketch..."
SKOBJS=()
{ echo '#include <Arduino.h>'; echo "#line 1 \"$SKETCH/TeensyChess.ino\""; cat "$SKETCH/TeensyChess.ino"; } > "$OUT/sketch/TeensyChess.ino.cpp"
FORCE=1 compile "$OUT/sketch/TeensyChess.ino.cpp" "$OUT/sketch/TeensyChess.ino.o" "-I$SKETCH"
SKOBJS+=("$OUT/sketch/TeensyChess.ino.o")
for f in "$SKETCH"/*.cpp "$SKETCH"/src/engine/*.cpp; do
  o="$OUT/sketch/$(basename "$f").o"
  FORCE=1 compile "$f" "$o" "-I$SKETCH"
  SKOBJS+=("$o")
done

echo "Linking..."
"$BIN/arm-none-eabi-gcc" -O2 -Wl,--gc-sections,--relax,--no-warn-rwx-segments "-T$CORE/imxrt1062.ld" $CPU \
  -o "$OUT/TeensyChess.elf" "${SKOBJS[@]}" "${LIBOBJS[@]}" "$OUT/core.a" "-L$OUT" "-L$CORE" \
  -larm_cortexM7lfsp_math -lm -lstdc++
"$BIN/arm-none-eabi-objcopy" -O ihex -R .eeprom "$OUT/TeensyChess.elf" "$OUT/TeensyChess.hex"

"$BIN/arm-none-eabi-size" -A "$OUT/TeensyChess.elf" | grep -E "^\.(text|data|bss|ARM)"
TSIZE="$(ls -d "$TEENSY"/tools/teensy-tools/*/ | head -1)teensy_size.exe"
[ -f "$TSIZE" ] && "$TSIZE" "$OUT/TeensyChess.elf"
echo "Build OK: $OUT/TeensyChess.hex"
