// =============================================================================
//  Automated Saltwater Aquarium  -  Arduino Mega 2560
//
//  Reads water temperature (DS18B20), pH (PH-4502C), TDS and water level
//  (ultrasonic), shows them on a 16x2 LCD, and automates feeding (servo), lighting
//  (LEDs), water top-up (pump) and safe-range alerts (buzzer).
//
//  Libraries to install (Arduino IDE > Library Manager):
//     OneWire, DallasTemperature, LiquidCrystal I2C   (Servo, EEPROM, Wire are built in)
//  Open the Serial Monitor at 9600 baud and type HELP.
// =============================================================================
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "logic.h"
#include "calibration.h"
#include "aqclock.h"
#include "sensors.h"
#include "actuators.h"
#include "buttons.h"
#include "cli.h"
#include "display.h"

namespace {

enum LightMode : uint8_t { LIGHT_AUTO = 0, LIGHT_FORCE_ON = 1, LIGHT_FORCE_OFF = 2 };
enum PumpMode  : uint8_t { PUMP_AUTO = 0, PUMP_FORCE_OFF = 1, PUMP_MANUAL_ON = 2 };
enum PumpState : uint8_t { PS_IDLE = 0, PS_FILLING = 1, PS_COOLDOWN = 2, PS_FAULT = 3 };

// ---- controller state ---------------------------------------------------------
LightMode lightMode = LIGHT_AUTO;
PumpMode  pumpMode = PUMP_AUTO;
PumpState pumpState = PS_IDLE;
uint32_t  pumpStateSince = 0;
uint8_t   pumpFailures = 0;

bool      fedToday[FEED_TIME_COUNT];
uint8_t   feedsToday = 0;
bool      everFed = false;
uint32_t  lastFeedAbsMinute = 0;

uint16_t  pendingAlarms = 0;
uint8_t   pendingTicks = 0;
uint16_t  activeAlarms = 0;
bool      muted = false;
uint32_t  mutedAt = 0;

uint32_t  lastTick = 0;
uint32_t  lastLog = 0;

// ---- helpers ----------------------------------------------------------------------
uint32_t absoluteMinute() { return AquaClock::dayCount() * 1440UL + AquaClock::minuteOfDay(); }

void markPastFeedTimes() {                       // after power-up or TIME: do not "catch up" old feeds
  uint16_t now = AquaClock::minuteOfDay();
  for (uint8_t i = 0; i < FEED_TIME_COUNT; i++) fedToday[i] = (FEED_TIMES_MIN[i] <= now);
}

void printTimePrefix() {
  char tb[6];
  AquaClock::format(tb);
  Serial.print('[');
  Serial.print(tb);
  Serial.print(F("] "));
}

// ---- feeding ----------------------------------------------------------------------------
bool tryFeed(bool manual) {
  printTimePrefix();
  if (Feeder::busy()) { Serial.println(F("Feeder busy.")); return false; }
  if (feedsToday >= MAX_FEEDS_PER_DAY) { Serial.println(F("Feed refused: daily limit reached (overfeeding protection).")); return false; }
  uint32_t absMin = absoluteMinute();
  if (everFed && absMin >= lastFeedAbsMinute && (absMin - lastFeedAbsMinute) < MIN_FEED_GAP_MIN) {
    Serial.println(F("Feed refused: previous feed was too recent."));
    return false;
  }
  if (!Feeder::request()) return false;
  feedsToday++;
  everFed = true;
  lastFeedAbsMinute = absMin;
  Serial.print(manual ? F("Manual feed #") : F("Scheduled feed #"));
  Serial.println(feedsToday);
  return true;
}

void runFeedSchedule(uint32_t nowMs) {
  uint16_t minute = AquaClock::minuteOfDay();
  for (uint8_t i = 0; i < FEED_TIME_COUNT; i++) {
    if (!fedToday[i] && minute == FEED_TIMES_MIN[i]) {
      fedToday[i] = true;
      if (nowMs < BOOT_FEED_LOCKOUT_MS) continue;       // skip right after a reboot
      tryFeed(false);
    }
  }
}

// ---- lighting ----------------------------------------------------------------------------
void updateLight() {
  uint8_t pct;
  if (lightMode == LIGHT_FORCE_ON) pct = LIGHT_DAY_PERCENT;
  else if (lightMode == LIGHT_FORCE_OFF) pct = 0;
  else pct = aq::lightPercent(AquaClock::minuteOfDay());
  Light::setPercent(pct);
}

// ---- water level / pump ------------------------------------------------------------------
void setPumpState(PumpState s, uint32_t now) { pumpState = s; pumpStateSince = now; }

void updatePump(uint32_t now) {
  const aq::Readings& r = Sensors::readings();
  bool overflow = r.levelValid && r.levelCm <= LEVEL_OVERFLOW_CM;
  bool blocked = (pumpMode == PUMP_FORCE_OFF) || !r.levelValid || overflow;
  bool manual = (pumpMode == PUMP_MANUAL_ON);

  if (pumpState == PS_FAULT) {                          // locked out until PUMP RESET
    Pump::set(false);
    if (manual) pumpMode = PUMP_AUTO;
    return;
  }
  if (blocked) {                                        // safety first: never pump blind or into overflow
    Pump::set(false);
    if (pumpState == PS_FILLING) setPumpState(PS_IDLE, now);
    if (manual) pumpMode = PUMP_AUTO;
    return;
  }

  switch (pumpState) {
    case PS_IDLE:
      if (manual || r.levelCm >= LEVEL_LOW_CM) {
        Pump::set(true);
        setPumpState(PS_FILLING, now);
        printTimePrefix();
        Serial.println(F("Pump ON (top-up)"));
      }
      break;

    case PS_FILLING:
      if (r.levelCm <= LEVEL_FULL_CM) {                 // reached the full mark
        Pump::set(false);
        pumpFailures = 0;
        if (manual) pumpMode = PUMP_AUTO;
        setPumpState(PS_IDLE, now);
        printTimePrefix();
        Serial.println(F("Pump OFF (level OK)"));
      } else if (now - pumpStateSince >= PUMP_MAX_RUN_MS) {   // dry-run / leak protection
        Pump::set(false);
        if (manual) pumpMode = PUMP_AUTO;
        pumpFailures++;
        printTimePrefix();
        if (pumpFailures >= PUMP_MAX_FAILS) {
          setPumpState(PS_FAULT, now);
          Serial.println(F("Pump LOCKED OUT after repeated failed top-ups. Check reservoir and tubing, then type PUMP RESET."));
        } else {
          setPumpState(PS_COOLDOWN, now);
          Serial.println(F("Pump stopped: maximum run time reached."));
        }
      }
      break;

    case PS_COOLDOWN:
      if (manual) {
        Pump::set(true);
        setPumpState(PS_FILLING, now);
      } else if (now - pumpStateSince >= PUMP_COOLDOWN_MS) {
        setPumpState(PS_IDLE, now);
      }
      break;

    default:
      break;
  }
}

// ---- alarms -----------------------------------------------------------------------------------
void printAlarmList(uint16_t mask) {
  static const uint16_t bits[] = {
    aq::ALARM_TEMP_FAULT, aq::ALARM_LEVEL_FAULT, aq::ALARM_PUMP_FAULT, aq::ALARM_LEVEL_HIGH, aq::ALARM_LEVEL_LOW,
    aq::ALARM_TEMP_HIGH, aq::ALARM_TEMP_LOW, aq::ALARM_PH_HIGH, aq::ALARM_PH_LOW, aq::ALARM_TDS_HIGH, aq::ALARM_TDS_LOW };
  bool first = true;
  for (uint8_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++) {
    if (mask & bits[i]) {
      if (!first) Serial.print(F(", "));
      Serial.print(aq::alarmShortName(bits[i]));
      first = false;
    }
  }
  if (first) Serial.print(F("none"));
}

void updateAlarms(uint32_t now) {
  uint16_t raw = aq::evaluateAlarms(Sensors::readings(), pumpState == PS_FAULT);
  if (raw == pendingAlarms) {
    if (pendingTicks < 255) pendingTicks++;
  } else {
    pendingAlarms = raw;
    pendingTicks = 1;
  }
  if (pendingTicks >= ALARM_CONFIRM_TICKS && pendingAlarms != activeAlarms) {
    uint16_t added = pendingAlarms & (uint16_t)~activeAlarms;
    activeAlarms = pendingAlarms;
    if (added) muted = false;                          // a new problem un-mutes the buzzer
    printTimePrefix();
    Serial.print(F("ALARM STATE: "));
    printAlarmList(activeAlarms);
    Serial.println();
  }
  if (muted && now - mutedAt >= ALARM_MUTE_MS) muted = false;
  Buzzer::setAlarm(activeAlarms != 0 && !muted);
}

void muteAlarms(uint32_t now) {
  if (activeAlarms == 0) return;
  muted = true;
  mutedAt = now;
  Buzzer::setAlarm(false);                              // silence immediately, not on the next tick
  printTimePrefix();
  Serial.println(F("Buzzer muted."));
}

// ---- serial output -------------------------------------------------------------------------------
const char* lightModeText() { return lightMode == LIGHT_AUTO ? "AUTO" : (lightMode == LIGHT_FORCE_ON ? "ON" : "OFF"); }
const char* pumpStateText() {
  switch (pumpState) {
    case PS_FILLING: return "FILLING";
    case PS_COOLDOWN: return "COOLDOWN";
    case PS_FAULT: return "FAULT";
    default: return "IDLE";
  }
}

void printLogLine() {
  const aq::Readings& r = Sensors::readings();
  printTimePrefix();
  Serial.print(F("T="));
  if (r.tempValid) Serial.print(r.tempC, 1); else Serial.print(F("ERR"));
  Serial.print(F("C pH="));
  Serial.print(r.ph, 2);
  Serial.print(F(" TDS="));
  Serial.print(r.tds, 0);
  Serial.print(F("ppm L="));
  if (r.levelValid) Serial.print(r.levelCm, 1); else Serial.print(F("ERR"));
  Serial.print(F("cm Light="));
  Serial.print(Light::percent());
  Serial.print(F("% Pump="));
  Serial.print(Pump::isOn() ? F("ON") : F("OFF"));
  Serial.print(F(" Feeds="));
  Serial.print(feedsToday);
  Serial.print(F(" Alarm="));
  printAlarmList(activeAlarms);
  Serial.println();
}

void printStatus() {
  const aq::Readings& r = Sensors::readings();
  char tb[6];
  AquaClock::format(tb);
  Serial.println(F("------ STATUS ------"));
  Serial.print(F("Time: ")); Serial.println(tb);
  Serial.print(F("Temperature: "));                       // same layout as the original displayResults()
  if (r.tempValid) {
    Serial.print(r.tempC, 2); Serial.print(F(" C | "));
    Serial.print(aq::celsiusToFahrenheit(r.tempC), 2); Serial.println(F(" F"));
  } else {
    Serial.println(F("sensor not detected"));
  }
  Serial.print(F("pH Value: ")); Serial.print(r.ph, 2);
  Serial.print(F("  (sensor ")); Serial.print(r.phVoltage, 3); Serial.println(F(" V)"));
  Serial.print(F("TDS Value: ")); Serial.print(r.tds, 0); Serial.println(F(" ppm"));
  Serial.print(F("Water level: "));
  if (r.levelValid) { Serial.print(r.levelCm, 1); Serial.println(F(" cm to surface")); }
  else Serial.println(F("sensor error"));
  Serial.print(F("Light: ")); Serial.print(Light::percent()); Serial.print(F("% ("));
  Serial.print(lightModeText()); Serial.println(')');
  Serial.print(F("Pump: ")); Serial.print(Pump::isOn() ? F("ON") : F("OFF"));
  Serial.print(F(" / ")); Serial.println(pumpStateText());
  Serial.print(F("Feeds today: ")); Serial.println(feedsToday);
  Serial.print(F("Alarms: ")); printAlarmList(activeAlarms); Serial.println();
  const CalData& c = Calibration::get();
  Serial.print(F("Calibration: pH7=")); Serial.print(c.phNeutralV, 3);
  Serial.print(F(" V, slope=")); Serial.print(c.phSlope, 3);
  Serial.print(F(" V/pH, TDS factor=")); Serial.print(c.tdsFactor, 3);
  Serial.println(Calibration::isStored() ? F(" (saved)") : F(" (defaults)"));
}

void printProfile() {
  Serial.println(F("------ SAFE RANGES ------"));
  Serial.print(F("Temperature: ")); Serial.print(TEMP_MIN_C, 1); Serial.print(F(" - ")); Serial.print(TEMP_MAX_C, 1); Serial.println(F(" C"));
  Serial.print(F("pH: ")); Serial.print(PH_MIN, 2); Serial.print(F(" - ")); Serial.println(PH_MAX, 2);
#if TDS_ALARM_ENABLED
  Serial.print(F("TDS: ")); Serial.print(TDS_MIN_PPM, 0); Serial.print(F(" - ")); Serial.print(TDS_MAX_PPM, 0); Serial.println(F(" ppm"));
#else
  Serial.println(F("TDS: alarm disabled in this profile"));
#endif
  Serial.print(F("Level: full <= ")); Serial.print(LEVEL_FULL_CM, 1);
  Serial.print(F(" cm, top-up at >= ")); Serial.print(LEVEL_LOW_CM, 1);
  Serial.print(F(" cm, overflow <= ")); Serial.print(LEVEL_OVERFLOW_CM, 1); Serial.println(F(" cm"));
}

void printHelp() {
  Serial.println(F("Commands: HELP STATUS PROFILE FEED MUTE"));
  Serial.println(F("  LIGHT AUTO|ON|OFF     PUMP AUTO|ON|OFF|RESET     TIME HH:MM"));
  Serial.println(F("  CALPH7                (probe in pH 7 buffer: store neutral voltage)"));
  Serial.println(F("  CALPH <buffer pH>     (probe in pH 4 or 9 buffer: store slope)"));
  Serial.println(F("  CALTDS <ppm>          (probe in a known TDS solution)"));
  Serial.println(F("  CALRESET              (restore default calibration)"));
}

// ---- commands and buttons --------------------------------------------------------------------------
void handleCommand(const Command& c, uint32_t now) {
  const aq::Readings& r = Sensors::readings();
  switch (c.type) {
    case CMD_HELP: printHelp(); break;
    case CMD_STATUS: printStatus(); break;
    case CMD_PROFILE: printProfile(); break;
    case CMD_FEED: tryFeed(true); break;
    case CMD_MUTE: muteAlarms(now); break;
    case CMD_LIGHT_AUTO: lightMode = LIGHT_AUTO; updateLight(); Serial.println(F("Light: AUTO")); break;
    case CMD_LIGHT_ON: lightMode = LIGHT_FORCE_ON; updateLight(); Serial.println(F("Light: forced ON")); break;
    case CMD_LIGHT_OFF: lightMode = LIGHT_FORCE_OFF; updateLight(); Serial.println(F("Light: forced OFF")); break;
    case CMD_PUMP_AUTO: pumpMode = PUMP_AUTO; Serial.println(F("Pump: AUTO")); break;
    case CMD_PUMP_OFF: pumpMode = PUMP_FORCE_OFF; Pump::set(false); Serial.println(F("Pump: forced OFF")); break;
    case CMD_PUMP_ON:
      pumpMode = PUMP_MANUAL_ON;
      Serial.println(F("Pump: manual run (stops at the full mark, at overflow, or after the maximum run time)"));
      break;
    case CMD_PUMP_RESET:
      pumpFailures = 0;
      if (pumpState == PS_FAULT) setPumpState(PS_IDLE, now);
      pumpMode = PUMP_AUTO;
      Serial.println(F("Pump fault cleared."));
      break;
    case CMD_TIME:
      AquaClock::setTime(c.hour, c.minute);
      markPastFeedTimes();
      Serial.print(F("Time set to ")); Serial.print(c.hour); Serial.print(':');
      if (c.minute < 10) Serial.print('0');
      Serial.println(c.minute);
      break;
    case CMD_CAL_PH7:
      Calibration::setPhNeutral(r.phVoltage);
      Serial.print(F("pH 7 point saved: ")); Serial.print(r.phVoltage, 3); Serial.println(F(" V"));
      break;
    case CMD_CAL_PH:
      if (Calibration::setPhSlopePoint(c.value, r.phVoltage)) {
        Serial.print(F("pH slope saved: ")); Serial.print(Calibration::get().phSlope, 3); Serial.println(F(" V per pH"));
      } else {
        Serial.println(F("Slope not saved: use a buffer at least 1 pH unit from 7 (after CALPH7) and wait for a stable reading."));
      }
      break;
    case CMD_CAL_TDS:
      if (Calibration::setTdsFactor(c.value, r.tds)) {
        Serial.print(F("TDS factor saved: ")); Serial.println(Calibration::get().tdsFactor, 3);
      } else {
        Serial.println(F("TDS factor not saved: reading or value implausible."));
      }
      break;
    case CMD_CAL_RESET: Calibration::resetDefaults(); Serial.println(F("Calibration reset to defaults.")); break;
    case CMD_UNKNOWN: Serial.println(F("Unknown command. Type HELP.")); break;
    default: break;
  }
}

void handleButton(uint32_t now) {
  switch (Buttons::poll()) {
    case BTN_FEED:
      Buzzer::chirp();
      tryFeed(true);
      break;
    case BTN_LIGHT:
      lightMode = (LightMode)((lightMode + 1) % 3);
      updateLight();
      Buzzer::chirp();
      printTimePrefix(); Serial.print(F("Light mode: ")); Serial.println(lightModeText());
      break;
    case BTN_PAGE:
      Display::nextPage();
      break;
    case BTN_MUTE:
      muteAlarms(now);
      Buzzer::chirp();
      break;
    default:
      break;
  }
}

// ---- once-per-second control tick -----------------------------------------------------------------------
void controlTick(uint32_t now) {
  if (AquaClock::consumeDayRollover()) {                // new day: reset daily counters
    feedsToday = 0;
    for (uint8_t i = 0; i < FEED_TIME_COUNT; i++) fedToday[i] = false;
    Dosing::newDay();
    printTimePrefix();
    Serial.println(F("New day."));
  }
  runFeedSchedule(now);
  updateLight();
  updatePump(now);
  Heater::update(Sensors::readings());
  Dosing::update(Sensors::readings(), absoluteMinute());
  updateAlarms(now);
}

}  // namespace

