// SPDX-License-Identifier: GPL-3.0-or-later
// Scripted stand-in for the XPT2046 touch controller (preview only). Each queued
// tap is a short release gap followed by a press, counted in touched() calls.
// Before the first press it calls a hook so the preview can save the screen.
#pragma once

#include "Arduino.h"

class TS_Point {
 public:
  TS_Point() : x(0), y(0), z(0) {}
  TS_Point(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
  int16_t x, y, z;
};

class XPT2046_Touchscreen {
 public:
  XPT2046_Touchscreen(uint8_t, uint8_t = 255) {}
  bool begin() { return true; }
  void setRotation(uint8_t) {}

  static const int GAP = 3, PRESS = 6;  // touched() calls released, then pressed

  // Queue of taps in screen coordinates (the preview uses an identity calibration)
  int tapX[8], tapY[8], tapCount = 0, tapIndex = 0;
  int phase = 0;
  void (*beforeTap)() = nullptr;

  bool touched() {
    if (tapIndex >= tapCount) return false;
    int p = phase++;
    if (p < GAP) return false;
    if (p == GAP && beforeTap) beforeTap();
    if (p < GAP + PRESS) return true;
    phase = 1;  // this call is the first released read of the next tap's gap
    tapIndex++;
    return false;
  }
  TS_Point getPoint() {
    int i = tapIndex < tapCount ? tapIndex : tapCount - 1;
    return TS_Point(tapX[i], tapY[i], 500);
  }
};
