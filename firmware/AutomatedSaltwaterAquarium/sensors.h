// sensors.h - DS18B20 temperature, pH-4502C, TDS probe and HC-SR04 water level.
// All sensors are read without blocking, so call Sensors::update() on every loop pass.
#ifndef AQ_SENSORS_H
#define AQ_SENSORS_H

#include "logic.h"

namespace Sensors {
  void begin();
  void update();
  const aq::Readings& readings();
}

#endif
