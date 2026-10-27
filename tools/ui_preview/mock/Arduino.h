// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal Arduino API for rendering the Teensy Chess UI on a PC (preview only)
#pragma once

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PROGMEM
#define OUTPUT 1
#define HIGH 1
#define LOW 0
typedef bool boolean;

inline void delay(unsigned long) {}
inline unsigned long millis() {
  static unsigned long t = 0;
  return t += 50;  // time advances quickly so countdown loops finish
}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) {
  return x < lo ? T(lo) : x > hi ? T(hi) : x;
}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

struct MockSerial {
  void begin(long) {}
  explicit operator bool() const { return true; }
  int available() { return 0; }
  int read() { return -1; }
  void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  void println(const char *s) { ::printf("%s\n", s); }
  void print(const char *s) { ::printf("%s", s); }
};
extern MockSerial Serial;
