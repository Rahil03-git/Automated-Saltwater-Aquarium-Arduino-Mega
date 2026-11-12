#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "logic.h"
#include "calibration.h"
#include "sensors.h"

namespace {

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);        // DallasTemperature library for DS18B20 (as in the report)

aq::Readings g;

// ---- pH ----
uint16_t phBuffer[PH_SAMPLE_COUNT];
uint8_t  phIndex = 0;
uint32_t lastPhSample = 0;

// ---- TDS (buffer of SCOUNT samples, as in the report) ----
uint16_t tdsBuffer[TDS_SAMPLE_COUNT];
uint8_t  tdsIndex = 0;
uint32_t lastTdsSample = 0;
uint32_t lastTdsReport = 0;

// ---- temperature ----
bool     tempBusy = false;
uint32_t lastTempRequest = 0;
uint8_t  tempFailures = 0;

// ---- level ----
float    pingValues[5];
uint8_t  pingCount = 0;
uint8_t  pingValid = 0;
bool     levelCycleActive = false;
uint32_t lastPing = 0;
uint32_t levelCycleStart = 0;
uint8_t  levelFailures = 0;

float sumAverage(const uint16_t* buf, uint8_t count) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < count; i++) sum += buf[i];
  return (float)sum / (float)count;
}

float pingOnceCm() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  unsigned long us = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000UL);   // 30 ms ~ 5 m timeout
  if (us == 0) return -1.0f;
  return aq::echoUsToCm((uint32_t)us);
}

void updateTemperature(uint32_t now) {
  if (!tempBusy) {
    if (now - lastTempRequest >= TEMP_REQUEST_MS) {
      ds18b20.requestTemperatures();
      lastTempRequest = now;
      tempBusy = true;
    }
  } else if (now - lastTempRequest >= TEMP_CONVERSION_MS) {
    float t = ds18b20.getTempCByIndex(0);
    tempBusy = false;
    if (t == DEVICE_DISCONNECTED_C || t < -55.0f || t > 125.0f) {
      if (tempFailures < 255) tempFailures++;
      if (tempFailures >= 3) g.tempValid = false;
    } else {
      g.tempC = t;
      g.tempValid = true;
      tempFailures = 0;
    }
  }
}

void updateLevel(uint32_t now) {
  if (!levelCycleActive) {
    if (now - levelCycleStart >= LEVEL_INTERVAL_MS) {
      levelCycleActive = true;
      levelCycleStart = now;
      pingCount = 0;
      pingValid = 0;
      lastPing = now - LEVEL_PING_GAP_MS;     // first ping immediately
    }
    return;
  }
  if (now - lastPing < LEVEL_PING_GAP_MS) return;
  lastPing = now;

  float cm = pingOnceCm();
  if (cm >= LEVEL_VALID_MIN_CM && cm <= LEVEL_VALID_MAX_CM) pingValues[pingValid++] = cm;
  pingCount++;

  if (pingCount >= 5) {
    levelCycleActive = false;
    if (pingValid >= 3) {
      g.levelCm = aq::medianOf(pingValues, pingValid);
      g.levelValid = true;
      levelFailures = 0;
    } else {
      if (levelFailures < 255) levelFailures++;
      if (levelFailures >= 2) g.levelValid = false;
    }
  }
}

}  // namespace

namespace Sensors {

void begin() {
  pinMode(PH_SENSOR_PIN, INPUT);
  pinMode(TDS_SENSOR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  ds18b20.begin();
  ds18b20.setWaitForConversion(false);       // never block the control loop

  g.tempC = TEMP_DEFAULT_C;
  g.tempValid = false;
  g.levelCm = 0.0f;
  g.levelValid = false;

  for (uint8_t i = 0; i < PH_SAMPLE_COUNT; i++) phBuffer[i] = analogRead(PH_SENSOR_PIN);
  for (uint8_t i = 0; i < TDS_SAMPLE_COUNT; i++) tdsBuffer[i] = analogRead(TDS_SENSOR_PIN);

  uint32_t now = millis();
  lastPhSample = lastTdsSample = lastTdsReport = now;
  lastTempRequest = now - TEMP_REQUEST_MS;   // first conversion right away
  levelCycleStart = now - LEVEL_INTERVAL_MS;

  g.phVoltage = aq::adcToVoltage(sumAverage(phBuffer, PH_SAMPLE_COUNT));
  g.tdsVoltage = aq::adcToVoltage(sumAverage(tdsBuffer, TDS_SAMPLE_COUNT));
  const CalData& c = Calibration::get();
  g.ph = aq::phFromVoltage(g.phVoltage, c.phNeutralV, c.phSlope, c.phOffset);
  g.tds = aq::tdsFromVoltage(g.tdsVoltage, g.tempC) * c.tdsFactor;
}

void update() {
  uint32_t now = millis();

  if (now - lastPhSample >= PH_SAMPLE_MS) {
    lastPhSample = now;
    phBuffer[phIndex] = analogRead(PH_SENSOR_PIN);
    phIndex = (uint8_t)((phIndex + 1) % PH_SAMPLE_COUNT);
  }

  if (now - lastTdsSample >= TDS_SAMPLE_MS) {          // sample every 40 ms
    lastTdsSample = now;
    tdsBuffer[tdsIndex] = analogRead(TDS_SENSOR_PIN);   // analogBuffer[analogBufferIndex] in the report
    tdsIndex = (uint8_t)((tdsIndex + 1) % TDS_SAMPLE_COUNT);
  }

  if (now - lastTdsReport >= TDS_REPORT_MS) {          // compute pH and TDS once per second
    lastTdsReport = now;
    const CalData& c = Calibration::get();
    g.phVoltage = aq::adcToVoltage(sumAverage(phBuffer, PH_SAMPLE_COUNT));
    g.ph = aq::phFromVoltage(g.phVoltage, c.phNeutralV, c.phSlope, c.phOffset);
    g.tdsVoltage = aq::adcToVoltage(sumAverage(tdsBuffer, TDS_SAMPLE_COUNT));
    float tempForComp = g.tempValid ? g.tempC : TEMP_DEFAULT_C;
    g.tds = aq::tdsFromVoltage(g.tdsVoltage, tempForComp) * c.tdsFactor;
  }

  updateTemperature(now);
  updateLevel(now);
}

const aq::Readings& readings() { return g; }

}  // namespace Sensors
