#pragma once
#include "Arduino.h"
class Servo {
  bool on = false;
 public:
  void attach(int) { on = true; sim.servoAttachCount++; sim.servoLog += "A,"; }
  void write(int angle) { char b[16]; snprintf(b, 16, "W%d,", angle); sim.servoLog += b; }
  void detach() { on = false; sim.servoLog += "D,"; }
  bool attached() const { return on; }
};
