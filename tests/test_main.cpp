// =============================================================================
//  Host-side tests: the REAL firmware (all .cpp files + the .ino) runs against a
//  simulated Arduino Mega (see mocks/). Each scenario runs in its own process:
//      ./runner <scenario>
// =============================================================================
#include "AutomatedSaltwaterAquarium.ino"
#include <string>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } else { g_fail++; printf("  FAIL: %s  (line %d)\n", msg, __LINE__); } } while (0)

static bool has(const char* text) { return sim.serialOut.find(text) != std::string::npos; }
static void clearOut() { sim.serialOut.clear(); }
static bool pumpIsOn() { return sim.level[PUMP_RELAY_PIN] == RELAY_ACTIVE_LEVEL; }
static std::string lcdLine(int r) { return std::string(sim.lcd[r]); }

static void run(uint32_t ms) {
  uint32_t end = sim.nowMs + ms;
  while (sim.nowMs < end) { sim.nowMs += 10; loop(); }
}
static void cmd(const char* text, uint32_t settleMs = 100) {
  sim.serialIn += text; sim.serialIn += "\n";
  run(settleMs);
}
static void goodSensors() {                 // pH 6.55, TDS about 335 ppm, 30 C, level 5 cm, buttons released
  sim.tempC = 30.0f; sim.tempConnected = true;
  sim.setPhVoltage(3.85f);
  sim.setTdsVoltage(1.0f);
  sim.levelCm = 5.0f; sim.levelConnected = true;
  sim.level[BTN_FEED_PIN] = HIGH; sim.level[BTN_LIGHT_PIN] = HIGH; sim.level[BTN_PAGE_PIN] = HIGH;
}
static void boot() {
  sim.reset();
  memset(sim_eeprom, 0xFF, sizeof(sim_eeprom));
  goodSensors();
  setup();
}
static void press(uint8_t pin, uint32_t holdMs) {
  sim.level[pin] = LOW; run(holdMs);
  sim.level[pin] = HIGH; run(120);
}

// -------------------------------------------------------------------------------------------
static void t_logic() {
  CHECK(fabsf(aq::adcToVoltage(512) - 2.5f) < 1e-4f, "ADC 512 -> 2.5 V");
  CHECK(fabsf(aq::tdsFromVoltage(1.0f, 25.0f) - 367.475f) < 0.01f, "TDS(1.0 V, 25 C) = 367.475 ppm");
  CHECK(fabsf(aq::tdsFromVoltage(1.0f, 30.0f) - 334.118f) < 0.05f, "TDS(1.0 V, 30 C) = 334.1 ppm (temperature compensation)");
  CHECK(fabsf(aq::tdsCompensationCoeff(25.0f) - 1.0f) < 1e-6f, "compensation = 1.0 at 25 C");
  CHECK(fabsf(aq::phFromVoltage(2.5f, 2.5f, -3.0f, 0.0f) - 7.0f) < 1e-5f, "pH 7.0 at 2.5 V");
  CHECK(fabsf(aq::phFromVoltage(2.2f, 2.5f, -3.0f, 0.0f) - 7.1f) < 1e-4f, "pH 7.1 at 2.2 V (report formula)");
  CHECK(fabsf(aq::celsiusToFahrenheit(30.0f) - 86.0f) < 1e-4f, "30 C = 86 F");
  CHECK(fabsf(aq::echoUsToCm(580) - 10.0f) < 1e-4f, "580 us = 10 cm");
  float v5[5] = {9.0f, 1.0f, 5.0f, 100.0f, 4.0f};
  CHECK(aq::medianOf(v5, 5) == 5.0f, "median of 5 ignores outlier");
  CHECK(aq::lightPercent(6 * 60 + 59) == 0, "light 06:59 = 0 %");
  CHECK(aq::lightPercent(7 * 60 + 30) == 50, "light 07:30 = 50 % (sunrise ramp)");
  CHECK(aq::lightPercent(8 * 60) == 100 && aq::lightPercent(12 * 60) == 100, "light daytime = 100 %");
  CHECK(aq::lightPercent(18 * 60) == 100 && aq::lightPercent(18 * 60 + 30) == 50, "light 18:30 = 50 % (sunset ramp)");
  CHECK(aq::lightPercent(19 * 60) == 0 && aq::lightPercent(23 * 60 + 59) == 0, "light night = 0 %");
  CHECK(aq::pwmFromPercent(0) == 0 && aq::pwmFromPercent(100) == 255 && aq::pwmFromPercent(50) == 63, "PWM square-law mapping");
  aq::Readings r; r.tempC = 31; r.tempValid = true; r.ph = 6.55f; r.tds = 350; r.levelCm = 5; r.levelValid = true;
  r.phVoltage = r.tdsVoltage = 0;
#if ACTIVE_PROFILE == PROFILE_REPORT
  CHECK(aq::evaluateAlarms(r, false) == 0, "no alarm inside the report's safe ranges");
  r.tempC = 27.9f; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_TEMP_LOW, "temperature below 28 C alarms (report code never did: && bug)");
  r.tempC = 35.1f; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_TEMP_HIGH, "temperature above 35 C alarms");
  r.tempC = 31; r.ph = 6.2f; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_PH_LOW, "pH below 6.32 alarms (report code never did)");
  r.ph = 6.9f; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_PH_HIGH, "pH above 6.80 alarms");
  r.ph = 6.55f; r.tds = 250; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_TDS_LOW, "TDS below 300 alarms");
  r.tds = 450; CHECK(aq::evaluateAlarms(r, false) == aq::ALARM_TDS_HIGH, "TDS above 400 alarms");
  r.tds = 350;
