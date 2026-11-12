#include <Arduino.h>
#include <Servo.h>
#include "config.h"
#include "logic.h"
#include "actuators.h"

namespace {
inline uint8_t relayLevel(bool on) {
  return on ? RELAY_ACTIVE_LEVEL : (RELAY_ACTIVE_LEVEL == LOW ? HIGH : LOW);
}
}

// ============================ Feeder ========================================
namespace {
Servo feederServo;
enum FeederState : uint8_t { FS_IDLE, FS_OPEN, FS_RETURN };
FeederState feederState = FS_IDLE;
uint32_t feederSince = 0;
}

namespace Feeder {
void begin() { feederState = FS_IDLE; }       // servo stays detached (no jitter) until used

bool request() {
  if (feederState != FS_IDLE) return false;
  feederServo.attach(FEEDER_SERVO_PIN);
  feederServo.write(FEED_SERVO_OPEN);
  feederSince = millis();
  feederState = FS_OPEN;
  return true;
}

void update() {
  uint32_t now = millis();
  if (feederState == FS_OPEN && now - feederSince >= FEED_OPEN_MS) {
    feederServo.write(FEED_SERVO_CLOSED);
    feederSince = now;
    feederState = FS_RETURN;
  } else if (feederState == FS_RETURN && now - feederSince >= FEED_RETURN_MS) {
    feederServo.detach();
    feederState = FS_IDLE;
  }
}

bool busy() { return feederState != FS_IDLE; }
}

// ============================ Light =========================================
namespace {
uint8_t lightPct = 0;
}

namespace Light {
void begin() {
  pinMode(LED_PWM_PIN, OUTPUT);
  setPercent(0);
}
void setPercent(uint8_t percent) {
  if (percent > 100) percent = 100;
  lightPct = percent;
  analogWrite(LED_PWM_PIN, aq::pwmFromPercent(percent));
}
uint8_t percent() { return lightPct; }
}

// ============================ Pump ==========================================
namespace {
bool pumpOn = false;
}

namespace Pump {
void begin() {
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  set(false);
}
void set(bool on) {
  pumpOn = on;
  digitalWrite(PUMP_RELAY_PIN, relayLevel(on));
}
bool isOn() { return pumpOn; }
}

// ============================ Buzzer ========================================
namespace {
bool buzzerAlarm = false;
bool chirpActive = false;
uint32_t chirpStart = 0;
}

namespace Buzzer {
void begin() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);               // initially off, as in the report
}
void setAlarm(bool active) { buzzerAlarm = active; }
void chirp() {
  chirpActive = true;
  chirpStart = millis();
}
void update() {
  uint32_t now = millis();
  bool on = false;
  if (chirpActive) {
    if (now - chirpStart < 80) on = true; else chirpActive = false;
  }
  if (!on && buzzerAlarm) on = (now % 1000UL) < 300UL;   // 0.3 s beep every second
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}
}

// ============================ Heater (optional) =============================
namespace {
bool heaterOn = false;
}

namespace Heater {
void begin() {
#if ENABLE_HEATER_RELAY
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  digitalWrite(HEATER_RELAY_PIN, relayLevel(false));
#endif
  heaterOn = false;
}

void update(const aq::Readings& r) {
#if ENABLE_HEATER_RELAY
  bool want = heaterOn;
  if (!r.tempValid || r.tempC >= TEMP_MAX_C) {
    want = false;                                         // fail-safe: never heat blind or when hot
  } else if (r.tempC <= HEATER_TARGET_C - HEATER_HYSTERESIS_C) {
    want = true;
  } else if (r.tempC >= HEATER_TARGET_C + HEATER_HYSTERESIS_C) {
    want = false;
  }
  if (want != heaterOn) {
    heaterOn = want;
    digitalWrite(HEATER_RELAY_PIN, relayLevel(want));
  }
#else
  (void)r;
#endif
}

bool isOn() { return heaterOn; }
}

// ============================ pH dosing (optional) ==========================
namespace {
bool dosingActive = false;
uint8_t dosesToday = 0;
bool dosedBefore = false;
#if ENABLE_PH_DOSING
uint32_t dosingStart = 0;
uint32_t lastDoseAbsMin = 0;
#endif
}

namespace Dosing {
void begin() {
#if ENABLE_PH_DOSING
  pinMode(DOSING_PUMP_PIN, OUTPUT);
  digitalWrite(DOSING_PUMP_PIN, relayLevel(false));
#endif
  dosingActive = false;
  dosesToday = 0;
  dosedBefore = false;
}

void newDay() { dosesToday = 0; }

void update(const aq::Readings& r, uint32_t absoluteMinute) {
#if ENABLE_PH_DOSING
  uint32_t now = millis();
  if (dosingActive) {
    if (now - dosingStart >= DOSING_PULSE_MS) {
      dosingActive = false;
      digitalWrite(DOSING_PUMP_PIN, relayLevel(false));
    }
    return;
  }
  bool gapOk = !dosedBefore || (absoluteMinute - lastDoseAbsMin) >= DOSING_MIN_GAP_MIN;
  if (r.ph < PH_MIN && dosesToday < DOSING_MAX_PER_DAY && gapOk) {
    dosingActive = true;
    dosingStart = now;
    dosesToday++;
    dosedBefore = true;
    lastDoseAbsMin = absoluteMinute;
    digitalWrite(DOSING_PUMP_PIN, relayLevel(true));
  }
#else
  (void)r; (void)absoluteMinute;
#endif
}
}
