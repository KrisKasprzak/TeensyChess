// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - touch input: calibration and tap detection.
// The calibration follows the two-point method of the ESP32_VideoCamera project.

#include "touch_input.h"

#include <font_Arial.h>
#include <font_ArialBold.h>

#include "config.h"
#include "hardware.h"
#include "storage.h"

namespace {

bool wasPressed = false;  // getTap() edge detection

// Print text centered horizontally at baseline-ish y
void printCentered(const char *text, int y, const ILI9341_t3_font_t &font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);
  int w = tft.measureTextWidth(text);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.print(text);
}

// Calibration screen text: title, instruction, and a small step line, centered
void drawCalibrationText(const char *title, const char *line, const char *step) {
  tft.fillScreen(COLOR_PANEL_BG);
  printCentered(title, 88, Arial_14_Bold, COLOR_PANEL_ACCENT);
  tft.drawFastHLine(70, 108, tft.width() - 140, COLOR_PANEL_LINE);
  printCentered(line, 120, Arial_12, COLOR_PANEL_TEXT);
  if (step[0]) printCentered(step, 145, Arial_10, COLOR_PANEL_MUTED);
}

// Draw a crosshair at (X, Y), wait for a press, and return the averaged raw touch reading
void getTouchTarget(int X, int Y, long &rawX, long &rawY) {
  tft.drawCircle(X, Y, 9, COLOR_PANEL_ACCENT);
  tft.drawCircle(X, Y, 10, COLOR_PANEL_ACCENT);
  tft.drawFastHLine(X - 16, Y, 33, COLOR_PANEL_TEXT);
  tft.drawFastVLine(X, Y - 16, 33, COLOR_PANEL_TEXT);
  tft.fillCircle(X, Y, 2, COLOR_BAD);

  // wait for any current press (like the one that started calibration) to end, then a new one
  while (touch.touched()) delay(10);
  delay(200);
  while (!touch.touched()) delay(10);
  delay(50);  // let the reading settle

  long sx = 0, sy = 0;
  int samples = 0;
  while (touch.touched() && samples < 32) {
    TS_Point p = touch.getPoint();
    sx += p.x;
    sy += p.y;
    samples++;
    delay(10);
  }
  if (samples == 0) samples = 1;
  rawX = sx / samples;
  rawY = sy / samples;

  tft.fillCircle(X, Y, 10, COLOR_GOOD);  // confirm the tap
  while (touch.touched()) delay(10);
  delay(200);
}

// Convert a raw touch reading to screen coordinates using the calibration
void mapTouch(const TS_Point &p, int &x, int &y) {
  x = constrain(map(p.x, settings.calLeft, settings.calRight, 0, tft.width() - 1), 0, tft.width() - 1);
  y = constrain(map(p.y, settings.calTop, settings.calBottom, 0, tft.height() - 1), 0, tft.height() - 1);
}

}  // namespace

bool screenPressed() {
  if (!touch.touched()) return false;
  delay(20);
  return touch.touched();
}

void touchPosition(int &x, int &y) {
  TS_Point p = touch.getPoint();
  mapTouch(p, x, y);
}

bool getTap(int &x, int &y) {
  bool pressed = touch.touched();
  if (!pressed) {
    wasPressed = false;
    return false;
  }
  if (wasPressed) return false;  // still the same press

  // New press: confirm it is not noise, then report where it started
  delay(20);
  if (!touch.touched()) return false;
  wasPressed = true;
  touchPosition(x, y);
  return true;
}

void waitForRelease() {
  unsigned long t0 = millis();
  while (touch.touched() && millis() - t0 < 3000) delay(20);
  wasPressed = touch.touched();
}

bool calibrationRequested(int barY) {
  const int bx = 60, bw = tft.width() - 120;
  tft.fillRoundRect(bx, barY, bw, 4, 2, COLOR_PANEL_ACCENT);
  unsigned long t0 = millis();
  while (millis() - t0 < CAL_WINDOW_MS) {
    if (screenPressed()) return true;
    int left = bw - bw * (millis() - t0) / CAL_WINDOW_MS;
    tft.fillRect(bx + left, barY, bw - left, 4, COLOR_PANEL_BG);
    delay(20);
  }
  return false;
}

void runTouchCalibration() {
  const int W = tft.width(), H = tft.height();
  long x1, y1, x2, y2;
  Settings old = settings;

  Serial.println("Touch calibration started");
  drawCalibrationText("Touch calibration", "Tap the crosshair", "Step 1 of 2");
  getTouchTarget(CAL_MARGIN, CAL_MARGIN, x1, y1);
  drawCalibrationText("Touch calibration", "Tap the crosshair", "Step 2 of 2");
  getTouchTarget(W - CAL_MARGIN, H - CAL_MARGIN, x2, y2);

  // raw counts per pixel times the margin gives the raw value at the true edge
  settings.calLeft = constrain(x1 - ((x2 - x1) * CAL_MARGIN) / (W - 2 * CAL_MARGIN), 0, 4095);
  settings.calRight = constrain(x2 + ((x2 - x1) * CAL_MARGIN) / (W - 2 * CAL_MARGIN), 0, 4095);
  settings.calTop = constrain(y1 - ((y2 - y1) * CAL_MARGIN) / (H - 2 * CAL_MARGIN), 0, 4095);
  settings.calBottom = constrain(y2 + ((y2 - y1) * CAL_MARGIN) / (H - 2 * CAL_MARGIN), 0, 4095);
  Serial.printf("Touch calibration: L %d  R %d  T %d  B %d\n", settings.calLeft, settings.calRight,
                settings.calTop, settings.calBottom);

  // Let the user check it: dots should follow the finger. A progress bar counts down 6 s.
  drawCalibrationText("Draw to test", "Dots follow your finger", "");
  const unsigned long TEST_MS = 6000;
  const int bx = 60, bw = W - 120, by = 190;
  tft.fillRoundRect(bx, by, bw, 4, 2, COLOR_PANEL_ACCENT);
  unsigned long t0 = millis();
  while (millis() - t0 < TEST_MS) {
    if (touch.touched()) {
      int x, y;
      mapTouch(touch.getPoint(), x, y);
      tft.fillCircle(x, y, 3, COLOR_PANEL_ACCENT);
    }
    int left = bw - bw * (millis() - t0) / TEST_MS;
    tft.fillRect(bx + left, by, bw - left, 4, COLOR_PANEL_BG);
    delay(10);
  }

  if (calibrationValid()) {
    saveSettings();
    drawCalibrationText("Calibration saved", "", "");
  } else {
    // taps were too close together: go back to the previous values
    settings.calLeft = old.calLeft;
    settings.calRight = old.calRight;
    settings.calTop = old.calTop;
    settings.calBottom = old.calBottom;
    Serial.println("Touch calibration rejected, not saved");
    drawCalibrationText("Calibration failed", "Not saved", "Restart and tap splash to retry");
  }
  delay(1500);
  waitForRelease();
}