#endif
  r.levelCm = 13; CHECK(aq::evaluateAlarms(r, false) & aq::ALARM_LEVEL_LOW, "level far too low alarms");
  r.levelCm = 2.5f; CHECK(aq::evaluateAlarms(r, false) & aq::ALARM_LEVEL_HIGH, "level near overflow alarms");
  r.levelCm = 5; r.levelValid = false; CHECK(aq::evaluateAlarms(r, false) & aq::ALARM_LEVEL_FAULT, "level sensor fault");
  r.levelValid = true; r.tempValid = false; CHECK(aq::evaluateAlarms(r, false) & aq::ALARM_TEMP_FAULT, "temperature sensor fault");
  r.tempValid = true; CHECK(aq::evaluateAlarms(r, true) & aq::ALARM_PUMP_FAULT, "pump fault flag");
}

static void t_normal() {
  boot();
  run(2200);                         // LCD page 0 is shown for the first 4 s (splash takes 1.2 s)
  CHECK(!has("ALARM STATE"), "no alarm with good sensors");
  CHECK(lcdLine(0).find("T:30.0C") == 0, "LCD line 1 shows temperature");
  CHECK(lcdLine(0).find("pH:6.5") != std::string::npos, "LCD line 1 shows pH 6.5x");
  CHECK(lcdLine(1).find("TDS:") == 0 && lcdLine(1).find("ppm") != std::string::npos, "LCD line 2 shows TDS");
#if ACTIVE_PROFILE == PROFILE_REPORT
  CHECK(lcdLine(1).find("OK") != std::string::npos, "TDS status OK");
#endif
  CHECK(sim.pwm[LED_PWM_PIN] == 255, "lights at 100 % at 12:00");
  CHECK(!pumpIsOn(), "pump off when level is full");
  CHECK(sim.level[BUZZER_PIN] == LOW && sim.highWrites[BUZZER_PIN] == 0, "buzzer silent");
  run(2500);
  CHECK(has("T=30.0C") && has("pH=6.5") && has("Alarm=none"), "serial log line has the readings");
  cmd("STATUS");
  CHECK(has("Temperature: 30.00 C | 86.00 F"), "STATUS temperature in C and F (as in the report)");
  CHECK(has("pH Value: 6.5") && has("TDS Value: 33"), "STATUS pH and TDS");
}

