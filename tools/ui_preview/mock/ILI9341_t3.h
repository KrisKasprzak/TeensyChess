// SPDX-License-Identifier: GPL-3.0-or-later
// Framebuffer stand-in for the ILI9341_t3 display driver (preview only). The font
// structure and glyph decoding follow ILI9341_t3 by Paul Stoffregen so that text
// is laid out exactly as on the real display.
#pragma once

#include <stdint.h>

typedef struct {
  const unsigned char *index;
  const unsigned char *unicode;
  const unsigned char *data;
  unsigned char version;
  unsigned char reserved;
  unsigned char index1_first;
  unsigned char index1_last;
  unsigned char index2_first;
  unsigned char index2_last;
  unsigned char bits_index;
  unsigned char bits_width;
  unsigned char bits_height;
  unsigned char bits_xoffset;
  unsigned char bits_yoffset;
  unsigned char bits_delta;
  unsigned char line_space;
  unsigned char cap_height;
} ILI9341_t3_font_t;

#ifdef __cplusplus

#include "Arduino.h"

#define ILI9341_BLACK 0x0000
#define ILI9341_WHITE 0xFFFF
#define ILI9341_ORANGE 0xFD20

class ILI9341_t3 {
 public:
  ILI9341_t3(uint8_t, uint8_t) {}
  void begin() {}
  void setRotation(uint8_t) {}
  int16_t width() const { return W; }
  int16_t height() const { return H; }

  void drawPixel(int16_t x, int16_t y, uint16_t c) {
    if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c;
  }
  void fillScreen(uint16_t c) { fillRect(0, 0, W, H, c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    drawFastHLine(x, y, w, c);
    drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c);
    drawFastVLine(x + w - 1, y, h, c);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) {
        int dx = i < r ? r - i : i >= w - r ? i - (w - r - 1) : 0;
        int dy = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0;
        if (dx * dx + dy * dy <= r * r) drawPixel(x + i, y + j, c);
      }
  }
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) {
    for (int j = -r; j <= r; j++)
      for (int i = -r; i <= r; i++) {
        int d = i * i + j * j;
        if (d <= r * r && d > (r - 1) * (r - 1)) drawPixel(x0 + i, y0 + j, c);
      }
  }
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) {
    for (int j = -r; j <= r; j++)
      for (int i = -r; i <= r; i++)
        if (i * i + j * j <= r * r) drawPixel(x0 + i, y0 + j, c);
  }
  void writeRect(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) drawPixel(x + i, y + j, p[j * w + i]);
  }

  void setFont(const ILI9341_t3_font_t &f) { font = &f; }
  void setTextColor(uint16_t c) { textcolor = c; }
  void setCursor(int16_t x, int16_t y) {
    cursor_x = x;
    cursor_y = y;
  }
  void print(const char *s) {
    while (*s) drawFontChar((unsigned char)*s++);
  }
  void print(int v) {
    char b[16];
    snprintf(b, sizeof(b), "%d", v);
    print(b);
  }
  uint16_t measureTextWidth(const char *text, int num = 0);
  void measureChar(uint8_t c, uint16_t *w, uint16_t *h);

  static const int W = 320, H = 240;
  uint16_t fb[W * H];

 private:
  void drawFontChar(unsigned int c);
  const ILI9341_t3_font_t *font = nullptr;
  uint16_t textcolor = 0xFFFF;
  int16_t cursor_x = 0, cursor_y = 0;
};

#endif
