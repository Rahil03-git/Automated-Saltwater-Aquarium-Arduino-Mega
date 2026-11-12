#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "display.h"

namespace {
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

const uint8_t PAGE_COUNT = 3;
uint8_t page = 0;
uint32_t lastRefresh = 0;
uint32_t lastPageChange = 0;

void put(uint8_t row, const char* text) {
  char buf[LCD_COLUMNS + 1];
  snprintf(buf, sizeof(buf), "%-16.16s", text);   // pad / cut to 16 characters
  lcd.setCursor(0, row);
  lcd.print(buf);
}

uint8_t bitCount(uint16_t v) {
  uint8_t n = 0;
  while (v) { n += (uint8_t)(v & 1u); v >>= 1; }
  return n;
}

void drawAlarm(const UiState& s) {
  put(0, s.muted ? "! ALARM MUTED !" : "!!   ALARM   !!");
  char l2[24];
  uint8_t n = bitCount(s.alarms);
  if (n > 1) snprintf(l2, sizeof(l2), "%s +%u", aq::alarmShortName(s.alarms), (unsigned)(n - 1));
  else snprintf(l2, sizeof(l2), "%s", aq::alarmShortName(s.alarms));
  put(1, l2);
}

void drawPage(const UiState& s) {
  char a[24], b[24], t[8], p[8], d[8];
  if (page == 0) {
    if (s.r.tempValid) dtostrf(s.r.tempC, 4, 1, t); else snprintf(t, sizeof(t), "--.-");
    dtostrf(s.r.ph, 4, 2, p);
    snprintf(a, sizeof(a), "T:%sC pH:%s", t, p);
#if TDS_ALARM_ENABLED
    bool tdsOk = !aq::outsideRange(s.r.tds, TDS_MIN_PPM, TDS_MAX_PPM);
    snprintf(b, sizeof(b), "TDS:%4dppm %s", (int)(s.r.tds + 0.5f), tdsOk ? "OK" : "!!");
#else
    snprintf(b, sizeof(b), "TDS:%4dppm", (int)(s.r.tds + 0.5f));
#endif
  } else if (page == 1) {
    if (s.r.levelValid) {
      dtostrf(s.r.levelCm, 4, 1, d);
      const char* st = (s.r.levelCm >= LEVEL_LOW_CM) ? "LOW" : (s.r.levelCm <= LEVEL_OVERFLOW_CM ? "HIGH" : "OK");
      snprintf(a, sizeof(a), "Lvl:%scm %s", d, st);
    } else {
      snprintf(a, sizeof(a), "Lvl: --.-cm ERR");
    }
    const char* ps = s.pumpState == 3 ? "FAULT" : (s.pumpOn ? "ON" : (s.pumpState == 2 ? "WAIT" : "OFF"));
    snprintf(b, sizeof(b), "Pump:%s Feed:%u", ps, (unsigned)s.feedsToday);
  } else {
    snprintf(a, sizeof(a), "Time %02u:%02u", (unsigned)(s.minuteOfDay / 60), (unsigned)(s.minuteOfDay % 60));
    const char* mode = s.lightMode == 0 ? "AUTO" : (s.lightMode == 1 ? "ON" : "OFF");
    snprintf(b, sizeof(b), "Light:%3u%% %s", (unsigned)s.lightPct, mode);
  }
  put(0, a);
  put(1, b);
}
}  // namespace

namespace Display {

void begin() {
  lcd.init();
  lcd.backlight();
  put(0, "Saltwater Aquar.");
  put(1, "Starting...");
  delay(1200);
  lcd.clear();
}

void nextPage() {
  page = (uint8_t)((page + 1) % PAGE_COUNT);
  lastPageChange = millis();
  lastRefresh = 0;                          // redraw immediately
}

void update(const UiState& s, uint32_t now) {
  if (now - lastRefresh < LCD_REFRESH_MS && lastRefresh != 0) return;
  lastRefresh = now ? now : 1;

  if (now - lastPageChange >= LCD_PAGE_MS) {
    page = (uint8_t)((page + 1) % PAGE_COUNT);
    lastPageChange = now;
  }
  bool showAlarm = (s.alarms != 0) && (((now / LCD_PAGE_MS) % 2UL) == 0UL);  // alternate with the pages
  if (showAlarm) drawAlarm(s); else drawPage(s);
}

}  // namespace Display