static void t_alarms() {
  boot(); run(3500);
  sim.tempC = 36.0f;
  bool sawAlarmScreen = false;
  for (int i = 0; i < 24; i++) { run(500); if (lcdLine(0).find("ALARM") != std::string::npos && lcdLine(1).find("TEMP TOO HIGH") == 0) sawAlarmScreen = true; }
#if ACTIVE_PROFILE == PROFILE_REPORT
  CHECK(has("ALARM STATE: TEMP TOO HIGH"), "temperature alarm confirmed");
#endif
  CHECK(sim.highWrites[BUZZER_PIN] > 0, "buzzer beeps during alarm");
  CHECK(sawAlarmScreen, "LCD alternates to the alarm screen");
  cmd("MUTE", 200);
  int before = sim.highWrites[BUZZER_PIN];
  run(3000);
  CHECK(has("Buzzer muted.") && sim.highWrites[BUZZER_PIN] == before, "MUTE silences the buzzer");
  clearOut(); sim.tempC = 31.0f; run(8000);
  CHECK(has("ALARM STATE: none"), "alarm clears when the value returns to range");
  sim.tempConnected = false; clearOut(); run(12000);
  CHECK(has("TEMP SENSOR"), "missing DS18B20 raises TEMP SENSOR alarm");
  sim.tempConnected = true; run(8000);
#if ACTIVE_PROFILE == PROFILE_REPORT
  clearOut(); sim.setPhVoltage(4.9f); run(10000);
  CHECK(has("PH TOO LOW"), "pH low alarm");
  clearOut(); sim.setPhVoltage(2.5f); run(10000);
  CHECK(has("PH TOO HIGH"), "pH high alarm");
  sim.setPhVoltage(3.85f); run(8000);
  clearOut(); sim.setTdsVoltage(0.5f); run(10000);
  CHECK(has("TDS TOO LOW"), "TDS low alarm");
  clearOut(); sim.setTdsVoltage(1.3f); run(10000);
  CHECK(has("TDS TOO HIGH"), "TDS high alarm");
#endif
}

static void t_feeding() {
  boot(); run(61000);                              // pass the boot lockout (default time 12:00)
  CHECK(sim.servoLog.empty(), "no feed at boot");
  cmd("TIME 17:59"); run(70000);
  CHECK(has("Scheduled feed #1"), "scheduled feed at 18:00");
  CHECK(sim.servoLog == "A,W90,W0,D,", "servo sequence: attach, open 90, close 0, detach");
  clearOut(); cmd("FEED", 2500);
  CHECK(has("too recent"), "second feed within 60 min is refused");
  cmd("TIME 19:30"); clearOut(); cmd("FEED", 3000);
  CHECK(has("Manual feed #2"), "manual feed allowed after the minimum gap");
  cmd("TIME 21:00"); clearOut(); cmd("FEED", 3000);
  CHECK(has("Manual feed #3"), "third feed");
  cmd("TIME 22:30"); clearOut(); cmd("FEED", 3000);
  CHECK(has("Manual feed #4"), "fourth feed");
  cmd("TIME 23:45"); clearOut(); cmd("FEED", 3000);
  CHECK(has("daily limit"), "fifth feed refused: overfeeding protection");
  int attaches = sim.servoAttachCount;
  clearOut(); cmd("TIME 23:59"); run(65000);
  CHECK(has("New day."), "midnight rollover detected");
  cmd("TIME 03:00"); clearOut(); cmd("FEED", 3000);
  CHECK(has("Manual feed #1") && sim.servoAttachCount == attaches + 1, "daily counter reset after midnight");
  clearOut(); cmd("FEED", 3000);
  CHECK(has("too recent"), "gap rule still applies");
}

static void t_lockout() {                          // built with -DBOOT_FEED_LOCKOUT_MS=300000UL
  boot(); cmd("TIME 07:59"); run(70000);
  CHECK(sim.servoLog.empty() && !has("Scheduled feed"), "no scheduled feed inside the boot lockout");
}

