// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - settings saved in the Teensy's EEPROM (emulated in flash):
// touch calibration, strength level and the side the player plays.

#pragma once

#include <Arduino.h>

struct Settings {
  // Raw XPT2046 readings at the left/right/top/bottom screen edges. A panel that
  // counts backwards just gets left > right (or top > bottom) and map() handles it.
  int16_t calLeft = 300, calRight = 3800, calTop = 300, calBottom = 3800;
  uint8_t level = 10;        // 0..20
  uint8_t humanWhite = 1;    // 1 = player has white, 0 = player has black
};

extern Settings settings;

// Load from EEPROM. Returns false (and keeps the defaults) if nothing valid is saved.
bool loadSettings();

// Save to EEPROM (only writes the bytes that changed)
void saveSettings();

// True when the calibration values span a plausible part of the 0..4095 range
bool calibrationValid();
