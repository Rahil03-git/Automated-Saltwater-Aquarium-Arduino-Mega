#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cli.h"

namespace {
char line[40];
uint8_t len = 0;

bool parseTime(const char* s, uint8_t& h, uint8_t& m) {
  const char* colon = strchr(s, ':');
  if (!colon) return false;
  int hh = atoi(s);
  int mm = atoi(colon + 1);
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
  h = (uint8_t)hh;
  m = (uint8_t)mm;
  return true;
}

void parse(Command& c) {
  c.type = CMD_UNKNOWN;
  c.value = 0;
  c.hour = c.minute = 0;
  for (uint8_t i = 0; line[i]; i++) line[i] = (char)toupper((unsigned char)line[i]);

  char* arg = strchr(line, ' ');
  if (arg) { *arg = 0; arg++; while (*arg == ' ') arg++; } else { arg = line + strlen(line); }

  if (!strcmp(line, "HELP") || !strcmp(line, "?")) c.type = CMD_HELP;
  else if (!strcmp(line, "STATUS")) c.type = CMD_STATUS;
  else if (!strcmp(line, "PROFILE")) c.type = CMD_PROFILE;
  else if (!strcmp(line, "FEED")) c.type = CMD_FEED;
  else if (!strcmp(line, "MUTE")) c.type = CMD_MUTE;
  else if (!strcmp(line, "LIGHT")) {
    if (!strcmp(arg, "AUTO")) c.type = CMD_LIGHT_AUTO;
    else if (!strcmp(arg, "ON")) c.type = CMD_LIGHT_ON;
    else if (!strcmp(arg, "OFF")) c.type = CMD_LIGHT_OFF;
  } else if (!strcmp(line, "PUMP")) {
    if (!strcmp(arg, "AUTO")) c.type = CMD_PUMP_AUTO;
    else if (!strcmp(arg, "ON")) c.type = CMD_PUMP_ON;
    else if (!strcmp(arg, "OFF")) c.type = CMD_PUMP_OFF;
    else if (!strcmp(arg, "RESET")) c.type = CMD_PUMP_RESET;
  } else if (!strcmp(line, "TIME")) {
    if (parseTime(arg, c.hour, c.minute)) c.type = CMD_TIME;
  } else if (!strcmp(line, "CALPH7")) c.type = CMD_CAL_PH7;
  else if (!strcmp(line, "CALPH")) {
    c.value = (float)atof(arg);
    if (c.value > 0.0f && c.value < 14.0f) c.type = CMD_CAL_PH;
  } else if (!strcmp(line, "CALTDS")) {
    c.value = (float)atof(arg);
    if (c.value > 0.0f) c.type = CMD_CAL_TDS;
  } else if (!strcmp(line, "CALRESET")) c.type = CMD_CAL_RESET;
}
}  // namespace

namespace Cli {

void begin() { len = 0; }

bool poll(Command& out) {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (len == 0) continue;               // ignore empty lines / CRLF second char
      line[len] = 0;
      len = 0;
      parse(out);
      return true;
    }
    if (len < sizeof(line) - 1) line[len++] = ch;
  }
  return false;
}

}  // namespace Cli