static void t_lighting() {
  boot(); run(2000);
  cmd("TIME 06:59"); run(1500);  CHECK(sim.pwm[LED_PWM_PIN] == 0, "night: lights off");
  cmd("TIME 07:30"); run(1500);  CHECK(sim.pwm[LED_PWM_PIN] == aq::pwmFromPercent(50), "07:30: lights at 50 %");
  cmd("TIME 12:00"); run(1500);  CHECK(sim.pwm[LED_PWM_PIN] == 255, "noon: lights at 100 %");
  cmd("TIME 18:30"); run(1500);  CHECK(sim.pwm[LED_PWM_PIN] == aq::pwmFromPercent(50), "18:30: sunset fade");
  cmd("TIME 23:00"); run(1500);  CHECK(sim.pwm[LED_PWM_PIN] == 0, "23:00: lights off");
  cmd("LIGHT ON", 1500);         CHECK(sim.pwm[LED_PWM_PIN] == 255, "LIGHT ON forces lights on at night");
  cmd("TIME 12:00"); cmd("LIGHT OFF", 1500); CHECK(sim.pwm[LED_PWM_PIN] == 0, "LIGHT OFF forces lights off at noon");
  cmd("LIGHT AUTO", 1500);       CHECK(sim.pwm[LED_PWM_PIN] == 255, "LIGHT AUTO resumes the schedule");
}

static void t_pump() {
  boot(); run(4000);
  CHECK(!pumpIsOn(), "pump idle when level is full");
  sim.levelCm = 10.0f; run(6000);
  CHECK(pumpIsOn() && has("Pump ON (top-up)"), "pump starts when water is low (distance >= 9 cm)");
  sim.levelCm = 7.0f; run(5000);
  CHECK(pumpIsOn(), "pump keeps running between the thresholds (hysteresis)");
  sim.levelCm = 4.8f; run(6000);
  CHECK(!pumpIsOn() && has("Pump OFF (level OK)"), "pump stops at the full mark");
  clearOut(); sim.levelCm = 2.5f; run(8000);
  CHECK(!pumpIsOn(), "no pumping when the tank is about to overflow");
#if ACTIVE_PROFILE == PROFILE_REPORT
  CHECK(has("LEVEL TOO HIGH"), "overflow alarm");
#endif
  sim.levelCm = 5.0f; run(6000);
  sim.levelConnected = false; clearOut(); run(12000);
  CHECK(!pumpIsOn() && has("LEVEL SENSOR"), "level sensor failure: pump forced off + alarm");
  sim.levelConnected = true; sim.levelCm = 10.0f; run(8000);
  CHECK(pumpIsOn(), "pump resumes after the sensor recovers");
  cmd("PUMP OFF", 3000);
  CHECK(!pumpIsOn(), "PUMP OFF forces the pump off");
  cmd("PUMP AUTO", 4000);
  CHECK(pumpIsOn(), "PUMP AUTO restores automatic control");
}

static void t_pumpfault() {
  boot(); run(3000);
  sim.levelCm = 10.0f;                                          // water never rises (empty reservoir)
  clearOut(); run(100000);
  CHECK(!pumpIsOn() && has("maximum run time"), "pump stops after the maximum run time");
  run(1500000);
  CHECK(has("LOCKED OUT"), "pump locked out after 3 failed top-ups");
  CHECK(!pumpIsOn(), "pump stays off while locked out");
#if ACTIVE_PROFILE == PROFILE_REPORT
  CHECK(has("PUMP FAULT"), "PUMP FAULT alarm raised");
#endif
  clearOut(); cmd("PUMP RESET", 6000);
  CHECK(has("Pump fault cleared."), "PUMP RESET clears the lockout");
  CHECK(pumpIsOn(), "pump tries again after reset (level still low)");
  clearOut(); cmd("PUMP ON", 1500);
  CHECK(has("manual run"), "manual pump command accepted");
}

