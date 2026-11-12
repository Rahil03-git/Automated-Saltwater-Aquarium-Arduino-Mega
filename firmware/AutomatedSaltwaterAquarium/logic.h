// =============================================================================
//  logic.h - hardware-independent calculations.
//  Everything here is a pure function (no pins, no timers), so it can be unit-tested
//  on a PC. The formulas for pH and TDS are the ones used in the project report.
// =============================================================================
#ifndef AQ_LOGIC_H
#define AQ_LOGIC_H

#include <stdint.h>
#include <math.h>
#include "config.h"

namespace aq {

// ---- measurements ---------------------------------------------------------
struct Readings {
  float tempC;        // water temperature (DS18B20)
  bool  tempValid;
  float ph;           // pH value
  float phVoltage;    // averaged pH sensor voltage (used by calibration)
  float tds;          // TDS in ppm (temperature compensated)
  float tdsVoltage;   // averaged TDS sensor voltage
  float levelCm;      // distance from sensor to water surface
  bool  levelValid;
};

// ---- alarm bits -----------------------------------------------------------
static const uint16_t ALARM_TEMP_HIGH   = 1u << 0;
static const uint16_t ALARM_TEMP_LOW    = 1u << 1;
static const uint16_t ALARM_PH_HIGH     = 1u << 2;
static const uint16_t ALARM_PH_LOW      = 1u << 3;
static const uint16_t ALARM_TDS_HIGH    = 1u << 4;
static const uint16_t ALARM_TDS_LOW     = 1u << 5;
static const uint16_t ALARM_LEVEL_LOW   = 1u << 6;
static const uint16_t ALARM_LEVEL_HIGH  = 1u << 7;
static const uint16_t ALARM_TEMP_FAULT  = 1u << 8;   // DS18B20 missing / unreadable
static const uint16_t ALARM_LEVEL_FAULT = 1u << 9;   // ultrasonic sensor unreadable
static const uint16_t ALARM_PUMP_FAULT  = 1u << 10;  // top-up failed repeatedly

// ---- conversions ----------------------------------------------------------
inline float adcToVoltage(float adcValue) {
  return adcValue * (ADC_VREF / ADC_STEPS);                    // report: value * (5.0 / 1024.0)
}

// Report: pHValue = 7 + ((voltage - 2.5) / slope) + calibrationOffset
inline float phFromVoltage(float voltage, float neutralVoltage, float slopeVPerPh, float offset) {
  return 7.0f + ((voltage - neutralVoltage) / slopeVPerPh) + offset;
}

// Report: compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0)
inline float tdsCompensationCoeff(float tempC) {
  return 1.0f + 0.02f * (tempC - 25.0f);
}

// Report: tds = (133.42 V^3 - 255.86 V^2 + 857.39 V) * 0.5   with V = voltage / coefficient
inline float tdsFromVoltage(float voltage, float tempC) {
  float v = voltage / tdsCompensationCoeff(tempC);
  return (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) * 0.5f;
}

inline float celsiusToFahrenheit(float c) { return (c * 9.0f) / 5.0f + 32.0f; }

// HC-SR04: distance (cm) = echo time (us) / 58
inline float echoUsToCm(uint32_t echoUs) { return (float)echoUs / 58.0f; }

// Median of up to 5 values (used to reject ultrasonic outliers)
inline float medianOf(const float* values, uint8_t count) {
  float s[5];
  if (count == 0) return 0.0f;
  if (count > 5) count = 5;
  for (uint8_t i = 0; i < count; i++) s[i] = values[i];
  for (uint8_t i = 1; i < count; i++) {          // insertion sort
    float key = s[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && s[j] > key) { s[j + 1] = s[j]; j--; }
    s[j + 1] = key;
  }
  return s[count / 2];
}

// ---- ranges and alarms ----------------------------------------------------
inline bool outsideRange(float value, float lo, float hi) { return value < lo || value > hi; }

// The report's checkBuzzer() used "&&" for the temperature and pH tests, which can never
// be true; here every parameter alarms when it leaves its safe band.
inline uint16_t evaluateAlarms(const Readings& r, bool pumpFault) {
  uint16_t m = 0;
  if (!r.tempValid) {
    m |= ALARM_TEMP_FAULT;
  } else {
    if (r.tempC < TEMP_MIN_C) m |= ALARM_TEMP_LOW;
    if (r.tempC > TEMP_MAX_C) m |= ALARM_TEMP_HIGH;
  }
  if (r.ph < PH_MIN) m |= ALARM_PH_LOW;
  if (r.ph > PH_MAX) m |= ALARM_PH_HIGH;
#if TDS_ALARM_ENABLED
  if (r.tds < TDS_MIN_PPM) m |= ALARM_TDS_LOW;
  if (r.tds > TDS_MAX_PPM) m |= ALARM_TDS_HIGH;
#endif
  if (!r.levelValid) {
    m |= ALARM_LEVEL_FAULT;
  } else {
    if (r.levelCm >= LEVEL_ALARM_LOW_CM) m |= ALARM_LEVEL_LOW;
    if (r.levelCm <= LEVEL_OVERFLOW_CM)  m |= ALARM_LEVEL_HIGH;
  }
  if (pumpFault) m |= ALARM_PUMP_FAULT;
  return m;
}

// Short name of the first (most important) alarm for the 16x2 LCD.
inline const char* alarmShortName(uint16_t mask) {
  if (mask & ALARM_TEMP_FAULT)  return "TEMP SENSOR";
  if (mask & ALARM_LEVEL_FAULT) return "LEVEL SENSOR";
  if (mask & ALARM_PUMP_FAULT)  return "PUMP FAULT";
  if (mask & ALARM_LEVEL_HIGH)  return "LEVEL TOO HIGH";
  if (mask & ALARM_LEVEL_LOW)   return "LEVEL TOO LOW";
  if (mask & ALARM_TEMP_HIGH)   return "TEMP TOO HIGH";
  if (mask & ALARM_TEMP_LOW)    return "TEMP TOO LOW";
  if (mask & ALARM_PH_HIGH)     return "PH TOO HIGH";
  if (mask & ALARM_PH_LOW)      return "PH TOO LOW";
  if (mask & ALARM_TDS_HIGH)    return "TDS TOO HIGH";
  if (mask & ALARM_TDS_LOW)     return "TDS TOO LOW";
  return "OK";
}

// ---- lighting --------------------------------------------------------------
// Day-night cycle: ramp up after SUNRISE_MIN, full day, ramp down after SUNSET_MIN, night.
inline uint8_t lightPercent(uint16_t minuteOfDay) {
  const int day = LIGHT_DAY_PERCENT, night = LIGHT_NIGHT_PERCENT;
  int m = (int)minuteOfDay;
  if (m < SUNRISE_MIN) return (uint8_t)night;
  if (m < SUNRISE_MIN + LIGHT_RAMP_MIN)
    return (uint8_t)(night + ((day - night) * (m - SUNRISE_MIN)) / LIGHT_RAMP_MIN);
  if (m < SUNSET_MIN) return (uint8_t)day;
  if (m < SUNSET_MIN + LIGHT_RAMP_MIN)
    return (uint8_t)(day - ((day - night) * (m - SUNSET_MIN)) / LIGHT_RAMP_MIN);
  return (uint8_t)night;
}

// Perceived brightness is roughly quadratic, so map 0-100 % to PWM 0-255 with a square law.
inline uint8_t pwmFromPercent(uint8_t percent) {
  if (percent > 100) percent = 100;
  return (uint8_t)(((uint32_t)percent * percent * 255UL) / 10000UL);
}

}  // namespace aq

#endif  // AQ_LOGIC_H
