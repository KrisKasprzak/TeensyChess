// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - configuration: wiring, colors, screen layout and engine settings

#pragma once

#include <Arduino.h>

// ===========================
// Wiring (Teensy 4.0)
// ===========================
// 2.8" ILI9341 SPI display (320x240) with XPT2046 touch controller.
// Display and touch share the hardware SPI bus:
//   MOSI (SDI, T_DIN) -> 11, MISO (SDO, T_DO) -> 12, SCK (T_CLK) -> 13
//   Display: CS -> 10, DC -> 9, RESET -> 8, LED -> 3.3V, VCC -> 3.3V
//   Touch:   T_CS -> 0, T_IRQ -> not connected (the touch panel is polled)
#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST    8
#define TOUCH_CS  0

// Buzzer (+ -> pin 3, - -> GND), driven with PWM: the duty cycle sets the volume
#define BUZZER_PIN       3
#define CLICK_FREQ_HZ    500    // pitch of the click
#define CLICK_MS         10     // length of the click
#define CLICK_VOLUME     15     // PWM duty 0..255 (higher is louder, 128 is loudest)
#define WARN_FREQ_HZ     200    // pitch of the illegal-tap warning (lower than the click)
#define WARN_MS          120    // length of the warning beep
#define WARN_VOLUME      15     // PWM duty 0..255 (higher is louder, 128 is loudest)

// ===========================
// Screen layout
// ===========================
#define SCREEN_W     320
#define SCREEN_H     240
#define SQUARE_SIZE  30                  // 8 x 30 = 240 px board on the left
#define BOARD_PX     (8 * SQUARE_SIZE)
#define PANEL_X      BOARD_PX           // info panel on the right
#define PANEL_W      (SCREEN_W - BOARD_PX)

// Info panel sections (y coordinates)
#define PANEL_HEADER_H   20
#define PANEL_STATUS_Y   22
#define PANEL_POINTS_Y   56
#define PANEL_HINT_Y     134
#define PANEL_BUTTON_X   (PANEL_X + 4)
#define PANEL_BUTTON_W   (PANEL_W - 8)
#define PANEL_BUTTON_H   22
#define PANEL_BUTTON_Y0  164             // HINT, UNDO and NEW buttons below each other
#define PANEL_BUTTON_GAP 3

// ===========================
// Colors (RGB565)
// ===========================
#define RGB565(r, g, b) uint16_t((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define COLOR_LIGHT_SQUARE   RGB565(192, 192, 192)  // light grey
#define COLOR_DARK_SQUARE    RGB565(60, 100, 200)   // medium blue
#define COLOR_WHITE_PIECE    RGB565(255, 255, 255)
#define COLOR_WHITE_OUTLINE  RGB565(0, 0, 0)
#define COLOR_BLACK_PIECE    RGB565(0, 0, 0)
#define COLOR_BLACK_OUTLINE  RGB565(150, 150, 150)  // lets black pieces show their shape

#define COLOR_SELECTED       RGB565(255, 165, 0)    // orange: the piece the user picked up
#define COLOR_CHECK          RGB565(220, 60, 60)    // king in check
#define COLOR_HINT_LIGHT     RGB565(130, 215, 120)  // hint move squares
#define COLOR_HINT_DARK      RGB565(70, 170, 90)
#define COLOR_LAST_LIGHT     RGB565(230, 220, 120)  // the engine's last move
#define COLOR_LAST_DARK      RGB565(150, 160, 120)
#define COLOR_TARGET_DOT     RGB565(40, 40, 40)     // legal destinations of the selected piece

#define COLOR_PANEL_BG       RGB565(24, 28, 40)
#define COLOR_PANEL_HEADER   RGB565(45, 70, 140)
#define COLOR_PANEL_TEXT     RGB565(235, 235, 235)
#define COLOR_PANEL_MUTED    RGB565(140, 145, 160)
#define COLOR_PANEL_ACCENT   RGB565(255, 165, 0)
#define COLOR_PANEL_LINE     RGB565(60, 66, 85)
#define COLOR_BUTTON         RGB565(55, 75, 125)
#define COLOR_BUTTON_PRESSED RGB565(255, 165, 0)
#define COLOR_BUTTON_OFF     RGB565(45, 48, 58)
#define COLOR_BUTTON_TEXT    RGB565(255, 255, 255)
#define COLOR_GOOD           RGB565(90, 210, 110)
#define COLOR_BAD            RGB565(240, 90, 90)

// Show small dots on the squares the selected piece can move to (0 to disable)
#define SHOW_LEGAL_MOVES 1

// ===========================
// Engine
// ===========================
// Engine name shown on the display
#define ENGINE_NAME       "TeensyFish"
#define ENGINE_SHORT      "TF"           // used before the engine's last move, e.g. "TF: Nf6"

// Strength level 0..20 as in Stockfish's "Skill Level" option. 20 is full strength;
// lower levels search shallower and sometimes pick a weaker move on purpose.
#define DEFAULT_LEVEL     10
#define MAX_LEVEL         20

// Thinking time per move in milliseconds for a given level (no game clock is used)
#define ENGINE_TIME_MS(level) (500 + (level) * 125)

// Thinking time for the HINT button (always full strength)
#define HINT_TIME_MS      2000

// ===========================
// Touch
// ===========================
#define CAL_MARGIN        20    // calibration crosshairs are drawn this far in from the corners
#define CAL_WINDOW_MS     3000  // time the splash screen waits for a tap to start calibration