static void t_calibration() {
  boot(); run(3000);
  sim.setPhVoltage(2.5f); run(3000);
  cmd("CALPH7"); CHECK(has("pH 7 point saved: 2.500"), "CALPH7 stores 2.500 V");
  sim.setPhVoltage(3.04f); run(3000);
  clearOut(); cmd("CALPH 4");
  CHECK(has("pH slope saved: -0.18"), "CALPH 4 stores slope about -0.18 V/pH");
  run(2000); clearOut(); cmd("STATUS");
  CHECK(has("pH Value: 4.0") || has("pH Value: 3.9"), "pH reads 4.0 at the pH 4 buffer voltage");
  clearOut(); cmd("CALPH 6.5"); CHECK(has("Slope not saved"), "buffer too close to pH 7 rejected");
  // simulate a reboot: values must survive in EEPROM
  setup(); run(3000); clearOut(); cmd("STATUS");
  CHECK(has("(saved)") && has("slope=-0.18"), "calibration restored from EEPROM after reboot");
  // TDS
  sim.tempC = 25.0f; sim.setTdsVoltage(1.0f); run(5000);
  clearOut(); cmd("CALTDS 342");
  CHECK(has("TDS factor saved: 0.9"), "CALTDS stores a factor of about 0.93");
  run(3000); clearOut(); cmd("STATUS");
  CHECK(has("TDS Value: 342") || has("TDS Value: 341") || has("TDS Value: 343"), "TDS reads 342 ppm after calibration");
  clearOut(); cmd("CALRESET"); CHECK(has("Calibration reset"), "CALRESET");
  run(2000); clearOut(); cmd("STATUS"); CHECK(has("(defaults)"), "defaults restored");
}

static void t_ui() {
  boot(); run(2200);
  press(BTN_PAGE_PIN, 100);
  CHECK(lcdLine(0).find("Lvl:") == 0 && lcdLine(0).find("5.0cm") != std::string::npos, "PAGE button: level page");
  CHECK(lcdLine(1).find("Pump:OFF Feed:0") == 0, "level page shows pump and feed count");
  press(BTN_PAGE_PIN, 100);
  CHECK(lcdLine(0).find("Time 12:0") == 0 && lcdLine(1).find("Light:100% AUTO") == 0, "PAGE button: time and light page");
  press(BTN_PAGE_PIN, 100);
  CHECK(lcdLine(0).find("T:30.0C") == 0, "PAGE button wraps to the first page");
  clearOut(); press(BTN_FEED_PIN, 100); run(2000);
  CHECK(has("Manual feed #1") && sim.servoLog == "A,W90,W0,D,", "FEED button feeds once");
  clearOut(); press(BTN_LIGHT_PIN, 100); run(1500);
  CHECK(has("Light mode: ON"), "LIGHT button -> ON");
  press(BTN_LIGHT_PIN, 100); run(1500);
  CHECK(has("Light mode: OFF") && sim.pwm[LED_PWM_PIN] == 0, "LIGHT button -> OFF (lights off)");
  press(BTN_LIGHT_PIN, 100); run(1500);
  CHECK(has("Light mode: AUTO") && sim.pwm[LED_PWM_PIN] == 255, "LIGHT button -> AUTO");
  // long press on the PAGE button mutes an active alarm
  sim.tempC = 36.0f; run(8000);
  clearOut(); press(BTN_PAGE_PIN, 1700);
  CHECK(has("Buzzer muted."), "long press on PAGE mutes the buzzer");
}

static void t_cli() {
  boot(); run(2000);
  clearOut(); cmd("HELP");        CHECK(has("Commands:") && has("CALPH7"), "HELP lists commands");
  clearOut(); cmd("bogus");       CHECK(has("Unknown command"), "unknown command reported");
  clearOut(); cmd("TIME 25:00");  CHECK(has("Unknown command"), "invalid time rejected");
  clearOut(); cmd("TIME 9:05");   CHECK(has("Time set to 9:05"), "TIME 9:05 accepted");
  clearOut(); cmd("light");       CHECK(has("Unknown command"), "LIGHT without argument rejected");
  clearOut(); cmd("status");      CHECK(has("Temperature:") && has("TDS Value:") && has("Alarms:"), "lower-case commands work");
  clearOut(); cmd("PROFILE");     CHECK(has("SAFE RANGES") && has("Level: full"), "PROFILE prints limits");
}

#if ACTIVE_PROFILE == PROFILE_MARINE
static void t_marine() {
  boot(); run(3500);
  sim.tempC = 26.0f; sim.setTdsVoltage(0.1f);
  sim.setPhVoltage(2.5f); run(3000); cmd("CALPH7");
  sim.setPhVoltage(2.0f); run(3000); cmd("CALPH 9.18");        // two-point calibration (pH 7 and 9.18)
  sim.setPhVoltage(2.2f); run(10000);                          // let the calibration-time alarm clear
  clearOut(); run(8000);
  CHECK(!has("ALARM STATE") && has("Alarm=none"), "marine profile: pH about 8.3 at 26 C is safe, TDS 40 ppm raises no alarm");
  sim.setPhVoltage(2.5f); clearOut(); run(10000);
  CHECK(has("PH TOO LOW"), "marine profile: pH 7.0 is too low for a marine tank");
  sim.setPhVoltage(2.2f); sim.tempC = 30.0f; clearOut(); run(10000);
  CHECK(has("TEMP TOO HIGH"), "marine profile: 30 C is too warm");
}
#endif


