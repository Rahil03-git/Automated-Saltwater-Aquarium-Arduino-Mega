// calibration.h - pH and TDS calibration values stored in EEPROM.
#ifndef AQ_CALIBRATION_H
#define AQ_CALIBRATION_H

#include <stdint.h>

struct CalData {
  uint16_t magic;        // marks valid data
  float    phNeutralV;   // sensor voltage at pH 7
  float    phSlope;      // volts per pH unit (negative for the PH-4502C module)
  float    phOffset;     // fine offset added to the pH value
  float    tdsFactor;    // multiplier applied to the computed TDS
  uint16_t checksum;
};

namespace Calibration {
  void begin();                              // load from EEPROM or use defaults
  const CalData& get();
  void setPhNeutral(float voltageAtPh7);     // step 1: probe in pH 7 buffer
  bool setPhSlopePoint(float bufferPh, float voltage);  // step 2: probe in pH 4 or 9 buffer
  bool setTdsFactor(float knownPpm, float measuredPpm); // probe in a known TDS solution
  void resetDefaults();
  bool isStored();                           // true when values came from EEPROM
}

#endif
