#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include "config.h"
#include "calibration.h"

namespace {
const uint16_t kMagic = 0xA9C5;
const int kAddress = 0;
CalData data;
bool stored = false;

void mix(uint16_t& sum, const void* p, size_t n) {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
  for (size_t i = 0; i < n; i++) sum = (uint16_t)(sum * 31u + b[i]);
}

// Checksum over the data fields only (not the checksum itself, not struct padding).
uint16_t checksumOf(const CalData& d) {
  uint16_t sum = 7;
  mix(sum, &d.magic, sizeof(d.magic));
  mix(sum, &d.phNeutralV, sizeof(d.phNeutralV));
  mix(sum, &d.phSlope, sizeof(d.phSlope));
  mix(sum, &d.phOffset, sizeof(d.phOffset));
  mix(sum, &d.tdsFactor, sizeof(d.tdsFactor));
  return sum;
}

void defaults() {
  data.magic = kMagic;
  data.phNeutralV = PH_NEUTRAL_VOLTAGE;
  data.phSlope = PH_SLOPE_V_PER_PH;
  data.phOffset = PH_OFFSET;
  data.tdsFactor = 1.0f;
  data.checksum = 0;
}

void save() {
  data.magic = kMagic;
  data.checksum = checksumOf(data);
  EEPROM.put(kAddress, data);
  stored = true;
}
}  // namespace

namespace Calibration {

void begin() {
  CalData tmp;
  EEPROM.get(kAddress, tmp);
  if (tmp.magic == kMagic && tmp.checksum == checksumOf(tmp) &&
      tmp.phSlope != 0.0f && tmp.tdsFactor > 0.0f) {
    data = tmp;
    stored = true;
  } else {
    defaults();
    stored = false;
  }
}

const CalData& get() { return data; }

void setPhNeutral(float voltageAtPh7) {
  data.phNeutralV = voltageAtPh7;
  save();
}

bool setPhSlopePoint(float bufferPh, float voltage) {
  float dPh = bufferPh - 7.0f;
  float dV = voltage - data.phNeutralV;
  if (fabsf(dPh) < 1.0f || fabsf(dV) < 0.01f) return false;   // points too close
  data.phSlope = dV / dPh;
  save();
  return true;
}

bool setTdsFactor(float knownPpm, float measuredPpm) {
  if (knownPpm <= 0.0f || measuredPpm <= 1.0f) return false;
  float f = data.tdsFactor * (knownPpm / measuredPpm);       // measured already includes old factor
  if (f < 0.2f || f > 5.0f) return false;                    // implausible -> reject
  data.tdsFactor = f;
  save();
  return true;
}

void resetDefaults() {
  defaults();
  save();
  stored = false;
}

bool isStored() { return stored; }

}  // namespace Calibration
