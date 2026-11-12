// display.h - 16x2 I2C LCD with automatic pages and an alarm screen.
#ifndef AQ_DISPLAY_H
#define AQ_DISPLAY_H

#include <stdint.h>
#include "logic.h"

struct UiState {
  aq::Readings r;
  uint16_t alarms;        // active alarm bits
  bool     muted;
  uint8_t  lightPct;
  uint8_t  lightMode;     // 0 AUTO, 1 ON, 2 OFF
  bool     pumpOn;
  uint8_t  pumpState;     // 0 idle, 1 filling, 2 cooldown, 3 fault
  uint8_t  feedsToday;
  uint16_t minuteOfDay;
};

namespace Display {
  void begin();                                   // shows a splash screen
  void update(const UiState& s, uint32_t now);    // call every loop pass
  void nextPage();
}

#endif
