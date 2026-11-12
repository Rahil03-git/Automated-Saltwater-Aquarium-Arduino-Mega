#include <Arduino.h>
#include <stdio.h>
#include "config.h"
#include "aqclock.h"

namespace {
uint32_t secondOfDay = 0;
uint32_t days = 0;
uint32_t fracMs = 0;      // accumulated (scaled) milliseconds below one second
uint32_t lastMs = 0;
bool rolled = false;
}

namespace AquaClock {

void begin(uint16_t startMinuteOfDay) {
  secondOfDay = (uint32_t)(startMinuteOfDay % 1440) * 60UL;
  days = 0;
  fracMs = 0;
  rolled = false;
  lastMs = millis();
}

void update() {
  uint32_t now = millis();
  uint32_t dt = now - lastMs;            // wrap-safe
  lastMs = now;
  fracMs += dt * (uint32_t)TIME_SCALE;
  while (fracMs >= 1000UL) {
    fracMs -= 1000UL;
    secondOfDay++;
    if (secondOfDay >= 86400UL) {
      secondOfDay = 0;
      days++;
      rolled = true;
    }
  }
}

uint16_t minuteOfDay() { return (uint16_t)(secondOfDay / 60UL); }
uint32_t dayCount() { return days; }

void setTime(uint8_t hour, uint8_t minute) {
  secondOfDay = ((uint32_t)(hour % 24) * 60UL + (minute % 60)) * 60UL;
  fracMs = 0;
  lastMs = millis();
}

bool consumeDayRollover() {
  bool r = rolled;
  rolled = false;
  return r;
}

void format(char* out6) {
  uint16_t m = minuteOfDay();
  unsigned hh = (unsigned)((m / 60) % 24);
  unsigned mm = (unsigned)(m % 60);
  snprintf(out6, 6, "%02u:%02u", hh, mm);
}

}  // namespace AquaClock
