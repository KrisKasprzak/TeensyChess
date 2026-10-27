// SPDX-License-Identifier: GPL-3.0-or-later
// No-op EEPROM (preview only)
#pragma once

struct MockEEPROM {
  template <typename T>
  T &get(int, T &t) {
    memset((void *)&t, 0, sizeof(T));
    return t;
  }
  template <typename T>
  const T &put(int, const T &t) {
    return t;
  }
};
static MockEEPROM EEPROM;
