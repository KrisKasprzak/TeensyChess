// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - touch input: calibration and tap detection

#pragma once

#include <Arduino.h>

// True when the screen is being pressed (two readings 20 ms apart, to ignore noise)
bool screenPressed();

// Screen position of the current touch, using the calibration
void touchPosition(int &x, int &y);

// Returns true once per press, with the screen position where the press started.
// Call it often from loop(); it never blocks.
bool getTap(int &x, int &y);

// Wait (up to 3 s) for the finger to lift so one tap doesn't trigger twice
void waitForRelease();

// On the splash screen, give the user a few seconds to tap for touch calibration.
// barY is where the countdown bar is drawn.
bool calibrationRequested(int barY);

// Tap crosshairs near the top-left and bottom-right corners, work out the raw reading
// at each screen edge, let the user test it by drawing, then save it to EEPROM
void runTouchCalibration();