// =================================================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  Calibration::begin();
  AquaClock::begin(DEFAULT_START_MINUTE);
  Sensors::begin();
  Feeder::begin();
  Light::begin();
  Pump::begin();
  Buzzer::begin();
  Heater::begin();
  Dosing::begin();
  Buttons::begin();
  Cli::begin();
  Display::begin();
  markPastFeedTimes();

  Serial.println(F("=== Automated Saltwater Aquarium ==="));
  Serial.println(F("Set the clock with:  TIME HH:MM     (type HELP for all commands)"));
  printProfile();
}

void loop() {
  uint32_t now = millis();

  AquaClock::update();
  Sensors::update();
  Feeder::update();
  Buzzer::update();

  handleButton(now);
  Command cmd;
  if (Cli::poll(cmd)) handleCommand(cmd, now);

  if (now - lastTick >= CONTROL_TICK_MS) {
    lastTick = now;
    controlTick(now);
  }
  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    printLogLine();
  }

  UiState ui;
  ui.r = Sensors::readings();
  ui.alarms = activeAlarms;
  ui.muted = muted;
  ui.lightPct = Light::percent();
  ui.lightMode = (uint8_t)lightMode;
  ui.pumpOn = Pump::isOn();
  ui.pumpState = (uint8_t)pumpState;
  ui.feedsToday = feedsToday;
  ui.minuteOfDay = AquaClock::minuteOfDay();
  Display::update(ui, now);
}
