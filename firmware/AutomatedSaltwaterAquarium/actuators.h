// actuators.h - fish feeder (servo), LED lighting, water pump, buzzer, optional heater and dosing.
#ifndef AQ_ACTUATORS_H
#define AQ_ACTUATORS_H

#include <stdint.h>
#include "logic.h"

namespace Feeder {                    // servo-motor feeder
  void begin();
  bool request();                     // start one portion; false if already busy
  void update();                      // non-blocking state machine
  bool busy();
}

namespace Light {                     // PWM LED lighting
  void begin();
  void setPercent(uint8_t percent);
  uint8_t percent();
}

namespace Pump {                      // water pump through a relay module
  void begin();
  void set(bool on);
  bool isOn();
}

namespace Buzzer {                    // buzzer alert on A11
  void begin();
  void setAlarm(bool active);         // beeping pattern while true
  void chirp();                       // short confirmation beep
  void update();
}

namespace Heater {                    // OPTIONAL (ENABLE_HEATER_RELAY)
  void begin();
  void update(const aq::Readings& r);
  bool isOn();
}

namespace Dosing {                    // OPTIONAL (ENABLE_PH_DOSING)
  void begin();
  void update(const aq::Readings& r, uint32_t absoluteMinute);
  void newDay();
}

#endif
