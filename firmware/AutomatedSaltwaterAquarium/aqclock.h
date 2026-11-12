// aqclock.h - software day clock (no RTC in the original parts list).
#ifndef AQ_CLOCK_H
#define AQ_CLOCK_H

#include <stdint.h>

namespace AquaClock {
  void begin(uint16_t startMinuteOfDay);
  void update();                       // call every loop pass
  uint16_t minuteOfDay();              // 0 .. 1439
  uint32_t dayCount();                 // days since power-up
  void setTime(uint8_t hour, uint8_t minute);
  bool consumeDayRollover();           // true once after each midnight
  void format(char* out6);             // writes "HH:MM" (needs 6 bytes)
}

#endif
