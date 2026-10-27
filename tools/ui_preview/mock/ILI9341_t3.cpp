// SPDX-License-Identifier: GPL-3.0-or-later
// Glyph decoding for the framebuffer display (preview only), following
// ILI9341_t3::drawFontChar()/measureChar() by Paul Stoffregen.
#include "ILI9341_t3.h"

static uint32_t fetchbit(const uint8_t *p, uint32_t index) { return p[index >> 3] & (0x80 >> (index & 7)); }

static uint32_t fetchbits_unsigned(const uint8_t *p, uint32_t index, uint32_t required) {
  uint32_t val = 0;
  for (uint32_t i = 0; i < required; i++) val = (val << 1) | (fetchbit(p, index + i) ? 1 : 0);
  return val;
}

static int32_t fetchbits_signed(const uint8_t *p, uint32_t index, uint32_t required) {
  uint32_t val = fetchbits_unsigned(p, index, required);
  if (val & (1 << (required - 1))) return (int32_t)val - (1 << required);
  return (int32_t)val;
}

static const uint8_t *glyph(const ILI9341_t3_font_t *font, unsigned int c) {
  uint32_t bitoffset;
  if (c >= font->index1_first && c <= font->index1_last)
    bitoffset = (c - font->index1_first) * font->bits_index;
  else if (c >= font->index2_first && c <= font->index2_last)
    bitoffset = (c - font->index2_first + font->index1_last - font->index1_first + 1) * font->bits_index;
  else
    return nullptr;
  return font->data + fetchbits_unsigned(font->index, bitoffset, font->bits_index);
}

void ILI9341_t3::measureChar(uint8_t c, uint16_t *w, uint16_t *h) {
  *w = *h = 0;
  const uint8_t *data = glyph(font, c);
  if (!data || fetchbits_unsigned(data, 0, 3) != 0) return;
  uint32_t bitoffset = 3 + font->bits_width + font->bits_height + font->bits_xoffset + font->bits_yoffset;
  *w = fetchbits_unsigned(data, bitoffset, font->bits_delta);
  *h = font->cap_height;
}

uint16_t ILI9341_t3::measureTextWidth(const char *text, int num) {
  uint16_t total = 0, w, h;
  int n = num ? num : (int)strlen(text);
  for (int i = 0; i < n; i++) {
    measureChar(text[i], &w, &h);
    total += w;
  }
  return total;
}

void ILI9341_t3::drawFontChar(unsigned int c) {
  const uint8_t *data = glyph(font, c);
  if (!data || fetchbits_unsigned(data, 0, 3) != 0) return;
  uint32_t width = fetchbits_unsigned(data, 3, font->bits_width);
  uint32_t bitoffset = font->bits_width + 3;
  uint32_t height = fetchbits_unsigned(data, bitoffset, font->bits_height);
  bitoffset += font->bits_height;
  int32_t xoffset = fetchbits_signed(data, bitoffset, font->bits_xoffset);
  bitoffset += font->bits_xoffset;
  int32_t yoffset = fetchbits_signed(data, bitoffset, font->bits_yoffset);
  bitoffset += font->bits_yoffset;
  uint32_t delta = fetchbits_unsigned(data, bitoffset, font->bits_delta);
  bitoffset += font->bits_delta;

  int32_t origin_x = cursor_x + xoffset;
  cursor_x += delta;
  int32_t origin_y = cursor_y + font->cap_height - height - yoffset;

  int32_t linecount = height;
  int32_t y = origin_y;
  while (linecount > 0) {
    uint32_t b = fetchbit(data, bitoffset++);
    uint32_t n = 1;
    if (b) {
      n = fetchbits_unsigned(data, bitoffset, 3) + 2;
      bitoffset += 3;
    }
    for (uint32_t x = 0; x < width; x++) {
      if (fetchbit(data, bitoffset + x))
        for (uint32_t k = 0; k < n; k++) drawPixel(origin_x + x, y + k, textcolor);
    }
    bitoffset += width;
    y += n;
    linecount -= n;
  }
}
