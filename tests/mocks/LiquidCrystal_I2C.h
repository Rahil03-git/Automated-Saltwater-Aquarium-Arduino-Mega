#pragma once
#include "Arduino.h"
class LiquidCrystal_I2C {
  uint8_t col = 0, row = 0;
 public:
  LiquidCrystal_I2C(uint8_t, uint8_t, uint8_t) {}
  void init() {}
  void backlight() {}
  void clear() { for (int r = 0; r < 2; r++) memset(sim.lcd[r], ' ', 16); }
  void setCursor(uint8_t c, uint8_t r) { col = c; row = r; }
  void print(const char* s) { while (*s && col < 16 && row < 2) sim.lcd[row][col++] = *s++; }
};
