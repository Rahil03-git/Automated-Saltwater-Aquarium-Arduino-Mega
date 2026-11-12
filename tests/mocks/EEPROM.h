#pragma once
#include "Arduino.h"
struct EEPROMClass {
  template <typename T> T& get(int idx, T& t) { memcpy(&t, sim_eeprom + idx, sizeof(T)); return t; }
  template <typename T> const T& put(int idx, const T& t) { memcpy(sim_eeprom + idx, &t, sizeof(T)); return t; }
};
static EEPROMClass EEPROM;
