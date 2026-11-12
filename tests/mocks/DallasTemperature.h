#pragma once
#include "Arduino.h"
#include "OneWire.h"
#define DEVICE_DISCONNECTED_C -127
class DallasTemperature {
 public:
  explicit DallasTemperature(OneWire*) {}
  void begin() {}
  void setWaitForConversion(bool) {}
  void requestTemperatures() {}
  float getTempCByIndex(uint8_t) { return sim.tempConnected ? sim.tempC : (float)DEVICE_DISCONNECTED_C; }
};