#if ENABLE_HEATER_RELAY && ENABLE_PH_DOSING
static bool heaterOn() { return sim.level[HEATER_RELAY_PIN] == RELAY_ACTIVE_LEVEL; }
static int doseEdges(uint32_t ms) {                // count OFF->ON transitions of the dosing pump
  int edges = 0; bool prev = sim.level[DOSING_PUMP_PIN] == RELAY_ACTIVE_LEVEL;
  uint32_t end = sim.nowMs + ms;
  while (sim.nowMs < end) { run(100); bool on = sim.level[DOSING_PUMP_PIN] == RELAY_ACTIVE_LEVEL; if (on && !prev) edges++; prev = on; }
  return edges;
}
static void t_optional() {
  boot(); sim.tempC = 31.5f; run(4000);
  CHECK(!heaterOn(), "heater off in range (target 31.5 C +/- 0.5)");
  sim.tempC = 29.0f; run(4000);          CHECK(heaterOn(), "heater switches on below the lower hysteresis limit");
  sim.tempC = 31.5f; run(3000);          CHECK(heaterOn(), "heater stays on inside the hysteresis band");
  sim.tempC = 32.5f; run(3000);          CHECK(!heaterOn(), "heater switches off above the upper limit");
  sim.tempC = 29.0f; run(3000);          CHECK(heaterOn(), "heater on again");
  sim.tempConnected = false; run(8000);  CHECK(!heaterOn(), "fail-safe: heater off when the temperature sensor fails");
  sim.tempConnected = true; sim.tempC = 31.5f; run(5000);
  // pH dosing: pH 6.2 (below 6.32) -> limited pulses
  sim.setPhVoltage(4.9f);
  int d = doseEdges(20000);
  CHECK(d == 1, "exactly one dosing pulse within the minimum gap");
  CHECK(sim.level[DOSING_PUMP_PIN] != RELAY_ACTIVE_LEVEL, "dosing pump stops after the pulse");
  const char* t[] = {"12:35", "13:10", "13:45", "14:20"};
  int total = d;
  for (int i = 0; i < 4; i++) { sim.serialIn += "TIME "; sim.serialIn += t[i]; sim.serialIn += "\n"; total += doseEdges(6000); }
  CHECK(total == 5, "five doses allowed per day");
  sim.serialIn += "TIME 14:55\n"; int extra = doseEdges(6000);
  CHECK(extra == 0, "daily dosing limit enforced");
}
#endif

int main(int argc, char** argv) {
  if (argc < 2) { printf("usage: runner <scenario>\n"); return 2; }
  std::string t = argv[1];
  if (t == "logic") t_logic();
  else if (t == "normal") t_normal();
  else if (t == "alarms") t_alarms();
  else if (t == "feeding") t_feeding();
  else if (t == "lockout") t_lockout();
  else if (t == "lighting") t_lighting();
  else if (t == "pump") t_pump();
  else if (t == "pumpfault") t_pumpfault();
  else if (t == "calibration") t_calibration();
  else if (t == "ui") t_ui();
  else if (t == "cli") t_cli();
#if ENABLE_HEATER_RELAY && ENABLE_PH_DOSING
  else if (t == "optional") t_optional();
#endif
#if ACTIVE_PROFILE == PROFILE_MARINE
  else if (t == "marine") t_marine();
#endif
  else { printf("unknown scenario %s\n", t.c_str()); return 2; }
  printf("%-12s %3d passed, %d failed\n", t.c_str(), g_pass, g_fail);
  if (g_fail) { printf("---- serial output ----\n%s\n", sim.serialOut.c_str()); }
  return g_fail ? 1 : 0;
}
